Swiftwing NAS V7.1 - Wi-Fi UI / ESP32 Monitor

Goal
- Keep the original V6 domain design unchanged.
- The normal NAS address remains: http://swiftwingnas.tplinkdns.com
- Keep C3 DNS behavior, external DDNS/port-forward behavior, storage nodes and monitor behavior.
- Use an ESP32 Dev Module / ESP32-WROOM-32 for the Monitor node.
- Optional SSD1306 OLED wiring on that board: SDA GPIO21, SCL GPIO22; OLED support is disabled by default.
- Add one Wi-Fi control in the Master UI.

Normal use
1. Open http://swiftwingnas.tplinkdns.com
2. Login.
3. Tap/click the "📶 Wi-Fi" button.
4. Scan nearby Wi-Fi, choose one, enter the password, and press "Connect all 5 boards".
5. Master sends the new Wi-Fi to Node1, Node2, C3 DNS and ESP32 Monitor.
6. All five boards save it and restart.

Recovery / first setup
- Master always provides the password-protected recovery AP:
  SSID: Swiftwing-Bridge
  Password: swiftwing-setup
- If home Wi-Fi is unavailable, connect a phone to that AP and open:
  http://192.168.4.1
- The same Wi-Fi picker is shown.
- Other nodes automatically fall back to Swiftwing-Bridge when their saved home Wi-Fi is unavailable, so one configuration on Master can provision them.

Important
- This version intentionally does not redesign the original tplinkdns.com logic.
- Wi-Fi credentials selected in the UI are stored in flash and do not require Arduino IDE edits afterward.
