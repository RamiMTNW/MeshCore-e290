// ============================================================
// target.cpp — Heltec Vision Master E290
// Inicjalizacja sprzętu: E-Ink, SX1262, CardKB
// ============================================================

#include "target.h"

// ── Dwa osobne magistrale SPI ────────────────────────────────
// SPI1 → LoRa SX1262 (piny wbudowane)
// SPI2 → E-Ink EPD (piny wymagają soft-SPI lub HSPI)
SPIClass loraSPI(HSPI);
SPIClass epdSPI(FSPI);

// ── Instancja radia SX1262 ───────────────────────────────────
SX1262 radio = new Module(
    LORA_NSS,   // CS
    LORA_DIO1,  // IRQ (DIO1)
    LORA_RST,   // RST
    LORA_BUSY   // BUSY
);

// ── Instancja wyświetlacza E-Ink ─────────────────────────────
// GxEPD2_290_BS: Waveshare/Heltec 2.9" B&W, 296x128, SSD1680
DISPLAY_CLASS display(
    GxEPD2_290_BS(
        EPD_CS,    // CS
        EPD_DC,    // DC
        EPD_RST,   // RST
        EPD_BUSY   // BUSY
    )
);

// ────────────────────────────────────────────────────────────
bool radio_init() {
    loraSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);

    int state = radio.begin(
        (float)LORA_FREQ / 1e6f,  // MHz
        LORA_BW,                   // kHz bandwidth
        LORA_SF,                   // spreading factor
        LORA_CR,                   // coding rate (5..8 → CR4/5..CR4/8)
        0x12,                      // sync word (MeshCore default)
        LORA_TX_POWER,             // dBm
        16,                        // preamble length
        1.8f                       // TCXO voltage
    );

    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[RADIO] Init failed: %d\n", state);
        return false;
    }

    // EU Narrow wymaga DutyCycle — ustawiamy CRC i inne parametry
    radio.setCRC(1);
    Serial.printf("[RADIO] OK: %.4f MHz SF%d BW%.0f\n",
                  (float)LORA_FREQ / 1e6f, LORA_SF, LORA_BW);
    return true;
}

// ────────────────────────────────────────────────────────────
bool display_init() {
    epdSPI.begin(EPD_SCLK, EPD_MISO, EPD_MOSI, EPD_CS);
    display.epd2.selectSPI(epdSPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
    display.init(115200, true, 2, false);
    display.setRotation(1);        // 0° — landscape, antenna góra
    display.setTextColor(GxEPD_BLACK);
    display.fillScreen(GxEPD_WHITE);
    display.display(false);        // full refresh przy starcie
    Serial.println("[DISPLAY] E-Ink OK");
    return true;
}

// ────────────────────────────────────────────────────────────
bool cardkb_init() {
    Wire.begin(CARDKB_SDA, CARDKB_SCL);
    Wire.beginTransmission(CARDKB_ADDR);
    uint8_t err = Wire.endTransmission();
    if (err != 0) {
        Serial.printf("[CARDKB] Not found at 0x%02X (err=%d)\n",
                      CARDKB_ADDR, err);
        return false;
    }
    Serial.printf("[CARDKB] OK at 0x%02X\n", CARDKB_ADDR);
    return true;
}

// ────────────────────────────────────────────────────────────
void target_setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=== MeshCore E290 Standalone ===");

    // Przycisk BOOT jako wejście
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    bool ok_disp  = display_init();
    bool ok_radio = radio_init();
    bool ok_kb    = cardkb_init();

    if (!ok_disp || !ok_radio) {
        Serial.println("[ERROR] Krytyczny błąd sprzętu — restart za 5s");
        delay(5000);
        ESP.restart();
    }
}
