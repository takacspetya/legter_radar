#include "feed.h"

#include <ctype.h>
#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "json_stream.h"
#include "wifi.h"

// ADSBExchange v2 kompatibilis végpontok, kulcs nélkül.
// Paraméterek: lat, lon, sugár tengeri mérföldben.
static const char *const kApis[] = {
    "https://api.airplanes.live/v2/point/%.4f/%.4f/%d",
    "https://api.adsb.lol/v2/lat/%.4f/lon/%.4f/dist/%d",
};

namespace {

const char *TAG = "feed";
constexpr double kEarthRadiusNm = 3440.065;

Snapshot shared;  // a mutex védi
Snapshot work;    // csak a lekérdező task használja
SemaphoreHandle_t mutex;
double lat0, lon0;

inline double deg2rad(double d) { return d * M_PI / 180.0; }

// Kelet/észak eltolás tengeri mérföldben a saját pozíciótól.
// Ekvirektanguláris közelítés – pár száz km-en belül bőven elég pontos.
void offsetNm(double lat, double lon, float &east, float &north) {
  east = (float)(deg2rad(lon - lon0) * cos(deg2rad((lat0 + lat) / 2)) * kEarthRadiusNm);
  north = (float)(deg2rad(lat - lat0) * kEarthRadiusNm);
}

// Másolás a záró szóközök levágásával (a hívójelek szóközzel vannak kitöltve).
void copyStr(char *dst, size_t n, const char *src, bool upper = false) {
  size_t len = strnlen(src, n - 1);
  while (len && src[len - 1] == ' ') len--;
  for (size_t i = 0; i < len; ++i) dst[i] = upper ? toupper((unsigned char)src[i]) : src[i];
  dst[len] = '\0';
}

// Ha betelt a tömb, a legtávolabbit cseréljük, ha az új közelebb van.
void insertNearest(const Aircraft &a) {
  if (work.count < kMaxAircraft) {
    work.ac[work.count++] = a;
    return;
  }
  float farD = a.east * a.east + a.north * a.north;
  int far = -1;
  for (int i = 0; i < work.count; ++i) {
    const Aircraft &b = work.ac[i];
    float d = b.east * b.east + b.north * b.north;
    if (d > farD) {
      farD = d;
      far = i;
    }
  }
  if (far >= 0) work.ac[far] = a;
}

// A válasz {"ac":[{...}, ...], ...} alakú; a gépek a 3. mélységben vannak.
struct AircraftHandler : JsonStream::Handler {
  bool inAc = false;
  bool hasLat = false, hasLon = false;
  double lat = 0, lon = 0;
  Aircraft cur{};

  void onStart(int depth, const char *key, bool isObject) override {
    if (depth == 2 && !isObject && strcmp(key, "ac") == 0) {
      inAc = true;
    } else if (inAc && depth == 3 && isObject) {
      cur = Aircraft{};
      cur.gsKt = NAN;
      cur.trackDeg = NAN;
      hasLat = hasLon = false;
    }
  }

  void onEnd(int depth, bool isObject) override {
    if (inAc && depth == 3 && isObject && hasLat && hasLon) {
      offsetNm(lat, lon, cur.east, cur.north);
      insertNearest(cur);
    } else if (depth == 2 && !isObject) {
      inAc = false;
    }
  }

  void onValue(int depth, const char *key, const char *v, bool isString) override {
    if (!inAc || depth != 3) return;
    bool isNum = !isString && strcmp(v, "null") != 0;
    if (!strcmp(key, "hex")) copyStr(cur.hex, sizeof cur.hex, v, true);
    else if (!strcmp(key, "flight")) copyStr(cur.callsign, sizeof cur.callsign, v);
    else if (!strcmp(key, "t")) copyStr(cur.type, sizeof cur.type, v);
    else if (!strcmp(key, "r")) copyStr(cur.reg, sizeof cur.reg, v);
    else if (!isNum && !strcmp(key, "alt_baro")) cur.onGround = isString;  // "ground"
    else if (!isNum) return;
    else if (!strcmp(key, "lat")) { lat = strtod(v, nullptr); hasLat = true; }
    else if (!strcmp(key, "lon")) { lon = strtod(v, nullptr); hasLon = true; }
    else if (!strcmp(key, "alt_baro")) { cur.altFt = (int32_t)strtod(v, nullptr); cur.hasAlt = true; }
    else if (!strcmp(key, "gs")) cur.gsKt = strtof(v, nullptr);
    else if (!strcmp(key, "track")) cur.trackDeg = strtof(v, nullptr);
    else if (!strcmp(key, "baro_rate")) cur.vRateFpm = (int16_t)strtol(v, nullptr, 10);
    else if (!strcmp(key, "seen_pos")) cur.posAgeS = strtof(v, nullptr);
  }
};

bool fetch(const char *fmt) {
  char url[160];
  snprintf(url, sizeof url, fmt, lat0, lon0, units::fetchRadiusNm());

  esp_http_client_config_t cfg = {};
  cfg.url = url;
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  cfg.timeout_ms = 8000;
  cfg.user_agent = "legter-radar/1.0";
  esp_http_client_handle_t client = esp_http_client_init(&cfg);

  bool ok = false;
  work.lastHttpCode = -1;
  work.status = FeedStatus::HttpError;
  if (esp_http_client_open(client, 0) == ESP_OK) {
    esp_http_client_fetch_headers(client);
    work.lastHttpCode = esp_http_client_get_status_code(client);
    if (work.lastHttpCode == 200) {
      static AircraftHandler handler;
      handler = AircraftHandler{};
      JsonStream parser(handler);
      work.count = 0;
      char buf[1024];
      int n;
      while ((n = esp_http_client_read(client, buf, sizeof buf)) > 0) parser.feed(buf, n);
      ok = parser.complete();
      work.status = ok ? FeedStatus::Ok : FeedStatus::ParseError;
    }
  }
  esp_http_client_close(client);
  esp_http_client_cleanup(client);

  if (ok) {
    work.fetchedAtMs = millis();
    ESP_LOGI(TAG, "%u gep", work.count);
  } else {
    ESP_LOGW(TAG, "%s -> HTTP %d", url, work.lastHttpCode);
  }
  return ok;
}

void publish(bool ok) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  if (ok) {
    shared = work;
  } else {
    // A régi adatok maradnak a kijelzőn, csak az állapot frissül.
    shared.status = work.status;
    shared.lastHttpCode = work.lastHttpCode;
  }
  xSemaphoreGive(mutex);
}

void fetchTask(void *) {
  for (;;) {
    if (wifiConnected()) {
      bool ok = false;
      for (const char *api : kApis) {
        if ((ok = fetch(api))) break;
      }
      publish(ok);
    } else {
      work.status = FeedStatus::Connecting;
      publish(false);
    }
    vTaskDelay(pdMS_TO_TICKS(CONFIG_RADAR_FETCH_INTERVAL_S * 1000));
  }
}

}  // namespace

void feedBegin() {
  lat0 = homeLat();
  lon0 = homeLon();
  mutex = xSemaphoreCreateMutex();
  shared.status = work.status = FeedStatus::Connecting;
  xTaskCreatePinnedToCore(fetchTask, "feed", 10240, nullptr, 5, nullptr, 0);
}

void feedCopy(Snapshot &out) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  out = shared;
  xSemaphoreGive(mutex);
}
