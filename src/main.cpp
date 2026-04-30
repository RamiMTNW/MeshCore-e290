// ============================================================
// main.cpp — MeshCore E290 Standalone Companion
// Autonomiczny komunikator E-Ink + LoRa + CardKB
//
// Architektura:
//   target_setup()   → init sprzętu (SPI, I2C, Radio, EPD)
//   ui_init()        → init interfejsu
//   MeshCore loop    → odbiór/wysyłanie pakietów LoRa
//   cardkb_poll()    → odczyt klawiatury
// ============================================================

#include <Arduino.h>
#include "..variants/heltec_vme290/target.h"
#include "ui/ui.h"
#include "input/cardkb.h"

// ── Uproszczony stos MeshCore ────────────────────────────────
// W docelowej wersji zastąpiony przez src/meshcore/mesh_engine.h
// Tymczasem: obsługa bezpośrednia RadioLib
// ────────────────────────────────────────────────────────────

// Identyfikator węzła (generowany z MAC)
static char node_name[16] = "E290_node";
static uint32_t node_id   = 0;

// Bufor odbioru
static uint8_t rx_buf[256];
static bool    rx_ready  = false;
static int16_t rx_rssi   = 0;
static float   rx_snr    = 0.0f;

// ── Przerwanie radia ─────────────────────────────────────────
volatile bool radio_irq = false;

void IRAM_ATTR radio_isr() {
    radio_irq = true;
}

// ── Odczyt baterii ADC ───────────────────────────────────────
static uint8_t read_battery_pct() {
    uint32_t raw = analogRead(BATTERY_ADC_PIN);
    // VME290: dzielnik napięcia 1:2, ADC 12-bit, Vref 3.3V
    // Vbat = raw * 3.3 * 2 / 4095
    float v = raw * 6.6f / 4095.0f;
    if (v >= 4.2f) return 100;
    if (v <= 3.3f) return 0;
    return (uint8_t)((v - 3.3f) / 0.9f * 100.0f);
}

// ── Generuj node_id z MAC ────────────────────────────────────
static void init_node_id() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    node_id = ((uint32_t)mac[2] << 24) | ((uint32_t)mac[3] << 16)
            | ((uint32_t)mac[4] << 8)  |  (uint32_t)mac[5];
    snprintf(node_name, sizeof(node_name), "E290_%04X", (uint16_t)node_id);
}

// ── Wyślij pakiet tekstowy MeshCore ─────────────────────────
// Format uproszczony: [0x01][len][nadawca\0][tekst\0]
static bool mesh_send_text(const char* text, bool is_public) {
    uint8_t pkt[256];
    uint8_t pos = 0;
    pkt[pos++] = is_public ? 0x01 : 0x02;  // 0x01=public, 0x02=DM
    pkt[pos++] = (uint8_t)strlen(node_name);
    memcpy(&pkt[pos], node_name, strlen(node_name));
    pos += strlen(node_name);
    pkt[pos++] = 0;  // separator
    uint8_t tlen = strlen(text);
    if (tlen > 200) tlen = 200;
    memcpy(&pkt[pos], text, tlen);
    pos += tlen;

    int state = radio.transmit(pkt, pos);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[TX] Błąd: %d\n", state);
        return false;
    }
    // Wróć do trybu odbioru
    radio.startReceive();
    radio.setDio1Action(radio_isr);
    return true;
}

// ── Przetworz odebrany pakiet ────────────────────────────────
static void process_rx_packet(uint8_t* buf, size_t len) {
    if (len < 3) return;
    uint8_t type    = buf[0];
    uint8_t namelen = buf[1];
    if (namelen + 2 >= (uint8_t)len) return;

    char sender[16] = {0};
    strncpy(sender, (char*)&buf[2], min((int)namelen, 15));

    const char* text = (const char*)&buf[2 + namelen + 1];

    Serial.printf("[RX] Od: %s | %s\n", sender, text);
    ui_add_message(sender, text, false);

    // Aktualizuj kontakty
    Contact c;
    strncpy(c.name, sender, 15);
    c.node_id    = 0;  // TODO: parsuj z pakietu
    c.rssi       = rx_rssi;
    c.snr        = rx_snr;
    c.last_seen  = millis();
    c.is_repeater= (type == 0x10);
    ui_add_contact(c);
}

// ── Callback — wysyłanie z UI ────────────────────────────────
static void on_send_message(const char* text, UITab channel) {
    bool is_pub = (channel == TAB_CHAT);
    if (mesh_send_text(text, is_pub)) {
        ui_add_message(node_name, text, true);
        Serial.printf("[TX] Wysłano: %s\n", text);
    }
    ui_clear_input();
    ui_draw_partial();
}

// ── Timery ───────────────────────────────────────────────────
static uint32_t last_status_update = 0;
static uint32_t last_kb_poll       = 0;

// ────────────────────────────────────────────────────────────
// SETUP
// ────────────────────────────────────────────────────────────
void setup() {
    target_setup();
    init_node_id();

    // Ustaw tryb odbioru z przerwaniem
    radio.setDio1Action(radio_isr);
    int state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[RADIO] startReceive failed: %d\n", state);
    }

    // Inicjalizuj UI
    ui_set_send_callback(on_send_message);
    ui_update_status(0, 0.0f, read_battery_pct(), true, node_name);
    ui_init();

    Serial.printf("[BOOT] Węzeł: %s (ID: %08X)\n", node_name, node_id);
}

// ────────────────────────────────────────────────────────────
// LOOP
// ────────────────────────────────────────────────────────────
void loop() {
    // ── Odbiór LoRa ─────────────────────────────────────────
    if (radio_irq) {
        radio_irq = false;
        size_t len = 0;
        int state  = radio.readData(rx_buf, sizeof(rx_buf) - 1);
        if (state == RADIOLIB_ERR_NONE) {
            len     = radio.getPacketLength();
            rx_rssi = radio.getRSSI();
            rx_snr  = radio.getSNR();
            rx_buf[len] = 0;
            process_rx_packet(rx_buf, len);
            // Aktualizuj status bar (RSSI/SNR)
            ui_update_status(rx_rssi, rx_snr,
                             read_battery_pct(), true, node_name);
        }
        // Wróć do nasłuchu
        radio.startReceive();
        radio.setDio1Action(radio_isr);
    }

    // ── Klawiatura (poll co 50ms) ────────────────────────────
    uint32_t now = millis();
    if (now - last_kb_poll >= 50) {
        last_kb_poll = now;
        uint8_t key  = cardkb_read();
        if (key != 0) {
            bool send = cardkb_process(key);
            if (send) {
                on_send_message(ui_get_input(),
                                ui_get_tab());
            }
        }
    }

    // ── Status update co 30s ─────────────────────────────────
    if (now - last_status_update >= 30000) {
        last_status_update = now;
        ui_update_status(rx_rssi, rx_snr,
                         read_battery_pct(), true, node_name);
        // Odśwież status bar bez pełnego refresh
        if (ui_get_tab() == TAB_STATUS) ui_draw_full();
    }

    // ── Przycisk BOOT — przełącz zakładkę ───────────────────
    static bool btn_prev = HIGH;
    bool btn_now = digitalRead(BUTTON_PIN);
    if (btn_prev == HIGH && btn_now == LOW) {
        delay(20);  // debounce
        UITab next = (UITab)((ui_get_tab() + 1) % TAB_COUNT);
        ui_set_tab(next);
    }
    btn_prev = btn_now;
}
