| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- |

# Dabble Joystick BLE — ESP32-S3 Robot Controller

คำอธิบายสั้น ๆ (ไทย / EN):
- โครงการนี้ใช้ ESP-IDF + NimBLE เพื่อรับคำสั่งจากแอพ *Dabble* (Gamepad module) และควบคุมมอเตอร์ 4 ตัวผ่าน TB6612FNG หนึ่งตัว (ซ้าย 2 ตัว / ขวา 2 ตัวแบบจับคู่)
- This project uses ESP-IDF + NimBLE to receive Dabble Gamepad input and drive 4 motors with one TB6612FNG driver (paired as left 2 / right 2).

**ไฟล์สำคัญ**:
- Source: [main/main.c](main/main.c#L1-L30)
- Motor driver: [main/src/motors.c](main/src/motors.c#L1-L80)
- BLE GAP/advertising: [main/src/gap.c](main/src/gap.c#L1-L80)
- LED control: [main/src/led.c](main/src/led.c#L1-L120)

---

## ฟีเจอร์หลัก / Features

- รองรับการเชื่อมต่อ BLE ผ่าน NimBLE และรับข้อมูลจากแอพ Dabble (Gamepad module)
- Joystick บังคับทิศทาง (analog หรือ digital)
- ปุ่ม: Circle = เพิ่มเกียร์, Square = ลดเกียร์, START = หยุดฉุกเฉิน (gear 0)
- ระบบเกียร์ 3 ระดับ (0=หยุด, 1-2=ต่ำ-สูง) เพื่อจำกัดความเร็ว (0=หยุด, 1=50%, 2=100%)
- ควบคุมมอเตอร์ 4 ตัวแบบจับคู่เป็น 2 กลุ่ม (ซ้าย/ขวา) ผ่าน TB6612FNG 2 แชนเนล โดยใช้ LEDC PWM
- LED RGB เปลี่ยนสีตามเกียร์ (ถ้าใช้ LED strip backend), หากเป็น GPIO LED จะกระพริบแบบ fallback

---

## ผังการต่อ (Wiring)

ต่อ ESP32-S3 กับ TB6612FNG **หนึ่งตัว** ตามนี้:

- TB6612FNG Channel A (กลุ่มมอเตอร์ซ้าย 2 ตัวต่อขนาน):
  - PWMA → ESP32 GPIO 4
  - AIN1 → ESP32 GPIO 5
  - AIN2 → ESP32 GPIO 6
  - AO1/AO2 → มอเตอร์ซ้าย 2 ตัว (ต่อขนานตามขั้วเดียวกัน)

- TB6612FNG Channel B (กลุ่มมอเตอร์ขวา 2 ตัวต่อขนาน):
  - PWMB → ESP32 GPIO 8
  - BIN1 → ESP32 GPIO 9
  - BIN2 → ESP32 GPIO 10
  - BO1/BO2 → มอเตอร์ขวา 2 ตัว (ต่อขนานตามขั้วเดียวกัน)

- Shared power/control:
  - STBY → ESP32 GPIO 7
  - VCC  → 3.3V
  - GND  → GND (ร่วมกับ ESP32 และแบต)
  - VM   → Battery +

หมายเหตุ: ในโค้ด `motors.c` มีการแม็ปแบบ logical/physical ดังนี้:
- Logical motors: 0,1 = กลุ่มซ้าย และ 2,3 = กลุ่มขวา
- Physical GPIO groups:
  - Left group: PWM=4, IN1=5, IN2=6
  - Right group: PWM=8, IN1=9, IN2=10
- MOTOR_STBY_GPIO = 7

---

## การควบคุม (Controls)

- Joystick: บังคับทิศทาง (X/Y)
- Circle: เพิ่มเกียร์ (ย้ายไปเกียร์ถัดไป สูงสุดเกียร์ 2)
- Square: ลดเกียร์ (ย้ายไปเกียร์ก่อนหน้า ต่ำสุดเกียร์ 1)
- START: หยุดฉุกเฉิน (ตั้งเกียร์เป็น 0 และหยุดมอเตอร์)

โลจิก:
- ค่าแกนจาก Dabble จะอยู่ในช่วง -127..127 และถูกแปลงเป็นค่า duty PWM แล้วคูณด้วยตัวคูณของเกียร์

---

## LED RGB ตามเกียร์

ถ้าคอนฟิกเป็น LED strip backend (`CONFIG_BLINK_LED_STRIP`):
- gear 0 → แดง (#FF0000)
- gear 1 → ส้ม (#FF7800)
- gear 2 → ม่วง (#8000FF)

ถ้าใช้ GPIO LED ปกติ (ไม่ใช่ RGB), ฟังก์ชัน `led_set_gear_color()` จะทำเป็น fallback (ไม่เปลี่ยนสี)

---

## Build & Flash

1. ตั้ง target (ตัวอย่าง ESP32-S3):

```
idf.py set-target esp32s3
```

2. Build และ flash ให้บอร์ด (แทนที่ `<PORT>` ด้วยพอร์ตของคุณ):

```
idf.py -p <PORT> flash monitor
```

ตัวอย่าง (Windows COM port):

```
idf.py -p COM5 flash monitor
```

---

## ข้อควรระวัง / Notes

- `motors.c` ต้องการแม็ปพินที่ถูกต้องตามผังด้านบน มิฉะนั้น `motors_init()` จะล้มเหลวและโปรแกรมจะไม่เริ่มระบบมอเตอร์
- ตรวจสอบ `sdkconfig` ว่าเลือก backend ของ LED ให้ตรงกับฮาร์ดแวร์ของคุณ (`CONFIG_BLINK_LED_STRIP` หรือ `CONFIG_BLINK_LED_GPIO`)
- ระวังการจ่ายแรงดันให้ VM ของ TB6612FNG ให้ตรงกับมอเตอร์ (ไม่จ่ายจาก 3.3V ของบอร์ด)

---

ถ้าต้องการ ผมสามารถ build ให้, หรือเพิ่มตัวอย่าง `sdkconfig.defaults` สำหรับบอร์ดของคุณได้

---

ไฟล์เพิ่มเติมที่น่าสนใจ:
- [main/main.c](main/main.c#L1-L120) — logic ของการ parse ข้อมูล Dabble และการควบคุมมอเตอร์
- [main/src/motors.c](main/src/motors.c#L1-L200) — ไดรเวอร์มอเตอร์และแม็ปพิน
- [main/src/led.c](main/src/led.c#L1-L200) — การตั้งค่าสี LED ตามเกียร์

