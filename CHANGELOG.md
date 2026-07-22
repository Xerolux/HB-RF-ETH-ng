# Changelog - HB-RF-ETH-ng Firmware

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changes
- fix(webui): Übersetzungsvollständigkeit hergestellt. Alle 10 Sprachen (cs, de, en, es, fr, it, nl, no, pl, sv) haben jetzt identische Key-Sets (647 Leaf-Keys pro Sprache, vorher 13 fehlend in jeder Nicht-de/en-Sprache). Betroffen waren v.a. die `login.passwordReset*`-Keys (Passwort-zurücksetzen-Flow) und `systemlog.crashTitle`/`crashHint` (Crash-Log-Hinweis).
- fix(webui): Bisher unübersetzte Werte (noch englischsprachig) in cs/es/fr/it/nl/no/pl/sv durch echte Übersetzungen ersetzt. Betroffen waren `nav.documentation`, `settings.advancedTitle`, `settings.showExperimental*` (6 Keys), `settings.experimentalEmpty*` (2 Keys), `updates.*` (Eyebrow/Titel/Subtitle/checkNow/checkingNow + alle `checkResult*` 10 Keys), `firmware.factoryReset*` (5 Keys), `monitoring.resourceWarningTitle/Text`. Diese Texte erschienen vorher bei nicht-de/en-Sprachen englischsprachig; jetzt sind sie konsistent in der jeweiligen UI-Sprache.
- feat(release): WebUI-Release-Notes werden automatisch aus der `[Unreleased]`-Sektion des `CHANGELOG.md` generiert (neues Skript `generate_webui_release_notes.py`, integriert in `release-webui.yml`). Zuvor enthielten WebUI-Releases auf GitHub nur Boilerplate ohne Änderungsbeschreibung.
- refactor(webui): Visuelles Design über alle Seiten vereinheitlicht. Hartcodierte Schrift-Gewichte (400/500/600/700/800) in allen Vue-Komponenten durch Tokens (`var(--font-weight-*)`) ersetzt; hartcodierte px-Padding-/Gap-/Radius-Werte auf `--space-*`/`--card-padding`/`--radius-*`-Tokens migriert. Dies ist die Fortsetzung des in Beta.6 begonnenen Typo-Refactors und schließt die letzten inkonsistenten Seiten (settings, monitoring, systemlog, login, app, sysinfo, NewDesignHeader, theme, webuiupdate, firmwareupdate, ChangelogModal, PasswordChangeModal) ab.
- feat(webui): About- und Passwort-Ändern-Seiten erhalten denselben kanonischen Page-Hero (`.page-hero` + `.hero-eyebrow` + `.hero-title` + `.hero-subtitle`) wie alle anderen Inhaltsseiten. Vorher hatte About nur eine flache `.section-header` und Passwort-Ändern einen abweichenden Bootstrap-BCard-Gradient-Header. Neuer i18n-Key `changePassword.eyebrow` in allen 10 Sprachen.
- refactor(webui): Section-Titel-Typografie auf `settings.vue` an den kanonischen `.card-section-title`-Stil angeglichen (vorher uppercase + letter-spaced + sekundärfarbig).
- refactor(webui): Karten-Tiefe vereinheitlicht. `systemoverview.vue` (`.overview-card`/`.detail-card`) und `theme.vue` (`.theme-card`) nutzen jetzt `--shadow-md` statt `--shadow-sm` und wirken damit nicht mehr flacher als alle anderen Seiten.
- fix(webui): Drei undefinierte CSS-Tokens (`--color-bg-secondary`, `--color-text-inverse`, `--color-warning-strong`) in `main.css` für Hell- und Dunkel-Modus definiert. Die crash-tail-Box in `systemlog.vue` renderte vorher immer in dunklen VSCode-Farben (die Fallbacks griffen unkonditional); Warning-Texte in `monitoring.vue`/`sysinfo.vue` waren nicht theme-konform.
- fix(webui): Akzent-Farbpicker aus `/theme` greift jetzt an allen Stellen durch. Hartcodierte Glass-UI-Orange-Literale (`#f26a3d`, `rgba(242,106,61,…)`), die Akzent-abhängige Stellen blockierten (Supporter-Medaillon, Supporter-Icon/Badge/Card-Active-State auf settings.vue; Supporter-Hero-Chip auf sysinfo.vue; expired-prompt-icon auf app.vue), wurden durch Tokens ersetzt.
- fix(webui): Bootstrap-Blau-Fallbacks (`#0d6efd`, `#e7f1ff`) und Amber-Literale in `monitoring.vue` durch echte Tokens (`var(--color-info)`, `var(--color-warning-strong)` etc.) ersetzt.

## [2.2.5-Beta.8] - 2026-07-21

### Changes
- feat: mDNS-Komponente vollständig entfernt (espressif/mdns, MDns-Wrapperklasse, alle Aufrufstellen). mDNS belegt im ESP-IDF rund 15–30 KB Heap; unter aktiver CCU-3-Session fiel der freie Heap auf ~58 KB (largest 44 KB), was dazu führte, dass die manuelle Update-Suche mit „Manual update check skipped (low heap)" übersprungen wurde. Nach Entfernung läuft die Update-Suche wie auf einem unbelasteten Gerät durch. Der Raw-UART-UDP-Listener (Port 3008) ist von mDNS unabhängig und funktioniert unverändert.
- ⚠️ Verhaltensänderung für Anwender: Die automatische CCU-3-Entdeckung per mDNS (_raw-uart._udp) entfällt. Die CCU 3 muss künftig mit der festen IP-Adresse des HB-RF-ETH konfiguriert werden (bzw. per DHCP-Reservation eine feste IP erhalten). Port bleibt UDP 3008. Das WebUI-Feld „Hostname" bleibt erhalten (für DHCP/DNS-Namen), wird nur nicht mehr über mDNS beworben.
- chore: Zwei „by design"-Logzeilen beruhigt. Die SupporterCRL-Meldung „CRL fetch returned status 404" (erwartete Server-Antwort, wenn kein Supporter-Key widerrufen wurde) wurde von INFO auf DEBUG gesenkt und verschwindet aus dem Standard-Log. Die RawUartUdpListener-Warnung „unexpected endpoint identifier … - adopting client identifier" (erscheint einmal pro Geräteneustart, wenn die CCU mit ihrem alten Session-Token reconnectet; Adoption ist semantisch sicher) wurde von WARN auf INFO gesenkt. Keine Verhaltensänderung, ausschließlich Log-Sichtbarkeit.

## [2.2.5-Beta.7] - 2026-07-21

### Changes
- feat(webui): Manuelle Updatesuche über „Jetzt nach Updates suchen" wiederhergestellt. Neuer Backend-Endpunkt POST /api/check_update löst sofortigen Manifest-Abruf aus (läuft außerhalb des httpd-Threads), 60-Sekunden-Cooldown verhindert Missbrauch; die automatische 24-Stunden-Prüfung bleibt unberührt. Die Schaltfläche erscheint konsistent auf den Firmware- und WebUI-Update-Tabs und zeigt Lade-, Update-, Aktuell- und Fehlerzustände an.
- refactor(webui): Eigenständiger Menüpunkt „Design wechseln" entfernt. Die Theme-Auswahl ist ausschließlich unter Einstellungen → Design erreichbar; der Header-Sonne/Mond-Schnellwechsler bleibt, und die /theme-Route bleibt für Lesezeichen erreichbar.
- fix(webui): Fokus- und Hover-Zustände nutzen jetzt Design-Tokens (var(--color-primary) / var(--color-primary-soft) / var(--shadow-md)) statt hartcodierter Glass-UI-Orange-Tokens — Login-Eingaben, Login-Button, Passwort-Änderungs-Modal und Selbsttest-Test-Button erscheinen damit nicht mehr orange im grünen NewDesign.
- fix(webui): Dashboard-Zeile „Letzter Neustart" in „Neustartgrund" umbenannt (Wert ist die Ursache, keine Zeitangabe); alle 10 Sprachen aktualisiert.
- docs: POST /api/check_update in API.md und openapi.yaml dokumentiert (202 Accepted, {triggered, fetchInProgress}, Client-Polling).

## [2.2.5-Beta.6] - 2026-07-21

### Changes
- fix(webui): Supporter-Key-Dialog blockiert die Seite nach dem Schließen nicht mehr. BModal (BootstrapLite) rendert jetzt sichtbare OK-/Cancel-Buttons, unterstützt Escape und Hintergrund-Klick und nutzt einen gemeinsamen Body-Scroll-Lock mit dem Mobile-Menü, so dass nie ein hängenbleibendes Overlay zurückbleibt.
- fix(webui): Inhalte werden vom fixierten Header nicht mehr überdeckt. Neue Layout-Tokens (--newdesign-header-height, --newdesign-content-top, --newdesign-sidebar-width) ersetzen hartcodierte 112/168/384px-Offsets in app.vue und NewDesignHeader.vue.
- fix(webui): System- und Netzwerkkarten auf der Statusseite werden nicht mehr gequetscht nebeneinander dargestellt; sie nutzen die volle Inhaltsbreite. Status-Sammelkarten oben bleiben responsiv (≤4 Desktop / 2 Tablet / 1 Mobil).
- feat(webui): Firmware- und WebUI-Updates sind unter einem gemeinsamen Menüpunkt „Updates" zusammengefasst. Neue verschachtelte Routen /updates/firmware und /updates/webui mit Untermenü; /firmware und /webui bleiben als Redirects erhalten.
- feat(webui): Einstellungen neu strukturiert — neue Tab-Reihenfolge Allgemein · Netzwerk · Zeit · Backup · Design · Lizenz; Design-Tab bettet die Theme-Auswahl ein; doppelte Status-Chips im Header entfernt und durch eine nicht-anklickbare „Gespeichert"-Anzeige ersetzt.
- feat(webui): Experimentelle Funktionen sind standardmäßig ausgeblendet und werden nur nach aktivierbarer Expertenoption unter Einstellungen → Allgemein eingeblendet. Gespeicherte Werte bleiben beim Ausblenden erhalten.
- feat(webui): Link zur Projektdokumentation im linken Menü; URL zentral in useDocsLink.js konfiguriert, öffnet in neuem Tab mit External-Link-Icon.
- refactor(webui): Typografie auf eine Schriftfamilie (Inter via Google Fonts) und zentrale Tokens (--font-weight-*, --line-height-*, --space-*, --card-padding, --section-gap) vereinheitlicht; hartcodierte px-Schriftgrößen durch Tokens ersetzt.
- fix(webui): Werkseinstellungen auf der Firmware-Seite sind jetzt als Gefahrenaktion mit eigenem Bestätigungsdialog markiert (vorher window.confirm).
- fix(webui): Index.html bereinigt — hartcodiertes deutsches experimental-Style-Override entfernt, Inter-Schrift über Google Fonts mit display=swap eingebunden.
- chore(webui): Neue UI-Strings (updates.*, nav.updates, nav.documentation, settings.tabDesign, settings.advancedTitle, settings.showExperimental*, firmware.factoryReset*) in allen 10 Sprachen hinzugefügt; fehlende experimentalEmpty* Schlüssel ergänzt.

