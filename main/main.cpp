#include "display.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "feed.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "radar.h"
#include "wifi.h"

namespace {

constexpr uint32_t kLongPressMs = 600;
constexpr TickType_t kFramePeriod = pdMS_TO_TICKS(33);  // ~30 fps

enum class Press { None, Short, Long };

Snapshot snap;
UiState ui = {ViewMode::Radar, units::kDefaultRange, ""};

Press pollButton() {
  static bool down = false, longFired = false;
  static uint32_t since = 0;
  bool now = gpio_get_level((gpio_num_t)CONFIG_RADAR_BUTTON_GPIO) == 0;
  uint32_t t = millis();
  if (now && !down) {
    down = true;
    longFired = false;
    since = t;
  } else if (now && !longFired && t - since >= kLongPressMs) {
    longFired = true;
    return Press::Long;
  } else if (!now && down) {
    down = false;
    if (!longFired && t - since > 30) return Press::Short;  // pergésmentesítés
  }
  return Press::None;
}

void handleButton() {
  switch (pollButton()) {
    case Press::Short:
      if (ui.mode == ViewMode::Radar) {
        ui.rangeIdx = (ui.rangeIdx + 1) % units::kRangeCount;
      } else {
        radarSelectNext(snap, ui);
      }
      break;
    case Press::Long:
      if (ui.mode == ViewMode::Radar) {
        ui.mode = ViewMode::Detail;
        ui.selectedHex[0] = '\0';  // a legközelebbivel kezd
      } else {
        ui.mode = ViewMode::Radar;
      }
      break;
    case Press::None:
      break;
  }
}

}  // namespace

extern "C" void app_main() {
  gpio_config_t btn = {};
  btn.pin_bit_mask = 1ULL << CONFIG_RADAR_BUTTON_GPIO;
  btn.mode = GPIO_MODE_INPUT;
  btn.pull_up_en = GPIO_PULLUP_ENABLE;
  ESP_ERROR_CHECK(gpio_config(&btn));

  displayBegin();
  auto *fb = (uint16_t *)heap_caps_malloc(kLcdSize * kLcdSize * sizeof(uint16_t),
                                          MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if (!fb) {
    ESP_LOGE("main", "nincs eleg memoria a keppufferhez");
    return;
  }
  Canvas canvas(kLcdSize, kLcdSize, fb);
  radarBegin(canvas);
  radarSplash(canvas, "WiFi...");
  displayFlush(fb);

  wifiBegin();
  feedBegin();

  TickType_t last = xTaskGetTickCount();
  for (;;) {
    handleButton();
    feedCopy(snap);
    radarDraw(canvas, snap, ui, wifiConnected());
    displayFlush(fb);
    xTaskDelayUntil(&last, kFramePeriod);
  }
}
