#!/usr/bin/env bash
#
# Bound the ESP32 UART RX-FIFO drain loop in ESP-IDF (issue #362).
#
# On the ESP32 the hardware RX-FIFO reset bit is unusable (it corrupts the
# neighbouring UART), so ESP-IDF's uart_ll_rxfifo_rst() "resets" the FIFO by
# reading bytes out of it in a loop:
#
#     do {
#         fifo_cnt = <status.rxfifo_cnt via DPORT>;
#         rxmem_sta = <mem_rx_status>;
#         if (fifo_cnt != 0 || rd_addr != wr_addr) READ_PERI_REG(fifo);
#         else break;
#     } while (1);
#
# The loop has no iteration bound and its exit condition trusts
# `rxfifo_cnt`, a counter Espressif documents as unreliable on every ESP32
# revision (errata UART-3.17, "UART fifo_cnt does not indicate the data
# length in FIFO correctly": a DPORT read of fifo_cnt that gets interrupted
# decrements the counter erroneously). The driver's own length function
# already avoids fifo_cnt for that reason; the reset loop does not.
#
# The UART driver runs this loop INSIDE an ISR critical section whenever the
# RX FIFO overflows (uart_rx_intr_handler_default, UART_INTR_RXFIFO_OVF) and
# again from task context in uart_flush_input(). An overflow only happens
# under sustained inbound UART traffic — on the HB-RF-ETH that is the radio
# module in a busy HomeMatic installation, never on an idle bench unit. A
# FIFO whose counter and pointers disagree keeps this loop spinning with
# interrupts masked, the tick on that core stops, and 300 ms later the
# interrupt watchdog resets the board: no panic output, healthy heap,
# random uptime, independent of chip revision and IDF generation. That is
# the field signature of issue #362.
#
# This script replaces the unbounded loop with a bounded one (1024
# iterations, a few hundred microseconds worst case; the FIFO holds 128
# bytes, so any consistent FIFO drains long before the bound). Functionally
# identical for a healthy FIFO; on an inconsistent one the ISR returns and
# the driver recovers through its pointer-based length calculation instead
# of hanging the core. Idempotent and safe to re-run. The 5.5.x line uses
# slightly different whitespace in the same loop, which the matcher accepts.
#
# Usage: scripts/patch_idf_uart_rxfifo_rst.sh [idf_path]   (default: $IDF_PATH or ~/esp-idf)

set -euo pipefail

IDF_PATH_ARG="${1:-${IDF_PATH:-$HOME/esp-idf}}"

CANDIDATES=(
    "$IDF_PATH_ARG/components/esp_hal_uart/esp32/include/hal/uart_ll.h"   # IDF >= 6.1
    "$IDF_PATH_ARG/components/hal/esp32/include/hal/uart_ll.h"            # IDF <= 6.0
)

LLFILE=""
for candidate in "${CANDIDATES[@]}"; do
    if [ -f "$candidate" ]; then
        LLFILE="$candidate"
        break
    fi
done

if [ -z "$LLFILE" ]; then
    echo "patch_idf_uart_rxfifo_rst.sh: esp32 uart_ll.h not found under $IDF_PATH_ARG — is this an ESP-IDF checkout?" >&2
    exit 1
fi

python3 - "$LLFILE" <<'PY'
import re
import sys

path = sys.argv[1]
src = open(path, encoding="utf-8").read()

MARKER = "HB-RF-ETH-ng: bounded RX-FIFO drain (errata UART-3.17, issue #362)"
if MARKER in src:
    print(f"patch_idf_uart_rxfifo_rst.sh: already patched — nothing to do ({path})")
    sys.exit(0)

start = src.find("void uart_ll_rxfifo_rst(uart_dev_t *hw)")
if start < 0:
    print("patch_idf_uart_rxfifo_rst.sh: uart_ll_rxfifo_rst not found — unexpected IDF layout, refusing to guess",
          file=sys.stderr)
    sys.exit(1)
end = src.find("\n}\n", start)
if end < 0:
    print("patch_idf_uart_rxfifo_rst.sh: could not delimit uart_ll_rxfifo_rst", file=sys.stderr)
    sys.exit(1)
body = src[start:end]

loop_re = re.compile(
    r"(?P<indent>[ \t]*)do \{\n"
    r"(?P<inner>(?:.*\n)*?)"
    r"(?P=indent)\} while ?\(1\);",
)
m = loop_re.search(body)
if not m:
    print("patch_idf_uart_rxfifo_rst.sh: the do/while(1) drain loop was not found in uart_ll_rxfifo_rst — "
          "IDF may already have changed this code; inspect manually", file=sys.stderr)
    sys.exit(1)

indent = m.group("indent")
inner = m.group("inner")
if "READ_PERI_REG(fifo_addr)" not in inner or "rxfifo_cnt" not in inner:
    print("patch_idf_uart_rxfifo_rst.sh: loop body does not look like the known drain loop — refusing to patch",
          file=sys.stderr)
    sys.exit(1)

replacement = (
    f"{indent}// {MARKER}\n"
    f"{indent}// The exit condition trusts rxfifo_cnt, which the ESP32 errata documents\n"
    f"{indent}// as unreliable; an inconsistent FIFO would spin here forever inside the\n"
    f"{indent}// ISR critical section (tick starved -> interrupt watchdog). 1024\n"
    f"{indent}// iterations drain any consistent 128-byte FIFO with a wide margin.\n"
    f"{indent}for (unsigned int hb_drain_iter = 0; hb_drain_iter < 1024; hb_drain_iter++) {{\n"
    f"{inner}"
    f"{indent}}}"
)
new_body = body[:m.start()] + replacement + body[m.end():]
src = src[:start] + new_body + src[end:]
open(path, "w", encoding="utf-8").write(src)
print(f"patch_idf_uart_rxfifo_rst.sh: bounded the RX-FIFO drain loop in {path}")
PY
