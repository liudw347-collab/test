/*
 * ===================================================================
 *  RC522 引脚扫描诊断固件 v2.0
 *  Platform : ESP8266 (NodeMCU) + RC522
 *  Library  : MFRC522 by miguelbalboa
 *  Purpose  : RC522 读不到时，自动扫描所有可能的 SS/RST 组合
 *  Author   : Attacker (Red Team)
 * ===================================================================
 *  ⚠️ 本固件只读 SPI 寄存器，不写卡、不修改任何数据。
 * ===================================================================
 *
 *  思路：
 *    - SCK/MOSI/MISO 保持固定（HSPI 硬件引脚 D5/D6/D7）
 *    - SS（片选）和 RST 是"软引脚"，可以接到任何 GPIO
 *    - 固件遍历常见 SS/RST 组合，读 VersionReg
 *    - 读到 0x91/0x92/0x88 就说明这个组合能通
 *
 *  运行：
 *    1. 保持现在的接线不动
 *    2. 烧录本固件
 *    3. 复位 ESP8266，看串口输出
 *    4. 如果扫到一组能用的，按提示重新接线即可
 *       如果所有组合都失败，说明问题在 SCK/MOSI/MISO/电源
 *       需要用万用表排查
 */

#include <ESP8266WiFi.h>
#include <SPI.h>
#include <MFRC522.h>

/* NodeMCU 上的所有数字引脚（除 TX/RX） */
static const uint8_t PIN_LIST[] = {D0, D1, D2, D3, D4, D5, D6, D7, D8};
static const char* PIN_NAMES[] = {"D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8"};
static const int N_PINS = sizeof(PIN_LIST) / sizeof(PIN_LIST[0]);

/* SCK/MOSI/MISO 必须是 HSPI 固定引脚：
 *   SCK  = D5 (GPIO14)
 *   MOSI = D7 (GPIO13)
 *   MISO = D6 (GPIO12)
 * 这三根线在固件里没法换，必须物理接对。
 */

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println(F("============================================"));
  Serial.println(F("= RC522 Pin Scan v2.0                      ="));
  Serial.println(F("= Scanning SS/RST combinations to find RC522 ="));
  Serial.println(F("============================================"));

  /* 先确认 HSPI 三根线对应引脚工作正常 */
  SPI.begin();
  Serial.println(F("\n[INFO] HSPI pins (must be wired to RC522):"));
  Serial.println(F("   SCK  -> D5 (GPIO14)"));
  Serial.println(F("   MOSI -> D7 (GPIO13)"));
  Serial.println(F("   MISO -> D6 (GPIO12)"));
  Serial.println(F("   (These 3 cannot be remapped, must be wired as above)"));

  Serial.println(F("\n[SCAN] Trying all SS/RST combinations...\n"));

  int hits = 0;
  for (int i = 0; i < N_PINS; i++) {
    for (int j = 0; j < N_PINS; j++) {
      if (i == j) continue;  /* SS 和 RST 不能同一根 */

      uint8_t ss = PIN_LIST[i];
      uint8_t rst = PIN_LIST[j];

      /* 重新初始化 MFRC522 */
      MFRC522 tmp(ss, rst);
      tmp.PCD_Init();
      delay(20);

      byte v = tmp.PCD_ReadRegister(tmp.VersionReg);

      if (v != 0x00 && v != 0xFF) {
        Serial.printf("  [HIT] SS=%s RST=%s -> VersionReg=0x%02X\n",
                      PIN_NAMES[i], PIN_NAMES[j], v);
        hits++;
      }

      tmp.PCD_SoftPowerDown();
      delay(5);
    }
  }

  Serial.println();
  if (hits > 0) {
    Serial.printf("[OK] Found %d working SS/RST combination(s).\n", hits);
    Serial.println(F("     Pick one, rewire your SS and RST to those pins,"));
    Serial.println(F("     then flash rfid_recon.ino (v1.4 or later)."));
  } else {
    Serial.println(F("[FAIL] No SS/RST combination worked."));
    Serial.println(F("       RC522 is NOT responding on ANY pin combo."));
    Serial.println(F("       The problem is in SCK/MOSI/MISO/POWER, NOT in SS/RST."));
    Serial.println(F(""));
    Serial.println(F("       Next steps:"));
    Serial.println(F("       1. Use multimeter continuity mode on these 3 wires:"));
    Serial.println(F("          - D5 (NodeMCU) <-> SCK  (RC522)"));
    Serial.println(F("          - D7 (NodeMCU) <-> MOSI (RC522)"));
    Serial.println(F("          - D6 (NodeMCU) <-> MISO (RC522)"));
    Serial.println(F("          Each pair must BEEP. No beep = wire broken or unsoldered."));
    Serial.println(F("       2. Check RC522 board's header solder joints."));
    Serial.println(F("          Re-solder if any look cold/dull."));
    Serial.println(F("       3. Confirm 3.3V power rail with multimeter:"));
    Serial.println(F("          - Measure RC522 VCC pin to GND: must be 3.0-3.6V"));
    Serial.println(F("          - If < 3.0V, NodeMCU's 3V3 regulator may be weak."));
    Serial.println(F("            Power RC522 from external 3.3V supply (share GND)."));
    Serial.println(F("       4. Try a different RC522 module (current one may be dead)."));
  }

  Serial.println(F("\n[DONE] Firmware halting."));
  while (true) { delay(1000); }
}

void loop() {
  /* 不用 */
}
