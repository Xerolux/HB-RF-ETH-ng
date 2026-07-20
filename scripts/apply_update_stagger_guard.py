from pathlib import Path

path = Path('main/updatecheck.cpp')
text = path.read_text(encoding='utf-8')


def replace_once(old, new):
    global text
    if old not in text:
        if new in text:
            return
        raise SystemExit(f'Expected block not found: {old[:120]!r}')
    text = text.replace(old, new, 1)

callback_end = '''static void _periodic_timer_callback(void *arg)
{
  UpdateCheck *uc = static_cast<UpdateCheck *>(arg);
  // refresh() allocates a ReleaseInfo struct (~4.9 KB, the bulk being
  // body[4096]) on the stack and then performs an HTTPS fetch whose TLS
  // handshake consumes another 2-4 KB. 12 KB matches the stack budget the
  // former background UpdateCheck task used for the same call.
  BaseType_t created = xTaskCreate(_periodic_check_task, "upd_chk", 12288,
                                   uc, 3, NULL);
  if (created != pdPASS) {
    ESP_LOGE(TAG, "Failed to create periodic update-check task");
  }
}
'''
callback_new = '''static void _periodic_timer_callback(void *arg)
{
  UpdateCheck *uc = static_cast<UpdateCheck *>(arg);

  // Do not create the 12 KB worker stack or begin TLS when the WROOM-32 heap is
  // already fragmented. Skipping one daily check is safer than destabilising
  // Homematic/MQTT operation; the persistent cache remains available.
  const size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
  const size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
  constexpr size_t MIN_FREE_HEAP = 72 * 1024;
  constexpr size_t MIN_LARGEST_BLOCK = 18 * 1024;
  if (free_heap < MIN_FREE_HEAP || largest_block < MIN_LARGEST_BLOCK) {
    ESP_LOGW(TAG,
             "Daily update check skipped before task creation: free=%u KB largest=%u KB",
             (unsigned)(free_heap / 1024),
             (unsigned)(largest_block / 1024));
    return;
  }

  // refresh() allocates a ReleaseInfo struct (~5 KB) on the worker stack and
  // performs the HTTPS fetch. The stack only exists for this short operation.
  BaseType_t created = xTaskCreate(_periodic_check_task, "upd_chk", 12288,
                                   uc, 2, NULL);
  if (created != pdPASS) {
    ESP_LOGE(TAG, "Failed to create daily update-check task");
  }
}

static uint32_t update_check_stagger_seconds(SysInfo *sysInfo)
{
  // Stable FNV-1a hash of the device serial. Devices updated or rebooted at the
  // same time are spread across a 2-60 minute window instead of opening TLS
  // simultaneously every day.
  const char *serial = sysInfo ? sysInfo->getSerialNumber() : nullptr;
  uint32_t hash = 2166136261u;
  if (serial) {
    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(serial);
         *p; ++p) {
      hash ^= *p;
      hash *= 16777619u;
    }
  } else {
    hash ^= esp_random();
  }
  constexpr uint32_t MIN_DELAY_SECONDS = 2 * 60;
  constexpr uint32_t WINDOW_SECONDS = 58 * 60 + 1;
  return MIN_DELAY_SECONDS + (hash % WINDOW_SECONDS);
}
'''
replace_once(callback_end, callback_new)

old_start_intro = '''    // One-shot timer: evaluate the persistent 24 h window 30 s after boot.
    // It performs a fetch only when due; rebooting cannot force a new request.
    if (!_initialTimer) {'''
new_start_intro = '''    const uint32_t staggerSeconds = update_check_stagger_seconds(_sysInfo);
    ESP_LOGI(TAG, "Update checks staggered by %u minutes for this device",
             (unsigned)((staggerSeconds + 59) / 60));

    // One-shot timer: evaluate the persistent 24 h window after the per-device
    // stagger. It performs a network fetch only when due; rebooting cannot
    // force another request inside the stored 24-hour window.
    if (!_initialTimer) {'''
replace_once(old_start_intro, new_start_intro)

replace_once('''            esp_timer_start_once(_initialTimer, 30 * 1000000ULL);  // 30 s''', '''            esp_timer_start_once(_initialTimer,
                                 static_cast<uint64_t>(staggerSeconds) * 1000000ULL);''')

old_periodic = '''    // Periodic timer: re-check every 24 h. Replaces the former always-running
    // background task that spent 12 KB of stack permanently in vTaskDelay.
    esp_timer_create_args_t periodicArgs = {};
    periodicArgs.callback = _periodic_timer_callback;
    periodicArgs.arg = this;
    periodicArgs.name = "updchk_24h";
    if (esp_timer_create(&periodicArgs, &_periodicTimer) == ESP_OK) {
        // 24 h in microseconds.
        esp_timer_start_periodic(_periodicTimer, 24ULL * 60 * 60 * 1000000ULL);
    } else {'''
new_periodic = '''    // Periodic timer: never more often than once per 24 h. The same stable
    // per-device offset is added to the period so fleets do not gradually
    // converge on one check time. refreshIfDue() additionally enforces the
    // persistent 24-hour lock across reboots.
    esp_timer_create_args_t periodicArgs = {};
    periodicArgs.callback = _periodic_timer_callback;
    periodicArgs.arg = this;
    periodicArgs.name = "updchk_24h";
    if (esp_timer_create(&periodicArgs, &_periodicTimer) == ESP_OK) {
        const uint64_t periodSeconds = 24ULL * 60 * 60 + staggerSeconds;
        esp_timer_start_periodic(_periodicTimer, periodSeconds * 1000000ULL);
    } else {'''
replace_once(old_periodic, new_periodic)

path.write_text(text, encoding='utf-8')
print('Added per-device update staggering and pre-TLS heap guard.')
