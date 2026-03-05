/*
 * TrekLink ESP32 Custom Variant
 * Hardware: ESP32-WROOM-32 + Ra-02 SX1278 433MHz SPI
 * Version: 2.0 (Meshtastic Fork)
 * Date: 2026-02-04
 * 
 * GPIO Configuration: "TrekLink Final" from gpio_pinout_comparison.md
 * - LoRa Ra-02 (SPI): Meshtastic DIY defaults (SCK=5, MISO=19, MOSI=27, CS=18, DIO0=26, RESET=14)
 * - I2C Bus: SDA=21, SCL=22 (OLED 0x3C + MPU6050 0x68)
 * - GPS Neo-6M (UART1): TX=16, RX=17
 * - Buttons: MENU=25, SOS=34, UP=32, DOWN=35
 * - Buzzer=33, Vibrator=4, LED=2
 * - Battery ADC: GPIO 36 (ADC1_CH0 input-only)
 */

// I2C Bus (OLED + MPU6050)
#define I2C_SDA 21
#define I2C_SCL 22

// GPS Neo-6M (UART1)
#undef GPS_RX_PIN
#undef GPS_TX_PIN
#define GPS_RX_PIN 16   // ESP32 TX1 → GPS RX
#define GPS_TX_PIN 17   // GPS TX → ESP32 RX1
#define GPS_UBLOX

// User Interface Buttons
#define BUTTON_PIN 25        // MENU button (Meshtastic default)
// Phase 2 additional buttons (TrekLink custom):
#define BUTTON_PIN_SOS 34    // SOS button (input only, requires external pull-up)
#define BUTTON_PIN_UP 32     // UP button
#define BUTTON_PIN_DOWN 35   // DOWN button (input only, requires external pull-up)

// Battery Monitoring
#define BATTERY_PIN 36       // ADC1_CH0 input-only (safer than GPIO 32)
#define ADC_CHANNEL ADC1_GPIO36_CHANNEL
#define ADC_MULTIPLIER 2.0   // Voltage divider ratio (adjust based on actual resistors)

// TP5100 Charge Status Detection (GPIO 13)
// CHRG pin: open-drain, LOW = actively charging, HIGH-Z = full/idle/no battery
// Requires external 10kΩ pull-up resistor from 3.3V to GPIO 13
#define EXT_CHRG_DETECT       13
#define EXT_CHRG_DETECT_VALUE LOW

// Notifications
#define PIN_BUZZER 33        // Passive buzzer PWM (avoid strapping pin 12)
#define BUZZER_LEDC_CHANNEL 0 // LEDC channel for buzzer PWM (shared via BuzzerManager)
#define PIN_VIBRATOR 4       // Vibration motor (via NPN transistor, avoid strapping pin 15)
#define LED_PIN 2            // Built-in LED (Meshtastic default)

// Power Management (REMOVED - Using Meshtastic Firmware Sleep Modes Instead)
// #define PIN_GPS_PWR_EN 13    // GPS P-MOSFET gate (via NPN driver) - REMOVED
// #define PIN_OLED_GND_EN 23   // OLED GND switch (Silent Mode) - REMOVED
// Note: Hardware power gating removed to avoid GPS cold start issues and user confusion
// Meshtastic firmware handles power saving via sleep modes and GPS power management


// LoRa Ra-02 SX1278 (SPI Configuration - MESHTASTIC DIY DEFAULTS)
#define LORA_SCK 5           // Meshtastic DIY standard
#define LORA_MISO 19         // Meshtastic DIY standard
#define LORA_MOSI 27         // Meshtastic DIY standard
#define LORA_CS 18           // Meshtastic DIY standard
#define LORA_DIO0 26         // Interrupt: RX/TX done
#define LORA_DIO1 RADIOLIB_NC  // Not connected on SX1278/Ra-02 (only SX126x uses DIO1)
#define LORA_RESET 14        // Module reset (moved from GPIO 23 to free OLED_GND_EN)

// Supported radio modules
#define USE_RF95              // RFM95/SX127x (Ra-02 uses SX1278)

// Default LoRa region for TrekLink (VN_433 = MY_433 alias, 433.0-435.0 MHz, 20 dBm)
#define REGULATORY_LORA_REGIONCODE meshtastic_Config_LoRaConfig_RegionCode_MY_433

// Default timezone for TrekLink (Vietnam, GMT+7 Indochina Time)
#define DEFAULT_TIMEZONE "ICT-7"

// TrekLink variant flag (enables TrekLink-specific modules)
#ifndef TREKLINK_VARIANT
#define TREKLINK_VARIANT
#endif

// ============================
//   Canned Message Defaults
// ============================

// Default emergency message list (REQ-MSG-02.1)
// Format: Pipe-delimited string (|), max 800 bytes total (CANNED_MESSAGE_MODULE_MESSAGES_SIZE)
// Priority order: Emergency messages first for fastest access (REQ-MSG-02.3)
#define CANNED_MESSAGE_MODULE_MESSAGES_DEFAULT \
    "LOST - HELP|MEDICAL ISSUE|I'M SAFE|WAIT FOR ME|COME TO ME|LOW BATTERY"

// RX/TX control for SX127x (if external RF switch exists)
// Uncomment if your Ra-02 module has external TX/RX switch pins
// #define RF95_RXEN 14
// #define RF95_TXEN 13

