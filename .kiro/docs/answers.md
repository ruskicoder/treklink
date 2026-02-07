[This file will change depending on the questions asked by the AI agent. It is temporary and should be only used for immediate answers]

1. I am replacing the E32 with the Ra-02 completely.
2. for Ra-02 allocation, use the Meshtastic defaults with our custom:

#pragma once

// LoRa SPI (Ra-02 / SX1278)
#define LORA_SCK 14
#define LORA_MISO 12
#define LORA_MOSI 13
#define LORA_CS 15
#define LORA_DIO0 26 // Required for SX127x
#define LORA_RESET 32

// I2C Peripherals (OLED & MPU6050)
#define SDA 21
#define SCL 22

// Hardware Silent Mode (OLED GND Switch)
#define PIN_OLED_GND_EN 23 

// GPS (Neo-6M)
#define GPS_RX 16
#define GPS_TX 17

// User Interface
#define BUTTON_PIN_MENU 25
#define BUTTON_PIN_UP 27
#define BUTTON_PIN_DOWN 35 // Input only
#define BUTTON_PIN_SOS 34  // Input only

// Notifications
#define PIN_BUZZER 4
#define PIN_VIBRATOR 2
#define PIN_LED 5

// Battery Sensing
#define BATTERY_PIN 32 // ADC channel

note this only proposed, because lack of actual knowledge of which default pin the meshtastic firmware is using. 


3. Ra-02 ready to run! returned the E32 module already. The Ra-02 is a pinout version ready for breadboard testing with DIO0 pre-exposed. it also have a through-hole unsoldered rail if we want to expand to 1,2,3.
4. we will reuse the antenna. i already purchased the connector for it. 
5. antenna should be remained mounted on the right side of the device, as per original specs.
6. our power gating circuit will be remained, though allocate different pins for those.
7. use per original meshtastic firmware or custom
8. I am okay with higher idle current, though we will reuse meshtastic's sleep mode.
9. if possible use hybrid option to still use Meshtastic radio layer while custom packets of our own for lightness & security. This means that traditional messages would not be visible to other meshtastic users, but in the event of SOS, or public broadcast, we will disable our custom packet layer and use the meshtastic radio layer and transmit public broadcast messages. So it will be a hybrid of both.
10. for encryption, we will use both: PKC at the bottom, our AES 128 GCM & channel ID layer on top, for private "TrekLink" devices only; public, broadcast messages including SOS will only use meshtastic's protocol.
11. we will not use the header obfuscation of our own anymore, use meshtastic's instead. Instead we will encrypt the payload only.
12. Because in development, nodes cannot communicate with each other due to mismatch of boot time, and unable to generate equal hopping, for MVP, sadly we will disable the hopping feature. 
13. we will use meshtastic's improving routing algorithm.
14. For interoperability, we will implement modes: 
    - Public Broadcast, universal to all Meshtastic devices (use conventional meshtastic protocols only)
    - Private Broadcast, Treklink devices only (use our own custom protocol + encryption)
    - SOS, universal to all Meshtastic devices (use conventional meshtastic protocols only)
15. We will implement local only, and all rebroadcast modes are within the Treklink protocol only. this means that there are 3 separate mode of rebroadcast: Treklink local, Treklink public, and Meshtastic public. As mentioned, the Treklink protocols will use our AES 128 GCM & channel ID layer on top with channel IDs for teams, public Treklink will still use our proprietary protocol + encryption but public to only TrekLink devices only (this is important, in this mode Meshtastic devices cannot decipher our own custom protocol), and only in Meshtastic public mode do we use conventional meshtastic protocols.
16. I do not know, it depends on the default pinouts of the original meshtastic firmware or custom
17. We will use Meshtastic GPS service + navigation UI.
18. We will keep fall detection as a separate service that always runs when on, and will trigger Meshtastic SOS when detected and user not respond in under 30s.
19. Option C.
20. Yes, we will custom implementation.
21. Hardware level power gating + only need a single additional GPIO for signal to the mosfets.
22. use meshtastic battery telemetry.
23. Option A for best customizability.
24. use meshtastic's OTA feature, but our OWN OTA service. (because our device supports custom proprietary features regular meshtastic devices dont have)
25. features are MUST-HAVE for the fork to be viable:
 Working Ra-02 433MHz SPI radio, fall detection, and working device, other are NOT optional but if the developmet window is too short that ended prematurely, a working fork of meshtastic (no need our custom protocol whatsoever) is still viable.
26. 2.6.x for latest stable release for support of 433mhz ESP32 devkit + Ra-02
27. Yes, please but do mind other GPIOs, or use my proposal, or use meshtastic's defualt, any, as long as all modules are plugged in the ESP32 and our code can work flawlessly with each of the modules.
28. Yes i have and we will:
 Ra-02 modules already available for testing
 Start from scratch with new hardware
29. Primary reason is to use existing meshtastic UI, mesh protocol, phone connect support, and GPS service. Secondary reason is to use existing meshtastic phone app. NOTE: a BIG IF here, if we cannot managed to pull our custom protocols in the timeframe, WE MUST PRIORITIZE our device as a generic Meshtastic compatible mesh device. Which means first phases of development is to get our device to work as a generic Meshtastic compatible mesh device, and then we will implement our custom protocols on top of it. This is a BIG IF, and we will need to reevaluate our strategy if this is not possible or not done under the timeframe we will SKIP our own implementation (no power gating, no sleep mode, keep custom buttons, but NO FALL DETECTION).

