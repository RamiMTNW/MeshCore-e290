// ============================================================
// cardkb.cpp — M5Stack CardKB I2C driver
// ============================================================

#include "cardkb.h"

bool cardkb_begin() {
    Wire.beginTransmission(CARDKB_ADDR);
    return (Wire.endTransmission() == 0);
}

uint8_t cardkb_read() {
    Wire.requestFrom((uint8_t)CARDKB_ADDR, (uint8_t)1);
    if (Wire.available()) {
        uint8_t key = Wire.read();
        return key;
    }
    return 0;
}

// Zwraca true gdy wiadomość gotowa do wysłania (Enter)
bool cardkb_process(uint8_t key) {
    if (key == 0) return false;

    // ── Zakładki przez Fn1–Fn4 ──────────────────────────────
    if (key == CARDKB_FN1) { ui_set_tab(TAB_CHAT);     return false; }
    if (key == CARDKB_FN2) { ui_set_tab(TAB_DM);       return false; }
    if (key == CARDKB_FN3) { ui_set_tab(TAB_CONTACTS); return false; }
    if (key == CARDKB_FN4) { ui_set_tab(TAB_STATUS);   return false; }

    // ── TAB — przełącz następną zakładkę ───────────────────
    if (key == CARDKB_KEY_TAB) {
        UITab next = (UITab)((ui_get_tab() + 1) % TAB_COUNT);
        ui_set_tab(next);
        return false;
    }

    // ── ESC — wróć do CHAT i wyczyść input ─────────────────
    if (key == CARDKB_KEY_ESC) {
        ui_clear_input();
        ui_set_tab(TAB_CHAT);
        return false;
    }

    // ── Enter — wyślij wiadomość ────────────────────────────
    if (key == CARDKB_KEY_ENTER) {
        if (strlen(ui_get_input()) > 0) return true;
        return false;
    }

    // ── Backspace / Delete ──────────────────────────────────
    if (key == CARDKB_KEY_BACKSPACE || key == CARDKB_KEY_DELETE) {
        ui_backspace_input();
        return false;
    }

    // ── Znaki drukowalne ────────────────────────────────────
    if (key >= 0x20 && key < 0x7F) {
        ui_append_input((char)key);
        return false;
    }

    // Ignoruj inne kody
    return false;
}

void cardkb_poll() {
    uint8_t key = cardkb_read();
    cardkb_process(key);
}
