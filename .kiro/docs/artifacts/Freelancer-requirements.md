Your purchase was confirmed! Next, fill in some info get your order started
The freelancer will need this info before they can start working on your order:

1. If you’re ordering for a business, what’s your industry?(Optional)
Select the project that best fits your needs

Save industry to my profile.
2. Is this order part of a bigger project you're working on?(Optional)
Yes
Don’t see what you’re looking for? Start typing to find your project

3. What type of Project you want that I design for you?
Off-grid LoRa mesh communication device (TrekLink v2.0) running Meshtastic. Integrates ESP32-S3-WROOM-1-N8R8, SX1262 LoRa, GNSS GPS, IMU, and OLED. Fits a 100x70mm 4-layer PCB inside a 30mm thick enclosure.
Deliverables: KiCad project files (.kicad_pcb, .kicad_sch) compatible with direct JLCPCB import (BOM/CPL generated via KiCad JLCPCB plugin).
Aesthetic: Vintage 80s RAM card style. Low-speed digital/power traces must be curvy/rounded (no sharp 45/90 deg angles). Add teardrops to all pads.
Silkscreen: Meshtastic logo + "TrekLink" text on top; add a serial number frame placeholder: "v2.0 - [______]".

4. What type of circuit or pcb design you want ?
4-layer PCB (Signal-GND-PWR-Signal). 100% in-stock LCSC parts only.
Key Components:
- U1: ESP32-S3-WROOM-1-N8R8 (C2913201)
- U2: E22-400M22S SX1262 LoRa (C411291)
- U3: u-blox NEO-M9N GNSS (C5119087)
- U4: ICM-20948 IMU (C726001)
- U5: TPS63802DLAT Buck-Boost 3.3V (C1849531)
- U6: BQ24074RGTR Charger (C54313)
- U7: DW01A Protection (C351410) + Q1: FS8205A Dual N-MOSFET (C32254)
- J1: USB4105-GF-A USB-C (C2688138)
- L1: cjiang FTC252012SR47MBCA 2520 0.47uH (C5832368)
Layout Rules & Checklist (Designers must verify):
1. GPS Antenna: NEO-M9N does NOT have an internal antenna. Route RF_IN (pin 11) to a dedicated U.FL (IPEX) connector for external GPS antenna. Do not layout a passive patch directly on the module.
2. Power Path: Connect TPS63802 buck-boost input to BQ24074 OUT pin, NOT to BAT pin, to maintain USB power-path operation.
3. USB-C CC: Use independent 5.1k-ohm pull-down resistors for CC1 and CC2. Do NOT tie them together. Route D+/D- as a 90-ohm differential pair directly to ESP32-S3 GPIO19/20. ESD protection (USBLC6-2SC6) placed close to port.
4. RF Lines: Route 433MHz LoRa RF path as a straight 50-ohm microstrip to U.FL connector. No curves on RF/high-speed lines. Keep-out copper 5mm around RF paths.
5. Inductor: Use 2520 metric footprint for L1 (not 2016).
6. Power/GND: 3.3V power rails must be min 20mil wide. Put solid ground plane under ESP32, LoRa, and GPS.
7. Test Pads: Round 1.5mm labeled test pads for TP_3V3, VBAT, VUSB, GND, SDA, SCL, GPS_TX, LORA_CS, CHG.


The information I provided is accurate and complete. Any changes will require the seller's approval, and may be subject to additional costs.