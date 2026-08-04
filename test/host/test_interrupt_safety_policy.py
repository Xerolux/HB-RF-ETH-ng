from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[2]
STABLE_IDF = "v6.0.2"
FIRMWARE_WORKFLOWS = (
    ".github/workflows/build.yml",
    ".github/workflows/pr-check.yml",
    ".github/workflows/release.yml",
    ".github/workflows/release-webui.yml",
    ".github/workflows/security.yml",
)


class InterruptSafetyPolicyTest(unittest.TestCase):
    def read(self, relative_path: str) -> str:
        return (ROOT / relative_path).read_text(encoding="utf-8")

    def test_emac_interrupt_is_suspended_while_flash_cache_is_disabled(self) -> None:
        ethernet_source = self.read("main/ethernet.cpp")
        self.assertNotIn("ETH_MAC_FLAG_WORK_WITH_CACHE_DISABLE", ethernet_source)

        sdkconfig = self.read("sdkconfig.hb-rf-eth-ng")
        self.assertIn("# CONFIG_ETH_IRAM_OPTIMIZATION is not set", sdkconfig)
        self.assertNotIn("CONFIG_ETH_IRAM_OPTIMIZATION=y", sdkconfig)

    def test_firmware_builds_use_the_same_stable_idf_release(self) -> None:
        expected_clone = f"--branch {STABLE_IDF} https://github.com/espressif/esp-idf.git"
        for workflow in FIRMWARE_WORKFLOWS:
            with self.subTest(workflow=workflow):
                content = self.read(workflow)
                self.assertIn(expected_clone, content)
                self.assertNotIn("-beta", content)

        lockfile = self.read("dependencies.lock")
        self.assertIn("\n    version: 6.0.2\n", lockfile)

        sdkconfig = self.read("sdkconfig.hb-rf-eth-ng")
        self.assertIn('CONFIG_IDF_INIT_VERSION="6.0.2"', sdkconfig)

        setup_script = self.read("scripts/setup_esp_idf.sh")
        self.assertIn('ESP_IDF_VERSION="${ESP_IDF_VERSION:-v6.0.2}"', setup_script)

        readme = self.read("README.md")
        self.assertIn("ESP-IDF 6.0.2", readme)
        self.assertNotIn("ESP-IDF 6.1-beta1", readme)

    def test_watchdog_remains_a_fault_detector(self) -> None:
        sdkconfig = self.read("sdkconfig.hb-rf-eth-ng")
        self.assertIn("CONFIG_ESP_INT_WDT_TIMEOUT_MS=300", sdkconfig)

    def test_runtime_uses_no_emulated_64_bit_atomics(self) -> None:
        forbidden = re.compile(
            r"(?:std::atomic\s*<|_Atomic\s*\()\s*"
            r"(?:(?:std::)?u?int64_t|unsigned\s+long\s+long|long\s+long)"
        )
        for source_root in (ROOT / "include", ROOT / "main"):
            for path in source_root.rglob("*"):
                if path.suffix not in {".h", ".hpp", ".c", ".cpp"}:
                    continue
                with self.subTest(path=path.relative_to(ROOT)):
                    self.assertIsNone(forbidden.search(path.read_text(encoding="utf-8")))

        metrics_source = self.read("main/metrics.cpp")
        self.assertIn(
            "std::atomic<uint32_t>::is_always_lock_free", metrics_source
        )
        self.assertIn("std::atomic<uint32_t> active_writers", metrics_source)
        self.assertIn("std::atomic<uint32_t> generation", metrics_source)
        self.assertIn("std::memory_order_seq_cst", metrics_source)
        self.assertIn("vTaskDelay(1)", metrics_source)

    def test_network_workers_stop_cooperatively(self) -> None:
        worker_sources = {
            "events": self.read("main/events.cpp"),
            "prometheus": self.read("main/prometheus.cpp"),
            "supporter_crl": self.read("main/supporter_crl.cpp"),
            "syslog": self.read("main/syslog.cpp"),
        }
        external_delete = re.compile(r"vTaskDelete\s*\(\s*(?!NULL\s*\))")
        for worker, source in worker_sources.items():
            with self.subTest(worker=worker):
                self.assertIsNone(external_delete.search(source))
                self.assertIn("xSemaphoreCreateMutexStatic", source)

        events = worker_sources["events"]
        self.assertIn("std::atomic<int>          s_active_socket{-1}", events)
        self.assertIn("SO_RCVTIMEO", events)
        self.assertIn("SO_SNDTIMEO", events)
        self.assertNotIn("shutdown(active_socket, SHUT_RDWR)", events)
        self.assertIn("Do NOT tear down the active socket", events)
        self.assertIn("EVENT_HTTP_TOTAL_TIMEOUT_MS", events)
        self.assertIn("cfg.is_async = true", events)
        self.assertIn("post_event_http(", events)
        self.assertIn("esp_http_client_open", events)
        self.assertIn("esp_http_client_write", events)
        self.assertIn("esp_http_client_fetch_headers", events)
        self.assertIn("cfg.disable_auto_redirect = true", events)
        self.assertIn("return ESP_ERR_TIMEOUT", events)

        prometheus = worker_sources["prometheus"]
        self.assertIn("std::atomic<int>          s_client_sock{-1}", prometheus)
        self.assertIn("SO_RCVTIMEO", prometheus)
        self.assertIn("SO_SNDTIMEO", prometheus)
        self.assertIn("shutdown(client, SHUT_RDWR)", prometheus)
        self.assertIn("if (!body) body = (char *)malloc(RESP_CAP)", prometheus)
        self.assertEqual(prometheus.count("free(body)"), 1)
        self.assertIn("return ESP_ERR_TIMEOUT", prometheus)

        supporter_crl = worker_sources["supporter_crl"]
        self.assertIn("std::atomic<bool> s_stop_requested{false}", supporter_crl)
        self.assertIn("ulTaskNotifyTake", supporter_crl)
        self.assertIn("xTaskNotifyGive(task)", supporter_crl)
        self.assertIn("CRL_REFRESH_TOTAL_TIMEOUT_MS", supporter_crl)
        self.assertIn("config.is_async = true", supporter_crl)
        self.assertIn("crl_cancelled_or_expired", supporter_crl)
        self.assertIn("return ESP_ERR_TIMEOUT", supporter_crl)

        syslog = worker_sources["syslog"]
        self.assertIn("SYSLOG_STOP_TIMEOUT_MS = 15000", syslog)
        self.assertIn("!s_running.load(std::memory_order_acquire)", syslog)
        self.assertIn("return ESP_ERR_TIMEOUT", syslog)

        raw_uart = self.read("main/rawuartudplistener.cpp")
        self.assertIsNone(external_delete.search(raw_uart))
        self.assertIn("xSemaphoreCreateMutexStatic", raw_uart)
        self.assertIn("_stopRequested.store(true", raw_uart)
        self.assertIsNone(re.search(r"\bxTaskAbortDelay\s*\(", raw_uart))
        self.assertIn(
            "xQueueReceive(queue, &event, (TickType_t)pdMS_TO_TICKS(10))",
            raw_uart,
        )
        self.assertIn("_udp_recv(pcb, NULL, NULL)", raw_uart)
        self.assertIn("_activeSenders.load", raw_uart)
        self.assertIn("_tHandle.store(NULL", raw_uart)
        self.assertIn("return ESP_ERR_TIMEOUT", raw_uart)
        self.assertIn(
            "std::atomic<uint32_t>::is_always_lock_free", raw_uart
        )
        self.assertIn(
            "_activeSenders.fetch_add(1, std::memory_order_seq_cst)",
            raw_uart,
        )
        self.assertIn(
            "_stopRequested.store(true, std::memory_order_seq_cst)",
            raw_uart,
        )
        self.assertIn(
            "_activeSenders.load(std::memory_order_seq_cst)", raw_uart
        )
        raw_stop = raw_uart[
            raw_uart.index("esp_err_t RawUartUdpListener::stop()") :
            raw_uart.index("void RawUartUdpListener::_udpQueueHandler()")
        ]
        self.assertLess(
            raw_stop.index("xSemaphoreTake(_lifecycleMutex"),
            raw_stop.index("setFrameHandler(NULL, false)"),
        )
        self.assertLess(
            raw_stop.index("xSemaphoreTake(_lifecycleMutex"),
            raw_stop.index("TaskHandle_t task"),
        )
        start_failure = raw_uart[
            raw_uart.index("if (xTaskCreate(_raw_uart_udpQueueHandlerTask") :
            raw_uart.index("_tHandle.store(task", raw_uart.index(
                "if (xTaskCreate(_raw_uart_udpQueueHandlerTask"
            ))
        ]
        stop_admission = start_failure.index("_stopRequested.store(true")
        unregister = start_failure.index("_udp_recv(pcb, NULL, NULL)")
        drain = start_failure.index("xQueueReceive(queue, &pending, 0)")
        delete_queue = start_failure.index("vQueueDelete(queue)")
        self.assertLess(stop_admission, unregister)
        self.assertLess(unregister, drain)
        self.assertLess(drain, delete_queue)
        monitoring = self.read("main/monitoring.cpp")
        self.assertIn("esp_err_t stop_result = syslog_stop()", monitoring)
        self.assertIn("return stop_result", monitoring)
        self.assertIn("esp_err_t start_result = syslog_start(&config->syslog)", monitoring)
        self.assertIn("esp_err_t stop_result = events_stop()", monitoring)
        self.assertIn("esp_err_t stop_result = prometheus_stop()", monitoring)
        self.assertIn("rollback_bounded_config_transitions", monitoring)
        self.assertNotIn("monitoring_config_t previous_config", monitoring)
        self.assertIn("bounded_config_snapshot_t previous_config", monitoring)
        self.assertIn(
            "esp_err_t monitoring_pause_for_ota(uint32_t *paused_mask)",
            monitoring,
        )
        self.assertIn("rollback_ota_pause(paused, \"Events\"", monitoring)
        self.assertIn("rollback_ota_pause(paused, \"Syslog\"", monitoring)
        self.assertIn("rollback_ota_pause(paused, \"Prometheus\"", monitoring)

        webui = self.read("main/webui.cpp")
        self.assertIn("prepare_result = prepare_ota_heap", webui)
        self.assertIn("if (prepare_result != ESP_OK)", webui)
        self.assertIn("esp_ota_set_boot_partition(ctx->running)", webui)
        self.assertIn("monitoring_pause_for_ota(paused_monitoring)", webui)

        log_stream = self.read("main/log_stream.cpp")
        self.assertIn("STREAM_SEND_WORK_SLOTS", log_stream)
        self.assertIn("CloseSubscribersWork", log_stream)
        self.assertIn("queue_close_all_subscribers", log_stream)
        self.assertIn("httpd_queue_work", log_stream)
        self.assertIn("shutdown(work->fd, SHUT_RDWR)", log_stream)
        self.assertIn("shutdown(target.fd, SHUT_RDWR)", log_stream)
        self.assertIsNone(
            re.search(r"\bhttpd_sess_trigger_close\s*\(", log_stream)
        )
        self.assertNotIn("httpd_ws_send_data(", log_stream)
        self.assertIsNone(re.search(r"\b(?:malloc|free)\s*\(", log_stream))

        publish_worker = log_stream[
            log_stream.index("static void publish_worker") :
            log_stream.index("void log_stream_publish")
        ]
        self.assertLess(
            publish_worker.index("s_publish_overflow.exchange(false"),
            publish_worker.index("xQueueReceive(queue"),
        )
        self.assertIn("vTaskDelay(pdMS_TO_TICKS(20))", publish_worker)

    def test_diagnostics_lifecycle_storage_and_stack_units(self) -> None:
        log_manager_header = self.read("include/log_manager.h")
        log_manager_source = self.read("main/log_manager.cpp")
        self.assertIn("StaticSemaphore_t _mutex_storage", log_manager_header)
        self.assertIn(
            "xSemaphoreCreateMutexStatic(&_mutex_storage)", log_manager_source
        )
        self.assertNotIn("xSemaphoreCreateMutex()", log_manager_source)

        crash_tail = log_manager_source[
            log_manager_source.index("bool LogManager::saveCrashTailNvs") :
        ]
        self.assertIn(
            "NvsStorageLock storage_lock(pdMS_TO_TICKS(20))", crash_tail
        )
        self.assertLess(
            crash_tail.index("NvsStorageLock storage_lock"),
            crash_tail.index("nvs_open("),
        )

        sysinfo = self.read("main/sysinfo.cpp")
        task_stack = sysinfo[sysinfo.index("const char* SysInfo::getTaskStackInfo()") :]
        self.assertIn(
            "hwm_bytes = (unsigned)tasks[i].usStackHighWaterMark;", task_stack
        )
        self.assertNotIn("sizeof(StackType_t)", task_stack)

    def test_supporter_crl_cache_crypto_and_restart_are_fail_safe(self) -> None:
        source = self.read("main/supporter_crl.cpp")

        read_cache = source[
            source.index("static esp_err_t read_cache_from_namespace") :
            source.index("static esp_err_t write_cache_to_new_namespace")
        ]
        self.assertIn(
            "len == 0 || len > sizeof(s_fp) || len % CRL_FP_BYTES != 0",
            read_cache,
        )
        self.assertIn("size_t read_len = len", read_cache)
        self.assertIn(
            "nvs_get_blob(h, NVS_KEY, data, &read_len)", read_cache
        )
        self.assertLess(
            read_cache.index("len > sizeof(s_fp)"),
            read_cache.index("nvs_get_blob(h, NVS_KEY, data, &read_len)"),
        )
        self.assertIn("result != ESP_OK || read_len != len", read_cache)

        load_nvs = source[
            source.index("static void load_from_nvs()") :
            source.index("static esp_err_t save_to_nvs()")
        ]
        self.assertIn("result != ESP_ERR_NVS_NOT_FOUND", load_nvs)
        self.assertLess(
            load_nvs.index("write_cache_to_new_namespace"),
            load_nvs.index("erase_legacy_cache_best_effort"),
        )

        compute = source[
            source.index("static bool compute_fingerprint") :
            source.index("// ---- persistence ----")
        ]
        self.assertIn("memset(out, 0, CRL_FP_BYTES)", compute)
        self.assertIn("unsigned char full[32] = {}", compute)
        self.assertIn("const psa_status_t hash_status", compute)
        self.assertIn(
            "hash_status != PSA_SUCCESS || out_len != sizeof(full)", compute
        )
        self.assertNotIn("psa_crypto_init()", compute)

        init = source[
            source.index("void supporter_crl_init()") :
            source.index("void supporter_crl_start_refresh_task()")
        ]
        self.assertIn("std::atomic<bool> s_psa_ready{false}", source)
        self.assertIn("const psa_status_t init_status = psa_crypto_init()", init)
        self.assertIn("if (init_status == PSA_SUCCESS)", init)
        self.assertIn("s_psa_ready.store(true", init)
        self.assertLess(
            init.index("xSemaphoreTake(lifecycle"),
            init.index("psa_crypto_init()"),
        )

        is_revoked = source[
            source.index("bool supporter_crl_is_revoked") :
            source.index("bool supporter_crl_refresh()")
        ]
        self.assertRegex(
            is_revoked,
            r"(?s)if \(!compute_fingerprint\(norm, fp\)\) \{.*?return true;",
        )
        self.assertIn("treating supporter key as revoked", is_revoked)

        task = source[
            source.index("static void crl_refresh_task") :
            source.index("void supporter_crl_init()")
        ]
        start = source[
            source.index("void supporter_crl_start_refresh_task()") :
            source.index("esp_err_t supporter_crl_stop_refresh_task()")
        ]
        stop = source[source.index("esp_err_t supporter_crl_stop_refresh_task()") :]
        self.assertIn("std::atomic<bool> s_restart_requested{false}", source)
        self.assertIn("for (;;)", task)
        self.assertIn("s_restart_requested.exchange(false", task)
        self.assertIn("s_refresh_task.store(NULL", task)
        self.assertRegex(
            task,
            r"(?s)if \(restart\) \{\s*"
            r"s_stop_requested\.store\(false.*?\} else \{\s*"
            r"s_refresh_task\.store\(NULL",
        )
        self.assertRegex(
            start,
            r"if \(s_stop_requested\.load\(.*?\)\) \{\s*"
            r"s_restart_requested\.store\(true",
        )
        clear_restart = stop.index("s_restart_requested.store(false")
        load_task = stop.index("TaskHandle_t task")
        self.assertLess(clear_restart, load_task)

    def test_mqtt_tls_stop_and_retry_paths_remain_self_healing(self) -> None:
        for config_path in ("sdkconfig.defaults", "sdkconfig.hb-rf-eth-ng"):
            with self.subTest(config_path=config_path):
                self.assertIn(
                    "CONFIG_MQTT_TRANSPORT_SSL=y", self.read(config_path)
                )

        mqtt = self.read("main/mqtt_handler.cpp")
        connected = mqtt[
            mqtt.index("case MQTT_EVENT_CONNECTED:") :
            mqtt.index("case MQTT_EVENT_DISCONNECTED:")
        ]
        running_guard = "if (!mqtt_running.load(std::memory_order_acquire))"
        self.assertGreaterEqual(connected.count(running_guard), 2)
        connected_store = connected.index(
            "mqtt_connected.store(true, std::memory_order_release)"
        )
        self.assertLess(connected.index(running_guard), connected_store)
        self.assertLess(
            connected_store,
            connected.index(running_guard, connected_store),
        )

        component_stop = mqtt.index(
            "esp_err_t stop_result = esp_mqtt_client_stop(target)"
        )
        connected_clear = mqtt.index(
            "mqtt_connected.store(false, std::memory_order_release)",
            component_stop,
        )
        self.assertLess(component_stop, connected_clear)

        self.assertIn(
            "static std::atomic<uint32_t> mqtt_component_stop_deadline_ticks",
            mqtt,
        )
        self.assertRegex(
            mqtt,
            r"MQTT_COMPONENT_STOP_WATCHDOG_MS\s*=\s*"
            r"MQTT_RECONNECT_TIMEOUT_MS\s*/\s*2\s*\+\s*15000",
        )
        watchdog = mqtt[
            mqtt.index("static void mqtt_stop_watchdog_callback") :
            mqtt.index("static void mqtt_event_handler")
        ]
        self.assertIn("mqtt_component_stop_deadline_ticks.load", watchdog)
        self.assertIn("static_cast<int32_t>(now - deadline)", watchdog)
        self.assertIn("esp_restart()", watchdog)
        self.assertNotIn("mqtt_release_tls_gate_if_held", watchdog)
        self.assertIn(
            'xTimerCreateStatic(\n            "mqtt_stop_guard"', mqtt
        )

        tls_gate = mqtt[
            mqtt.index("static void mqtt_take_tls_gate_if_needed") :
            mqtt.index("static void mqtt_release_tls_gate_if_held")
        ]
        self.assertIn("xSemaphoreTake(g_net_fetch_mutex", tls_gate)
        self.assertIn("MQTT_TLS_GATE_MAX_WAIT_MS", tls_gate)
        self.assertIn("esp_restart()", tls_gate)
        self.assertNotIn("mqtt_running.load", tls_gate)
        self.assertNotIn("mqtt_desired_running.load", tls_gate)

        self.assertIn(
            "static std::atomic<uint32_t> mqtt_active_publishers", mqtt
        )
        operation_guard = mqtt[
            mqtt.index("class MqttPublishOperation") :
            mqtt.index("// \"Running\" only means")
        ]
        operation_enter = operation_guard.index(
            "mqtt_active_publishers.fetch_add(1, std::memory_order_seq_cst)"
        )
        operation_recheck = operation_guard.index(
            "mqtt_running.load(std::memory_order_seq_cst)", operation_enter
        )
        operation_leave = operation_guard.index(
            "mqtt_active_publishers.fetch_sub(1, std::memory_order_seq_cst)",
            operation_recheck,
        )
        self.assertLess(operation_enter, operation_recheck)
        self.assertLess(operation_recheck, operation_leave)

        publish_entry_points = (
            ("void mqtt_handler_publish_event", "void mqtt_handler_publish_task_stacks"),
            ("void mqtt_handler_publish_task_stacks", "#define PUBLISH_STR"),
            ("void mqtt_handler_publish_status", "#undef PUBLISH_STR"),
            ("void mqtt_handler_publish_ha_discovery(void)\n{", "esp_err_t mqtt_handler_init"),
        )
        for start_marker, end_marker in publish_entry_points:
            with self.subTest(publish_entry=start_marker):
                publish_body = mqtt[
                    mqtt.index(start_marker) : mqtt.index(end_marker, mqtt.index(start_marker))
                ]
                lease = publish_body.index("MqttPublishOperation operation")
                self.assertLess(
                    lease,
                    publish_body.find("current_mqtt_config")
                    if "current_mqtt_config" in publish_body
                    else len(publish_body),
                )

        publish_guard = mqtt[
            mqtt.index("static int mqtt_publish_connected") :
            mqtt.index("// Serializes mqtt_handler_start")
        ]
        publisher_enter = publish_guard.index(
            "mqtt_active_publishers.fetch_add(1, std::memory_order_seq_cst)"
        )
        lifecycle_recheck = publish_guard.index(
            "mqtt_running.load(std::memory_order_seq_cst)", publisher_enter
        )
        publish_call = publish_guard.index(
            "esp_mqtt_client_publish(", lifecycle_recheck
        )
        publisher_leave = publish_guard.index(
            "mqtt_active_publishers.fetch_sub(1, std::memory_order_seq_cst)",
            publish_call,
        )
        self.assertLess(publisher_enter, lifecycle_recheck)
        self.assertLess(lifecycle_recheck, publish_call)
        self.assertLess(publish_call, publisher_leave)

        cleanup = mqtt[
            mqtt.index("static void mqtt_cleanup_task(void *parameter)") :
            mqtt.index("esp_err_t mqtt_handler_start")
        ]
        active_drain = cleanup.index("mqtt_active_publishers.load")
        client_destroy = cleanup.index("esp_mqtt_client_destroy(target)")
        self.assertLess(active_drain, client_destroy)
        self.assertIn("MQTT_PUBLISH_DRAIN_TIMEOUT_MS", cleanup)
        self.assertIn("std::memory_order_seq_cst", cleanup)

        monitoring = self.read("main/monitoring.cpp")
        self.assertIn(
            "static StaticTimer_t mqtt_retry_guard_timer_buffer", monitoring
        )
        retry_guard = monitoring[
            monitoring.index("static void mqtt_retry_guard_timer_callback") :
            monitoring.index("static void mqtt_retry_task")
        ]
        self.assertIn("mqtt_start_deferred.load", retry_guard)
        self.assertIn("!mqtt_retry_task_running.load", retry_guard)
        self.assertIn("schedule_mqtt_retry()", retry_guard)
        self.assertIn(
            'xTimerCreateStatic(\n            "mqtt_retry_guard"', monitoring
        )

    def test_nvs_transactions_and_restore_report_failures(self) -> None:
        writer_files = (
            "main/log_manager.cpp",
            "main/monitoring.cpp",
            "main/mqtt_handler.cpp",
            "main/reset_info.cpp",
            "main/settings.cpp",
            "main/supporter_crl.cpp",
            "main/theme_api.cpp",
            "main/webui.cpp",
            "main/webui_storage.cpp",
        )
        nvs_write = re.compile(r"nvs_(?:set_|erase_|commit|flash_erase)")
        for relative_path in writer_files:
            with self.subTest(relative_path=relative_path):
                source = self.read(relative_path)
                self.assertRegex(source, nvs_write)
                self.assertIn("NvsStorageLock", source)

        lock_header = self.read("include/nvs_storage_lock.h")
        lock_source = self.read("main/nvs_storage_lock.cpp")
        self.assertIn("xSemaphoreCreateRecursiveMutexStatic", lock_source)
        self.assertIn("xSemaphoreTakeRecursive", lock_source)
        self.assertIn("void release()", lock_header)

        monitoring = self.read("main/monitoring.cpp")
        self.assertIn('#define NVS_TXN_NAMESPACE "monitoring_txn"', monitoring)
        self.assertIn("config_nvs_entry_requirement", monitoring)
        self.assertIn("verify_config_nvs_capacity", monitoring)
        self.assertIn("return nvs_set_blob(handle, field.key", monitoring)
        restore_save = monitoring[
            monitoring.index("esp_err_t monitoring_save_config_for_restore") :
            monitoring.index("static esp_err_t tcp_probe_endpoint")
        ]
        self.assertNotIn("memcpy(&current_config", restore_save)

        settings_header = self.read("include/settings.h")
        settings = self.read("main/settings.cpp")
        self.assertIn("esp_err_t save();", settings_header)
        self.assertIn("esp_err_t clear();", settings_header)
        self.assertIn("esp_err_t validateStorageCapacity();", settings_header)
        self.assertIn("static esp_err_t beginRestoreTransaction();", settings_header)
        self.assertIn("static esp_err_t finishRestoreTransaction();", settings_header)
        settings_save = settings[
            settings.index("esp_err_t Settings::save()") :
            settings.index("void Settings::snapshot")
        ]
        self.assertIn("SAVE_STEP(nvs_commit(handle))", settings_save)
        self.assertIn("return err", settings_save)

        webui = self.read("main/webui.cpp")
        restore = webui[
            webui.index("esp_err_t post_restore_handler_func") :
            webui.index("httpd_uri_t post_restore_handler")
        ]
        self.assertIn("NvsStorageLock restore_storage", restore)
        self.assertIn("const esp_err_t settings_save_result", restore)
        self.assertIn("_settings->restoreSnapshot", restore)
        settings_capacity = restore.index(
            "const esp_err_t settings_capacity_result"
        )
        restore_begin = restore.index("Settings::beginRestoreTransaction()")
        settings_write = restore.index("const esp_err_t settings_save_result")
        self.assertLess(settings_capacity, restore_begin)
        self.assertLess(restore_begin, settings_write)
        self.assertEqual(
            restore.count("monitoring_validate_config_storage("), 2
        )
        final_capacity = restore.index(
            "const esp_err_t final_monitoring_capacity"
        )
        monitoring_save = restore.index(
            "monitoring_save_config_for_restore(restored_monitoring.get())",
            final_capacity,
        )
        final_preflight = restore[final_capacity:monitoring_save]
        self.assertIn("monitoring_validate_config_storage", final_preflight)
        self.assertIn("_settings->restoreSnapshot", final_preflight)
        self.assertIn("theme_api_set_config", final_preflight)
        self.assertIn("ESP_ERR_NVS_NOT_ENOUGH_SPACE", final_preflight)
        self.assertIn('"507 Insufficient Storage"', final_preflight)
        self.assertIn('"restore_monitoring_capacity"', final_preflight)
        restore_finish = restore.index(
            "const esp_err_t restore_finish_result", monitoring_save
        )
        self.assertLess(monitoring_save, restore_finish)

        rollback_finisher = restore[
            restore.index("auto finish_rollback_transaction") : settings_write
        ]
        rollback_error_guard = rollback_finisher.index(
            "settings_rollback != ESP_OK"
        )
        rollback_marker_finish = rollback_finisher.index(
            "Settings::finishRestoreTransaction()"
        )
        self.assertLess(rollback_error_guard, rollback_marker_finish)
        self.assertIn("return false", rollback_finisher[
            rollback_error_guard:rollback_marker_finish
        ])
        self.assertLess(
            restore.index("settings_save_result"),
            restore.index(r'{\"success\":true}'),
        )

        factory_reset = webui[
            webui.index("esp_err_t post_factory_reset_handler_func") :
            webui.index("httpd_uri_t post_factory_reset_handler")
        ]
        clear_call = factory_reset.index(
            "const esp_err_t clear_result = _settings->clear()"
        )
        clear_failure = factory_reset.index("if (clear_result != ESP_OK)")
        reset_reason = factory_reset.index(
            "ResetInfo::storeResetReason(RESET_REASON_FACTORY_RESET)"
        )
        success_response = factory_reset.index(r'{\"success\":true}')
        restart = factory_reset.index(
            "full_system_restart_with_reserved_operation()"
        )
        self.assertLess(clear_call, clear_failure)
        self.assertLess(clear_failure, reset_reason)
        self.assertLess(reset_reason, success_response)
        self.assertLess(success_response, restart)
        failure_branch = factory_reset[clear_failure:reset_reason]
        self.assertIn("send_json_error", failure_branch)
        self.assertIn('"factory_reset_failed"', failure_branch)
        self.assertNotIn("RESET_REASON_FACTORY_RESET", failure_branch)
        self.assertNotIn("full_system_restart", failure_branch)

    def test_manual_upload_keeps_exclusive_restart_reservation(self) -> None:
        system_reset = self.read("main/system_reset.cpp")
        restart_entry = system_reset[
            system_reset.index("static void full_system_restart_impl") :
            system_reset.index("    // Reserve the same operation gate")
        ]
        self.assertIn("compare_exchange_weak", restart_entry)
        self.assertIn("vTaskDelay(pdMS_TO_TICKS(100))", restart_entry)
        self.assertNotIn("g_restart_in_progress.exchange", restart_entry)

        webui = self.read("main/webui.cpp")
        upload = webui[
            webui.index("esp_err_t post_ota_update_handler_func") :
            webui.index("httpd_uri_t post_ota_update_handler")
        ]
        self.assertIn("OTA_UPLOAD_TOTAL_TIMEOUT_US", upload)
        self.assertIn("OTA_UPLOAD_NO_PROGRESS_TIMEOUT_US", upload)

        # The finalize logic (prepare → boot-select → restart) runs on a
        # background task so httpd stays responsive during worker shutdown.
        finalize = webui[
            webui.index("static void ota_finalize_task") :
            webui.index("esp_err_t post_change_password_handler_func")
        ]
        self.assertIn("full_system_restart_with_reserved_operation()", finalize)
        uncertain = finalize[
            finalize.index("Boot selection uncertain") :
            finalize.index("OTA finished successfully")
        ]
        self.assertIn("full_system_restart_with_reserved_operation()", uncertain)
        self.assertNotIn("full_system_restart();", uncertain)
        rollback = uncertain[
            uncertain.index("if (running_restored)") :
            uncertain.index("vTaskDelay(pdMS_TO_TICKS(1000))")
        ]
        self.assertLess(
            rollback.index("resume_tasks_after_ota_failure()"),
            rollback.index("ota_operation_finish()"),
        )
        prepare_failure = finalize[
            finalize.index("if (prepare_result != ESP_OK)") :
            finalize.index("esp_err_t boot_result")
        ]
        self.assertLess(
            prepare_failure.index("monitoring_resume_after_ota"),
            prepare_failure.index("ota_operation_finish()"),
        )
        self.assertLess(
            prepare_failure.index("resume_tasks_after_ota_failure()"),
            prepare_failure.index("ota_operation_finish()"),
        )


if __name__ == "__main__":
    unittest.main()
