from pathlib import Path

path = Path('main/mqtt_handler.cpp')
text = path.read_text(encoding='utf-8')
start = text.find('static void handle_mqtt_command(')
end = text.find('static void mqtt_event_handler(', start)
if start < 0 or end < 0:
    raise RuntimeError('MQTT command-handler boundaries not found')

replacement = r'''static void handle_mqtt_command(const char* command, const char* payload, int payload_len)
{
    ESP_LOGI(TAG, "Received MQTT command: %s (payload %d bytes)", command, payload_len);

    if (!command_token_ok(payload, payload_len)) {
        ESP_LOGW(TAG, "Command %s rejected: missing/invalid token", command);
        char msg[96];
        snprintf(msg, sizeof(msg), "rejected cmd=%.48s reason=invalid_token", command);
        mqtt_handler_publish_event("event/command_rejected", msg);
        return;
    }

    if (strcmp(command, "restart") == 0) {
        ESP_LOGI(TAG, "Restart command received via MQTT");
        ResetInfo::storeResetReason(RESET_REASON_USER_RESTART);
        mqtt_handler_publish_event("event/restart", "requested");
        vTaskDelay(pdMS_TO_TICKS(300));
        full_system_restart();
    } else if (strcmp(command, "factory_reset") == 0) {
        ESP_LOGI(TAG, "Factory reset command received via MQTT");
        mqtt_handler_publish_event("event/factory_reset", "requested");
        vTaskDelay(pdMS_TO_TICKS(300));
        perform_factory_reset();
        full_system_restart();
    } else {
        ESP_LOGW(TAG, "Unknown MQTT command: %s", command);
        mqtt_handler_publish_event("event/command_rejected", "reason=unknown_command");
    }
}

'''

path.write_text(text[:start] + replacement + text[end:], encoding='utf-8')
print('Cleaned MQTT command handler; OTA install command removed.')
