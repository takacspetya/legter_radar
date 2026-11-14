# Légtér-radar

ESP32-S3 + kör alakú kijelző, amely radarszerűen mutatja a környéken repülő gépeket.
Az adatok WiFi-n keresztül az ingyenes, kulcs nélküli [airplanes.live](https://airplanes.live/api-guide/) API-ból jönnek, tartalékként az [adsb.lol](https://api.adsb.lol/docs) API-ból.

Natív ESP-IDF projekt (v5.3 vagy újabb, v5.5.1-gyel tesztelve), Arduino nélkül.

## Funkciók

- Radarnézet pásztázó sugárral, három távolsággyűrűvel és északi jelöléssel
- A gépek iránya szerint forgatott jelölők, magasság szerinti színezéssel (narancs = alacsonyan, zöld = közepesen, lila = magasan, szürke = a földön)
- Egyperces előrejelző vonal és hívójel a 8 legközelebbi gépnél
- Folyamatos mozgás: két lekérdezés között holtszámítással vezeti tovább a pozíciókat
- Részletes nézet: hívójel, típus, lajstromjel, magasság (emelkedés/süllyedés jelzéssel), sebesség, irány, távolság, irányszög
- Választható mértékegységek: km / m / km/h vagy nm / ft / kt

## Hardver

- ESP32-S3 fejlesztőpanel (PSRAM nem kell)
- GC9A01 kör alakú kijelző, 240×240, SPI
- Gomb: a panel BOOT gombja (GPIO0), külön gombot nem kell bekötni

### Bekötés (alapértelmezés, menuconfig-ban átírható)

| Kijelző  | ESP32-S3 |
|----------|----------|
| VCC      | 3V3      |
| GND      | GND      |
| SCL/SCK  | GPIO12   |
| SDA/MOSI | GPIO11   |
| CS       | GPIO10   |
| DC       | GPIO9    |
| RST      | GPIO14   |
| BL       | GPIO13   |

## Fordítás és feltöltés

**VS Code (ESP-IDF bővítmény):** nyisd meg a mappát, majd:

1. Állítsd a célt `esp32s3`-ra (alsó állapotsor).
2. Nyisd meg a *SDK Configuration Editor*-t (menuconfig), és a **Legter radar** menüben add meg:
   - a WiFi nevét és jelszavát,
   - a saját koordinátáidat (alapból Budapest),
   - a mértékegységet és a frissítési időt,
   - szükség esetén a lábkiosztást (*Display* almenü).
3. *Build, Flash and Monitor*.

**Parancssorból (ESP-IDF terminálban):**

```
idf.py set-target esp32s3
idf.py menuconfig
idf.py -p COM5 flash monitor
```

A beállítások az `sdkconfig` fájlba kerülnek (a WiFi jelszóval együtt), ezért ez a fájl nincs verziókezelve.

## Kezelés (BOOT gomb)

| Nézet     | Rövid nyomás    | Hosszú nyomás (0,6 s) |
|-----------|-----------------|-----------------------|
| Radar     | hatótáv váltása | részletes nézet       |
| Részletes | következő gép   | vissza a radarhoz     |

A radar alján lévő állapotsor jelentése:

- `12 AC`: ennyi gép van a hatótávon belül
- `CONNECTING` / `NO WIFI`: nincs kapcsolat
- `HTTP 429`: túl sok lekérdezés, `HTTP -1`: hálózati hiba
- `DATA ERR`: a válasz nem értelmezhető
- `… 45s`: az adat ennyi másodperce nem frissült

## Ha furcsa a kép

A *Display* almenüben:

- felcserélt piros/kék szín → **BGR color order**
- negatív színek → **Invert colors**
- tükrözött kép → **Mirror horizontally**

## Felépítés

| Fájl | Feladat |
|------|---------|
| `main/main.cpp` | indítás, gomb, képfrissítési ciklus (~30 fps) |
| `main/wifi.cpp` | WiFi kapcsolat, automatikus újracsatlakozás |
| `main/feed.cpp` | lekérdezés háttérszálon, HTTPS tanúsítvány-ellenőrzéssel |
| `main/json_stream.cpp` | folyamatos JSON-feldolgozó: a választ darabonként dolgozza fel, nem kell egyben a memóriában tartani |
| `main/display.cpp` | GC9A01 meghajtás `esp_lcd`-vel, DMA-s átvitel |
| `main/gfx.cpp` | képpuffer és rajzolóprimitívek (vonal, kör, háromszög, szöveg) |
| `main/radar.cpp` | radar- és részletes nézet |

Más kijelzőhöz csak a `display.cpp`-t kell átírni. A rajzolás felbontásfüggetlen, a méret a menuconfig-ban állítható.

## Megjegyzések

- Az API-k szabályai: legfeljebb 1 lekérdezés másodpercenként, és csak nem kereskedelmi használat. Az alapértelmezett 10 másodperces frissítés bőven belefér.
- A megjelenített adatok tájékoztató jellegűek, navigációra nem alkalmasak.
