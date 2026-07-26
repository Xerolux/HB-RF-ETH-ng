# 🚀 HB-RF-ETH-ng v2.2.5

[![License](https://img.shields.io/github/license/Xerolux/HB-RF-ETH-ng)](LICENSE.md)
[![Downloads](https://img.shields.io/github/downloads/Xerolux/HB-RF-ETH-ng/total)](https://github.com/Xerolux/HB-RF-ETH-ng/releases)

## 📋 Überblick

HB-RF-ETH-ng ist eine modernisierte Fork der originalen HB-RF-ETH Firmware von Alexander Reinert.
Diese Firmware ermöglicht es, ein Homematic Funkmodul (HM-MOD-RPI-PCB oder RPI-RF-MOD) per Netzwerk
an eine CCU-Installation (piVCCU3, debmatic, OpenCCU) anzubinden.

## 🆕 Was ist neu in v2.2.5?

### Changes
- Merge pull request #395 from Xerolux/chore/release-2.2.5-webui-note
- docs(changelog): add 2.2.5 upgrade note about separate WebUI install
- chore: update manifests for v2.2.5-Beta.19

### ⚠️ Wichtiger Hinweis für das Update von 2.2.4 auf 2.2.5

Ab **2.2.5** werden Firmware und WebUI **separat** ausgeliefert und müssen
beide aktualisiert werden, damit Gerät und Oberfläche kompatibel bleiben:

1. **Firmware** wie gewohnt über die Firmware-Update-Seite oder OTA flashen.
2. **Anschließend WebUI** über *Updates → WebUI* hochladen und installieren.
   Die passende WebUI-Version steht im Release-Anhang (`webui-v1.0.0-Beta.15`
   oder neuer) und ist zusätzlich unter
   <https://github.com/Xerolux/HB-RF-ETH-ng/releases/tag/webui-v1.0.0-Beta.15>
   verfügbar.

Eine **inkompatible oder veraltete WebUI** wird vom Gerät nicht ausgeliefert:
die eingebaute Recovery-WebUI bleibt aktiv und zeigt einen Reparaturhinweis.
In diesem Fall die zur Firmware passende WebUI-Version manuell hochladen.

Warum die Trennung? Firmware und WebUI haben unterschiedliche Release-Zyklen
und APIs. Der serverseitig geprüfte Kompatibilitätsvertrag
(`apiVersion` / `minFirmwareVersion`) stellt sicher, dass nie eine
unpassende Kombination aktiv wird.


### Changes
- fix: defer MQTT startup until IPv4 is ready
- chore: update manifests for v2.2.5-Beta.18


### Changes
- fix: stabilize updates, MQTT, Raw-UART and WebUI
- chore: update manifests for v2.2.5-Beta.17

- fix(mqtt): Alle Status-, Event-, OTA- und Home-Assistant-Publishes werden nun zusätzlich zum Client-Lebenszyklus auf den tatsächlichen Broker-Login (`MQTT_EVENT_CONNECTED`) geprüft. Der periodische Publisher wartet während Start und Reconnect, statt komplette QoS-0-Statusblöcke an einen noch nicht verbundenen Client zu übergeben und dadurch `Publish: Losing qos0 data when client not connected` zu fluten.
- fix(update): Die Heap-Freigabe der manuellen und täglichen Updatesuche berücksichtigt jetzt Gesamtgröße und Fragmentierung gemeinsam. Der auf einem Live-Gerät beobachtete gesunde Zustand mit 55 KB frei und 32 KB größtem Block darf die serialisierte Manifest-Abfrage starten; echte Risikozustände unter den absoluten Heap- beziehungsweise Blockgrenzen werden weiterhin abgewiesen.
- fix(recovery): `/recovery` bildet nun die aktuelle New-Design-Shell mit echter Seitenleiste, 88-px-Statusleiste, Markenlogo, flachen Karten, responsivem Mobile-Header sowie dem auf dem Gerät gespeicherten Farbschema nach. Die Seite bleibt vollständig in der Firmware eingebettet und damit auch bei einem beschädigten externen WebUI erreichbar.
- perf(raw-uart): Der UDP-Empfang legt Queue-Ereignisse nun direkt in der FreeRTOS-Queue ab und verarbeitet gewöhnliche Pakete in einem 256-Byte-Stackpuffer. Damit entfallen die beiden Heap-Allokationen pro Standardpaket; größere Frames bleiben über einen geprüften Fallback vollständig unterstützt. Datenformat, Timeouts und CCU-Kompatibilität ändern sich nicht.
- fix(webui): Die Einstellungen-Navigation nutzt auf Desktop die vollständige Inhaltsbreite und bleibt in einer Zeile, statt wegen einer künstlichen 700-px-Grenze vorzeitig umzubrechen. Auf kleineren Ansichten wechselt sie kontrolliert auf drei beziehungsweise zwei Spalten; das Ping-Feld erhält einen angemessen breiten Aktionsbereich und stapelt Eingabe und Vollbreiten-Button auf Mobilgeräten.
- release(webui): Die New-Design-Oberfläche wird wegen der sichtbaren Einstellungen-/Ping-Änderungen als eigenständig erkennbares Update `1.0.0-Beta.15` ausgeliefert.


### Changes
- fix: repair ping, MQTT updates and recovery UI
- docs: align backup navigation label
- chore: update manifests for v2.2.5-Beta.16

- fix(ping): Die Ping-Diagnose wartet nun auf den finalen ESP-IDF-Ping-Callback, bevor Callback-Kontext und EventGroup freigegeben werden. Der bisherige Erfolgspfad erzeugte einen Use-after-free und konnte dadurch einen Interrupt-Watchdog-Reboot auslösen, nach dem die Funkmodul-/CCU-Verbindung bis zum OpenCCU-Neustart gestört blieb (Issue #393).
- fix(mqtt): `command/check_update` verwendet wieder die gleiche timerbasierte, heapgeschützte und mit 60-Sekunden-Cooldown versehene Updatesuche wie die WebUI. Nach Abschluss werden die retained MQTT-Versions- und Update-Topics sofort neu veröffentlicht; Home Assistant erhält den „Check for Update“-Button wieder.
- feat(recovery): Die eigenständige Notfallseite entspricht nun dem New Design mit Header, Page-Hero, kompakten Karten, Light-/Dark-Modus, responsivem Layout und konsistenten Fokuszuständen, bleibt aber weiterhin vollständig unabhängig vom möglicherweise beschädigten WebUI-Bundle.
- test(stability): Ein nativer Host-Test bildet die asynchrone ESP-IDF-Callback-Reihenfolge beim Ping nach und verhindert eine erneute Freigabe vor `on_ping_end`; Build- und Release-Workflow führen ihn verpflichtend aus.


### Changes
- feat: secure portable backups and improve recovery UX
- fix: preserve WebUI release metadata
- chore: update manifests for v2.2.5-Beta.15

- feat(backup): Vollständige JSON-Sicherungen kennzeichnen Klartext-Zugangsdaten nun sowohl in der WebUI als auch direkt in der Datei. Die Datei bleibt bewusst editierbar und kann nach Anpassung von Hostname, IP, Administrator-Passwort und gerätespezifischen MQTT-Werten als Vorlage für weitere Geräte verwendet werden.
- feat(recovery): Die stets sichtbare Schaltfläche „Zur normalen WebUI“ führt sowohl von der Recovery-Ansicht als auch von deren Notfall-Anmeldung zurück zur regulären Oberfläche.
- fix(webui): Der Einstellungspunkt „Backup“ heißt nun in allen vier unterstützten Sprachen „Backup & Reset“ beziehungsweise die jeweilige Übersetzung.
- docs(i18n): README, Changelog, Wiki und Release-Notes nennen konsistent die vier unterstützten Sprachen Deutsch, Englisch, Französisch und Italienisch.


### Changes
- fix: recover monitoring after factory reset
- chore: add planning and browser artifacts
- chore: update manifests for v2.2.5-Beta.14


### Changes
- fix(factory-reset): Der Werksreset entfernt jetzt sämtliche Benutzerkonfigurationen aus den Einstellungen-, Monitoring-, Theme-, Reset-/Crash- und Update-Cache-Namespaces. Auch lokale Browser- und Sitzungsdaten einschließlich Theme und Akzentfarbe werden gelöscht.
- feat(backup): Backup und Wiederherstellung sichern jetzt alle wiederherstellbaren Benutzereinstellungen einschließlich Administrator-Zugangsdaten, Netzwerk, Zeit, LEDs, Theme, Akzentfarbe, Supporter-Key, Browser-Präferenzen sowie vollständiger Monitoring-Konfiguration mit MQTT-/Benachrichtigungs-Passwörtern, Tokens, Zertifikaten und privaten Schlüsseln.
- fix(settings): Feldgenaue Validierungsfehler für Administratorname, CCU-Adresse, Hostname, IPv4, Netzmaske, IPv6 und NTP ergänzt; ungültige Werte werden nun auch im Backend vor Änderungen vollständig abgewiesen.
- fix(network): Die Ping-Diagnose verwendet authentifizierte Requests, meldet Latenz sowie verständliche DNS-/Timeout-Fehler und leitet nach einer statischen IP-Änderung zur neuen Geräteadresse weiter.
- fix(restart): Wiederherstellung, manueller Neustart und MQTT-Neustart verwenden einheitlich die Neustart-Synchronisierung; doppelte Aktionen werden verhindert.
- fix(mqtt): Unsicheren MQTT-Werksreset sowie die nicht mehr installierbare Home-Assistant-Firmware-Update-Entität entfernt.
- fix(webui): Passwortfehler werden übersetzt, ANSI-farbige Systemlog-Zeilen korrekt gefiltert, die Recovery-Seite ist direkt verlinkt und Systemaktionen befinden sich unter „Sichern & Wiederherstellen“.
- test: Regressionstests für vollständigen Werksreset, Backup/Restore, Neustart-Synchronisierung, Validierung, Mobile-Layout und die korrigierten Bedienabläufe ergänzt.


### Changes
- feat: enforce WebUI compatibility and protect factory reset
- chore: update WebUI manifest for webui-v1.0.0-Beta.10


### Changes
- fix(build): unblock firmware build on ESP-IDF v6.1

### Changes
- refactor(webui): Sprachpakete auf Deutsch, Englisch, Französisch und Italienisch reduziert. Gespeicherte, nicht mehr unterstützte Sprachcodes werden automatisch auf eine verfügbare Sprache migriert; dadurch bleibt ausreichend Sicherheitsreserve in der 320-KB-WebUI-Partition.
- fix(webui): Werksreset gegen versehentliches Auslösen abgesichert. Der Reset-Dialog erzeugt bei jedem Öffnen einen kryptografisch zufälligen 8-stelligen Code aus Großbuchstaben, Kleinbuchstaben und Zahlen; erst die exakte, case-sensitive Eingabe schaltet die Reset-Aktion frei. Kopieren, Einfügen und Textauswahl des Codes sind in der Oberfläche gesperrt.
- fix(firmware/webui): Verbindliche Kompatibilitätsprüfung für die separat installierte WebUI ergänzt. Firmware und WebUI besitzen nun getrennte API-Vertragsquellen; beim Booten und nach jedem Upload werden `apiVersion` und `minFirmwareVersion` serverseitig geprüft. Eine unpassende oder unvollständige externe WebUI wird niemals ausgeliefert: Die eingebettete Recovery-WebUI bleibt aktiv und zeigt dauerhaft einen Reparaturhinweis. Release-passende Uploads senden Kompatibilitätsmetadaten bereits vor dem Flash-Löschen, damit ein Konflikt die bisher installierte WebUI nicht zerstört.
- fix(webui): Der globale Firmware-Hinweis verspricht keine direkte OTA-Installation mehr. „Jetzt aktualisieren“ wurde durch „Update ansehen“ sowie den klaren Hinweis auf Download und manuelle Installation ersetzt.
- docs(release): Firmware-/WebUI-Kompatibilitätsvertrag, zwingende API-Inkrementregeln, Fallback-Verhalten, Statusfelder und Upload-Header in Release-, Update- und API-Dokumentation festgeschrieben.
- fix(webui): Firmware- und WebUI-Update-Seiten vereinheitlicht; manuelle Prüfungen zeigen „aktuell“, Cooldown, Überspringen und Fehler dauerhaft und eindeutig an.
- fix(webui): Firmware-Seite vollständig internationalisiert, einschließlich Status, Datumsformatierung, Datei-Validierung und Toast-Meldungen.
- fix(webui): Lesbare Typografie appweit vereinheitlicht sowie Monitoring-Zeilen und Statuskarten pixelgenau ausgerichtet.


### Changes
- chore: update WebUI manifest for webui-v1.0.0-Beta.7

### Changes
- fix(firmware): Updatesuche — Heap-Grenze von 72 KB auf 56 KB gesenkt (mDNS ist in Beta.8 weggefallen, ~30 KB zusätzlicher Spielraum). Bisher wurde die manuelle „Jetzt nach Updates suchen“-Anfrage bei aktiver CCU-Sitzung still verworfen, sobald der freie Heap unter 72 KB fiel — das Gerät zeigte dann fälschlich „kein Update“ statt „übersprungen“. Jetzt: niedrigere Grenze macht die Suche zuverlässiger, UND wenn der Heap trotzdem zu niedrig ist, bekommt die WebUI über ein neues `lastSkipReason`-Feld in `GET /api/check_update` den Grund gemeldet und zeigt einen klaren Toast („Prüfung übersprungen — zu wenig freier Arbeitsspeicher, meist bei aktiver CCU-Sitzung, später erneut versuchen“). Kommt ins nächste Firmware-Release.
- fix(webui): Schriftgrößenmix beseitigt. `.hero-title` (bisher raw `clamp(1.6rem,4vw,2.5rem)` → auf Token-Skala gebunden), `.metric-value` (raw clamp → Tokens), `theme.vue .card-heading h2` (fs-xl → fs-lg, angleichen an die anderen Seiten), `login-btn`/`monitoring save-btn` (fs-lg → fs-sm, angleichen an den globalen `.btn`), `ChangelogModal code` (0.875em → fs-sm). Sichtbare Drifts zwischen Seiten und zwischen Glass-/NewDesign-Theme beim Hero-Titel sind damit verschwunden.
- feat(webui): `fallbackLocale` von `de` auf `en` geändert. Fehlende Übersetzungs-Keys fallen jetzt auf Englisch zurück (international verständlicher) statt auf Deutsch.
- feat(webui): `webuiupdate.vue` und `systemoverview.vue` vollständig auf i18n migriert. Bisher waren diese beiden Seiten für nicht-deutsche Sprachen komplett deutsch (hartcodierte Strings). Jetzt: ~110 neue i18n-Keys (`webuiUpdate.*`, `systemOverview.*`) in allen vier unterstützten Sprachen, beide Seiten rendern in der gewählten UI-Sprache.
- fix(security): `/api/ping` erfordert jetzt Authentifizierung. Bisher konnte jeder LAN-Client (und jeder, der das Gerät via MQTT-Konfiguration als Ping-Sonde nutzen konnte) ungeauthet `{"target":"<beliebig>"}` POSTen — ein SSRF-Vektor plus httpd-Worker-Starvation (jeder Aufruf blockiert bis zu 4 Sekunden einen Worker). Die Validierung entspricht jetzt den anderen state-changing POST-Handlern (restart, factory-reset, ota_update, change-password, restore). Kommt ins nächste Firmware-Release.
- fix(webui): `webuiupdate.vue` — `loadStatus()` hat kein try/catch; bei Netzwerkfehler beim Laden der Seite (Gerät mid-reboot, schlechtes WLAN, 500) blieb der "Neu laden"-Button deaktiviert, und `partitionSize` blieb bei 0 → jede hochgeladene Datei scheiterte mit "Falsche Image-Größe: 0 B erwartet". Jetzt: sauberer catch, klare Fehlermeldung + Retry-Banner, und `selectFile` gibt einen verständlichen Hinweis statt der 0-Byte-Falle.
- fix(webui): `monitoring.vue` — Mount-Fehler beim Laden der Monitoring-Konfiguration wurde still geschluckt (`catch {}`); das Form zeigte dann die Store-Defaults, als wären sie die echte Geräte-Konfiguration. Jetzt: Fehler-Toast + Banner, und `hasChanges` bleibt false (Save-Button erscheint nicht), sodass keine Default-Werte versehentlich über die echte Konfiguration geschrieben werden.
- fix(webui): `settings.vue` Restore — hochgeladene Backup-Datei wird jetzt client-seitig auf Plausibilität geprüft (Top-Level-Keys `hostname` + `useDHCP` müssen vorhanden), bevor ungeprüft beliebige Keys ans Gerät gepostet werden. Eine falsche/fremde/trunkierte Datei wird mit klarem Fehler abgelehnt.
- fix(webui): `firmwareupdate.vue` — Beta-Kanal-Toggle löst bei Fehler nicht mehr zwei überlappende Toasts aus (zentraler Interceptor + lokaler Handler). Die Anfrage ist jetzt `silent: true`; der lokale Handler zeigt die maßgeschneiderte Fehlermeldung.
- fix(webui): `app.vue` — Supporter-Expired-Prompt kann im Safari-Privatmodus nicht mehr bei jedem sysinfo-Poll neu aufklappen. sessionStorage ist dort schreibgeschützt (wird still ignoriert); ein zusätzlicher in-memory Dedup-Ref blockt Re-Triggers innerhalb der Session zuverlässig.
- chore(docs): Phantom-Referenzen auf einen nie existierenden "changelog proxy" in 5 Kommentaren/Docs korrigiert (`include/monitoring.h`, `main/updatecheck.cpp`, `main/log_manager.cpp`, `main/supporter_crl.cpp`, `sdkconfig.defaults`, `CLAUDE.md`). Verhindert zukünftige Verwirrung.
- fix(webui): Übersetzungsvollständigkeit hergestellt. Die vier unterstützten Sprachen (de, en, fr, it) haben identische Key-Sets. Betroffen waren v.a. die `login.passwordReset*`-Keys (Passwort-zurücksetzen-Flow) und `systemlog.crashTitle`/`crashHint` (Crash-Log-Hinweis).
- fix(webui): Bisher unübersetzte Werte in den unterstützten Nicht-de/en-Sprachen fr/it durch echte Übersetzungen ersetzt. Betroffen waren `nav.documentation`, `settings.advancedTitle`, `settings.showExperimental*`, `settings.experimentalEmpty*`, `updates.*`, `firmware.factoryReset*` und `monitoring.resourceWarningTitle/Text`.
- feat(release): WebUI-Release-Notes werden automatisch aus der `[Unreleased]`-Sektion des `CHANGELOG.md` generiert (neues Skript `generate_webui_release_notes.py`, integriert in `release-webui.yml`). Zuvor enthielten WebUI-Releases auf GitHub nur Boilerplate ohne Änderungsbeschreibung.
- refactor(webui): Visuelles Design über alle Seiten vereinheitlicht. Hartcodierte Schrift-Gewichte (400/500/600/700/800) in allen Vue-Komponenten durch Tokens (`var(--font-weight-*)`) ersetzt; hartcodierte px-Padding-/Gap-/Radius-Werte auf `--space-*`/`--card-padding`/`--radius-*`-Tokens migriert. Dies ist die Fortsetzung des in Beta.6 begonnenen Typo-Refactors und schließt die letzten inkonsistenten Seiten (settings, monitoring, systemlog, login, app, sysinfo, NewDesignHeader, theme, webuiupdate, firmwareupdate, ChangelogModal, PasswordChangeModal) ab.
- feat(webui): About- und Passwort-Ändern-Seiten erhalten denselben kanonischen Page-Hero (`.page-hero` + `.hero-eyebrow` + `.hero-title` + `.hero-subtitle`) wie alle anderen Inhaltsseiten. Vorher hatte About nur eine flache `.section-header` und Passwort-Ändern einen abweichenden Bootstrap-BCard-Gradient-Header. Neuer i18n-Key `changePassword.eyebrow` in allen vier unterstützten Sprachen.
- refactor(webui): Section-Titel-Typografie auf `settings.vue` an den kanonischen `.card-section-title`-Stil angeglichen (vorher uppercase + letter-spaced + sekundärfarbig).
- refactor(webui): Karten-Tiefe vereinheitlicht. `systemoverview.vue` (`.overview-card`/`.detail-card`) und `theme.vue` (`.theme-card`) nutzen jetzt `--shadow-md` statt `--shadow-sm` und wirken damit nicht mehr flacher als alle anderen Seiten.
- fix(webui): Drei undefinierte CSS-Tokens (`--color-bg-secondary`, `--color-text-inverse`, `--color-warning-strong`) in `main.css` für Hell- und Dunkel-Modus definiert. Die crash-tail-Box in `systemlog.vue` renderte vorher immer in dunklen VSCode-Farben (die Fallbacks griffen unkonditional); Warning-Texte in `monitoring.vue`/`sysinfo.vue` waren nicht theme-konform.
- fix(webui): Akzent-Farbpicker aus `/theme` greift jetzt an allen Stellen durch. Hartcodierte Glass-UI-Orange-Literale (`#f26a3d`, `rgba(242,106,61,…)`), die Akzent-abhängige Stellen blockierten (Supporter-Medaillon, Supporter-Icon/Badge/Card-Active-State auf settings.vue; Supporter-Hero-Chip auf sysinfo.vue; expired-prompt-icon auf app.vue), wurden durch Tokens ersetzt.
- fix(webui): Bootstrap-Blau-Fallbacks (`#0d6efd`, `#e7f1ff`) und Amber-Literale in `monitoring.vue` durch echte Tokens (`var(--color-info)`, `var(--color-warning-strong)` etc.) ersetzt.


### Changes
- feat: mDNS-Komponente vollständig entfernt (espressif/mdns, MDns-Wrapperklasse, alle Aufrufstellen). mDNS belegt im ESP-IDF rund 15–30 KB Heap; unter aktiver CCU-3-Session fiel der freie Heap auf ~58 KB (largest 44 KB), was dazu führte, dass die manuelle Update-Suche mit „Manual update check skipped (low heap)" übersprungen wurde. Nach Entfernung läuft die Update-Suche wie auf einem unbelasteten Gerät durch. Der Raw-UART-UDP-Listener (Port 3008) ist von mDNS unabhängig und funktioniert unverändert.
- ⚠️ Verhaltensänderung für Anwender: Die automatische CCU-3-Entdeckung per mDNS (_raw-uart._udp) entfällt. Die CCU 3 muss künftig mit der festen IP-Adresse des HB-RF-ETH konfiguriert werden (bzw. per DHCP-Reservation eine feste IP erhalten). Port bleibt UDP 3008. Das WebUI-Feld „Hostname" bleibt erhalten (für DHCP/DNS-Namen), wird nur nicht mehr über mDNS beworben.
- chore: Zwei „by design"-Logzeilen beruhigt. Die SupporterCRL-Meldung „CRL fetch returned status 404" (erwartete Server-Antwort, wenn kein Supporter-Key widerrufen wurde) wurde von INFO auf DEBUG gesenkt und verschwindet aus dem Standard-Log. Die RawUartUdpListener-Warnung „unexpected endpoint identifier … - adopting client identifier" (erscheint einmal pro Geräteneustart, wenn die CCU mit ihrem alten Session-Token reconnectet; Adoption ist semantisch sicher) wurde von WARN auf INFO gesenkt. Keine Verhaltensänderung, ausschließlich Log-Sichtbarkeit.


### Changes
- feat(webui): Manuelle Updatesuche über „Jetzt nach Updates suchen" wiederhergestellt. Neuer Backend-Endpunkt POST /api/check_update löst sofortigen Manifest-Abruf aus (läuft außerhalb des httpd-Threads), 60-Sekunden-Cooldown verhindert Missbrauch; die automatische 24-Stunden-Prüfung bleibt unberührt. Die Schaltfläche erscheint konsistent auf den Firmware- und WebUI-Update-Tabs und zeigt Lade-, Update-, Aktuell- und Fehlerzustände an.
- refactor(webui): Eigenständiger Menüpunkt „Design wechseln" entfernt. Die Theme-Auswahl ist ausschließlich unter Einstellungen → Design erreichbar; der Header-Sonne/Mond-Schnellwechsler bleibt, und die /theme-Route bleibt für Lesezeichen erreichbar.
- fix(webui): Fokus- und Hover-Zustände nutzen jetzt Design-Tokens (var(--color-primary) / var(--color-primary-soft) / var(--shadow-md)) statt hartcodierter Glass-UI-Orange-Tokens — Login-Eingaben, Login-Button, Passwort-Änderungs-Modal und Selbsttest-Test-Button erscheinen damit nicht mehr orange im grünen NewDesign.
- fix(webui): Dashboard-Zeile „Letzter Neustart" in „Neustartgrund" umbenannt (Wert ist die Ursache, keine Zeitangabe); alle vier unterstützten Sprachen aktualisiert.
- docs: POST /api/check_update in API.md und openapi.yaml dokumentiert (202 Accepted, {triggered, fetchInProgress}, Client-Polling).


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
- chore(webui): Neue UI-Strings (updates.*, nav.updates, nav.documentation, settings.tabDesign, settings.advancedTitle, settings.showExperimental*, firmware.factoryReset*) in allen vier unterstützten Sprachen hinzugefügt; fehlende experimentalEmpty* Schlüssel ergänzt.


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


### Changes
- Merge pull request #372 from Xerolux/fix/issue-371-reset-reason-heap-diag
- fix(diag): log reset reason at boot + expose task stack hwm
- chore: rebuild firmware archive
- chore: update manifests for v2.2.4

## ✨ Hauptfunktionen

- **Moderne WebUI** mit Responsive Design, Dark/Light Theme und 4 Sprachen
- **Online-Updates** - Firmware direkt ueber den integrierten Update-Dienst herunterladen
- **MQTT-Support** mit Home Assistant Auto-Discovery
- **CheckMK Monitoring** für Integration in Monitoringsysteme
- **IPv6-Support** mit Auto-Konfiguration
- **Sichere Authentifizierung** mit automatischem Session-Timeout
- **Verbesserte OTA-Updates** mit besserer Fehlerbehandlung
- **LED-Helligkeitssteuerung** (0-100%)
- **Konfigurations-Backup/Restore** über WebUI

## 📥 Installation

### Update über WebUI

1. Die `firmware_*.bin` Datei aus diesem Release herunterladen
2. In der WebUI zu **Firmware Update** navigieren
3. Die .bin Datei hochladen
4. Auf Abschluss des Updates und automatischen Neustart warten

### Update per URL

Alternativ kann das Update direkt aus diesem Release per URL in der WebUI durchgeführt werden.

### Prüfsummen

SHA256-Prüfsummen befinden sich in `SHA256SUMS.txt`.

## ⚠️ Wichtige Hinweise

- **Backup der Einstellungen** vor dem Update erstellen (Einstellungen → Backup & Reset)
- **Nicht abschalten** während des Update-Vorgangs
- Bei sehr alten Versionen kann ein **Werksreset** erforderlich sein
- Nach erfolgreichem Update startet das Gerät **automatisch neu**

## 📦 Im Release enthalten

- **Firmware-Binary** (`firmware_2.2.5.bin`)
- **Bootloader** (`bootloader.bin`)
- **Partitionstabelle** (`partitions.bin`)
- **SHA256-Prüfsummen** (`SHA256SUMS.txt`)
- **Versionsinformationen** (`version.txt`)

## 🔗 Kompatible CCU-Systeme

- **[OpenCCU](https://openccu.de/)** - Open-Source CCU-Betriebssystem
- **[piVCCU3](https://github.com/leon-vi/piVccu)** - Homematic auf Raspberry Pi
- **[debmatic](https://github.com/leopes91/debmatic)** - Homematic auf Debian-basierten Systemen

## 💬 Support & Community

- **Issues**: Bitte [Fehler melden](https://github.com/Xerolux/HB-RF-ETH-ng/issues)
- **Discussions**: [Community-Diskussionen](https://github.com/Xerolux/HB-RF-ETH-ng/discussions)
- **Dokumentation**: Siehe [README.md](https://github.com/Xerolux/HB-RF-ETH-ng/blob/main/README.md)

## 🙏 Unterstützung

Dir gefällt dieses Projekt und du möchtest es unterstützen?

[![Buy Me A Coffee][buymeacoffee-badge]][buymeacoffee]
[![Tesla Referral](https://img.shields.io/badge/Tesla-Referral-red?style=for-the-badge&logo=tesla)](https://ts.la/sebastian564489)

[buymeacoffee]: https://www.buymeacoffee.com/xerolux
[buymeacoffee-badge]: https://img.shields.io/badge/buy%20me%20a%20coffee-donate-yellow.svg?style=for-the-badge

## 📄 Lizenz

Diese Firmware steht unter [Creative Commons Attribution-NonCommercial-ShareAlike 4.0](LICENSE.md).

---

**Vielen Dank an alle Beitragenden!** 🙏

*Diese Firmware basiert auf der originalen Arbeit von [Alexander Reinert](https://github.com/ja-ra). 
Die modernisierte Fork wird von [Xerolux](https://github.com/Xerolux) gewartet.*

## Included WebUI

- WebUI version: `1.0.0-Beta.15`
- WebUI API: `1`
- Minimum firmware: `2.2.5-Beta.1`
