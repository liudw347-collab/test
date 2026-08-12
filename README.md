# RFID 门禁系统 - 攻击者侦察固件

> 这是我（攻击者）的第一步侦察代码。请按以下步骤运行，把串口输出全部贴回给我。

## 硬件接线（ESP8266 NodeMCU ↔ RC522）

**⚠️ 重要：RC522 必须接 3.3V，接 5V 会烧模块**

| RC522 引脚 | ESP8266 (NodeMCU) | 说明 |
|-----------|-------------------|------|
| SDA (SS)  | D8 (GPIO15)       | 片选 |
| SCK       | D5 (GPIO14)       | 时钟 |
| MOSI      | D7 (GPIO13)       | 主→从 |
| MISO      | D6 (GPIO12)       | 从→主 |
| IRQ       | 不接              |      |
| GND       | GND               |      |
| RST       | D0 (GPIO16)       | 复位 |
| 3.3V      | 3V3               | **绝对不能接 5V/VIN** |

## 编译环境

1. 安装 Arduino IDE（或 PlatformIO）
2. 在"库管理器"中搜索并安装 **`MFRC522`**（作者 miguelbalboa，版本 ≥ 1.4.x）
3. 开发板选 **NodeMCU 1.0 (ESP-12E Module)**

## 运行步骤

1. 打开 `firmware/rfid_recon/rfid_recon.ino` 烧录到 ESP8266
2. 打开串口监视器，波特率 **115200**
3. 把你的**合法门禁卡**放到 RC522 上
4. 等待输出完成（看到 "Done. Remove card."）
5. **把串口监视器里的所有内容（从开头到 Done）原样复制粘贴回给我**

## 这版固件会做什么

- 读出卡的 UID、ATQA、SAK，识别卡类型
- 对 MIFARE Classic：尝试 10 组常见默认密钥，能解就 dump 全扇区
- 对 Ultralight/NTAG：直接读所有 page
- 不写入、不修改卡内任何数据（只读侦察）

## 风险声明

代码完全是只读的，不会损坏你的卡。但我建议你先用卡试一次，再放回门禁读卡器上确认还能正常开门——理论上不会有任何影响。
