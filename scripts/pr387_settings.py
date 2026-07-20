from pathlib import Path
import re


def _read(path):
    return Path(path).read_text(encoding='utf-8')


def _write(path, text):
    Path(path).write_text(text, encoding='utf-8')


def apply():
    path = 'webui/src/settings.vue'
    text = _read(path)
    start = text.find('              <div class="experimental-warning">')
    end_marker = '            </div>\n          </div>\n        </div>\n      </Transition>'
    if start >= 0:
        end = text.find(end_marker, start)
        if end < 0:
            raise RuntimeError('settings.vue: experimental section end not found')
        replacement = '''              <div class="experimental-empty-state">
                <AppIcon name="check" />
                <div>
                  <strong>{{ t('settings.experimentalEmptyTitle') }}</strong>
                  <p>{{ t('settings.experimentalEmptyText') }}</p>
                </div>
              </div>
'''
        text = text[:start] + replacement + text[end:]
    text = text.replace(
        "import { useExperimentalStore, useSettingsStore, useLoginStore, useUiStore, useSysInfoStore, useRestartUiStore } from './stores.js'",
        "import { useSettingsStore, useLoginStore, useUiStore, useSysInfoStore, useRestartUiStore } from './stores.js'",
    )
    text = text.replace("const experimentalStore = useExperimentalStore()\n", '')
    text = text.replace("const flashPause = ref(false)\n", '')
    text = re.sub(r"\n\s*flashPause:\s*flashPause\.value,?", '', text)
    text = text.replace("  flashPause.value = settingsStore.flashPause\n", '')
    text = text.replace('v-if="settingsStore.flashPause"', 'v-if="true"')
    text = text.replace('includeFlashPause: settingsStore.flashPause', 'includeFlashPause: true')
    _write(path, text)

    path = 'webui/src/stores.js'
    text = _read(path)
    text = re.sub(
        r"export const useExperimentalStore = defineStore\('experimental', \{.*?\n\}\)\n\nexport const useLoginStore",
        """export const useExperimentalStore = defineStore('experimental', {
  state: () => ({ testDesignEnabled: true }),
  actions: {
    applyDesignClass() { document.body.classList.add('newdesign-active') },
    setTestDesignEnabled() { this.testDesignEnabled = true; this.applyDesignClass() },
    syncFromServer() { this.testDesignEnabled = true; this.applyDesignClass() },
    init() { this.applyDesignClass() }
  }
})

export const useLoginStore""",
        text,
        count=1,
        flags=re.S,
    )
    _write(path, text)

    path = 'main/settings.cpp'
    text = _read(path)
    text = text.replace(
        '  GET_BOOL(handle, "flashPause", _flashPause, false);\n  GET_BOOL(handle, "testDesign", _testDesignEnabled, false);',
        '  // Fixed defaults for all devices; legacy false values are ignored.\n  _flashPause = true;\n  _testDesignEnabled = true;',
    )
    text = text.replace(
        '  SET_BOOL(handle, "flashPause", _flashPause);\n  SET_BOOL(handle, "testDesign", _testDesignEnabled);',
        '  SET_BOOL(handle, "flashPause", true);\n  SET_BOOL(handle, "testDesign", true);',
    )
    text = re.sub(r"bool Settings::getFlashPause\(\)\s*\{.*?\n\}", "bool Settings::getFlashPause()\n{\n  return true;\n}", text, count=1, flags=re.S)
    text = re.sub(r"void Settings::setFlashPause\(bool enabled\)\s*\{.*?\n\}", "void Settings::setFlashPause(bool enabled)\n{\n  (void)enabled;\n  _flashPause = true;\n}", text, count=1, flags=re.S)
    text = re.sub(r"bool Settings::getTestDesignEnabled\(\)\s*\{.*?\n\}", "bool Settings::getTestDesignEnabled()\n{\n  return true;\n}", text, count=1, flags=re.S)
    text = re.sub(r"void Settings::setTestDesignEnabled\(bool enabled\)\s*\{.*?\n\}", "void Settings::setTestDesignEnabled(bool enabled)\n{\n  (void)enabled;\n  _testDesignEnabled = true;\n}", text, count=1, flags=re.S)
    _write(path, text)

    path = 'webui/src/locales/de.js'
    text = _read(path)
    anchor = "    experimentalWarningText: 'Diese Funktionen sind zum Testen gedacht. Es gibt keine Garantie auf Funktion oder Darstellung.',"
    if 'experimentalEmptyTitle' not in text:
        if anchor not in text:
            raise RuntimeError('de.js: experimental translation anchor missing')
        text = text.replace(
            anchor,
            anchor + "\n    experimentalEmptyTitle: 'Derzeit nichts vorhanden',\n    experimentalEmptyText: 'Aktuell sind keine experimentellen Funktionen verfügbar. Das New Design und der Neustart-Sync sind feste Standards für alle Geräte.',",
            1,
        )
    _write(path, text)