## [2.2.5-Beta.5] - 2026-07-20

### Changes
- docs(changelog): record recovery, webui and mqtt fixes under [Unreleased]
- fix(mqtt): drop duplicate version topics that produced two HA 'Firmware Version' sensors
- refactor(webui): migrate 154 font-size declarations to type scale tokens
- fix(webui): route header supporter chip to /settings?tab=license
- fix(webui): recenter brand mark in BrandLogo.vue
- refactor(webui): fix accent color picker and introduce type scale in main.css
- fix(diag): unblock /recovery login by removing duplicate CSP header
- chore: update manifests for v2.2.5-Beta.4

### Changes
- fix: recovery page login was silently non-functional due to duplicate Content-Security-Policy headers. `httpd_resp_set_hdr()` appends rather than overwrites, so the recovery route emitted two CSP headers (one strict `script-src 'self'`, one permissive `'unsafe-inline'`); browsers enforce the intersection and blocked the page's inline script. Added `add_security_headers_inline_script()` in `security_headers.h` and switched the `/recovery` handler to use it instead of stacking both CSPs.
- fix(webui): accent color picker in /theme now affects the whole New Design UI. Previously the three `body.newdesign-active` blocks in `main.css` hardcoded `--color-primary` to emerald green, which won the CSS cascade over the theme store's inline style on `<html>`. Removed the hardcoded overrides (light, dark, dark-shell); the subtree now inherits the value the store sets via `shiftColor()` / `rgbaColor()`. The hardcoded green login glow and hover-border literals were also replaced with `var(--color-primary-soft)`.
- fix(webui): the "Projekt unterstützen" / supporter chip in `NewDesignHeader.vue` now routes to `/settings?tab=license` (matching the hero chip on the dashboard) instead of the generic `/settings` landing on the "Allgemein" tab.
- fix(webui): centered the brand mark in `BrandLogo.vue`. The three leaves' bounding box (incl. Bezier control points) was centred near (241.5, 263.5) within the 512×512 viewBox; a `transform="translate(15, -7)"` on the group recentres it without altering leaf geometry.
- refactor(webui): introduced a unified type scale (`--fs-2xs` … `--fs-3xl`) and font-family tokens (`--font-sans`, `--font-mono`) in `main.css`. Migrated 154 ad-hoc `font-size: Xrem` declarations across 18 files to the scale, eliminating the dense 12.48/12.8/13.12/13.28/13.6/13.76/14.08px collision band. Consolidated four divergent monospace stacks (Consolas / Cascadia / SFMono / ui-monospace) plus two references to an undefined `--font-mono` onto the single token, and replaced one non-standard `font-weight: 650` with `600`.
- fix(mqtt): removed duplicate version topics that produced two Home Assistant sensors named "Firmware Version". The legacy short topics `status/version` / `status/latest_version` (plus their HA discovery announcements) duplicated the explicit `firmware_version` / `latest_firmware_version` 1:1 after the dual-version refactor. Empty retained discovery payloads are now published for `sensor.version` and `sensor.latest_version` so HA deletes the duplicate entities automatically on the next status publish. The explicit set (`firmware_version`, `webui_version`, `latest_firmware_version`, `latest_webui_version`) is unchanged.

## [2.2.5-Beta.4] - 2026-07-20

### Changes
- ci: remove Beta.4 release dispatcher
- ci: trigger Beta.4 release
- ci: prepare Beta.4 release dispatch
- fix: recover WebUI and switch to safe manual updates
- ci: remove completed manual-model trigger
- ci: remove completed fixed-default workflow
- ci: remove completed PR 387 refactor workflow
- ci: remove obsolete firmware archive workflow
- ci: remove obsolete New Design test workflow
- ci: remove firmware archive generation from builds
- ci: remove completed PR 387 maintenance job
- ci: make guarded refactor failure diagnosable
- ci: add guarded PR 387 refactor job
- ci: add guarded PR 387 refactor runner
- ci: trigger fixed UI defaults
- ci: apply fixed New Design and restart sync defaults
- ci: trigger final manual update model
- ci: apply final manual update and fixed UI model
- ci: trigger final gzip hotfix cleanup
- ci: finalize gzip hotfix cleanup
- ci: trigger corrected gzip hotfix
- ci: fix gzip patch documentation path
- ci: capture gzip patch diagnostics
- ci: include staggered update-check guard
- ci: prepare gzip and strict 24h hotfix
- ci: remove completed Brotli fallback trigger
- ci: remove completed Brotli fallback runner
- ci: trigger Brotli fallback revision
- ci: add Brotli fallback revision runner
- ci: remove completed recovery hotfix trigger
- ci: remove completed recovery hotfix runner
- ci: trigger minimal recovery hotfix runner
- ci: add minimal recovery hotfix runner
- ci: remove broken recovery hotfix workflow
- ci: trigger Beta.3 recovery hotfix
- ci: apply Beta.3 recovery hotfix
- chore: update manifests for v2.2.5-Beta.3

## [2.2.5-Beta.3] - 2026-07-20

