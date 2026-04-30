// ============================================================
// ui.cpp — Interfejs E-Ink dla MeshCore E290 Standalone
// Layout: StatusBar | Content | TabBar
// Zakładki: CHAT | DM | CONTACTS | STATUS
// ============================================================

#include "ui.h"
#include "../variants/heltec_vme290/target.h"

// ────────────────────────────────────────────────────────────
// Stan wewnętrzny UI
// ────────────────────────────────────────────────────────────
static UITab         current_tab    = TAB_CHAT;
static ChatMessage   messages[MAX_MESSAGES];
static uint8_t       msg_count      = 0;
static uint8_t       msg_start      = 0;   // ring buffer start
static Contact       contacts[MAX_CONTACTS];
static uint8_t       contact_count  = 0;
static char          input_buf[MAX_INPUT_LEN + 1] = {0};
static uint8_t       input_len      = 0;

static int16_t       status_rssi    = 0;
static float         status_snr     = 0.0f;
static uint8_t       status_batt    = 0;
static bool          status_radio   = false;
static char          status_node[16]= "E290";
static bool          new_msg_notify = false;

static MessageSendCallback send_cb  = nullptr;

// ────────────────────────────────────────────────────────────
// Pomocnicze: rysowanie prostokąta z wypełnieniem
// ────────────────────────────────────────────────────────────
static void fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    display.fillRect(x, y, w, h, color);
}

