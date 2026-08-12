/*
 * ===================================================================
 *  RC522 最后一搏诊断固件 v3.0
 *  Platform : ESP8266 (NodeMCU) + RC522
 *  Library  : MFRC522 by miguelbalboa
 *  Purpose  : 在没有万用表的情况下做最后的软件诊断
 *  Author   : Attacker (Red Team)
 * ===================================================================
 *  ⚠️ 本固件只读 SPI 寄存器，不写卡、不修改任何数据。
 * ===================================================================
 *
 *  诊断逻辑：
 *  1. 用 3 种 SPI 速度依次尝试：4MHz（默认）→ 1MHz → 100kHz
 *  2. 每种速度读 VersionReg 5 次，打印每次结果
 *  3. 如果某次读到 0x91/0x92/0x88 → 找到能用的配置
 *  4. 如果全部 0x00/0xFF → SPI 总线物理层问题，软件无解
 */

#include <ESP8266WiFi.h>
#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN  D8
#define RST_PIN D0

MFRC522 rfid(SS_PIN, RST_PIN);

const unsigned long SPI_SPEEDS[] = {4000000, 1000000, 100000};
const char* SPEED_NAMES[] = {"4 MHz", "1 MHz", "100 kHz"};
const int N_SPEEDS = 3;

byte readVersionOnce() {
  return rfid.PCD_ReadRegister(rfid.VersionReg);
}

void trySpeed(unsigned long speed, const char* name) {
  Serial.printf("\n--- Testing SPI @ %s ---\n", name);
  SPI.setFrequency(speed);
  SPI.beginTransaction(SPISettings(speed, MSBFIRST, SPI_MODE0));

  rfid.PCD_Init();
  delay(50);

  byte vals[5];
  int hits = 0;
  for (int i = 0; i < 5; i++) {
    vals[i] = readVersionOnce();
    delay(20);
    if (vals[i] != 0x00 && vals[i] != 0xFF) hits++;
    Serial.printf("  Read #%d: 0x%02X %s\n",
                  i + 1, vals[i],
                  (vals[i] != 0x00 && vals[i] != 0xFF) ? "[HIT]" : "");
  }

  if (hits > 0) {
    Serial.printf(">>> %s WORKS! Got %d/5 valid reads.\n", name, hits);
  } else {
    Serial.printf(">>> %s failed (all reads 0x00/0xFF).\n", name);
  }
  SPI.endTransaction();
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println(F("============================================"));
  Serial.println(F("= RC522 Last-Resort Diagnostic v3.0        ="));
  Serial.println(F("= Tries multiple SPI speeds + retries      ="));
  Serial.println(F("============================================"));

  Serial.println(F("\n[INFO] Current pin assignment:"));
  Serial.println(F("   SS  -> D8 (GPIO15)"));
  Serial.println(F("   RST -> D0 (GPIO16)"));
  Serial.println(F("   SCK -> D5 (GPIO14)  [hardware SPI, fixed]"));
  Serial.println(F("   MOSI-> D7 (GPIO13)  [hardware SPI, fixed]"));
  Serial.println(F("   MISO-> D6 (GPIO12)  [hardware SPI, fixed]"));
  Serial.println(F("   VCC -> 3V3"));
  Serial.println(F("   GND -> GND"));

  SPI.begin();

  bool anyHit = false;
  for (int i = 0; i < N_SPEEDS; i++) {
    trySpeed(SPI_SPEEDS[i], SPEED_NAMES[i]);
    byte v = rfid.PCD_ReadRegister(rfid.VersionReg);
    if (v != 0x00 && v != 0xFF) anyHit = true;
  }

  Serial.println(F("\n============================================"));
  if (anyHit) {
    Serial.println(F("[VERDICT] RC522 IS ALIVE on at least one speed."));
    Serial.println(F("          Wiring is OK. Slower speed compensated"));
    Serial.println(F("          for marginal signal integrity."));
    Serial.println(F("          Now flash rfid_recon.ino (v1.4+) — it"));
    Serial.println(F("          uses default speed; if it fails, we need"));
    Serial.println(F("          to slow it down in the main firmware."));
  } else {
    Serial.println(F("[VERDICT] RC522 NOT RESPONDING at ANY speed."));
    Serial.println(F("          Software cannot fix this. The problem"));
    Serial.println(F("          is physical: broken wire, bad solder,"));
    Serial.println(F("          or dead module. No multimeter needed"));
    Serial.println(F("          to test wires — just SWAP them one by"));
    Serial.println(F("          one with spare Dupont wires."));
    Serial.println(F(""));
    Serial.println(F("          >>> If you don't have spare wires: <<<"));
    Serial.println(F("          Order a new RC522 module (5 RMB on Taobao)."));
    Serial.println(F("          This one is either oxidized from storage"));
    Serial.println(F("          or has cold solder joints on the header."));
  }
  Serial.println(F("============================================"));

  Serial.println(F("\n[DONE] Firmware halting."));
  while (true) { delay(1000); }
}

void loop() {
  /* 不用 */
}