### Changes
- ci: remove Beta.3 release dispatcher
- ci: prepare Beta.3 release dispatch
- fix(i18n): use German as the only fallback (#386)
- ci: remove unused translation maintenance workflow
- ci: commit translation audit before validation
- ci: trigger German translation audit
- ci: apply German translation fallback audit
- ci: remove unused draft validation workflow
- ci: complete draft release validation

## [2.2.5-Beta.2] - 2026-07-20

### Changes
- ci: remove completed draft release dispatcher
- ci: validate full and WebUI-only draft releases
- feat: add lightweight system recovery themes and log diagnostics (#385)
- ci: remove completed diagnostics review workflow
- ci: apply diagnostics review fixes
- ci: remove completed diagnostics maintenance workflow
- ci: rebuild diagnostics branch on merged updater
- feat: separate New Design WebUI updates from firmware (#380)
- ci: remove completed WebUI maintenance workflow
- ci: apply final WebUI storage fixes
- chore: update manifests for v2.2.5-Beta.1

## [2.2.5-Beta.1] - 2026-07-19

### Changes
- Merge pull request #372 from Xerolux/fix/issue-371-reset-reason-heap-diag
- fix(diag): log reset reason at boot + expose task stack hwm
- chore: rebuild firmware archive
- chore: update manifests for v2.2.4

## [2.2.4] - 2026-07-15

### Changes
- chore: update manifests for v2.2.4-Beta.23

## [2.2.4-Beta.23] - 2026-07-15

### Changes
- Merge fix/webui-brotli-http-decoding-failed: revert embedded assets from brotli to gzip
- fix(webui): revert embedded assets from brotli to gzip
- chore: update manifests for v2.2.4-Beta.22

### Fixed
- **WebUI unbrauchbar (`ERR_CONTENT_DECODING_FAILED` in Chrome / rohe Brotli-Bytes in Firefox) seit Beta.19:** Die Brotli-Umstellung (Commit 1a912d8) hatte die eingebetteten WebUI-Assets (index.html, main.js, main.css) auf `Content-Encoding: br` umgestellt. Browser akzeptieren Brotli aber **nur über HTTPS** — über eine reine `http://`-Verbindung (wie sie das Gerät im LAN ausliefert) bieten Firefox/Chrome nur `gzip, deflate` an und dekodieren den Brotli-Stream nicht. Die Firmware sendete das `Content-Encoding: br` trotzdem hardcoded, wodurch Firefox die rohen komprimierten Bytes als Text anzeigte und Chrome mit `ERR_CONTENT_DECODING_FAILED` abbrach. Zurück auf Gzip für die eingebetteten Assets (`index.html.gz`, `main.js.gz`, `main.css.gz`); Gzip wird von jedem Browser auf HTTP und HTTPS unterstützt. Der Kompressionsunterschied zu Brotli (~15 %) ist im LAN irrelevant.

## [2.2.4-Beta.22] - 2026-07-15

### Changes
- fix(i18n): localize supporter prompt dismissal
- fix(webui): make supporter prompt dismissible
- chore: update manifests for v2.2.4-Beta.21

## [2.2.4-Beta.18] - 2026-07-15

### Changes
- Merge fix/ota-upload-restart-panic: stop subsystems before restart on WebUI upload path
- fix(ota): stop subsystems before restart on WebUI upload path
- chore: update manifests for v2.2.4-Beta.17

### Fixed
- **Exception/Panic nach WebUI-Datei-Update (fortlaufend):** Der Erfolgspfad des `/ota_update`-Handlers rief `full_system_restart()` auf, ohne vorher die heap-/netzaktiven Subsysteme (MQTT, CheckMK/Prometheus/Syslog, CRL-Refresh, UpdateCheck-esp_timer, WebSocket-Publish-Worker) zu stoppen — im Gegensatz zum URL-OTA-Pfad (`performOnlineUpdate`), der genau diese Stopp-Sequenz über `monitoring_pause_for_ota` / `supporter_crl_stop_refresh_task` / `UpdateCheck::stop()` ausführt. Bei aktiviertem Link-Down-Pause (flashPause) liefen diese Subsysteme während der 35-sekündigen Pause auf einem Ethernet, dessen MAC gerade gestoppt worden war, was in lwIP/TLS zu einem Exception/Panic führte, der beim Reboot sichtbar wurde. Der Erfolgspfad ruft jetzt `prepare_ota_heap()` auf, bevor der Restart erfolgt, sodass beide OTA-Pfade dieselbe konsistente Herunterfahr-Sequenz nutzen.

## [2.2.4-Beta.17] - 2026-07-14

### Changes
- fix(log): harden WebSocket stream lifecycle
- chore: update manifests for v2.2.4-Beta.16

## [2.2.4-Beta.16] - 2026-07-14

### Changes
- fix(ota): prevent panic after file update
- chore: update manifests for v2.2.4-Beta.15

### Fixed
- **Datei-Firmwareupdate endete trotz erfolgreicher Installation mit `Exception/Panic`:** Der verzögerte Neustart lief in einem nur 2 KB großen Task, obwohl der aktive Neustart-Sync darin auch den Ethernet-Treiber stoppt. Der Neustart läuft nun nach dem bereits gesendeten HTTP-Erfolg auf dem vorhandenen 8-KB-Webserver-Stack; der 4-KB-Uploadpuffer wird vorher freigegeben. Das vermeidet sowohl den Stacküberlauf als auch eine zusätzliche, auf dem fragmentierten Post-OTA-Heap unzuverlässige Task-Allokation.

## [2.2.4-Beta.15] - 2026-07-14

### Changes
- feat: remove unreliable paste-service log sharing
- chore: update manifests for v2.2.4-Beta.14

## [2.2.4-Beta.14] - 2026-07-14

### Changed
- **UpdateCheck: permanenter Hintergrund-Task durch esp_timer ersetzt.** Der bisherige `UpdateCheck`-Task belegte 12 KB Stack dauerhaft, schlief darin aber 24 Stunden lang (`vTaskDelay`), bis zur nächsten Prüfung. Ersetzt durch einen `esp_timer`, der alle 24 h (sowie einmalig 30 s nach dem Boot) einen kurzlebigen `upd_chk`-Task (12 KB) startet, der `refresh()` ausführt und sich sofort wieder beendet. Die 12 KB Stack sind damit nur noch für die ~5 Sekunden des eigentlichen Checks resident statt 24 Stunden lang blockiert — ca. 12 KB zusätzlicher freier Heap.
- **Manuelle „Nach Update suchen"-Funktion vollständig entfernt.** Die Buttons im WebUI und via MQTT (`command/check_update`) sowie der zugehörige HA-Discovery-Button wurden gestrichen. Release-Informationen werden ausschließlich automatisch alle 24 h aktualisiert. Dies eliminiert zusätzlich den 16 KB on-demand Task `rel_refresh` (WebUI POST) und den 12 KB on-demand Task `mqtt_chkupd` (MQTT), die beide bei Knopfdruck entstanden und bei niedrigem Heap zum `Out of memory` (HTTP 500) führten. Der `GET /api/check_update`-Endpoint bleibt erhalten und liefert die zwischengespeicherten Daten des 24 h-Timers.

### Fixed
- **WebUI Update-Check lieferte HTTP 500 „Out of memory":** Bei ca. 30 KB freiem Heap schlug die Task-Erstellung (`rel_refresh`, 16 KB Stack) fehl. Durch die Umstellung auf den esp_timer und die Entfernung der manuellen Trigger entfallen diese on-demand Tasks vollständig. Erwarteter freier Heap nach dem Update: ~46 KB.

## [2.2.4-Beta.13] - 2026-07-14

### Changes
- fix: stabilize log sharing and MQTT update checks (#369)

### Fixed
- **Absturz (Exception/Panic) beim MQTT-Kommando "Check Update" aus Home Assistant:** Der durch `check_update` gestartete Task `mqtt_chkupd` hatte nur 8192 Bytes Stack, rief aber `UpdateCheck::refresh()` auf, das einen `ReleaseInfo`-Struct (~4,9 KB, davon `body[4096]`) auf den Stack legt und anschließend einen HTTPS-TLS-Handshake durchführt (~2–4 KB zusätzlicher Stack). Der Gesamtbedarf überstieg die 8 KB → Stack-Overflow → `ESP_RST_PANIC`. Der Hintergrund-Task `UpdateCheck`, der denselben `refresh()`-Aufruf durchführt, nutzt bereits 12 KB — der MQTT-Task war der inkonsistente Ausreißer. Beide MQTT-Tasks (`mqtt_chkupd` für Check-Update und `mqtt_ota` für OTA-Installation) wurden über die Konstante `MQTT_UPDATE_TASK_STACK_BYTES = 12288` auf 12 KB angehoben.
- **Share-Log ("Log teilen") schlug mit `ESP_ERR_HTTP_CONNECT` bzw. Timeout fehl:** Der Upload-Timer wurde von 20 s auf 45 s erhöht, damit auch große Logs vollständig übertragen werden. Zudem wird die vom MicroBin-Service in `Location` zurückgegebene URL nun unverändert übernommen, statt sie clientseitig von `/upload/<id>` nach `/p/<id>` umzuschreiben — das Service besitzt das URL-Format und kann Aliase unabhängig ändern.

## [2.2.4-Beta.12] - 2026-07-13

### Changes
- fix(updatecheck): use epoch seconds + random suffix as manifest cache-buster

### Fixed
- **Update-Check lieferte veraltete Daten nach einem neuen Release:** Der Cache-Buster in der Manifest-URL basierte auf `esp_timer_get_time()` (Sekunden seit Boot). Direkt nach jedem Reboot war dieser Wert wieder klein, und mehrere Reboots kurz hintereinander erzeugten überlappende Werte. Da `raw.githubusercontent.com` über Fastly 5 Minuten lang cached (`Cache-Control: max-age=300`), bekamen Geräte nach einem neuen Release stundenlang die alte `beta.json` aus dem Cache und boten das Update nicht an. Die URL nutzt jetzt echte Epoch-Sekunden (sofern die Uhrzeit via NTP/GPS/DCF synchronisiert ist) plus einen Zufalls-Suffix, sodass jeder Fetch eine einzigartige URL erzeugt und jede Caching-Schicht zwischen Gerät und GitHub umgangen wird.

## [2.2.4-Beta.11] - 2026-07-13

### Changes
- fix(webui): repair normalizeSupporterKey so the settings save bar renders

### Fixed
- **Speichern-/Verwerfen-Buttons fehlten auf der Einstellungs-Seite:** Die Funktion `normalizeSupporterKey` endete mit `.split('').filter().slice()`, was ein Array zurückgibt, und rief dann `.match()` darauf auf — eine String-Methode, die auf Arrays nicht existiert. Der resultierende `TypeError: ...match is not a function` wurde während der Settings-Initialisierung geworfen und hat das Setzen von `loadedSnapshot` abgebrochen. Damit blieb die Bedingung `loadedSnapshot || showError` permanent false und die schwebende Speichern-/Verwerfen-Leiste wurde gar nicht erst ins DOM gerendert — auf keinem Tab, insbesondere nicht unter Netzwerk. Der Aufruf wird jetzt zu einem String zusammengeführt, bevor `.match()` angewendet wird; die Leiste erscheint wieder, sobald Änderungen vorliegen.

## [2.2.4-Beta.10] - 2026-07-13

### Changes
- chore: upgrade ESP-IDF from v6.0.2 to v6.1-beta1 in CI workflows

### Changed
- **ESP-IDF auf v6.1-beta1 aktualisiert:** Alle Build-Pipelines (build, release, pr-check, security, newdesign) verwenden jetzt ESP-IDF v6.1-beta1 statt v6.0.2. Diese Version bringt unter anderem Bugfixes für lwIP, MbedTLS 4.1 und den GPIO/SPI-Flash-Treiber sowie eine neu strukturierte MQTT-Komponente. Da wir MQTT bereits über den ESP Component Manager (`espressif/mqtt: ^1.0.0` in `main/idf_component.yml`) beziehen, ist die wichtigste in 6.1 enthaltene Breaking Change (MQTT wurde aus dem IDF-Kern in den Component Manager verschoben) für uns automatisch abgedeckt. Alle anderen für den ESP32 relevanten Breaking Changes (GPIO ROM-Prefix, SPI-Flash private Header, UART-Header-Pfad) wurden geprüft und im Projekt nicht verwendet.
- **Dokumentation aktualisiert:** CLAUDE.md, README.md, sdkconfig.defaults und sdkconfig.hb-rf-eth-ng referenzieren jetzt v6.1-beta1.
- **Toolchain:** GCC 15.2 (xtensa-esp-elf) löst GCC 14.2 ab.

### Hinweise
- Diese Beta ist ein **Kompatibilitätstest** für ESP-IDF 6.1. Die Stabilitäts-Fixes aus Beta.9 (TLS-Leck, Syslog-Persistenz-Session, CheckMK-Socket-Leck, Crash-Log-NVS-Persistenz, Socket-Limit-Fix) sind alle enthalten.
- Falls beim Build mit 6.1-beta1 neue Inkompatibilitäten auftreten, wird der Build-Workflow fehlschlagen — in diesem Fall bleibt Beta.9 die letzte funktionierende Version.

## [2.2.4-Beta.9] - 2026-07-13

### Changes
- fix(system): close mbedtls TLS leak in SMTP STARTTLS path
- fix(syslog): keep persistent TLS session instead of handshake per log line
- fix(monitoring): close CheckMK client socket before force-deleting the task
- feat(log): persist crash log tail to NVS and surface via WebUI modal
- feat(reset): auto-classify panic/task-WDT/brownout resets and store diagnostics
- fix(sdkconfig): restore LWIP_MAX_SOCKETS=16 and TCP_MSL=15000 after silent regression
- chore(sdkconfig): remove deprecated ESP-IDF 5.5 symbols from sdkconfig.hb-rf-eth-ng

### Fixed
- **Speicherverlust bei E-Mail-Benachrichtigung mit STARTTLS:** Bei jedem fehlerhaften STARTTLS-Handshake (z.B. bei Erreichbarkeitsproblemen des SMTP-Servers) sind bisher die mbedtls-TLS-Kontexte (~6–8 KB pro Versuch) nicht freigegeben worden. Über Tage/Wochen häufte sich das an, der freie Heap sank unter die Watchdog-Schwelle und das Gerät startete neu — ohne dass die Ursache im Log sichtbar wurde, weil der Ringpuffer im RAM liegt. Die Aufräum-Logik ist jetzt an einem eigenen Flag `tls_setup` orientiert, das bereits vor dem Handshake gesetzt wird, sodass jeder Fehlversuch sauber aufgeräumt wird.
- **Heap-Fragmentierung durch Syslog-over-TLS:** Bisher wurde für **jede einzelne Log-Zeile** ein vollständiger TLS-Handshake durchgeführt (6–8 KB Allokation + Freigabe pro Zeile). Auf dem WROOM-32 ohne PSRAM führt das zu starker Fragmentierung des Heaps — der größte zusammenhängende freie Block schrumpft stetig, obwohl der Gesamt-Heap noch gut aussieht. Irgendwann schlägt dann der nächste TLS-Handshake (OTA, Update-Check) fehl und das Gerät startet. Syslog-over-TLS nutzt jetzt eine **persistente TLS-Session** wie TCP auch — Reconnect nur bei Fehler oder nach 5 Minuten Inaktivität.
- **Socket-Leck im CheckMK-Agent:** Wenn der CheckMK-Task per `vTaskDelete()` abgebrochen wurde, während gerade ein Client verbunden war (z.B. bei Speichern der Monitoring-Konfiguration), ist der Client-Socket in die lwIP-FD-Tabelle eingegangen. Bei nur 10–16 verfügbaren Sockets firmwareweit führte das nach mehreren Config-Speicherzyklen dazu, dass `socket()` überall im System fehlschlug (WebUI, MQTT, Syslog, OTA) — das Gerät wurde unbrauchbar. Der Client-Socket wird jetzt atomar getrackt und vor `vTaskDelete()` explizit geschlossen.
- **Socket-Limit und TIME_WAIT nachgestellt:** Die aktive Build-Konfiguration war unbemerkt auf die ESP-IDF-Defaults zurückgefallen (`LWIP_MAX_SOCKETS=10` statt 16, `LWIP_TCP_MSL=60000` statt 15000). Mit nur 10 Sockets firmwareweit und 60 Sekunden TIME_WAIT pro geschlossenem Socket war das System bei jeder HTTPS-Aktivität (Update-Check alle 24 h, OTA, CRL-Refresh) am Limit — WebUI und MQTT kamen sich in die Quere. Beide Werte sind wieder korrekt auf 16 bzw. 15 s gesetzt und in allen drei Config-Dateien (`sdkconfig.defaults`, `sdkconfig.hb-rf-eth-ng`, `sdkconfig`) synchronisiert.
- **Veraltete ESP-IDF-5.5-Konfigurationssymbole:** Die Datei `sdkconfig.hb-rf-eth-ng` stammte noch aus ESP-IDF 5.5.3 und enthielt Symbole, die in 6.0 nicht mehr existieren (`CONFIG_ADC_CAL_EFUSE_TP_ENABLE`, `CONFIG_ADC_CAL_LUT_ENABLE`, `CONFIG_ADC2_DISABLE_DAC`, `CONFIG_OPTIMIZATION_LEVEL_RELEASE`, `CONFIG_LOG_BOOTLOADER_LEVEL_WARN`). Bei einem `idf.py fullclean` oder einem Neu-Klon konnten diese veralteten Defaults zu unerwartetem Runtime-Verhalten führen. Alle deprecated Symbole sind entfernt bzw. auf ihre 6.0-Äquivalente umbenannt.

### Added
- **Absturzprotokoll überlebt jetzt den Neustart:** Der häufigste Schmerzpunkt bei der Diagnose war bisher, dass der Log-Ringpuffer im RAM liegt und beim Restart verloren geht. Der Heap-Watchdog speichert jetzt unmittelbar vor dem Restart einen **Tail der letzten ~1024 Bytes** (die letzten Log-Zeilen) ins NVS. Nach dem Reboot öffnet sich auf der System-Log-Seite automatisch ein Modal mit diesen Zeilen — einmalig, danach wird der Snapshot gelöscht. So siehst du endlich, was unmittelbar vor dem Absturz passiert ist.
- **Reset-Grund-Diagnose:** Bisher wurde nach einem Task-Watchdog-Timeout oder einem echten Panic (Exception) oft "Normaler Start" angezeigt, weil die Software den Reset-Grund nicht selbst gesetzt hatte. Jetzt wertet `ResetInfo` automatisch den Hardware-Reset-Grund (`esp_reset_reason()`) aus und klassifiziert: Panic → "Systemfehler (Exception/Panic)", Task/Interrupt-WDT → "Watchdog Reset", Brownout → "Spannungsabfall". Zusätzlich wird ein Diagnose-String im NVS abgelegt (z.B. `low heap: free=18432 largest=16384 min_ever=16384 uptime=84213s`), der im Reset-Details-Feld angezeigt wird.
- **Neue API `/api/crash_log`:** Liefert den persistierten Log-Tail als JSON und löscht ihn nach dem ersten Lesen. Wird von der WebUI automatisch beim Öffnen der System-Log-Seite abgefragt.

## [2.2.4-Beta.8] - 2026-07-12

### Changes
- fix(webui): keep settings actions visible
- chore: update manifests for v2.2.4-Beta.7

## [2.2.4-Beta.7] - 2026-07-12

### Changes
- fix(webui): preserve settings edits during initialization
- chore: update manifests for v2.2.4-Beta.6

## [2.2.4-Beta.6] - 2026-07-12

### Changes
- fix: restore settings saves and localize update dialog
- chore: update manifests for v2.2.4-Beta.5

## [2.2.4-Beta.5] - 2026-07-12

### Changes
- fix(webui): prevent aggressive browser caching on sysinfo and settings json endpoints
- fix(system): release ethernet and radio reset pins before esp_restart to prevent boot hangs

### Fixed
- **Einstellungen: Änderungen erst nach Neuladen sichtbar:** Ein Bug durch aggressives Browser-Caching wurde behoben. Die WebUI ergänzt nun dynamische Parameter (`?t=...`) bei Abfragen an `/settings.json` und `/sysinfo.json`, und der Webserver sendet striktere Cache-Control Header (`Pragma: no-cache`, `Expires: 0`). Gespeicherte Änderungen sind nun sofort in der WebUI sichtbar.
- **Neustart: Boot-Hänger nach OTA oder Neustart:** Bei aktiviertem Link-Down-Pause (Neustart-Sync) oder regulären Reboots konnte der Ethernet PHY im Reset-Zustand stecken bleiben, da die Reset-Pins während des `esp_restart()` auf LOW gehalten wurden. Die Pins werden nun unmittelbar vor dem Neustart wieder freigegeben (HIGH). Ein Kaltstart (stromlos machen) nach Firmware-Updates ist nicht mehr nötig.
## [2.2.4-Beta.4] - 2026-07-12

### Changes
- fix(webui): keep archive offline and unblock settings saves
- chore: update manifests for v2.2.4-Beta.3

### Fixed
- **Einstellungen: Speichern scheinbar ohne Funktion:** Die WebUI-Validierung entspricht jetzt dem Firmware-Vertrag (Hostnamen bis 63 Zeichen, optionale IPv6-Gateway/DNS-Felder und IPv4-gemappte IPv6-Adressen). Bereits gültig gespeicherte Werte blockieren dadurch keinen späteren Sammel-Save mehr. Bei echten Eingabefehlern öffnet die Oberfläche den betroffenen Tab und zeigt eine verständliche Meldung.
- **Firmware-Archiv vollständig offline:** Die Archivliste wird ausschließlich aus dem in die Firmware eingebetteten `archive.json` über `/api/firmware_archive` geladen. Online-Fallback, manuelles Aktualisieren, Retry und Browser-Cache wurden entfernt; nur Update-Prüfung und OTA verwenden weiterhin Onlinequellen.

## [2.2.4-Beta.3] - 2026-07-12

### Fixed
- **Memory fragmentation & Uptime Instability (Issue #362):** Replaced highly frequent dynamic `cJSON` memory allocations in the WebUI's 1-second background polling routes (`/api/sysinfo`, `/api/ota_status`) with zero-allocation `snprintf` and chunked HTTP streams. This prevents the severe heap fragmentation ("Swiss Cheese") that occurred over 1-2 days of uptime, and ultimately caused "Serverfehler" (HTTP 500) or Out-Of-Memory crashes when a 12 KB OTA task or TLS handshake could no longer find contiguous free memory.

## [2.2.4-Beta.2] - 2026-07-11

### Changes
- fix(webui): harden settings/restore payload handling
- docs(screenshots): add NewDesign full-page screenshots for wiki
- chore: update manifests for v2.2.4-Beta.1

## [2.2.4-Beta.1] - 2026-07-09

### Fixed
- **CPU-Last ~50 % bei aktivem System-Log-Capture:** der projekteigene `log_vprintf`-Hook formatierte jede Log-Zeile zweimal (ein `vsnprintf(NULL,0,…)` zur Längenmessung + ein `vsnprintf(buf,…)` zum Formatieren) sobald der Ring-Buffer aktiv war — selbst wenn niemand die Logs las. Bei chattigen Subsystemen (Raw-UART-Bridge, Netzwerk-Events) summierte sich das zu ~50 % CPU-Last. Der Capture-Pfad formatiert jetzt einmal in einen festen 256-B-Stack-Buffer und reicht das Ergebnis an Ring-Buffer/Subscriber weiter; die teure Längenmessung und der `malloc`-Fallback für überlange Zeilen entfallen. UART erhält weiterhin die ungekürzte Originalzeile.

### Changed
- **ESP-IDF 6.0.1 → 6.0.2:** Bugfix-Release. CI, `dependencies.lock`, `sdkconfig.defaults` und Doku auf 6.0.2 aktualisiert. Alle Component-Constraints (`>=6.0` oder niedriger) sind semver-kompatibel, keine Blocker. `sdkconfig` (`CONFIG_IDF_INIT_VERSION`, Header-Kommentar) und der `manifest_hash` in `dependencies.lock` werden beim ersten CI-Build mit 6.0.2 automatisch regeneriert.

## [2.2.3] - 2026-07-09

### Breaking Changes
- **Login erfordert jetzt Benutzername und Passwort:** Anmeldung mit dem Standard-Benutzernamen `admin` und dem bestehenden Administrator-Passwort. Der Benutzername ist unter *Einstellungen > Allgemein > Sicherheit* frei wählbar (z. B. für Passwortmanager oder Mehrgeräte-Setups). Alte Browser-Sessions verfallen aus Sicherheitsgründen einmalig. Backups enthalten den Benutzernamen (das Passwort wird weiterhin nicht exportiert).

### Added
- **Passwort vergessen / Reset-Funktion:** über „Passwort vergessen?" auf der Login-Seite kann ein Einmal-Token generiert und per Status-Abfrage validiert werden (`/api/password-reset/start|status|complete`), mit dem ein vergessenes Administrator-Passwort direkt zurückgesetzt werden kann — kein Flashen mehr nötig.
- **Neustart-Funktion mit Countdown-Overlay:** Neustart direkt aus der WebUI (Firmware-Seite + Mobile-Menü) mit Vollbild-Countdown, optionalem Flash-Pause-Sync und automatischem Reload. Bestätigungsdialog verhindert versehentliche Neustarts.
- **Light/Dark-Theme-Toggle:** manuelle Umschaltung zwischen hellem und dunklem Theme in der Top-Bar (Sonne/Mond-Icon), persistent gespeichert.
- **WebUI NewDesign (experimentell):** umschaltbare emerald-grüne Industriepalette (Light + Dark) neben dem klassischen Glass-UI. Aktive Navigationspunkte als grüne Balken mit weißer Schrift/Icon, flache 4px-Kartenradien, korrekte Dark-Mode-Sidebar (`#24272B`) und Top-Bar (`#25282C`). Token-basiert (keine hartcodierten Farben in Komponenten).
- **Neues Marken-Logo:** dreiblättriges Symbol mit festem Gradient (`#D96A5A → #EAA08E`) als Inline-SVG, verwendet in Sidebar, Top-Bar, Mobile-Panel und Login-Seite. Neues Favicon + PWA-Icon, Cache-Busting für das Favicon nach Firmware-Update. `docs/WEBUI_DESIGN_SYSTEM.md` als verbindliche Design-Spezifikation.
- **Monitoring & Benachrichtigungen (Phasen A–E):**
  - Prometheus `/metrics`-Exporter (Port 9100, Quell-IP-Allowlist, lock-freie Zähler).
  - Syslog-Forwarding nach RFC 5424 (UDP/TCP/TLS, severity-gefiltert).
  - WebSocket Live-Log-Stream (`GET /api/log/stream`, bis zu 4 parallele Tabs, ersetzt 5s-Polling).
  - Event-Benachrichtigungen (Webhook / Telegram Bot / SMTP) für `eth_link_up/down`, `rf_module_detected`, `ota_started/succeeded/failed`, `mqtt_disconnected/reconnected`; pro-Event-Cooldown; Liefer-Metriken.
  - Fünf dynamische Status-Chips im Monitoring-Hero (MQTT, CheckMK, Prometheus, Syslog, Benachrichtigungen) mit Aktiv-Highlight.
- **Supporter-Key mit Sperrsystem (CRL):** Supporter-Keys mit revocation list (SHA-256-Fingerabdrücke via PSA Crypto), Dashboard-Badge, On-Demand-Changelog. CRL-Refresh-Task läuft nur, wenn ein Key konfiguriert ist.
- **Firmware-Archiv aus Flash:** `archive.json` ist in die Firmware eingebettet und wird über `/api/firmware_archive` sofort aus dem Flash serviert (offline, kein TLS/GitHub-Roundtrip). Die Liste lädt beim Öffnen der Firmware-Seite sofort, GitHub ist nur noch Fallback.
- **PWA:** WebUI als Progressive Web App installierbar (Manifest, theme-color, apple-touch-icon).
- **Test-Design-Toggle persistent:** Geräteweit (survives Reboots, browserübergreifend) über silent POST in die NVS.
- **Dashboard-Verbesserungen:** Hostname in Titel/Tab, breitere Info-Karten (`minmax(360px)`), Status-Chips ohne Zeilenumbruch, Mobile-Action-Buttons im dunklen Charcoal-Stil.

### Fixed
- **OTA braucht zwei Klicks:** nach einem fehlgeschlagenen OTA werden die zuvor gestoppten Hintergrund-Tasks (CRL-Refresh + UpdateCheck) auf dem Fehlerpfad wieder gestartet, sodass ein Retry nicht mehr durch den zusätzlich freigewordenen Heap bevorteilt wird. Betrifft WebUI- und MQTT-OTA-Pfade.
- **OTA-Redirect/Heap-OOM:** interner 3-fach-Retry für `esp_https_ota_begin` (GitHub-302 → zweiter TLS-Handshake → transienter Heap-OOM auf dem WROOM-32), Rollback-Schutz bei Socket-Erschöpfung.
- **CRL-Task-Stack-Overflow** bei Uptime 60s behoben (Stack 4 KB → 8 KB).
- **Boot-Race:** UpdateCheck-Hintergrundtask startet jetzt erst nach `monitoring_init()` (teilt sich den HTTPS-Fetch-Mutex), schließt die Lücke wo der erste Manifest-Fetch einen CRL/MQTT-Fetch überlappte und den TLS-Heap erschöpfte.
- **Dark-Mode NewDesign:** Sidebar/Top-Bar-Token in den `.newdesign-shell`-Scope gespiegelt (Body-Scope wurde von `:root`-Light überdeckt).
- **WebUI Layout:** Ethernet-Status-Chip („100 Mbit/s · Full") bricht nicht mehr um; lange Werte (IPs, Seriennummern, MACs) werden nicht mehr abgeschnitten.

### Security
- Auth-Token in sessionStorage (statt localStorage).
- Timing-sicherer `secure_strcmp` für Credentials.
- cJSON-Typ-Validierung, URI-Handler-Limits, 32-Zeichen-Credentials, Überlängen-Prüfung für OTA-URLs.
- Ethernet DNS-Cache mit Mutex + Initialisierung, MQTT-Lebenszyklus serialisiert.

### Changed
- WiFi dauerhaft deaktiviert (~30 KB RAM zurückgewonnen), da das Gerät über Ethernet läuft.
- `update_headers.py` scannt jetzt `main/` (statt `src/`).
- i18n: Übersetzungen in 10 Sprachen, dynamische Locale-Erkennung, fehlende Keys ergänzt.
- Build/CI: Vite 8.1.3, Dependabot-Bumps (actions/setup-node 6, actions/labeler 6, github/codeql-action 4).
- Mobile-Action-Buttons (Restart/Logout/Login) einheitlich dunkel mit weißer Schrift im NewDesign.

## [2.2.2] - 2026-07-04

### Changes
- fix: use explicit raw manifest ref
- fix: keep beta manifest on latest stable release
- chore: update latest.json for v2.2.1

## [2.2.1] - 2026-07-04

### Changes
- chore: enforce repository line endings
- fix: use static update manifests
- Revise LICENSE.md with updated copyright and license info

## [2.2.0] - 2026-07-01

### Changes
- fix: stabilize OTA and clean firmware diagnostics

## [2.2.0-Beta.24] - 2026-07-01

### Changes
- fix(webui): shorten idle logout and restore mobile logout

## [2.2.0-Beta.23] - 2026-07-01

### Changes
- fix(network): resolve F-21 guarantee HTTPS serialization
- fix(hardware): resolve F-20 average valid ADC samples
- fix(ota): resolve F-19 synchronize update status
- fix(memory): resolve F-18 handle allocation pressure
- fix(parser): resolve F-17 discard oversized input
- fix(runtime): resolve F-16 eliminate status data races
- fix(auth): resolve F-15 reject password truncation
- fix(settings): resolve F-14 sanitize persisted values
- fix(checkmk): resolve F-13 harden agent I/O
- fix(runtime): resolve F-12 harden service startup
- fix(ota): resolve F-11 preserve rollback window
- fix(mqtt): resolve F-10 handle lifecycle failures
- fix(web): resolve F-09 authenticate log sharing
- fix(web): resolve F-08 serialize HTTPS proxy
- fix(network): resolve F-07 preserve UDP datagrams
- fix(ota): resolve F-01..F-06 update stability
- fix: Firmware-Update-Suche crasht nicht mehr durch Heap-Erschoepfung
- feat: System-Log standardmaessig deaktiviert, per WebUI aktivierbar

## [2.2.0-Beta.22] - 2026-06-30

### Changes
- No new commits since last release

## [2.2.0-Beta.21] - 2026-06-30

### Changes
- No new commits since last release

## [2.2.0-Beta.20] - 2026-06-30

### Changes
- No new commits since last release

## [2.2.0-Beta.19] - 2026-06-30

### Changes
- No new commits since last release

## [2.2.0-Beta.18] - 2026-06-30

### Changes
- hb-rf-eth-update-failure
- fix: prevent truncated GitHub releases JSON breaking the update check
- fix: don't auto-check for firmware updates on every page open

## [2.2.0-Beta.17] - 2026-06-30

### Changes
- hb-rf-eth-update-failure
- fix: make WebUI clipboard copy work over HTTP and abort fetch on OOM
- fix: resolve TLS handshake OOM (-0x008D) breaking update check and OTA
- chore: bump version to 2.2.0-Beta.16
- update-check-and-log-share
- Fix update check, log share, and translation text

## [2.2.0-Beta.16] - 2026-06-30

### Changes
- update-check-and-log-share
- Fix update check, log share, and translation text

## [2.2.0-Beta.15] - 2026-06-30

### Changes
- ota-update-search-verify
- fix: drop redundant skip_cert_common_name_check on OTA path
- fix: disable TLS cert verification on OTA/update path (PSA OOM -141)
- ota-update-search-verify
- docs: add OTA & update-search hardware test protocol

## [2.2.0-Beta.14] - 2026-06-30

### Changes
- fix-clipboard-copy-mobile
- fix-update-check-timeout-mismatch
- fix: use textarea.value.length and preventScroll per review feedback
- fix: make clipboard copy fallback reliable on mobile browsers
- fix: raise check_update client timeout to cover device-side worst case
- fix-orphaned-wiki-repo-gitlink
- fix: remove orphaned wiki_repo gitlink

## [2.2.0-Beta.13] - 2026-06-30

### Changes
- feat: add low-heap watchdog as a last-resort restart safety net
- fix: free DCF flank-event queue handle on stop instead of just resetting it
- docs: bring CLAUDE.md in line with the actual repo (ESP-IDF, not PlatformIO)
- fix: 24h update-check interval overflowed to ~500s, hammering GitHub API
- fix: abort log-share with a clear error if the net-fetch mutex times out
- fix: serialize log-share TLS handshake and grow GitHub response buffer
- chore: bump version to 2.2.0-Beta.12
- copy-button-bug
- fix: copy-to-clipboard button not working on HTTP WebUI
- fix: iterate all fetched releases and pick highest version
- chore: bump version to 2.2.0-Beta.11
- fix: clipboard copy in non-HTTPS context (HTTP WebUI)
- fix: semver pre-release comparison now handles numeric segments correctly
- release: v2.2.0-Beta.10
- paste-sharing-upload
- perf: avoid temporary std::string allocations in appendField
- fix: use MicroBin multipart upload API for log sharing
- graphics-issue
- feat: replace favicon with satellite-dish app icon
- fix: restore TLS cert verification using full CA bundle on OTA path
- fix: remove crt_bundle_attach to work around ESP-IDF PSA Crypto TLS handshake failure
- release: v2.2.0-Beta.9 — OTA success modal, paste service fix, duplicate button fix
- fix: remove duplicate download button, fix paste service redirect handling (event handler + disable_auto_redirect)
- feat: redirect to home page after OTA update with success modal (auto-close 10s)
- fix: compile errors in share log task (format strings, enum, ip4_addr_t, header API)
- graphics-issue
- fix: prevent firmware update banner from overlapping header nav
- ci: auto-trigger release build on tag push
- docs: complete MQTT API reference in wiki (all status/event/command topics, HA entities, TLS/mTLS)
- feat: system log sharing, globe favicon, clipboard HTTP fallback, update buffer fix (Beta.8)
- webui-improvements-optimization
- fix(webui): complete i18n translations for 8 locales
- fix(build): snprintf instead of strncpy in getVersionSnapshot (-Wstringop-truncation)
- fix(build): move variable declarations above goto cleanup (C++17 jump-misses-init)
- docs: fold performance optimizations into Beta.7 changelog
- perf: lightweight getVersionSnapshot, GH_RESPONSE_CAP 12KB, net-fetch serialization, stack watermarks
- chore: bump version to 2.2.0-Beta.7
- fix: token NVS persistence (no 401 after reboot), task stack overflows, idle logout, log noise
- fix(webui): hide native file input to prevent double file dialog on manual firmware upload
- chore: bump version to 2.2.0-Beta.6
- fix(webui): show real error instead of 'up to date' when update check fails
- chore: bump version to 2.2.0-Beta.5
- fix: beta-channel JSON parse, updatecheck boot race, changelog proxy OOM retry
- fix(build): silence -Wstringop-truncation in OTA error snapshot (snprintf)
- fix(build): replace removed ESP_ERR_HTTPS_OTA_INCOMPLETE with private OTA error code
- fix(ci): make update_version.py idempotent; fix mangled TROUBLESHOOTING version string
- fix(build): resolve Beta.4 compile errors (NO_DATA macro clash, snprintf truncation)
- chore: bump version to 2.2.0-Beta.4
- fix(ci): make update_changelog.py idempotent to avoid duplicate sections
- chore: bump version to 2.2.0-Beta.4
- docs: rewrite CHANGELOG with full curated history (2.1.0 -> 2.2.0-Beta.4)
- Merge pull request #333 from Xerolux/dependabot/github_actions/actions/checkout-7
- Merge pull request #325 from Xerolux/dependabot/github_actions/crate-ci/typos-1.47.2
- mqtt-phase-a-b
- feat(mqtt): Phase A + B - LWT/Birth, command token, OTA state, extended status topics
- refactor(webui): unify icons and design-token usage across UI
- feat(update): switch to GitHub Releases API with optional beta channel
- chore: upgrade toolchain and dependencies to latest
- fix(webui): fix dirty-tracking, auth redirect, OTA feedback and version compare
- fix(firmware): harden memory safety and bounds checks
- chore(ci)(deps): bump actions/checkout from 6 to 7
- fix-firmware-hardening
- fix: harden monitoring and log handling
- chore: bump version to 2.2.0-Beta.3
- performance-stability-upgrade
- feat: async monitoring diagnostic, i18n for hardcoded UI strings
- perf(webui): move update-check/changelog proxies off the httpd task
- fix(webui): OTA upload timeout, 401 redirect for visitors, polling robustness
- fix: boot hang hardening, httpd/MQTT/NTP stability fixes
- fix: RTC detection broken on IDF 6.0 and perf-oriented build config
- chore(ci)(deps): bump crate-ci/typos from 1.45.1 to 1.47.2
- chore: bump version to 2.2.0-Beta.2
- chore: bump version to 2.2.0-Beta.2 - IDF 6.0.1 compat & deps upgrade
- Merge pull request #316 from EarlSneedSinclair/mqtt-tls
- feat(mqtt): add TLS/SSL support for MQTT configuration
- Merge pull request #310 from Xerolux/dependabot/npm_and_yarn/webui/vue-i18n-11.4.0
- chore(deps)(deps): bump vue-i18n from 11.3.2 to 11.4.0 in /webui
- fix: correct update banner showing when running version is newer than available
- fix: semver pre-release comparison and log polling stability
- feat: live LED brightness update without restart
- chore: bump version to 2.2.0-Beta.1
- chore: prepare v2.2.0-Beta.1 release

## [2.2.0-Beta.10] - 2026-06-29

### Fixed
- **Log sharing via multipart upload**: replaced URL-encoded form POST with proper multipart/form-data upload to MicroBin. Fixes the "Paste service returned HTTP 200" error — the paste service now correctly returns a 303 redirect with the share URL.
- **TLS certificate verification**: switched ESP-IDF CA bundle from CMN subset to full Mozilla root CA, covering the ECDSA root (ISRG Root X2) required by Let's Encrypt / GitHub CDN. Fixes persistent `PSA_ERROR_GENERIC_ERROR (0xffffff73)` / TLS handshake failures during update checks and OTA downloads.

## [2.2.0-Beta.9] - 2026-06-29

### Added
- **OTA success modal**: after a firmware update, the WebUI automatically redirects to the home page and shows a "Update to version X successful" dialog. Auto-closes after 10 seconds or on user confirmation.

### Fixed
- **TLS handshake failure with GitHub**: the ESP-IDF 6.x PSA Crypto CA bundle causes `PSA_ERROR_GENERIC_ERROR (0xffffff73)` when verifying newer Let's Encrypt / ISRG Root X2 certificates. Removed `crt_bundle_attach` from `configure_ota_http_client` — TLS encryption is retained but the server certificate chain is not verified. Fixes persistent `mbedtls_ssl_handshake -0x3000` / `-0x008D` errors when checking for updates and downloading firmware.
- **Paste service sharing**: MicroBin returns a 303 redirect, but ESP-IDF auto-followed it (returning 200). Now uses `disable_auto_redirect` + event handler to properly capture the `Location` response header.
- **Duplicate download button**: the system log page showed two download icons (one was the auto-scroll toggle using the wrong icon). Auto-scroll now uses a chevron icon.

## [2.2.0-Beta.8] - 2026-06-29

### Added
- **System Log sharing**: new "Share" button in the system log uploads a comprehensive debug report (system info, network config, radio module status, monitoring config, LED programs, and full log) to a MicroBin paste service. Passwords, MQTT command tokens, and TLS keys are automatically redacted as `****` or `<set>`.
- **Globe favicon**: replaced the generic favicon with a custom globe icon visible in browser tabs.

### Fixed
- **Log copy on HTTP**: the "Copy" button in the system log now falls back to `document.execCommand('copy')` when the Clipboard API is unavailable in non-HTTPS contexts (local ESP32 access).
- **Update check buffer**: increased `GH_RESPONSE_CAP` from 12 KB to 24 KB to avoid JSON truncation when a GitHub release has a large changelog body. The buffer was previously reduced to 12 KB in Beta.7 but proved insufficient for some releases.

### Changed
- Raised `max_uri_handlers` from 25 to 30 to accommodate the new share endpoint.

## [2.2.0-Beta.7] - 2026-06-28

### Added
- **Token persists across reboots**: the authentication token is now saved to NVS. "Remember me" / persistent login survives firmware updates and device restarts — no more spurious 401 / redirect-to-login after a reboot. Token is regenerated on password change and cleared on factory reset.

### Fixed
- **30-second reboot loop**: the UpdateCheck task copied the 5 KB `ReleaseInfo` struct (with its 4 KB `body` field) onto a 8 KB stack — tight enough that TLS stack frames could overflow it, triggering a watchdog reset exactly 30 s after boot (the wait-before-first-check delay). UpdateCheck task stack raised to 12 KB; MQTT publish task (also copies the struct) raised from 6→10 KB.
- **Immediate logout on page load**: the idle-timer `lastActivity` was loaded from `localStorage`, so a stale timestamp from hours before caused an instant logout on the first 30 s cycle. `lastActivity` now always starts fresh on page load.
- **Log noise**: `httpd_parse: parse_block: incomplete` and `esp-x509-crt-bundle: Certificate validated` (3–4× per boot) are suppressed at the log level.

### Optimized
- **Lightweight version snapshot**: `UpdateCheck::getVersionSnapshot()` returns a 160-byte struct instead of the full 5 KB `ReleaseInfo` — the MQTT publish task (every 5 s) and the background checker no longer copy the 4 KB body on their stack. The WebUI still uses the full `getReleaseInfo()` for the release-notes preview.
- **Smaller heap footprint**: GitHub response buffer reduced from 24→12 KB (a single-release fetch fits in ~8 KB after the `?per_page=1` change).
- **Serialized external HTTPS requests**: a shared mutex (`g_net_fetch_mutex`) ensures the UpdateCheck fetch and the changelog proxy never open two TLS handshakes at once on the ESP32's limited heap.
- **Stack high-water-mark logging**: the UpdateCheck task and MQTT publish task log their remaining stack at each (first) cycle for early detection of future tightness.

## [2.2.0-Beta.6] - 2026-06-28

### Fixed
- **WebUI falsely showed "Firmware is up to date" when the update check failed**: when the GitHub fetch errored (e.g. the beta-channel parse bug on Beta.4), the firmware reported `updateAvailable: false` and the firmware-update page fell back to "aktuell"/"up to date" instead of the actual error. The page now surfaces the real error ("Update-Prüfung fehlgeschlagen: …") with a retry button. New `firmware.checkFailed` translations added for all 10 locales.

## [2.2.0-Beta.5] - 2026-06-28

### Fixed
- **Beta update channel was broken**: the `/releases` payload (15+ entries with their full bodies) exceeded the 24 KB response cap and was truncated, so `cJSON_Parse` failed and beta-channel users never saw an update. The beta endpoint now fetches only the newest release (`?per_page=1`).
- **Bogus "Failed to determine latest version" error at boot**: `UpdateCheck::_taskFunc` treated a coalesced (already-in-progress) `refresh()` as a hard failure and logged an empty error string. It now uses the cached snapshot's validity as the authority and only logs an error when a real error is present.
- **Changelog proxy ran out of memory at boot**: opening the WebUI while the background update-check fetch was running opened two TLS connections at once and exhausted the heap (`HTTP_CLIENT: Allocation failed`). The proxy now retries `esp_http_client_init` once after a short delay so the changelog load self-heals.

## [2.2.0-Beta.4] - 2026-06-28

### Added
- **MQTT Last Will & Testament + Birth message**: the device publishes `status/online`, and the broker flips it to `offline` on unclean disconnects, so Home Assistant reliably detects the device going offline.
- **MQTT command-topic security**: optional shared-secret command token (`mqtt.command_token`, 8–63 chars, charset `A–Z a–z 0–9 - _ .`). When set, every command payload (`restart` / `update` / `check_update`) must match the token exactly. The token is write-only (never returned by `GET`) and published verbatim as HA `payload_press` / `payload_install` so HA buttons keep working. Broker ACL requirement documented in TROUBLESHOOTING.md and WIKI.md.
- **Command topics without HA discovery**: new `mqtt.command_enabled` flag (default on) lets plain-MQTT users trigger commands even without HA discovery.
- **OTA state machine with live progress**: OTA now reports `idle / checking / starting / downloading / flashing / success / failed` with real 0–100 % download progress. New MQTT topics `status/ota_state`, `status/ota_progress`, `status/ota_error` plus transient `event/*` events (`update_started`, `update_downloading`, `update_finished`, `command_rejected`, …).
- **Richer status topics**: Ethernet (link / speed / duplex / IP / gateway), radio module (type / serial / firmware), reset reason, detailed heap breakdown and NTP sync state.
- **`check_update` command** to refresh release info without flashing.
- **Optional Beta update channel** (`betaChannel`, default off): stable users stay on stable releases, beta testers opt into pre-releases. The update check now uses the public GitHub Releases API as the single source of truth, replacing the xerolux.de-hosted `version.txt` + firmware mirror.
- Release-notes preview, beta badge and collapsible changelog on the firmware-update page.
- New AppIcon SVG set (lock, radio, satellite, arrowRight, gitFork, link, package, list, externalLink).
- New i18n keys (beta, betaChannel, betaChannelHint, releaseNotesPreview, viewOnGithub, noDownloadUrl, …) across all 11 locales.

### Changed
- **OTA**: replaced the blocking `esp_https_ota()` with the advanced `esp_https_ota_begin/perform/finish/abort` API; the publish task now reacts to OTA state changes within ~1 s instead of the 60 s cycle and emits progress events every ~5 %. Publish-task stack raised 4 KB → 6 KB.
- **Update check is non-blocking and cached**: the 24 h background refresh and a manual "Check now" coalesce via a fetch lock to respect GitHub's unauthenticated rate limit (60 req/h/IP). Manual refresh runs via `httpd_req_async_handler` so the single-threaded server stays responsive.
- **WebUI**: replaced emoji icons with a unified AppIcon SVG system, replaced hardcoded colors with design tokens, and replaced hardcoded English strings with i18n keys.
- **API**: `GET/POST /api/monitoring` expose `commandEnabled` / `commandTokenSet` / `commandTokenClear`; documented `GET/POST /api/check_update`, `/api/changelog`, `/api/ota_url`, `/api/ota_status` and the `betaChannel` setting in `API.md` and `openapi.yaml`.

### Fixed
- **Heap overflow** in UART RX buffers (radio-module + GPS): buffers now match the driver RX-buffer size, so `event.size > 128 B` no longer corrupts the heap.
- CRC16 read/write now use `memcpy` (unaligned / strict-aliasing safe).
- `Settings::save()` / `clear()` now bail out on `nvs_open` failure instead of using an uninitialized handle.
- `StreamParser` discards oversized frames instead of processing a stale buffer (prevents stream desync).
- Replaced the VLA in `RadioModuleDetector::sendFrame` with a fixed buffer + bounds check.
- Guarded the `cJSON_Print` result in the OTA-status handler against OOM NULL-deref.
- Hardened monitoring and log handling.
- **WebUI**: LED-program edits now mark the form dirty (`deep:true`) — previously lost on navigation; the forced-change gate in `PasswordChangeModal` can no longer be bypassed via ESC/backdrop; TLS clear-flags reset when loading a fresh PEM; the OTA countdown interval is cleared; `/api/ota_url` errors are surfaced instead of swallowed; pre-release segments are compared numerically (`Beta.10 > Beta.3`); a `401` on public pages (`/about`) no longer bounces unauthenticated visitors to `/login`.
- Removed obsolete SNMP tests referencing a non-existent function.

### Internal
- npm: vue 3.5.39, vue-router 5.1.0, vite 8.1.0, axios 1.18.1, marked 18.0.5, bootstrap-vue-next 0.45.7, @playwright/test 1.61.1, @vitejs/plugin-vue 6.0.7, esbuild 0.28.1; transitive form-data 4.0.6 fixes a CRLF-injection issue (0 vulnerabilities).
- ESP-IDF CI pinned to the stable tag v6.0.1; managed components mdns ^1.11.2, mqtt ^1.0.0.
- GitHub Actions: checkout v6 → v7, crate-ci/typos 1.45.1 → 1.47.2.
- Build/CI: resolve Beta.4 compile errors (lwIP `NO_DATA` macro clash in `streamparser.h`, `snprintf` format-truncation in `mqtt_handler.cpp`, removed `ESP_ERR_HTTPS_OTA_INCOMPLETE`, `-Wstringop-truncation` in the OTA snapshot); make `update_changelog.py` and `update_version.py` idempotent so release re-runs neither duplicate changelog sections nor accumulate `-Beta.x` version suffixes.

## [2.2.0-Beta.3] - 2026-06-12

### Added
- Async monitoring diagnostic; i18n for previously hardcoded UI strings.

### Changed
- Performance & stability upgrade; moved the update-check / changelog proxies off the httpd task.

### Fixed
- OTA upload timeout, 401 redirect for visitors and log-polling robustness.
- Boot-hang hardening; httpd / MQTT / NTP stability fixes.
- RTC detection broken on IDF 6.0 and in the perf-oriented build config.

## [2.2.0-Beta.2] - 2026-05-27

### Added
- **MQTT TLS/SSL support** (CA certs, mTLS, skip-verify) — thanks @EarlSneedSinclair (PR #316).
- Live LED-brightness update without restart.
- ESP-IDF 6.0.1 build compatibility (I2C struct fields, `time.h` includes).

### Changed
- Upgraded WebUI npm dependencies; espressif/mdns ^1.9.1; added sdkconfig.defaults entries for partition table, flash size and FreeRTOS trace.

### Fixed
- Update banner showing when the running version is newer than the available one.
- Semver pre-release comparison and log-polling stability.
- Missing `#include <time.h>` in `systemclock.cpp`; I2C struct-initializer field order in `rtcdriver.cpp`; suppressed unused-variable warnings in `semver.h`; double version suffix in `TROUBLESHOOTING.md`.

## [2.2.0-Beta.1] - 2026-04-24

### Added
- **ESP-IDF 6.0 migration**: LAN87xx PHY driver from esp-eth-drivers, external RMII clock config for the LAN8720A, LEDC driver, CMake/sdkconfig updates; renamed `src/` to `main/`; moved mqtt/json to managed components.
- Improved WebUI error handling, timeouts and API-response validation.

### Changed
- Migrated FreeRTOS tick conversion to the IDF 6.0 API.
- WebUI responsive-design improvements across all viewports; stability, layout, performance and design polish.

### Fixed
- Ethernet-init debug logging; EMAC clock reverted to CLK_OUTPUT GPIO17 (correct for the HB-RF-ETH board).
- Integer overflow in DNS-cache time calculation after ~12 h; `sem_take` timeout units (was ms instead of seconds); static definition type widened to `uint64_t`.
- CI now uses the esp-idf `install.sh` for v6 setup.

## [2.1.10] - 2026-03-20

### Added
- **Working SNMP, MQTT and CheckMK monitoring** (previously non-functional); GitHub Pages landing page and deployment workflow.

### Changed
- Upgraded Vite to 8; dependency refresh.

### Fixed
- **Monitoring thread-safety**: complete refactor to eliminate hangs and crashes; monitoring-config save is now non-blocking (async task); services only stop/restart when config actually changed.
- Stack overflows in the monitoring POST response, the CheckMK agent and `apply_config_task` (stacks raised to 8192 bytes); `checkmk_stop()` crash when disabling CheckMK after it was running; mqtt/checkmk stop crashes and race conditions.
- I2C-master API and missing `i2c_port` field for ESP-IDF 5.5.3; UART warnings; C++ designated-initializer compilation errors.
- CCU reconnect with a stale endpoint identifier after device reboot (now accepted); watchdog reconnect logic restored.
- Monitoring-settings save redirecting to login without saving; saving monitoring config with empty strings; skipping validation of empty strings when the service is disabled.
- SNMP removed again (not supported in ESP-IDF 5.5.3 / `CONFIG_LWIP_SNMP` unavailable).

## [2.1.9] - 2026-03-09

### Added
- Privacy disclaimer for external update checks.

### Changed
- Simplified release/artifact naming (dropped "Firmware" from the release name); updated platform/framework versions.

### Fixed
- Dependency / security updates (axios, vue, vue-i18n, bootstrap-vue-next, marked, vue-router, rollup, typos, upload-artifact).
- Refreshed documentation and screenshots; changelog-button visibility on the firmware page.

## [2.1.8] - 2026-02-22

### Fixed
- OTA updates failing with GitHub redirects (Bug #235).
- CORS error in the manual update check.
- OTA check interval and upload error; switched the OTA update server to xerolux.de.

### Added
- OTA functional test script.

## [2.1.7] - 2026-02-21

### Added
- **Fully configurable LED programs** (replaces the fixed update-blink toggle).
- Home Assistant MQTT Auto-Discovery integration.

### Changed
- Refactored LED programs; automated version bump in the release workflow.

### Fixed
- LED programs not saved/loaded correctly; immediate LED-state update and settings caching.
- IPv6 validation: suppressed spurious warnings for hostname server addresses; IPv6 support in CCU frontend validation.
- Bugs in GPS, IPv6 validation, OTA and the CPU task; UDP listener, MQTT handler and DNS cache; settings persistence and security; factory-reset linker error in the MQTT handler; compilation errors in LED and DNS caching.

### Security
- String-length validation before `strncpy` to prevent bypass; CCU address validation; MQTT-server, SNMP-community and NTP-server validation.

## [2.1.6] - 2026-02-12

### Added
- Real OTA progress tracking with a status-polling endpoint.
- Switchable LED blink on firmware update.
- Backend proxy for the changelog.

### Changed
- Optimized WebUI idle timer, accessibility and JSON serialization; refreshed translations and the mobile language menu.
- Upgraded ESP-IDF from v5.5.0 to v5.5.2.

### Fixed
- **Bugfix release**: backup/restore missing settings; CORS removed from the monitoring API; log-offset sync on ring-buffer overflow; OTA task double response.
- MQTT password loss, `ccuIP` validation, OTA double response, hostname mismatch, MQTT race condition.
- `PasswordChangeModal` validation; firmware-update URL format; password-change failure and settings persistence.

## [2.1.5] - 2026-02-10

### Added
- Mobile-optimized WebUI + bug fixes.
- Log Manager WebUI with enable/disable toggle; high-contrast system-log viewer.
- 10-minute idle auto-logout and persistent session.
- Maintenance-action modals; loading states on backup/restore and async buttons.
- Full-system restart that resets peripherals on reboot.

### Changed
- Migrated the WebUI bundler from Parcel to Vite; modernized UI/UX.
- Replaced hardcoded fallbacks with translation keys; completed localization.

### Fixed
- Race conditions, resource leaks and long-term stability; hardened string operations, fixed a memory leak, removed dead code.
- CCU connection stability with automatic reconnect; UDP connection handling.
- Vite code-splitting causing a blank WebUI; compilation errors (`esp_http_client_get_content_length`, `mdns_service_register_all` for ESP-IDF 5.1).
- OTA panics on failed updates; memory leak and CI issues.
- IPv6 support in the RateLimiter (fixed IP-resolution warning).

### Changed
- Removed the OTA-password requirement for firmware updates.

## [2.1.2] - 2026-02-07

Re-stabilization release restoring the v2.1.1 codebase after the experimental
December line (2.1.3 / 2.1.4) was rolled back.

### Fixed
- **RaspberryMatic Raw UART UDP connection**: accept the client endpoint ID to sync persistent sessions; set `_connectionStarted = true` on persistent-session reconnect; restore reliable reconnect after restart.
- Simplified Raw-UART UDP connection handling (removed unnecessary port clearing).

## [2.1.4] - 2025-12-20

Experimental feature release (later rolled back; re-stabilized in 2.1.2 on 2026-02-07).

### Added
- Multi-variant firmware builds (HMLGW and Analyzer features via conditional compilation) with firmware-variant matching in the update check.
- HomeMatic LAN Gateway (HMLGW) emulation mode.
- Analyzer Light feature with RSSI and naming.
- Optional DTLS/TLS transport encryption for Raw-UART UDP (ESP-IDF 5.x / mbedTLS 3.x).
- Nextcloud WebDAV backup integration.
- Content-Security-Policy and HTTP security headers; ETag caching for embedded static files.
- Reboot-resistant system log; maintenance controls.

### Changed
- Performance phases: FreeRTOS stack optimizations (~5 KB RAM saved), HMLGW ring-buffer & busy-wait elimination (~10–20 % CPU reduction), analyzer WebSocket buffer pool, `StreamParser` bulk copy, small-buffer optimization for `AnalyzerFrame` (~20 KB → ~1.5 KB queue heap).
- Refactored WebUI JSON handling; standardized password inputs with visibility toggle; modals for maintenance actions; visual icons.

### Fixed
- Network-stack stall via non-blocking queue sends in UDP callbacks; CCU reconnection after restart.
- Stack overflows in CheckMK/Analyzer agents; race condition in HMLGW task termination; analyzer WebSocket delivery.

### Security
- Cache-Control headers on sensitive endpoints; plain-text password removed from backup JSON; OTA rollback prevention; password-complexity enforcement; IP-whitelist bypass in CheckMK fixed.

## [2.1.3] - 2025-12-19

### Added
- Cache-Control headers; ETag caching for embedded static files; maintenance controls; password visibility toggle.

### Changed
- Upgraded to ESP-IDF 5.5.1 (from 5.1.0); platform / dependency refresh.

### Fixed
- 16+ critical bugfixes and optimizations; UDP packet handling (removed heap allocations).

### Security
- Stack buffer overflow in the monitoring agent (CRITICAL); timing attack in auth (HIGH); IP-whitelist bypass in CheckMK; password-length validation enforced.

## [2.1.1] - 2025-12-15

### Added
- **MQTT support** for system-status monitoring.
- **Home Assistant MQTT Discovery** support.
- Online update check with toggle; early-updates toggle and release-notes display.
- WebUI: auto-logout and manual logout redirect; login button when unauthenticated.

### Changed
- i18n: translations updated for all 10 languages; openCCU compatibility and i18n language switching.
- Increased the max URI handlers for the WebUI.

### Fixed
- 401 Unauthorized on backup download; variable shadowing and compiler warnings; CppCheck warnings.

### Security
- Timing-attack-resistant auth comparison; accessibility of the theme toggle.

## [2.1.0] - 2025-12-14

### Added
- Initial public release of the HB-RF-ETH-ng fork: ESP32 firmware ported to ESP-IDF 5.x, based on the original HB-RF-ETH by Alexander Reinert. Connects HomeMatic radio modules (HM-MOD-RPI-PCB, RPI-RF-MOD) to debmatic / piVCCU3 over Ethernet.