// ────────────────────────────────────────────────────────────
// STATUS BAR (górna belka, 14px)
// [nodename]  [RSSI:-90 SNR:5.0]  [bat:85%]  [●]
// ────────────────────────────────────────────────────────────
static void draw_status_bar() {
    fill_rect(0, 0, SCREEN_W, STATUS_H, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setFont(nullptr);  // default 5x7 font
    display.setTextSize(1);

    // Nazwa węzła
    display.setCursor(3, 3);
    display.print(status_node);

    // RSSI / SNR
    char buf[32];
    snprintf(buf, sizeof(buf), "RSSI:%d SNR:%.1f", status_rssi, status_snr);
    display.setCursor(80, 3);
    display.print(buf);

    // Bateria
    snprintf(buf, sizeof(buf), "%u%%", status_batt);
    display.setCursor(230, 3);
    display.print(buf);

    // Kółko statusu radia
    if (status_radio) {
        display.fillCircle(280, 6, 4, GxEPD_WHITE);
    } else {
        display.drawCircle(280, 6, 4, GxEPD_WHITE);
        // X wewnątrz
        display.drawLine(277, 3, 283, 9, GxEPD_WHITE);
        display.drawLine(283, 3, 277, 9, GxEPD_WHITE);
    }

    // Powiadomienie o nowej wiadomości
    if (new_msg_notify) {
        display.fillRect(290, 2, 5, 10, GxEPD_WHITE);
    }

    display.setTextColor(GxEPD_BLACK);
}

// ────────────────────────────────────────────────────────────
// TAB BAR (dolna belka, 14px)
// [ CHAT ] [ DM ] [ NODES ] [ STATUS ]
// ────────────────────────────────────────────────────────────
static const char* TAB_LABELS[TAB_COUNT] = {"CHAT", "DM", "NODES", "INFO"};

static void draw_tab_bar() {
    int16_t y = SCREEN_H - TAB_H;
    fill_rect(0, y, SCREEN_W, TAB_H, GxEPD_BLACK);

    int16_t tab_w = SCREEN_W / TAB_COUNT;
    for (int i = 0; i < TAB_COUNT; i++) {
        int16_t x = i * tab_w;
        if (i == (int)current_tab) {
            // Aktywna zakładka — biały prostokąt z czarnym tekstem
            fill_rect(x + 1, y + 1, tab_w - 2, TAB_H - 2, GxEPD_WHITE);
            display.setTextColor(GxEPD_BLACK);
        } else {
            display.setTextColor(GxEPD_WHITE);
        }
        display.setFont(nullptr);
        display.setTextSize(1);
        // Wyśrodkuj tekst
        int16_t tx = x + (tab_w - strlen(TAB_LABELS[i]) * 6) / 2;
        display.setCursor(tx, y + 3);
        display.print(TAB_LABELS[i]);
    }
    display.setTextColor(GxEPD_BLACK);
}

// ────────────────────────────────────────────────────────────
// CHAT — dymki wiadomości + pasek wpisywania
// ────────────────────────────────────────────────────────────
static void draw_chat_content() {
    // Obszar wpisywania (dolny, nad tab bar): 14px
    int16_t input_y = CONTENT_Y + CONTENT_H - 14;
    fill_rect(0, input_y, SCREEN_W, 14, GxEPD_WHITE);
    display.drawRect(0, input_y, SCREEN_W, 14, GxEPD_BLACK);

    display.setFont(nullptr);
    display.setTextSize(1);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(3, input_y + 3);
    display.print("> ");
    display.print(input_buf);
    // Kursor migający (stały prostokąt)
    int16_t cx = 15 + input_len * 6;
    if (cx < SCREEN_W - 4) {
        fill_rect(cx, input_y + 2, 2, 10, GxEPD_BLACK);
    }

    // Obszar wiadomości
    int16_t msg_area_h = CONTENT_H - 14;
    int16_t line_h     = 12;
    int16_t max_lines  = msg_area_h / line_h;

    // Wyświetl ostatnie max_lines wiadomości (od dołu ku górze)
    int start = (int)msg_count - max_lines;
    if (start < 0) start = 0;

    for (int i = start; i < (int)msg_count; i++) {
        int idx = (msg_start + i) % MAX_MESSAGES;
        const ChatMessage& m = messages[idx];

        int16_t line_idx = i - start;
        int16_t y = CONTENT_Y + line_idx * line_h + 1;

        display.setFont(nullptr);
        display.setTextSize(1);

        if (m.is_own) {
            // Własna wiadomość — wyrównana do prawej, ramka
            char line[MAX_MSG_LEN + 8];
            snprintf(line, sizeof(line), "%s", m.text);
            int16_t tw = strlen(line) * 6 + 4;
            if (tw > SCREEN_W - 2) tw = SCREEN_W - 2;
            int16_t lx = SCREEN_W - tw - 1;
            display.drawRect(lx, y, tw, line_h - 1, GxEPD_BLACK);
            display.setCursor(lx + 2, y + 2);
            display.setTextColor(GxEPD_BLACK);
            display.print(m.text);
        } else {
            // Cudza wiadomość — wyrównana do lewej, pogrubiony nadawca
            display.setCursor(1, y + 2);
            display.setTextColor(GxEPD_BLACK);
            display.print(m.sender);
            display.print(":");
            int16_t sx = strlen(m.sender) * 6 + 8;
            display.setCursor(sx, y + 2);
            display.print(m.text);
        }
    }
}

// ────────────────────────────────────────────────────────────
// CONTACTS — lista węzłów mesh
// ────────────────────────────────────────────────────────────
static void draw_contacts_content() {
    display.setFont(nullptr);
    display.setTextSize(1);
    int16_t line_h   = 13;
    int16_t max_rows = CONTENT_H / line_h;

    if (contact_count == 0) {
        display.setCursor(10, CONTENT_Y + CONTENT_H / 2 - 5);
        display.print("Brak kontaktow w sieci");
        display.setCursor(10, CONTENT_Y + CONTENT_H / 2 + 5);
        display.print("Wyslij advert, aby sie oglosic");
        return;
    }

    for (int i = 0; i < (int)contact_count && i < max_rows; i++) {
        const Contact& c = contacts[i];
        int16_t y = CONTENT_Y + i * line_h;

        // Separator
        if (i > 0) display.drawLine(0, y, SCREEN_W, y, GxEPD_BLACK);

        // Ikona repeater
        if (c.is_repeater) {
            display.drawRect(1, y + 2, 8, 8, GxEPD_BLACK);
            display.setCursor(2, y + 3);
            display.print("R");
        }

        int16_t tx = c.is_repeater ? 12 : 2;
        display.setCursor(tx, y + 3);
        display.print(c.name);

        // RSSI po prawej stronie
        char buf[24];
        snprintf(buf, sizeof(buf), "%ddBm", c.rssi);
        int16_t bw = strlen(buf) * 6;
        display.setCursor(SCREEN_W - bw - 2, y + 3);
        display.print(buf);
    }
}

// ────────────────────────────────────────────────────────────
// STATUS — info o węźle i sieci
// ────────────────────────────────────────────────────────────
static void draw_status_content() {
    display.setFont(nullptr);
    display.setTextSize(1);

    char buf[64];
    int16_t y = CONTENT_Y + 4;
    int16_t lh = 12;

    snprintf(buf, sizeof(buf), "Wezel: %s", status_node);
    display.setCursor(4, y); display.print(buf); y += lh;

    snprintf(buf, sizeof(buf), "Radio: %s", status_radio ? "OK" : "BRAK");
    display.setCursor(4, y); display.print(buf); y += lh;

    snprintf(buf, sizeof(buf), "RSSI: %d dBm  SNR: %.1f dB", status_rssi, status_snr);
    display.setCursor(4, y); display.print(buf); y += lh;

    snprintf(buf, sizeof(buf), "Bateria: %u%%", status_batt);
    display.setCursor(4, y); display.print(buf); y += lh;

    snprintf(buf, sizeof(buf), "Kontakty: %u  Wiad: %u", contact_count, msg_count);
    display.setCursor(4, y); display.print(buf); y += lh;

    // Pasek baterii graficzny
    int16_t bx = 4, by = y + 2;
    display.drawRect(bx, by, 80, 10, GxEPD_BLACK);
    display.drawRect(bx + 80, by + 2, 4, 6, GxEPD_BLACK);
    int16_t fill = (int16_t)(status_batt * 78 / 100);
    if (fill > 0) display.fillRect(bx + 1, by + 1, fill, 8, GxEPD_BLACK);
}

// ────────────────────────────────────────────────────────────
// DM placeholder — do późniejszej implementacji
// ────────────────────────────────────────────────────────────
static void draw_dm_content() {
    display.setFont(nullptr);
    display.setTextSize(1);
    display.setCursor(4, CONTENT_Y + 4);
    display.print("DM — wiadomosci prywatne");
    display.setCursor(4, CONTENT_Y + 18);
    display.print("Wybierz kontakt z zakladki NODES");
    display.setCursor(4, CONTENT_Y + 30);
    display.print("i nacisnij Enter.");
}

// ────────────────────────────────────────────────────────────
// Główna funkcja rysowania
// ────────────────────────────────────────────────────────────
static void draw_all() {
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        draw_status_bar();
        draw_tab_bar();

        switch (current_tab) {
            case TAB_CHAT:     draw_chat_content();     break;
            case TAB_DM:       draw_dm_content();       break;
            case TAB_CONTACTS: draw_contacts_content(); break;
            case TAB_STATUS:   draw_status_content();   break;
            default: break;
        }
    } while (display.nextPage());
}

