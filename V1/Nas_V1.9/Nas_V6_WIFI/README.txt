Swiftwing NAS - Master UI V1.2.9

- Centers the Workspace / Snapshot title, current folder and description in its summary area. Desktop centers it in the summary's left section; on narrow layouts it centers across the full row. Summary metrics, file list, toolbar and APIs are unchanged.
- Updates the Master UI version label to V1.2.9. Rebuild and flash Swiftwing_Master only; child boards are unchanged.

Swiftwing NAS V1.3.0 - Login Intro / Master UI V1.2.8

Login opening animation
- Adds an approximately 2.5-second Swiftwing opening before the existing sign-in and account-request page. The SVG emblem gains a softly breathing glass-like core, rotating coordinate arcs and a passing optical highlight; a restrained blue-violet haze and dotted grid sit behind the circular reveal into the existing login panel.
- The intro contains only the centered animation. Click or tap its center, or press Escape, Enter or Space to skip. It skips automatically when the device requests reduced motion. Dark and light browser themes are both supported.
- Login, account approval, API routes, file UI, network setup and child-board firmware are unchanged. This UI update requires flashing Swiftwing_Master only.
- Rebuild the four embedded Master pages by running python .\build_webui.py from Swiftwing_Master. The archive includes the rebuilt Master sketch.
- The supplied development environment has no Arduino CLI or connected board, so this package has not been compiled or tested on hardware. Verify the sign-in and account-request flows after flashing.

Swiftwing NAS V1.3.0 - Owner Approved Accounts / Master UI V1.2.7

V1.3.0 account access changes
- New sign-ups create pending requests. The user chooses a username and password, but cannot sign in until the owner approves the request in the Master UI's Users panel.
- The owner can approve or reject pending names, and can revoke access for active accounts. The admin account remains separate and is never listed as a user account.
- The Users API returns usernames only. Each account retains its salted password hash; the owner cannot view a user's password or hash.
- Up to 8 requests can wait at once; the existing 16-account total limit remains. Rejecting a request removes it so the username can be requested again.
- Approved accounts still share the existing NAS volume; this update does not create private home folders.
- Existing active user accounts stay active after updating. Any legacy pending account remains locked until the owner approves it.
- The Master UI does not send email or SMS codes and does not support Apple sign-in in this release. Approve only requests whose identity you have checked outside this page. The current login page uses HTTP, so use a trusted LAN or VPN.
- This account change requires flashing Swiftwing_Master only. Node1, Node2, C3 DNS and ESP32 Monitor firmware are unchanged.

Swiftwing NAS V1.2.9 - Smaller Child Firmware and Offline Cooling / Master UI V1.2.6

Master UI V1.2.6 - Floating Liquid Glass navigation
- Reworks the System Status header as a floating, translucent glass bar with stronger refraction-like blur, layered edge highlights, and pointer-following reflections.
- The bar compresses while scrolling down and expands when scrolling up; interactive controls use larger, rounded glass surfaces in both themes.
- Keeps file rows and dense status data on solid, readable surfaces; existing APIs, upload handling and child firmware are unchanged.
- This UI patch requires updating only the Master board.

V1.2.9 changes relative to V1.2.8
- Flash Swiftwing_Node1, Swiftwing_Node2, Swiftwing_C3_DNS and Swiftwing_ESP32_Monitor to receive this update. Swiftwing_Master and all four UI pages are byte-identical to V1.2.8; a Master already running V1.2.8 does not need reflashing.
- The four child boards use a bounded Wi-Fi client to read the Master's fixed bootstrap endpoint instead of linking the general HTTPClient library. The matching Master sends a Content-Length response; valid Wi-Fi credentials require at most 213 bytes and fit in the 255-byte reader limit.
- Node1 and Node2 enable Wi-Fi modem sleep while connected to the Bridge and waiting for provisioning. When the Bridge cannot be found, Node1, Node2 and C3 turn their radios off for five seconds before retrying. Automatic provisioning remains enabled; recovery after an unavailable Bridge may start up to about 3.5 seconds later than before.
- Node1 and Node2 avoid repeating SD free-space queries in /api/info. Its JSON fields, values and route remain the same. Their upload handlers and the V1.2.8 raw upload path are unchanged.
- Default builds with ESP32 core 3.3.12 reduce Flash by 96,708 bytes in total across the four child boards and reduce compile-time global RAM by 3,408 bytes. The Master image is unchanged. Optional Monitor OLED plus legacy fan settings also compile.
- No physical boards are connected, so bootstrap timing, actual upload MB/s, current and temperatures remain unmeasured. If all boards are hot during normal connected operation, identify whether the heat comes from the ESP chip, board regulator, SD module or power connection before treating firmware alone as a complete fix.

Earlier release notes and operating instructions follow.

Swiftwing NAS V1.2.8 - Raw Upload Fast Path / Master UI V1.2.4

