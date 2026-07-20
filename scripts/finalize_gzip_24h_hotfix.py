from pathlib import Path

# Fix duplicated cache fields, shrink the release snapshot, and reduce the
# short-lived update-check task stack after the manifest cache redesign.
path = Path('include/updatecheck.h')
text = path.read_text(encoding='utf-8')
duplicate = '''    bool betaChannel;           // channel used to populate this cache
    WebUIReleaseInfo webui;     // optional WebUI block from the same manifest
    bool betaChannel;           // channel used to populate this cache
    WebUIReleaseInfo webui;     // optional WebUI block from the same manifest
'''
single = '''    bool betaChannel;           // channel used to populate this cache
    WebUIReleaseInfo webui;     // optional WebUI block from the same manifest
'''
if duplicate in text:
    text = text.replace(duplicate, single, 1)
text = text.replace('char body[4096];            // release notes markdown (truncated if too long)',
                    'char body[1024];            // compact release-note excerpt or notes URL')
text = text.replace('// Lightweight release snapshot — omits the 4 KB body / 256 B URL fields so\n// periodic callers (MQTT publish every 5 s, update-check task) don\'t copy a\n// 5 KB struct on their stack.  WebUI (which needs body/URLs) still uses',
                    '// Lightweight release snapshot — omits URLs, WebUI metadata and the note\n// excerpt so periodic callers do not copy the full cached manifest on stack.\n// WebUI callers use')
text = text.replace('// Periodic 24 h timer that replaces the former always-sleeping background\n    // task. Keeping a task alive only to vTaskDelay for 24 h wasted 12 KB of\n    // precious ESP32 (no PSRAM) heap permanently. The timer fires every 24 h,\n    // spawns a short-lived "upd_chk" task (12 KB) that runs refresh() +\n    // _evaluateReleaseInfo() and self-deletes, so the 12 KB stack is only',
                    '// Persistent, staggered timer replacing the former always-sleeping task.\n    // The short-lived worker is created only when heap checks pass and the\n    // stored 24-hour window is due.')
path.write_text(text, encoding='utf-8')

path = Path('main/updatecheck.cpp')
text = path.read_text(encoding='utf-8')
text = text.replace('// 12 KB stack is only resident for the ~5 s of the actual check instead of',
                    '// worker stack is only resident for the actual check instead of')
text = text.replace('// already fragmented. Skipping one daily check is safer than destabilising',
                    '// already fragmented. Skipping one scheduled check is safer than destabilising')
text = text.replace('// Do not create the 12 KB worker stack or begin TLS when the WROOM-32 heap is',
                    '// Do not create the worker stack or begin TLS when the WROOM-32 heap is')
text = text.replace('// refresh() allocates a ReleaseInfo struct (~5 KB) on the worker stack and\n  // performs the HTTPS fetch. The stack only exists for this short operation.\n  BaseType_t created = xTaskCreate(_periodic_check_task, "upd_chk", 12288,',
                    '// ReleaseInfo is now compact (~2.5 KB including WebUI metadata). A 9 KB\n  // worker leaves TLS/function headroom while reducing the temporary heap spike.\n  BaseType_t created = xTaskCreate(_periodic_check_task, "upd_chk", 9216,')
text = text.replace('// refresh() allocates a ReleaseInfo struct (~4.9 KB, the bulk being\n  // body[4096]) on the stack and then performs an HTTPS fetch whose TLS\n  // handshake consumes another 2-4 KB. 12 KB matches the stack budget the\n  // former background UpdateCheck task used for the same call.',
                    '// The timer callback never performs network work; the dedicated worker owns\n  // the compact manifest snapshot and TLS call.')
path.write_text(text, encoding='utf-8')

path = Path('docs/HOTFIX_BETA3_RECOVERY.md')
text = path.read_text(encoding='utf-8')
text = text.replace('The server now uses the embedded gzip assets whenever a browser does not\nadvertise Brotli support. Browsers that advertise Brotli continue to use the\ncompact external WebUI.',
                    'The standalone WebUI now uses gzip exclusively. Brotli assets and Brotli\nnegotiation are removed, so the same encoding is used on every private-LAN\nbrowser.')
if 'per-device stagger' not in text:
    text += '''

The daily update check is persisted in NVS and cannot be retriggered by page
visits or device reboots. A stable per-device stagger spreads fleets across a
2-60 minute window. Before creating the short-lived worker or opening TLS, the
firmware checks total free heap and the largest free block; an unsafe check is
skipped instead of risking a crash.
'''
path.write_text(text, encoding='utf-8')

print('Finalized compact update cache and 9 KB guarded worker.')