// ────────────────────────────────────────────────────────────
// Publiczne API
// ────────────────────────────────────────────────────────────
void ui_init() {
    memset(messages, 0, sizeof(messages));
    memset(contacts, 0, sizeof(contacts));
    memset(input_buf, 0, sizeof(input_buf));
    draw_all();
}

void ui_draw_full() {
    draw_all();
}

void ui_draw_partial() {
    // Partial refresh obszaru czatu (szybszy, mniej migania)
    // GxEPD2 partialUpdate w obszarze content
    display.setPartialWindow(0, CONTENT_Y, SCREEN_W, CONTENT_H);
    display.firstPage();
    do {
        display.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, GxEPD_WHITE);
        if (current_tab == TAB_CHAT) draw_chat_content();
    } while (display.nextPage());
}

void ui_set_tab(UITab tab) {
    if (tab == current_tab) return;
    current_tab = tab;
    if (tab == TAB_CHAT) new_msg_notify = false;
    draw_all();
}

UITab ui_get_tab() { return current_tab; }

void ui_add_message(const char* sender, const char* text, bool is_own) {
    uint8_t idx;
    if (msg_count < MAX_MESSAGES) {
        idx = msg_count++;
    } else {
        // Nadpisz najstarszą
        idx = msg_start;
        msg_start = (msg_start + 1) % MAX_MESSAGES;
    }
    uint8_t real_idx = (msg_start + idx) % MAX_MESSAGES;
    // Ring buffer — jeśli pełny
    if (msg_count == MAX_MESSAGES) {
        real_idx = msg_start;
        msg_start = (msg_start + 1) % MAX_MESSAGES;
    }

    strncpy(messages[real_idx].sender, sender, 15);
    strncpy(messages[real_idx].text,   text,   MAX_MSG_LEN - 1);
    messages[real_idx].is_own    = is_own;
    messages[real_idx].delivered = false;
    messages[real_idx].timestamp = millis();

    if (current_tab == TAB_CHAT) {
        ui_draw_partial();
    } else {
        new_msg_notify = true;
        // Odśwież status bar z notyfikacją
        draw_status_bar();
    }
}

void ui_add_contact(const Contact& c) {
    // Sprawdź czy już istnieje
    for (int i = 0; i < contact_count; i++) {
        if (contacts[i].node_id == c.node_id) {
            contacts[i] = c;
            if (current_tab == TAB_CONTACTS) draw_all();
            return;
        }
    }
    if (contact_count < MAX_CONTACTS) {
        contacts[contact_count++] = c;
        if (current_tab == TAB_CONTACTS) draw_all();
    }
}

void ui_update_status(int16_t rssi, float snr, uint8_t battery_pct,
                      bool radio_ok, const char* node_name) {
    status_rssi  = rssi;
    status_snr   = snr;
    status_batt  = battery_pct;
    status_radio = radio_ok;
    if (node_name) strncpy(status_node, node_name, 15);
}

void ui_append_input(char c) {
    if (input_len < MAX_INPUT_LEN) {
        input_buf[input_len++] = c;
        input_buf[input_len]   = '\0';
        if (current_tab == TAB_CHAT || current_tab == TAB_DM) {
            ui_draw_partial();
        }
    }
}

void ui_backspace_input() {
    if (input_len > 0) {
        input_buf[--input_len] = '\0';
        if (current_tab == TAB_CHAT || current_tab == TAB_DM) {
            ui_draw_partial();
        }
    }
}

void ui_clear_input() {
    input_len    = 0;
    input_buf[0] = '\0';
}

const char* ui_get_input() { return input_buf; }

void ui_set_send_callback(MessageSendCallback cb) { send_cb = cb; }

void ui_notify_new_message() {
    if (current_tab != TAB_CHAT) {
        new_msg_notify = true;
    }
}
