from pathlib import Path
import re


def apply():
    path = Path('main/mqtt_handler.cpp')
    text = path.read_text(encoding='utf-8')

    if '#include "webui_storage.h"' not in text:
        text = text.replace('#include "updatecheck.h"', '#include "updatecheck.h"\n#include "webui_storage.h"', 1)

    text = re.sub(r"static constexpr uint32_t MQTT_UPDATE_TASK_STACK_BYTES = \d+;\n\n", '', text)
    text = re.sub(
        r"\s*\} else if \(strcmp\(command, \"update\"\) == 0\) \{.*?\n\s*\} else \{",
        '\n    } else {',
        text,
        count=1,
        flags=re.S,
    )
    text = re.sub(r"\s*const char\* update_payload\s*=.*?;\n", '', text, count=1)

    old_identity = '''    PUBLISH_STR("status/serial", sysInfo->getSerialNumber());
    PUBLISH_STR("status/version", sysInfo->getCurrentVersion());
    PUBLISH_STR("status/board_revision", sysInfo->getBoardRevisionString());
'''
    new_identity = '''    PUBLISH_STR("status/serial", sysInfo->getSerialNumber());
    PUBLISH_STR("status/version", sysInfo->getCurrentVersion());
    PUBLISH_STR("status/firmware_version", sysInfo->getCurrentVersion());
    char webuiVersion[32] = {};
    webui_storage_get_effective_version(webuiVersion, sizeof(webuiVersion));
    PUBLISH_STR("status/webui_version", webuiVersion);
    PUBLISH_STR("status/board_revision", sysInfo->getBoardRevisionString());
'''
    if old_identity not in text:
        raise RuntimeError('mqtt_handler.cpp: identity status block not found')
    text = text.replace(old_identity, new_identity, 1)

    old_update = '''    // ---- Update info ------------------------------------------------------
    if (updateCheck) {
        VersionSnapshot rel = updateCheck->getVersionSnapshot();
        const char* currentVersion = sysInfo->getCurrentVersion();
        // Always publish a valid version string on latest_version so the HA
        // MQTT update entity can compare it against installed_version. If no
        // newer release is known, fall back to the current version so the
        // entity state stays "up to date" instead of "unknown".
        const char* latest = rel.valid ? rel.version : currentVersion;
        if (rel.valid && compareVersions(latest, currentVersion) <= 0) {
            latest = currentVersion;
        }
        PUBLISH_STR("status/latest_version", latest);
        bool updateAvailable = rel.valid &&
            compareVersions(rel.version, currentVersion) > 0;
        PUBLISH_STR("status/update_available", updateAvailable ? "true" : "false");
    }
'''
    new_update = '''    // ---- Informational update status -------------------------------------
    if (updateCheck) {
        VersionSnapshot rel = updateCheck->getVersionSnapshot();
        const char* currentVersion = sysInfo->getCurrentVersion();
        const char* latestFirmware = rel.valid ? rel.version : currentVersion;
        if (rel.valid && compareVersions(latestFirmware, currentVersion) <= 0) {
            latestFirmware = currentVersion;
        }
        const bool firmwareUpdateAvailable = rel.valid &&
            compareVersions(rel.version, currentVersion) > 0;
        const char* latestWebui = rel.webuiValid ? rel.webuiVersion : webuiVersion;
        const bool webuiUpdateAvailable = rel.webuiValid &&
            compareVersions(rel.webuiVersion, webuiVersion) > 0;

        PUBLISH_STR("status/latest_version", latestFirmware);
        PUBLISH_STR("status/update_available", firmwareUpdateAvailable ? "true" : "false");
        PUBLISH_STR("status/latest_firmware_version", latestFirmware);
        PUBLISH_STR("status/latest_webui_version", latestWebui);
        PUBLISH_STR("status/firmware_update_available", firmwareUpdateAvailable ? "true" : "false");
        PUBLISH_STR("status/webui_update_available", webuiUpdateAvailable ? "true" : "false");
    }
'''
    if old_update not in text:
        raise RuntimeError('mqtt_handler.cpp: update status block not found')
    text = text.replace(old_update, new_update, 1)

    old_sensors = '''    publish_config("sensor", "version", "Current Version", NULL, NULL, NULL, NULL, "diagnostic", "mdi:package-variant");
    publish_config("sensor", "latest_version", "Latest Version", NULL, NULL, NULL, NULL, "diagnostic", "mdi:package-up");
'''
    new_sensors = '''    publish_config("sensor", "version", "Firmware Version", NULL, NULL, NULL, NULL, "diagnostic", "mdi:package-variant");
    publish_config("sensor", "firmware_version", "Firmware Version", NULL, NULL, NULL, NULL, "diagnostic", "mdi:package-variant");
    publish_config("sensor", "webui_version", "WebUI Version", NULL, NULL, NULL, NULL, "diagnostic", "mdi:web");
    publish_config("sensor", "latest_version", "Latest Firmware Version", NULL, NULL, NULL, NULL, "diagnostic", "mdi:package-up");
    publish_config("sensor", "latest_firmware_version", "Latest Firmware Version", NULL, NULL, NULL, NULL, "diagnostic", "mdi:package-up");
    publish_config("sensor", "latest_webui_version", "Latest WebUI Version", NULL, NULL, NULL, NULL, "diagnostic", "mdi:web-sync");
    publish_config("binary_sensor", "firmware_update_available", "Firmware Update Available", "update", NULL, NULL, NULL, "diagnostic", "mdi:package-up", "true", "false");
    publish_config("binary_sensor", "webui_update_available", "WebUI Update Available", "update", NULL, NULL, NULL, "diagnostic", "mdi:web-sync", "true", "false");
'''
    if old_sensors in text:
        text = text.replace(old_sensors, new_sensors, 1)

    text = text.replace(
        '    publish_config("sensor", "ota_progress", "OTA Progress", NULL, "measurement", "%", NULL, "diagnostic", "mdi:progress-download");',
        '    remove_config("sensor", "ota_progress");',
        1,
    )

    install_start = text.find('    // ---- HA Update entity')
    install_end = text.find('    cJSON_Delete(device);', install_start)
    if install_start >= 0 and install_end > install_start:
        text = text[:install_start] + '    // Remove the former install-capable update entity.\n    remove_config("update", "firmware_update");\n\n' + text[install_end:]

    path.write_text(text, encoding='utf-8')
