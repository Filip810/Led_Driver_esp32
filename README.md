# LedControlProj

Sterownik diody **RGB LED** oparty na **ESP32**, zbudowany na **ESP-IDF** i **FreeRTOS**,
z interfejsem webowym dostępnym przez przeglądarkę przez **WiFi**.
System budowany przez **CMake + Ninja**.

---

## Funkcje

- Sterowanie kolorem RGB przez indywidualne suwaki R, G, B (0–255)
- Regulacja jasności wszystkich kanałów jednocześnie (0–100%)
- Włączenie / wyłączenie przez przełącznik Power
- Timer - automatyczne wyłączenie po zadanym czasie (np. 10 s)
- Interfejs webowy serwowany bezpośrednio przez ESP32 (brak zewnętrznego serwera)
- Obsługa diod z **anodą wspólną** i **katodą wspólną** (konfigurowalnie)
- Modułowa architektura komponentów ESP-IDF z mutexem FreeRTOS

---

## Wymagania

| Narzędzie | Wersja |
| --------- | ------ |
| ESP-IDF   | ≥ 5.0  |
| CMake     | ≥ 3.16 |
| Ninja     | ≥ 1.10 |
| Python    | ≥ 3.8  |

> Instalator ESP-IDF dla Windows: https://dl.espressif.com/dl/esp-idf/

---

## Struktura projektu

```
LedControlProj/
├── CMakeLists.txt                    ← Główny CMake ESP-IDF
├── sdkconfig.defaults                ← Domyślna konfiguracja
├── README.md
│
├── main/
│   ├── CMakeLists.txt
│   └── main.c                        ← app_main: init + start serwera
│
├── components/
│   ├── led_control/                  ← Sterowanie RGB przez PWM (LEDC)
│   │   ├── include/led_control.h     ← Publiczne API
│   │   ├── led_control.c
│   │   └── test/                     ← Testy jednostkowe (Unity)
│   │
│   ├── wifi_manager/                 ← Połączenie WiFi STA
│   │   ├── Kconfig.projbuild         ← Konfiguracja SSID/hasła
│   │   └── wifi_manager.c
│   │
│   └── http_server/                  ← Serwer HTTP + REST API
│       ├── http_server.c             ← 6 handlerów URI
│       └── web/index.html            ← Interfejs webowy (wbudowany w firmware)
│
└── test/                             ← Standalone aplikacja testowa Unity
```

---

## Schemat podłączenia

```
ESP32 DevKit V1          Dioda RGB (wspólna anoda)
─────────────────        ──────────────────────────
GPIO 25 (PWM) ──[220Ω]──► R (czerwony)
GPIO 26 (PWM) ──[220Ω]──► G (zielony)
GPIO 27 (PWM) ──[220Ω]──► B (niebieski)
3.3V          ────────────► Anoda wspólna (+)
```

---

## Konfiguracja

### Typ diody LED

W pliku [components/led_control/led_control.c](components/led_control/led_control.c):

```c
#define LED_COMMON_ANODE  1   // wspólna anoda  (+) → 3.3V
#define LED_COMMON_ANODE  0   // wspólna katoda (-) → GND
```

### Dane WiFi

Edytuj [sdkconfig.defaults](sdkconfig.defaults):

```
CONFIG_WIFI_SSID="nazwa_sieci"
CONFIG_WIFI_PASSWORD="haslo"
```

Lub przez menuconfig:

```bash
idf.py menuconfig   # → WiFi Configuration
```

---

## Budowanie i wgrywanie

```powershell
# Otwórz ESP-IDF PowerShell (ze Start Menu)

idf.py set-target esp32
idf.py build
idf.py -p COM4 flash monitor
```

Po uruchomieniu w monitorze pojawi się adres IP ESP32:

```
I (xxx) WIFI: Connected  IP: 192.168.x.xx
```

Otwórz ten adres w przeglądarce — interfejs gotowy.

Wyjście z monitora: `Ctrl+]`

---

## REST API

| Metoda | URL           | Ciało żądania             | Opis                       |
| ------ | ------------- | ------------------------- | -------------------------- |
| GET    | `/`           | —                         | Strona interfejsu webowego |
| GET    | `/state`      | —                         | Aktualny stan LED (JSON)   |
| POST   | `/color`      | `{"r":255,"g":0,"b":128}` | Ustaw kolor RGB            |
| POST   | `/brightness` | `{"value":80}`            | Ustaw jasność (0–100%)     |
| POST   | `/power`      | `{"on":true}`             | Włącz / wyłącz             |
| POST   | `/timer`      | `{"seconds":10}`          | Timer (0 = anuluj)         |

---

## Opis ważnych funkcji

### `led_rgb_init()`

Inicjalizuje timer LEDC (12-bit, 5 kHz) i trzy kanały PWM na GPIO 25, 26, 27.
Tworzy mutex FreeRTOS chroniący współdzielony stan.

### `led_rgb_set_color(r, g, b)`

Ustawia kolor diody. Wartości skalowane przez aktualną jasność.
Przy `LED_COMMON_ANODE = 1` duty cycle jest automatycznie odwracany.

### `led_rgb_set_brightness(brightness)`

Reguluje jasność globalną (0–100%) bez zmiany zapisanego koloru R/G/B.

### `led_rgb_set_power(on)`

Włącza lub wyłącza diodę. Przy wyłączeniu anuluje aktywny timer.

### `led_rgb_timer_start(seconds)`

Włącza diodę i tworzy task FreeRTOS odliczający czas.
Po upływie dioda wyłącza się automatycznie.

### `led_rgb_get_state(out)`

Kopiuje aktualny stan (kolor, jasność, power, timer) do struktury `led_rgb_state_t`.
Operacja atomowa wykonywana pod mutexem.

### `wifi_manager_init()` / `wifi_manager_wait_connected()`

Inicjalizuje WiFi w trybie STA i czeka na uzyskanie adresu IP.
Automatycznie ponawia połączenie po utracie sygnału (max 5 prób).

### `http_server_start()`

Uruchamia serwer HTTP na porcie 80 i rejestruje 6 handlerów URI dla REST API.
Plik HTML serwowany z pamięci Flash (wbudowany przez `EMBED_TXTFILES`).

---

## Testy jednostkowe

```powershell
cd test
idf.py set-target esp32
idf.py build flash monitor
```

Wynik w monitorze (format Unity):

```
TEST(led_control, init_configures_gpio) PASS
2 Tests 0 Failures 0 Ignored
OK
```

---

## Licencja

MIT