V1.2.8 changes relative to V1.2.7
- Flash Swiftwing_Master, Swiftwing_Node1 and Swiftwing_Node2 to enable the new upload path. C3 DNS and ESP32 Monitor are unchanged from V1.2.7; keep their V1.2.7 firmware if already installed.
- The Master checks each storage node's current /api/info before each upload. Updated nodes receive an application/octet-stream upload that bypasses the Arduino WebServer multipart boundary scan; older nodes keep using the original multipart endpoint.
- The original /upload endpoint remains available. The new /upload-raw endpoint retains its direct overwrite and 8 KiB SD buffer behavior, so an interrupted overwrite can leave a partial destination file, as before. Avoid power loss during writes and keep backups of important files.
- Upload progress now reports long SD finishing separately. A request with no upload progress for 60 seconds, or no final storage response for 180 seconds after sending, is reported as failed. Files above 2.147 GB are rejected because this WebServer core uses a signed Content-Length parser.
- The file manager appearance and visible UI_BUILD label are unchanged. The v1.2.7 idle-power and fan-startup changes remain included in this package.
- All five default sketches compile with ESP32 core 3.3.12; the four embedded UI pages match their editable sources. Browser simulations cover raw/legacy upload, UTF-8 paths and names, errors, zero-byte files and same-IP node downgrade.
- Actual MB/s, current and temperatures have not been measured on the physical boards. This version reduces upload parser work but does not prove faster uploads or normal temperatures. See OPTIMIZATION_REPORT.md for measured build sizes and test limits.

Earlier release notes and operating instructions follow.

Swiftwing NAS V1.2.7 - Idle Power and Fan Startup / Master UI V1.2.4

V1.2.7 changes relative to the supplied V1.2.6 package
- Flash all five boards to receive their respective idle-power and fan-startup changes.
- Master runs its CPU at 160 MHz; C3 DNS and ESP32 Monitor run at 80 MHz.
- Node1 and Node2 retain their existing 8 KiB upload buffering, fast Wi-Fi mode during transfers, and SD bus settings. Their idle loop waits longer; a stalled download connection now closes after 10 seconds without a successful TCP write.
- Monitor completes the 300 ms fan start pulse before Wi-Fi setup can block, and uses longer idle waits. The stored target speed and the existing fan controls are unchanged.
- The four Master UI pages and their API routes are unchanged. The visible UI_BUILD label remains V1.2.4 because this package does not change the page design.
- This is a source/build update. Upload speed, current draw, and temperature have not been measured on the five physical boards. See OPTIMIZATION_REPORT.md for validation and the measurements needed before claiming a thermal improvement.

Earlier release notes and operating instructions follow.

Swiftwing NAS V1.2.6 - Summary Border Fix / Master UI V1.2.4

Goal
- Keep the original V6 domain design unchanged.
- The normal NAS address remains: http://swiftwingnas.tplinkdns.com
- Keep C3 DNS behavior, external DDNS/port-forward behavior, storage nodes and monitor behavior.
- Use an ESP32 Dev Module / ESP32-WROOM-32 for the Monitor node.
- Optional SSD1306 OLED wiring on that board: SDA GPIO21, SCL GPIO22; OLED support is disabled by default.
- Add one Wi-Fi control in the Master UI.
- Refresh the existing file manager, login, status and Wi-Fi pages with a dark cobalt control-console theme.
- Keep the editable page sources under Swiftwing_Master\ui; run python .\build_webui.py from Swiftwing_Master to embed them in flash.
- The visible UI version comes from UI_BUILD in Swiftwing_Master. After changing it or the UI pages, run the build_webui.py command above and flash the Master board; the other boards do not need an upload for UI label changes.
- V1.2.6 closes the left and right edges of the workspace summary strip. This UI fix requires uploading Swiftwing_Master; the Monitor fan-startup correction from V1.2.5 remains included in this package.
- V1.2.3 account and UI changes are Master-only. Compile/upload Swiftwing_Master once; do not reflash the ESP32 Monitor, storage nodes or C3 DNS for this update.
- The theme control stores the selection in this browser and applies it to the file manager, system status, login and Wi-Fi setup pages. Shared theme rules live in Swiftwing_Master\ui\theme.css.
- The System Status page controls the ESP32 Monitor fan at 0–100% PWM in 5% steps. The selected speed is saved on that Monitor and restored after reboot; the fan starts OFF until a speed is chosen.
- Default fan mode supports the 4-wire PWM fan used by Raspberry Pi 5. Monitor GPIO25 drives a 25 kHz signal through an external open-collector transistor; the fan motor needs a suitable 5 V supply. Never connect the motor or PWM input directly to an ESP32 GPIO. See FAN_WIRING.txt for the Pi header pinout and safe wiring.
- The Monitor loads and applies the saved fan speed before it waits for Wi-Fi, so a router outage does not leave the fan at its power-on fail-safe speed while the board retries its connection.
- Legacy 2-wire/3-wire power-PWM fans remain selectable by setting FAN_OUTPUT_4WIRE_PWM to 0 in the Monitor sketch and using the documented low-side MOSFET circuit.
- Only the owner account can change fan speed. No tachometer input is connected, so the page reports the PWM setpoint/output rather than measured RPM.
- Anyone can create a user account on the login page and sign in immediately. Each user chooses their own username and password; the Master stores a per-user salted password hash, never the readable password. The Master stores up to 16 user accounts in NVS; passwords must be 8–64 characters.
- The admin account remains separate (`admin`) and only the owner account can open the Users panel, view usernames, revoke access or change Wi-Fi settings. The Users panel never returns password hashes or passwords.
- User accounts share the existing NAS volume and file permissions. The current system does not create private home folders per account. Older pending account requests are automatically enabled by this version.
- The Master serves the login page over HTTP on port 80. Use a trusted LAN or VPN for sign-in; the current firmware does not provide TLS encryption.

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
- If the Master already has the fan slider (V1.2.2 or later), upload only the updated Swiftwing_ESP32_Monitor for 4-wire PWM support. For an older Master without the fan slider, also upload Swiftwing_Master. Node1, Node2 and C3 DNS do not need changes.
