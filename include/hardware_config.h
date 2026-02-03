/**
 * TrekLink Hardware Configuration
 * GPIO Pin Definitions and Hardware Constants
 * 
 * Board: ESP32 DevKit V1
 * All pin assignments match design.md Section 3.2
 */

#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

#include <Arduino.h>

// ============================================================================
// LORA E32-433T20D MODULE PINS
// ============================================================================
#define LORA_TX_PIN         17  // TX2 (R) - ESP32 TX → E32 RX
#define LORA_RX_PIN         16  // RX2 (R) - E32 TX → ESP32 RX
#define LORA_AUX_PIN        27  // D27 (L) - RTC_GPIO17, Wake-on-Radio interrupt
#define LORA_M0_PIN         18  // D18 (R) - Mode control
#define LORA_M1_PIN         19  // D19 (R) - Mode control

// LoRa UART Configuration
#define LORA_SERIAL         Serial2
#define LORA_BAUD_RATE      9600

// ============================================================================
// GPS NEO-6M MODULE PINS
// ============================================================================
#define GPS_TX_PIN          14  // D14 (L) - GPS TX → ESP32 RX (NMEA input)
#define GPS_RX_PIN          -1  // Not connected (read-only GPS)
#define GPS_POWER_GATE_PIN  13  // D13 (L) - P-MOSFET gate control via NPN

// GPS UART Configuration (Software UART on GPIO 14)
#define GPS_BAUD_RATE       9600

// ============================================================================
// I2C BUS PINS (SHARED: OLED + MPU6050)
// ============================================================================
#define I2C_SDA_PIN         21  // D21 (R) - I2C SDA + 4.7kΩ pull-up
#define I2C_SCL_PIN         22  // D22 (R) - I2C SCL + 4.7kΩ pull-up
#define I2C_FREQUENCY       400000  // 400kHz Fast Mode

// I2C Device Addresses
#define OLED_I2C_ADDR       0x3C  // SSD1306 OLED
#define MPU6050_I2C_ADDR    0x68  // MPU6050 IMU (AD0 = GND)

// ============================================================================
// MPU6050 IMU PINS
// ============================================================================
#define MPU_INT_PIN         34  // D34 (L) - ADC1_CH6, Input Only, RTC_GPIO

// ============================================================================
// OLED DISPLAY CONTROL
// ============================================================================
#define OLED_GND_SWITCH_PIN 23  // D23 (R) - NPN low-side GND switch (Silent Mode)
#define OLED_WIDTH          128
#define OLED_HEIGHT         64

// ============================================================================
// USER INTERFACE - BUTTONS
// ============================================================================
#define BTN_MENU_PIN        25  // D25 (L) - DAC1, RTC_GPIO6
#define BTN_SOS_PIN         26  // D26 (L) - DAC2, RTC_GPIO7
#define BTN_UP_PIN          32  // D32 (L) - ADC1_CH4, RTC_GPIO9
#define BTN_DOWN_PIN        33  // D33 (L) - ADC1_CH5, RTC_GPIO8

// Button Configuration
#define BTN_DEBOUNCE_MS     50    // Debounce threshold
#define BTN_HOLD_MS         1000  // Hold detection (1s for Silent Mode)
#define BTN_DOUBLE_CLICK_MS 300   // Double-click window

// ============================================================================
// POWER CONTROL
// ============================================================================
#define SLIDE_SWITCH_PIN    4   // D4 (R) - Power ON/OFF switch
#define CHRG_STATUS_PIN     35  // D35 (L) - TP5100 CHRG (LOW = charging), Input Only

// ============================================================================
// AUDIO & HAPTIC FEEDBACK
// ============================================================================
#define BUZZER_PIN          12  // D12 (L) - Passive buzzer, PWM tone generation (3.3V)
#define VIBRATOR_PIN        15  // D15 (R) - Vibration motor via NPN transistor

// Buzzer PWM Configuration
#define BUZZER_PWM_CHANNEL  0
#define BUZZER_PWM_FREQ     2000  // 2kHz base frequency
#define BUZZER_PWM_RES      8     // 8-bit resolution

// ============================================================================
// POWER MANAGEMENT
// ============================================================================
// Deep sleep wake sources (RTC_GPIO pins)
#define WAKE_LORA_AUX       LORA_AUX_PIN   // GPIO 27 - Wake-on-Radio
#define WAKE_MPU_INT        MPU_INT_PIN    // GPIO 34 - Fall detection
#define WAKE_BTN_MENU       BTN_MENU_PIN   // GPIO 25
#define WAKE_BTN_SOS        BTN_SOS_PIN    // GPIO 26
#define WAKE_BTN_UP         BTN_UP_PIN     // GPIO 32
#define WAKE_BTN_DOWN       BTN_DOWN_PIN   // GPIO 33

// ============================================================================
// HARDWARE CONSTANTS
// ============================================================================
#define BATTERY_CELLS       2     // 2x 21700 in parallel
#define VBAT_NOMINAL        3.7   // Nominal voltage per cell
#define VBAT_MAX            4.2   // Fully charged
#define VBAT_MIN            3.0   // Cutoff voltage

#define LORA_TX_POWER_DBM   20    // E32-433T20D: 20dBm (100mW)
#define LORA_FREQUENCY_MHZ  433   // 433MHz ISM band

// ============================================================================
// PIN STATE DEFINITIONS
// ============================================================================
// Active states
#define GPS_POWER_ON        HIGH  // GPIO 13 HIGH = GPS powered
#define GPS_POWER_OFF       LOW   // GPIO 13 LOW = GPS unpowered

#define OLED_POWER_ON       HIGH  // GPIO 23 HIGH = OLED GND connected
#define OLED_POWER_OFF      LOW   // GPIO 23 LOW = OLED GND disconnected (Silent)

#define BUZZER_ON           HIGH
#define BUZZER_OFF          LOW

#define VIBRATOR_ON         HIGH
#define VIBRATOR_OFF        LOW

// Button states (active LOW with pull-down resistors)
#define BTN_PRESSED         LOW
#define BTN_RELEASED        HIGH

// Charging status
#define CHRG_CHARGING       LOW   // TP5100 CHRG pin LOW when charging
#define CHRG_COMPLETE       HIGH

// ============================================================================
// RESERVED / NOT USED PINS
// ============================================================================
// GPIO 0  - Boot mode (do not use)
// GPIO 1  - TX0 (USB Serial - reserved)
// GPIO 2  - Boot mode, built-in LED (avoid)
// GPIO 3  - RX0 (USB Serial - reserved)
// GPIO 5  - VSPI_CS (available for future use)
// GPIO 36 - VP (Input Only, reserved for battery ADC)
// GPIO 39 - VN (Input Only, available)

#endif // HARDWARE_CONFIG_H
