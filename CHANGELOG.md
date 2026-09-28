# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.1.28] - 2026-09-28

### Added
- **BLE Wireless Connectivity (NimBLE Stack):**
  - HID over GATT Profile (HOGP) virtual keyboard for wireless password typing.
  - Nordic UART Service (NUS) for wireless diagnostic console and configuration.
  - Battery Service (BAS) reporting live battery percentage and charging telemetry.
  - Dual-transport automatic routing: seamless switching between USB HID and BLE HOGP.
  - Zero-Trust BLE console filtering: restricted sensitive operations over wireless while permitting monitoring and biometric enrollment.
- **3-Tier Power Management:**
  - Active, Idle Standby with tickless dynamic frequency scaling, and Deep Sleep.
  - Capacitive touch wakeup on GPIO 2 with anti-ghost false trigger filtering.
- **Aura Breathing LED Customization:**
  - Dual-mode breathing color palettes configurable per mode (HID mode default Cyan ➔ Blue; PIV mode default Yellow ➔ Red).
  - 7 hardware RGB color choices supported (`SET LED_HID <start> <end>`, `SET LED_PIV <start> <end>`).
  - Stored in NVS Flash (`CONFIG_VERSION = 7`).
- **Web Controller Enhancements (Apple Glass UI):**
  - Web Bluetooth API integration alongside Web Serial API.
  - Drag-and-drop firmware upload modal with automatic SHA-256 verification for Serial OTA.
  - Apple Touch ID concentric circle animated fingerprint modal with pulse rings and scan beam.
  - Backup & Restore configuration with client-side AES-256-GCM encryption.
  - PIV SmartCard PIN reveal/copy buttons with dynamic SVG iconography.
  - Real-time build timestamp and firmware version display.
- **PIV SmartCard NIST SP 800-73 Emulation:**
  - Native USB CCID interface with on-chip RSA-2048 hardware signing via mbedTLS.
  - Default PIV PIN set to `754321` with automated macOS pairing guide (`sc_auth`).
- **Compiled Release Binaries:**
  - Automated generation of signed unified binary `firmware/build/tiny_touch_unified.bin` and timestamped build artifacts.

### Changed
- Bumped communication protocol to **Protocol v7** incorporating `led_hid`, `led_piv`, transport status, and battery levels into `OK STATUS` telemetry.
- Restructured repository layout into modular `firmware/`, `controller/`, and `docs/` directories.
- Refactored sensor LED breathing speed to a calm 0xc0 pulse rate.

### Fixed
- Resolved anti-ghost filter false triggers during USB disconnect and transition to deep sleep.
- Fixed USB HID Caps Lock compensation via Output Report to prevent mis-typed passwords.
- Resolved race conditions in BLE advertising restarts and MTU payload exchanges.
- Fixed command parsing for `SET LED_HID` and `SET LED_PIV` in console interface.

---

## [0.1.0] - 2026-09-20

### Added
- Initial release of **tinyTouch** biometric USB security dongle on ESP32-S3.
- Core USB HID keyboard emulation with Caps Lock inverted typing.
- UART driver for Grow/Synochip optical and capacitive fingerprint sensors (R503, SW111).
- Non-Volatile Storage (NVS) for host pairing keys and keystroke delay configurations.
- Web Serial Controller interface built with pure vanilla HTML5/CSS3/JavaScript.
- Challenge-Response Host Pairing with HMAC-SHA256 via Web Crypto API.
