#pragma once
// ============================================================
// cardkb.h — Obsługa klawiatury M5Stack CardKB (I2C 0x5F)
// Odczyt znaków, mapowanie klawiszy specjalnych
// ============================================================

#include <Arduino.h>
#include <Wire.h>
#include "ui/ui.h"

// ── Kody specjalne CardKB ────────────────────────────────────
#define CARDKB_KEY_ENTER     0x0D
#define CARDKB_KEY_BACKSPACE 0x08
#define CARDKB_KEY_TAB       0x09
#define CARDKB_KEY_ESC       0x1B
#define CARDKB_KEY_UP        0xB5
#define CARDKB_KEY_DOWN      0xB6
#define CARDKB_KEY_LEFT      0xB4
#define CARDKB_KEY_RIGHT     0xB7
#define CARDKB_KEY_DELETE    0x7F

// Fn+1..Fn+4 jako przełączenie zakładek (CardKB mapuje je jako 0xA1..0xA4)
#define CARDKB_FN1   0xA1
#define CARDKB_FN2   0xA2
#define CARDKB_FN3   0xA3
#define CARDKB_FN4   0xA4

typedef void (*KeyHandler)(uint8_t key, bool special);

// Inicjalizacja (Wire musi być wcześniej begin())
bool cardkb_begin();

// Odczyt klawisza (0 = brak)
uint8_t cardkb_read();

// Przetworz klawisz i zaktualizuj UI
// Zwraca true jeśli Enter — aplikacja powinna wysłać wiadomość
bool cardkb_process(uint8_t key);

// Poll — wywołaj w loop()
void cardkb_poll();
