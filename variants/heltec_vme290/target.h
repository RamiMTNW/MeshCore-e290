#pragma once
// ============================================================
// target.h — Heltec Vision Master E290
// Hardware abstraction layer dla MeshCore standalone
// ============================================================

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

// ── E-Ink display (GxEPD2, DEPG0290BNS800F6, 296×128) ───────
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
// Model panelu: DEPG0290BNS800F6 = GxEPD2_290_BS (black-white, SSD1680)
#define DISPLAY_CLASS GxEPD2_BW<GxEPD2_290_BS, GxEPD2_290_BS::HEIGHT>

// ── LoRa SX1262 ──────────────────────────────────────────────
#include <RadioLib.h>
#define RADIO_CLASS SX1262

// ── Piny — zdefiniowane w platformio.ini jako -D flagi ──────
// EPD:  CS=21, DC=22, RST=23, BUSY=24, MOSI=48, SCLK=47
// LoRa: NSS=8, SCK=9, MOSI=10, MISO=11, RST=12, BUSY=13, DIO1=14
// I2C:  SDA=17, SCL=18 (CardKB)

// Globalne instancje zadeklarowane w target.cpp
extern DISPLAY_CLASS display;
extern SX1262        radio;
extern SPIClass      loraSPI;
extern SPIClass      epdSPI;

// Funkcje inicjalizacyjne
void target_setup();
bool radio_init();
bool display_init();
bool cardkb_init();
