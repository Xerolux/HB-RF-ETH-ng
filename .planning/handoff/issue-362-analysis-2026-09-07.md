# Issue #362 — Analyse und Maßnahmen, 2026-09-07 (Fable)

Ergänzt `issue-362-investigation-handoff-v2.md` und `issue-362-fable-response.md`.
Stand nach Beta.6 mit den ersten gültigen `Tick sentinel (pre-reset)`-Zeilen
(Walki2000, rev 100, 2026-09-07).

## 1. Neue Feld-Fakten, die die bisherigen Hypothesen verändern

| Fakt | Quelle | Konsequenz |
|---|---|---|
| Crash 1 (up=1386 s): CPU0 stoppt zuerst, CPU1 tickt bis zum Reset weiter (gap −291 ms = 300 ms IWDT abzüglich Tick-Raster) | Sentinel Beta.6 | Ein **einzelner** Kern friert ein; kein Zwei-Kern-Deadlock, keine Netzwerk-Vorstufe |
| Crash 2 (up=426 s): CPU1 stoppt zuerst, CPU0 79 ms später | Sentinel Beta.6 | Zweiter Kern stirbt erst, als er ein vom ersten gehaltenes Spinlock braucht |
| tcpip-Liveness-Wächter (Beta.6) hat in beiden Fällen **nicht** ausgelöst | Log | lwIP lief bis zum Ende; der Wedge-Ansatz der Handoffs v1/v2 war ein Folgesymptom, kein Initiator |
| Heap gesund, keine Log-Zeile, alle Chip-Revisionen, beide IDF-Generationen (55EXP: 1× IWDT + 1× Hänger) | alle | Ursache liegt in Code, der auf beiden IDFs identisch ist und nur unter Feld-Last läuft |
| Bench (wenig RF-Geräte) stabil; Feld (viele RF-Geräte) crasht; 67 Mbit/s LAN-Flut am Bench ohne Reset | Bench-Tests | Die einzige ungetestete Last-Variable ist der **UART-Empfang vom Funkmodul** |

## 2. Mechanismus-Kandidat (code-verifiziert)

`components/esp_hal_uart/esp32/include/hal/uart_ll.h` (IDF 6.1) und
`components/hal/esp32/include/hal/uart_ll.h` (IDF 5.5.3) enthalten identisch:

```c
FORCE_INLINE_ATTR void uart_ll_rxfifo_rst(uart_dev_t *hw)
{
    //Hardware issue: we can not use `rxfifo_rst` to reset the hw rxfifo.
    ...
    do {
        fifo_cnt = HAL_FORCE_READ_U32_REG_FIELD(hw->status, rxfifo_cnt);   // DPORT-Lesung
        rxmem_sta.val = hw->mem_rx_status.val;
        if (fifo_cnt != 0 || (rxmem_sta.rd_addr != rxmem_sta.wr_addr)) {
            READ_PERI_REG(fifo_addr);
        } else {
            break;
        }
    } while (1);
}
```

Aufrufer im Treiber (`esp_driver_uart/src/uart.c`, 5.5.3 und 6.1 identisch):

- ISR `uart_rx_intr_handler_default`, Zweig `UART_INTR_RXFIFO_OVF`:
  `UART_ENTER_CRITICAL_ISR(spinlock); uart_hal_rxfifo_rst(); UART_EXIT_CRITICAL_ISR();`
  → Schleife läuft **mit maskierten Interrupts im ISR-Kontext** (Zeilen 1441–1445 in 6.1).
- Task-Kontext `uart_flush_input()` (unsere `_serialQueueHandler` ruft es bei
  `UART_FIFO_OVF`/`UART_BUFFER_FULL`).

Espressif-Errata **UART-3.17** („UART fifo_cnt Does Not Indicate the Data Length
In FIFO Correctly“, alle ESP32-Revisionen v0.0–v3.1, „no fix scheduled“): eine
DPORT-Lesung von `fifo_cnt`, die unterbrochen wird, dekrementiert den Zähler
fälschlich. Der Treiber nutzt für die Längenberechnung deshalb bewusst die
Pointer (`uart_ll_get_rxfifo_len`) — die Reset-Schleife oben prüft aber
`fifo_cnt != 0` als Abbruchbedingung. Ein FIFO, dessen Zähler und Pointer
nicht mehr zusammenpassen, hält diese Schleife unendlich: Tick des Kerns
steht, 300 ms später IWDT, kein Panic-Output aus der ISR-Critical-Section,
Heap gesund.

Warum nur im Feld: Der Overflow-Pfad wird erst erreicht, wenn (a) der
256-Byte-RX-Ring des Treibers voll ist (UART-Task blockiert länger als ~22 ms
in `sendMessage()`/`tcpip_api_call`, typisch bei CCU-Reconnect-Bursts) und
(b) danach das 128-Byte-Hardware-FIFO überläuft — oder wenn die ISR (nicht im
IRAM!) während eines Flash-Fensters länger als 0,7 ms maskiert ist
(Default-Schwelle 120/128 Bytes). Beides braucht **kontinuierlichen
Modul→ESP-Verkehr**; ein Bench-Gerät ohne Funkgeräte erzeugt ihn nie.

