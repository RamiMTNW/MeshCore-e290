# MeshCore E290 Standalone

Autonomiczny komunikator mesh na **Heltec Vision Master E290** z klawiaturą **CardKB**.

- **Protokół:** MeshCore (LoRa, 868 MHz EU Narrow)
- **Wyświetlacz:** E-Ink 2.9" (296×128 px)
- **Klawiatura:** M5Stack CardKB (I²C)
- **MCU:** ESP32-S3R8
- **Radio:** SX1262

---

## Sprzęt

| Element | Model | Uwagi |
|---------|-------|-------|
| Płytka | Heltec Vision Master E290 v0.3.x | Z modułem LoRa SX1262 |
| Klawiatura | M5Stack CardKB | Złącze Grove / dupont |
| Antena | LoRa 868 MHz | Wymagana przed wgraniem! |

### Podłączenie CardKB

| CardKB pin | E290 pin | GPIO |
|-----------|---------|------|
| GND | GND | — |
| 3V3 | 3V3 | — |
| SDA | SDA | **GPIO 17** |
| SCL | SCL | **GPIO 18** |

> Jeśli masz złącze Grove: użyj kabla Grove → dupont lub złącza SH2.0

---

## Interfejs użytkownika

```
┌─────────────────────────────────────────────────────────────┐
│ E290_ABCD    RSSI:-87 SNR:4.2    83%   ●                   │  ← Status bar
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Alice: Hej, ktoś nas słyszy?                              │
│                              Tak, tu E290_ABCD!  ▌         │
│                                                             │
│  > Wpisz wiadomość tutaj█                                   │  ← Input
├─────────────────────────────────────────────────────────────┤
│ [CHAT]  [ DM ] [NODES] [INFO]                              │  ← Zakładki
└─────────────────────────────────────────────────────────────┘
```

### Sterowanie klawiaturą

| Klawisz | Akcja |
|---------|-------|
| **Enter** | Wyślij wiadomość |
| **Backspace** | Usuń ostatni znak |
| **Tab** | Przełącz zakładkę |
| **Fn+1** | Zakładka CHAT |
| **Fn+2** | Zakładka DM |
| **Fn+3** | Zakładka NODES |
| **Fn+4** | Zakładka INFO |
| **ESC** | Wyczyść input / wróć do CHAT |
| **Przycisk BOOT** | Przełącz zakładkę |

---

## Kompilacja

### Opcja A: GitHub Actions (zalecana — bez instalacji)

1. Zrób **Fork** tego repozytorium na GitHub
2. Wejdź w **Actions** → uruchom workflow `Build MeshCore E290 Firmware`
3. Po zakończeniu pobierz artefakt `meshcore-e290-firmware-*.zip`
4. Wgraj plik `meshcore-e290-merged.bin` na płytkę

### Opcja B: Lokalnie (macOS)

```bash
# Zainstaluj narzędzia
xcode-select --install
pip3 install platformio esptool

# Klonuj i zbuduj
git clone https://github.com/TWOJ-USER/meshcore-e290.git
cd meshcore-e290
pio run -e VME290_companion

# Wgraj na płytkę
pio run -e VME290_companion --target upload
```

---

## Wgrywanie firmware

### Tryb DFU (wymagany dla ESP32-S3)

1. Odłącz USB
2. Przytrzymaj przycisk **BOOT** (GPIO 0)
3. Podłącz USB
4. Puść **BOOT** po 2-3 sekundach

Urządzenie pojawi się jako port szeregowy gotowy do wgrania.

### Przez esptool (merged binary)

```bash
esptool.py --chip esp32s3 \
           --port /dev/tty.usbserial-XXXX \
           --baud 921600 \
           write_flash 0x0 meshcore-e290-merged.bin
```

---

## Struktura projektu

```
meshcore-e290/
├── platformio.ini              # Konfiguracja budowania
├── src/
│   ├── main.cpp               # Główna pętla (setup/loop)
│   ├── ui/
│   │   ├── ui.h               # API interfejsu E-Ink
│   │   └── ui.cpp             # Implementacja UI
│   └── input/
│       ├── cardkb.h           # API klawiatury CardKB
│       └── cardkb.cpp         # Driver I²C CardKB
├── variants/
│   └── heltec_vme290/
│       ├── target.h           # Deklaracje hardware
│       └── target.cpp         # Inicjalizacja SPI/I2C/Radio
└── .github/
    └── workflows/
        └── build.yml          # GitHub Actions CI/CD
```

---

## Znane ograniczenia / roadmap

- [ ] Integracja pełnego stosu kryptograficznego MeshCore (klucze publiczne)
- [ ] Partial refresh E-Ink (wymaga wsparcia GxEPD2 dla DEPG0290BNS800F6)
- [ ] Wiadomości prywatne (DM) — implementacja routingu punkt-punkt
- [ ] Zapis kontaktów do flash (NVS/SPIFFS)
- [ ] Deep sleep z budzeniem przez radio (DIO1)
- [ ] OTA update przez LoRa

---

## Licencja

MIT — bazuje na MeshCore (MIT) by Ripple Radio.
