#pragma once
// ============================================================
// ui.h — Interfejs użytkownika dla E-Ink 296×128
// Wzorowany na T-Deck/MeshOS: zakładki, dymki, status bar
// ============================================================

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeMono9pt7b.h>

// ── Wymiary ──────────────────────────────────────────────────
#define SCREEN_W  296
#define SCREEN_H  128

// Status bar na górze (wysokość 14px)
#define STATUS_H  14
// Pasek zakładek (wysokość 14px) na dole
#define TAB_H     14
// Obszar roboczy
#define CONTENT_Y STATUS_H
#define CONTENT_H (SCREEN_H - STATUS_H - TAB_H)

// ── Zakładki ─────────────────────────────────────────────────
typedef enum {
    TAB_CHAT = 0,    // Publiczny czat / kanał
    TAB_DM,          // Wiadomości prywatne
    TAB_CONTACTS,    // Lista kontaktów / węzłów
    TAB_STATUS,      // Status sieci, RSSI, SNR
    TAB_COUNT
} UITab;

// ── Maksymalne rozmiary buforów ───────────────────────────────
#define MAX_MESSAGES    20
#define MAX_MSG_LEN     120
#define MAX_CONTACTS    32
#define MAX_INPUT_LEN   80

// ── Struktury danych ─────────────────────────────────────────
struct ChatMessage {
    char    sender[16];
    char    text[MAX_MSG_LEN];
    uint32_t timestamp;
    bool    is_own;       // true = moja wiadomość
    bool    delivered;
};

struct Contact {
    char     name[16];
    uint32_t node_id;
    int16_t  rssi;
    float    snr;
    uint32_t last_seen;
    bool     is_repeater;
};

// ── API warstwy UI ────────────────────────────────────────────
void ui_init();
void ui_draw_full();                    // Pełny refresh (po zmianie zakładki)
void ui_draw_partial();                 // Partial refresh (nowa wiad.)
void ui_set_tab(UITab tab);
UITab ui_get_tab();

// Aktualizacje danych
void ui_add_message(const char* sender, const char* text, bool is_own);
void ui_add_contact(const Contact& c);
void ui_update_status(int16_t rssi, float snr, uint8_t battery_pct,
                      bool radio_ok, const char* node_name);

// Wejście z klawiatury
void ui_append_input(char c);
void ui_backspace_input();
void ui_clear_input();
const char* ui_get_input();

// Callback — wywołany gdy użytkownik zatwierdza wiadomość (Enter)
typedef void (*MessageSendCallback)(const char* text, UITab channel);
void ui_set_send_callback(MessageSendCallback cb);

// Powiadomienie o nowej wiadomości (migający nagłówek)
void ui_notify_new_message();