Passung zu den Sentinel-Daten: Crash 1 = ISR-Kern (CPU0, `uart_driver_install`
läuft in `app_main` auf CPU0) friert allein ein. Crash 2 (CPU1 zuerst) ist mit
diesem einen Mechanismus **nicht** vollständig erklärt — möglich ist ein
Task-Kontext-Spin auf CPU1 mit gehaltenem UART-Spinlock; das Transkript der
nächsten Crashes wird es zeigen. Der Mechanismus ist ein Kandidat, kein
Beweis; das Beweismittel ist Punkt 3.

## 3. Umgesetzte Maßnahmen (dieser Commit)

1. **Panic-Transkript-Erfassung** (`main/panic_transcript.cpp`,
   `include/panic_transcript.h`, `-Wl,--wrap=uart_hal_write_txfifo` in
   `main/CMakeLists.txt`): Der Panic-Handler schreibt Register-Dump und
   Backtraces beider Kerne über `uart_hal_write_txfifo(&UART0,…)`; der Wrapper
   spiegelt die ersten 2 KiB in RTC-Slow-Memory. Beim nächsten Boot:
   Kernzeilen ins System-Log (`PanicLog:`), Volltext in den Crash-Tail-Slot
   (`/api/crash_log`, WebUI-Seite System-Log → „Crash-Tail laden“). Auflösen mit
   `xtensa-esp32-elf-addr2line -pfiaC -e build/HB-RF-ETH-ng.elf 0x…`.
2. **Raw-Reset-Ursache** (`ResetInfo::getRtcResetCause`, Boot-Zeile
   `rtc: SW_CPU` vs. `rtc: TG1WDT_SYS`): unterscheidet „Stage-0-Interrupt bedient,
   Panic-Handler lief“ (Transkript vorhanden, Software-Spin) von „Stage-1-Hard-
   Reset, Kern nahm keinen Level-4-Interrupt“ (Hardware-Stall).
3. **UART-Härtung** (`main/radiomoduleconnector.cpp`, sdkconfig): RX-Ring 256 →
   2048 Bytes, RX-FIFO-Schwelle 120 → 64, `CONFIG_UART_ISR_IN_IRAM=y`.
   Zusammen halten sie den Overflow-Pfad unter normaler Last außer Reichweite.
4. **IDF-Patch** `scripts/patch_idf_uart_rxfifo_rst.sh` (CI-Schritt in allen fünf
   Firmware-Workflows, analog zum ECO3-Patch): begrenzt die Drain-Schleife auf
   1024 Iterationen. Ein konsistentes 128-Byte-FIFO ist lange vorher leer; ein
   inkonsistentes hängt den Kern nicht mehr, der Treiber arbeitet mit der
   pointerbasierten Längenberechnung weiter.
5. `CONFIG_HEAP_POISONING_LIGHT=y` (aus der Vorsession, jetzt committet): macht
   eine stille Heap-Korruption als Panic sichtbar — die dann ebenfalls im
   Transkript landet.

## 4. Entscheidungsbaum für die nächsten Feldmeldungen

| Beobachtung | Bedeutung | Nächster Schritt |
|---|---|---|
| Keine Resets mehr über Tage bei zoephelweb/Walki2000/ChristophA | UART-Overflow-Pfad war der Initiator | Issue schließen, Espressif-Issue zu UART-3.17 / `uart_ll_rxfifo_rst` erwägen |
| Reset mit `PanicLog:`-Zeilen, Backtrace in `uart_ll_rxfifo_rst`/`uart_rx_intr_handler_default` | Mechanismus bestätigt, Bound greift nicht (unerwartet) | Bound-Wert prüfen, Task-seitiges `uart_flush_input` ersetzen |
| Reset mit `PanicLog:`-Zeilen, Backtrace woanders | Neuer, jetzt benannter Initiator | addr2line, gezielt fixen |
| Reset ohne Transkript, `rtc: TG1WDT_SYS` | Kern konnte keinen Level-4-Interrupt nehmen → Hardware-Stall (Cache-Livelock-Klasse, Bus) | ECO3-Workaround-Wirksamkeit / Flash-Timing / Spannungsversorgung |
| Reset ohne Transkript, `rtc: SW_CPU` | Panic-Handler lief, Ausgabe nicht erfasst | Wrapper-Pfad prüfen (`CONFIG_ESP_CONSOLE_UART_NUM`, `hal->dev`) |

## 5. Was weiter offen ist

- Kein Reproduktionsfall am Bench. Vorschlag: am Bench-Gerät UART1-RX
  (GPIO 35) mit einem USB-UART-Adapter dauerhaft mit 115200 Baud befüttern
  (z. B. `cat /dev/urandom | pv -L 11k > /dev/ttyUSB0`), Modul dafür abziehen.
  Vor dem Patch sollte das den Overflow-Pfad in Minuten erreichen.
- Crash 2 (CPU1 zuerst) bleibt bis zum ersten Transkript unerklärt.
- Der 55EXP-Hänger (kein WDT, LED dauerhaft blau) ist mit dem UART-Mechanismus
  nicht erklärt; er passt eher zu einem Task-Kontext-Spin in `uart_flush_input`
  (Ticks laufen, `rx_mux` gehalten, RX-Interrupts aus → Relay tot, Netz lebt
  nur bis das nächste `tcpip_api_call` hängt). Der Bound deckt auch diesen Pfad.
