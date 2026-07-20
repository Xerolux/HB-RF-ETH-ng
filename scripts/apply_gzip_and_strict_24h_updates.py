from pathlib import Path
import json
import re

ROOT = Path('.')


def read(path):
    return (ROOT / path).read_text(encoding='utf-8')


def write(path, text):
    (ROOT / path).write_text(text, encoding='utf-8')


def replace_once(text, old, new, path):
    if old not in text:
        if new in text:
            return text
        raise SystemExit(f'Expected text not found in {path}: {old[:120]!r}')
    return text.replace(old, new, 1)

# ---------------------------------------------------------------------------
# Lightweight Bootstrap compatibility components. This removes the large
# bootstrap-vue-next runtime while keeping the existing templates intact.
# Bootstrap CSS remains the common styling base.
# ---------------------------------------------------------------------------
bootstrap_lite = r'''import { Teleport, defineComponent, h, onBeforeUnmount, watch } from 'vue'

const classList = (...values) => values.flat().filter(Boolean)

export const BButton = defineComponent({
  name: 'BButton', inheritAttrs: false,
  props: { variant: { default: 'secondary' }, size: String, block: Boolean, disabled: Boolean, type: { default: 'button' } },
  emits: ['click'],
  setup(props, { attrs, slots, emit }) {
    return () => h('button', {
      ...attrs,
      type: props.type,
      disabled: props.disabled,
      class: classList('btn', `btn-${props.variant}`, props.size ? `btn-${props.size}` : '', props.block ? 'w-100' : '', attrs.class),
      onClick: event => emit('click', event)
    }, slots.default?.())
  }
})

export const BAlert = defineComponent({
  name: 'BAlert', inheritAttrs: false,
  props: { variant: { default: 'info' }, modelValue: { default: undefined }, show: { default: true }, dismissible: Boolean, fade: Boolean },
  emits: ['update:modelValue'],
  setup(props, { attrs, slots, emit }) {
    return () => {
      const visible = props.modelValue === undefined ? props.show !== false : props.modelValue !== false
      if (!visible) return null
      return h('div', { ...attrs, role: 'alert', class: classList('alert', `alert-${props.variant}`, 'show', attrs.class) }, [
        slots.default?.(),
        props.dismissible ? h('button', {
          type: 'button', class: 'btn-close', 'aria-label': 'Schließen',
          onClick: () => emit('update:modelValue', false)
        }) : null
      ])
    }
  }
})

export const BCard = defineComponent({
  name: 'BCard', inheritAttrs: false,
  setup(_props, { attrs, slots }) {
    return () => h('div', { ...attrs, class: classList('card', attrs.class) }, [
      slots.header ? h('div', { class: 'card-header' }, slots.header()) : null,
      h('div', { class: 'card-body' }, slots.default?.())
    ])
  }
})

export const BForm = defineComponent({
  name: 'BForm', inheritAttrs: false, emits: ['submit'],
  setup(_props, { attrs, slots, emit }) {
    return () => h('form', { ...attrs, onSubmit: event => emit('submit', event) }, slots.default?.())
  }
})

export const BFormGroup = defineComponent({
  name: 'BFormGroup', inheritAttrs: false,
  props: { label: String, labelFor: String },
  setup(props, { attrs, slots }) {
    return () => h('div', { ...attrs, class: classList('mb-3', attrs.class) }, [
      props.label ? h('label', { class: 'form-label', for: props.labelFor }, props.label) : null,
      slots.default?.()
    ])
  }
})

const normalizeValue = (value, modifiers = {}) => {
  let next = value
  if (modifiers.trim && typeof next === 'string') next = next.trim()
  if (modifiers.number && next !== '') {
    const number = Number(next)
    if (!Number.isNaN(number)) next = number
  }
  return next
}

export const BFormInput = defineComponent({
  name: 'BFormInput', inheritAttrs: false,
  props: {
    modelValue: [String, Number], modelModifiers: { default: () => ({}) },
    state: { default: null }, size: String, type: { default: 'text' }, disabled: Boolean
  },
  emits: ['update:modelValue', 'input', 'change', 'blur', 'keyup'],
  setup(props, { attrs, emit }) {
    return () => h('input', {
      ...attrs,
      value: props.modelValue ?? '', type: props.type, disabled: props.disabled,
      class: classList('form-control', props.size ? `form-control-${props.size}` : '', props.state === true ? 'is-valid' : '', props.state === false ? 'is-invalid' : '', attrs.class),
      onInput: event => {
        const value = normalizeValue(event.target.value, props.modelModifiers)
        emit('update:modelValue', value); emit('input', event)
      },
      onChange: event => emit('change', event),
      onBlur: event => emit('blur', event),
      onKeyup: event => emit('keyup', event)
    })
  }
})

export const BFormInvalidFeedback = defineComponent({
  name: 'BFormInvalidFeedback', inheritAttrs: false,
  setup(_props, { attrs, slots }) {
    return () => h('div', { ...attrs, class: classList('invalid-feedback', 'd-block', attrs.class) }, slots.default?.())
  }
})

export const BFormSelect = defineComponent({
  name: 'BFormSelect', inheritAttrs: false,
  props: { modelValue: [String, Number, Boolean], modelModifiers: { default: () => ({}) }, options: { type: Array, default: () => [] }, size: String, disabled: Boolean },
  emits: ['update:modelValue', 'change'],
  setup(props, { attrs, slots, emit }) {
    const optionNode = option => {
      if (option && typeof option === 'object') {
        const value = option.value ?? option.id ?? option.text
        return h('option', { value, disabled: !!option.disabled }, option.text ?? option.label ?? String(value ?? ''))
      }
      return h('option', { value: option }, String(option ?? ''))
    }
    return () => h('select', {
      ...attrs, value: props.modelValue, disabled: props.disabled,
      class: classList('form-select', props.size ? `form-select-${props.size}` : '', attrs.class),
      onChange: event => {
        const value = normalizeValue(event.target.value, props.modelModifiers)
        emit('update:modelValue', value); emit('change', event)
      }
    }, [...props.options.map(optionNode), ...(slots.default?.() || [])])
  }
})

export const BFormSelectOption = defineComponent({
  name: 'BFormSelectOption', inheritAttrs: false,
  props: { value: [String, Number, Boolean], disabled: Boolean },
  setup(props, { attrs, slots }) {
    return () => h('option', { ...attrs, value: props.value, disabled: props.disabled }, slots.default?.())
  }
})

export const BModal = defineComponent({
  name: 'BModal', inheritAttrs: false,
  props: {
    modelValue: Boolean, title: String, size: String, centered: Boolean, scrollable: Boolean,
    hideHeader: Boolean, hideFooter: Boolean, contentClass: [String, Array, Object],
    headerClass: [String, Array, Object], bodyClass: [String, Array, Object],
    footerClass: [String, Array, Object], titleClass: [String, Array, Object]
  },
  emits: ['update:modelValue', 'hide', 'shown'],
  setup(props, { attrs, slots, emit }) {
    let locked = false
    const lockBody = value => {
      if (value && !locked) { document.body.classList.add('modal-open'); document.body.style.overflow = 'hidden'; locked = true }
      if (!value && locked) { document.body.classList.remove('modal-open'); document.body.style.overflow = ''; locked = false }
    }
    watch(() => props.modelValue, value => { lockBody(value); if (value) queueMicrotask(() => emit('shown')) }, { immediate: true })
    onBeforeUnmount(() => lockBody(false))
    const requestClose = () => {
      const event = { defaultPrevented: false, preventDefault() { this.defaultPrevented = true } }
      emit('hide', event)
      if (!event.defaultPrevented) emit('update:modelValue', false)
    }
    return () => {
      if (!props.modelValue) return null
      const dialogClass = classList('modal-dialog', props.size ? `modal-${props.size}` : '', props.centered ? 'modal-dialog-centered' : '', props.scrollable ? 'modal-dialog-scrollable' : '')
      const modal = h('div', {
        ...attrs, class: classList('modal', 'fade', 'show', attrs.class), tabindex: '-1', role: 'dialog', style: { display: 'block' },
        onClick: event => { if (event.target === event.currentTarget) requestClose() }
      }, [h('div', { class: dialogClass }, [h('div', { class: classList('modal-content', props.contentClass) }, [
        props.hideHeader ? null : h('div', { class: classList('modal-header', props.headerClass) }, [
          h('h5', { class: classList('modal-title', props.titleClass) }, slots.title?.() || props.title || ''),
          h('button', { type: 'button', class: 'btn-close', 'aria-label': 'Schließen', onClick: requestClose })
        ]),
        h('div', { class: classList('modal-body', props.bodyClass) }, slots.default?.()),
        props.hideFooter ? null : h('div', { class: classList('modal-footer', props.footerClass) }, slots.footer?.() || [])
      ])])])
      return h(Teleport, { to: 'body' }, [modal, h('div', { class: 'modal-backdrop fade show' })])
    }
  }
})
'''
component_path = ROOT / 'webui/src/components/BootstrapLite.js'
component_path.write_text(bootstrap_lite, encoding='utf-8')

# main.js: remove bootstrap-vue-next runtime/CSS and register light components.
path = 'webui/src/main.js'
text = read(path)
text = text.replace("import 'bootstrap-vue-next/dist/bootstrap-vue-next.css'\n", '')
pattern = re.compile(r"// Create Bootstrap Vue Next\nimport \{ createBootstrap \} from 'bootstrap-vue-next'\nimport \{[\s\S]*?\} from 'bootstrap-vue-next'\n", re.M)
replacement = "// Lightweight Bootstrap-compatible Vue components\nimport {\n  BAlert, BButton, BCard, BForm, BFormGroup, BFormInput,\n  BFormInvalidFeedback, BFormSelect, BFormSelectOption, BModal\n} from './components/BootstrapLite.js'\n"
if pattern.search(text):
    text = pattern.sub(replacement, text, count=1)
elif "from './components/BootstrapLite.js'" not in text:
    raise SystemExit('bootstrap-vue-next import block not found')
text = re.sub(r"\napp\.use\(createBootstrap\(\{[\s\S]*?\}\)\)\n", "\n", text, count=1)
if "app.component('BFormSelectOption'" not in text:
    text = text.replace("app.component('BFormSelect', BFormSelect)\n", "app.component('BFormSelect', BFormSelect)\napp.component('BFormSelectOption', BFormSelectOption)\n")
write(path, text)

# package.json: remove heavy runtime and no-longer-needed Brotli build plugin.
path = 'webui/package.json'
package = json.loads(read(path))
package.get('dependencies', {}).pop('bootstrap-vue-next', None)
package.get('devDependencies', {}).pop('vite-plugin-compression2', None)
write(path, json.dumps(package, indent=2, ensure_ascii=False) + '\n')

# Vite only emits raw assets. rename_webui_files.py creates gzip deterministically.
path = 'webui/vite.config.js'
text = read(path)
text = text.replace("import { compression } from 'vite-plugin-compression2'\n", '')
text = text.replace("    cacheBustingPlugin(),\n    compression({ algorithm: 'brotliCompress', exclude: [/\\.(br)$/, /\\.(gz)$/], threshold: 0 })\n", "    cacheBustingPlugin()\n")
write(path, text)

# Standalone image contains gzip only.
path = 'rename_webui_files.py'
text = read(path)
text = text.replace('"main.js.br",', '"main.js.gz",')
text = text.replace('"main.css.br",', '"main.css.gz",')
text = text.replace('"main.js": "br"', '"main.js": "gzip"')
text = text.replace('"main.css": "br"', '"main.css": "gzip"')
write(path, text)

# Firmware storage validator/asset serving: gzip only, no Brotli negotiation.
path = 'main/webui_storage.cpp'
text = read(path)
text = text.replace('strcmp(js_encoding->valuestring, "br") == 0', 'strcmp(js_encoding->valuestring, "gzip") == 0')
text = text.replace('strcmp(css_encoding->valuestring, "br") == 0', 'strcmp(css_encoding->valuestring, "gzip") == 0')
text = text.replace('BASE_PATH "/main.js.br", "application/javascript", "br"', 'BASE_PATH "/main.js.gz", "application/javascript", "gzip"')
text = text.replace('BASE_PATH "/main.css.br", "text/css", "br"', 'BASE_PATH "/main.css.gz", "text/css", "gzip"')
text = text.replace('BASE_PATH "/main.js.br"', 'BASE_PATH "/main.js.gz"')
text = text.replace('BASE_PATH "/main.css.br"', 'BASE_PATH "/main.css.gz"')
text = text.replace('// Only the New Design is accepted. Brotli keeps the complete single-file\n    // Vue application inside the existing 320 KiB partition.', '// Only the New Design is accepted. All standalone assets use gzip.')
text = text.replace('// Only the New Design is accepted. Gzip works reliably on the device\n    // private-LAN HTTP origin.', '// Only the New Design is accepted. All standalone assets use gzip.')
text = re.sub(r"\nbool request_accepts_encoding\(httpd_req_t \*req, const char \*encoding\)[\s\S]*?\n}\n\nesp_err_t stream_external_file", "\nesp_err_t stream_external_file", text, count=1)
wrapped = re.compile(r"esp_err_t wrapped_asset_handler\(httpd_req_t \*req\)\n\{[\s\S]*?\n\}\n\nesp_err_t wrapped_firmware_update_handler", re.M)
wrapped_replacement = '''esp_err_t wrapped_asset_handler(httpd_req_t *req)
{
    auto *route = static_cast<WrappedRoute *>(req->user_ctx);
    if (!route || !route->original_handler) return ESP_FAIL;

    if (webui_storage_is_valid())
    {
        const esp_err_t result = stream_external_file(req, route->asset);
        if (result != ESP_ERR_NOT_FOUND) return result;

        // Optional assets intentionally remain embedded.
        ESP_LOGD(TAG, "Using embedded fallback asset for %s",
                 route->asset ? route->asset->uri : "unknown");
    }

    return route->original_handler(req);
}

esp_err_t wrapped_firmware_update_handler'''
if wrapped.search(text):
    text = wrapped.sub(wrapped_replacement, text, count=1)
else:
    raise SystemExit('wrapped_asset_handler block not found')
text = text.replace('set_error_locked("WWW image size does not match SPIFFS partition");', 'set_error_locked("Falsche Datei: erwartet wird ein 327680-Byte-WebUI-Abbild; Firmware unter System -> Firmware installieren");')
write(path, text)

# Clear frontend separation and errors.
path = 'webui/src/firmwareupdate.vue'
text = read(path)
text = text.replace("title: 'WebUI-Datei erkannt',\n      message: 'Diese Datei unter System → WebUI installieren, nicht als Firmware.'", "title: t('firmware.webuiFileDetectedTitle'),\n      message: t('firmware.webuiFileDetectedMessage')")
write(path, text)

path = 'webui/src/webuiupdate.vue'
text = read(path)
text = text.replace("    invalidFile: 'Die Datei muss exakt zur WWW-Partition passen.', failed: 'WebUI-Update fehlgeschlagen.'", "    invalidFile: 'Die Datei muss exakt zur WWW-Partition passen.', wrongFirmwareFile: 'Falsche Datei: Das ist eine Firmware-Datei. Bitte unter System → Firmware installieren.', failed: 'WebUI-Update fehlgeschlagen.'")
text = text.replace("    invalidFile: 'The file must exactly match the WWW partition.', failed: 'WebUI update failed.'", "    invalidFile: 'The file must exactly match the WWW partition.', wrongFirmwareFile: 'Wrong file: this is a firmware image. Install it under System → Firmware.', failed: 'WebUI update failed.'")
old_select = """const selectFile = event => {
  fileError.value = ''
  selectedFile.value = event.target.files?.[0] || null
  if (selectedFile.value && Number(selectedFile.value.size) !== Number(status.value.partitionSize)) {
    fileError.value = `${text.value.invalidFile} (${formatBytes(status.value.partitionSize)})`
  }
}"""
new_select = """const selectFile = event => {
  fileError.value = ''
  selectedFile.value = event.target.files?.[0] || null
  const name = String(selectedFile.value?.name || '').toLowerCase()
  if (name.startsWith('firmware_')) {
    fileError.value = text.value.wrongFirmwareFile
  } else if (selectedFile.value && Number(selectedFile.value.size) !== Number(status.value.partitionSize)) {
    fileError.value = `${text.value.invalidFile} (${formatBytes(status.value.partitionSize)})`
  }
}"""
text = replace_once(text, old_select, new_select, path)
write(path, text)

# Backend firmware upload rejects the WWW image before any erase/write and
# validates the ESP image magic byte on the first chunk.
path = 'main/webui.cpp'
text = read(path)
old = """    int content_length = req->content_len;
    int content_received = 0;"""
new = """    int content_length = req->content_len;
    if (content_length == 0x50000) {
        ESP_LOGW(TAG, "Rejected 320 KiB WebUI image on firmware endpoint");
        free(ota_buff);
        _ota_status = OTA_FAILED;
        _updateCheck->finishOtaOperation();
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
            "Falsche Datei: WebUI-Abbild erkannt. Unter System -> WebUI installieren.");
    }
    int content_received = 0;"""
text = replace_once(text, old, new, path)
old = """            // Only raw binary uploads are supported (the WebUI posts the file
            // as the request body). The previous multipart/form-data path was"""
new = """            if (recv_len <= 0 || static_cast<unsigned char>(ota_buff[0]) != 0xE9) {
                ESP_LOGW(TAG, "Rejected non-ESP firmware image (magic 0x%02x)",
                         recv_len > 0 ? static_cast<unsigned char>(ota_buff[0]) : 0);
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                    "Falsche Datei: kein gueltiges ESP32-Firmware-Abbild. WebUI unter System -> WebUI installieren.");
                goto err;
            }

            // Only raw binary uploads are supported (the WebUI posts the file
            // as the request body). The previous multipart/form-data path was"""
text = replace_once(text, old, new, path)
write(path, text)

# German reference keys; other locales safely fall back to German.
path = 'webui/src/locales/de.js'
text = read(path)
needle = "    upload: 'Firmware hochladen',"
if needle in text and 'webuiFileDetectedTitle' not in text:
    text = text.replace(needle, needle + "\n    webuiFileDetectedTitle: 'WebUI-Datei erkannt',\n    webuiFileDetectedMessage: 'Diese Datei gehört nicht in das Firmware-Update. Bitte unter System → WebUI installieren.',")
write(path, text)
path = 'webui/src/locales/en.js'
text = read(path)
needle = "    upload: 'Upload firmware',"
if needle in text and 'webuiFileDetectedTitle' not in text:
    text = text.replace(needle, needle + "\n    webuiFileDetectedTitle: 'WebUI image detected',\n    webuiFileDetectedMessage: 'This file does not belong in the firmware updater. Install it under System → WebUI.',")
write(path, text)

# ---------------------------------------------------------------------------
# Strict 24-hour update policy and one shared firmware/WebUI cache.
# ---------------------------------------------------------------------------
path = 'include/updatecheck.h'
text = read(path)
if 'struct WebUIReleaseInfo' not in text:
    marker = '// Snapshot of the latest release known to the firmware.\n'
    struct_text = '''struct WebUIReleaseInfo {
    bool valid = false;
    char version[32] = {0};
    char design[16] = {0};
    int apiVersion = 0;
    char minFirmwareVersion[32] = {0};
    char downloadUrl[256] = {0};
    char sha256[65] = {0};
    uint32_t size = 0;
    char partition[16] = {0};
    int format = 0;
    char releaseUrl[256] = {0};
    char publishedAt[32] = {0};
};

'''
    text = replace_once(text, marker, struct_text + marker, path)
text = text.replace('    bool isPrerelease;          // matches GitHub "prerelease" flag\n', '    bool isPrerelease;          // matches GitHub "prerelease" flag\n    bool betaChannel;           // channel used to populate this cache\n    WebUIReleaseInfo webui;     // optional WebUI block from the same manifest\n')
if 'bool refreshIfDue();' not in text:
    text = text.replace('    bool refresh();\n', '    bool refresh();\n\n    // Performs an online fetch only when the persistent 24 h window is due.\n    // Reboots and page visits cannot bypass this limit.\n    bool refreshIfDue();\n')
write(path, text)

path = 'main/updatecheck.cpp'
text = read(path)
if '#include "nvs.h"' not in text:
    text = text.replace('#include "cJSON.h"\n', '#include "cJSON.h"\n#include "nvs.h"\n')
const_marker = 'static const size_t MANIFEST_RESPONSE_CAP = 4 * 1024;\n'
if 'UPDATE_INTERVAL_SECONDS' not in text:
    constants = '''static constexpr int64_t UPDATE_INTERVAL_SECONDS = 24LL * 60 * 60;
static constexpr int64_t VALID_EPOCH_THRESHOLD = 1700000000LL;
static const char *UPDATE_CACHE_NAMESPACE = "upd_cache";
static const char *UPDATE_CACHE_RELEASE_KEY = "release";
static const char *UPDATE_CACHE_LAST_TRY_KEY = "last_try";

static int64_t current_epoch_seconds()
{
    struct timeval tv = {};
    gettimeofday(&tv, nullptr);
    return tv.tv_sec >= VALID_EPOCH_THRESHOLD ? static_cast<int64_t>(tv.tv_sec) : 0;
}

static bool load_cached_release(ReleaseInfo *out)
{
    if (!out) return false;
    nvs_handle_t handle = 0;
    if (nvs_open(UPDATE_CACHE_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return false;
    size_t size = sizeof(*out);
    const esp_err_t result = nvs_get_blob(handle, UPDATE_CACHE_RELEASE_KEY, out, &size);
    nvs_close(handle);
    return result == ESP_OK && size == sizeof(*out) && out->valid;
}

static void save_cached_release(const ReleaseInfo &info)
{
    nvs_handle_t handle = 0;
    if (nvs_open(UPDATE_CACHE_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) return;
    if (nvs_set_blob(handle, UPDATE_CACHE_RELEASE_KEY, &info, sizeof(info)) == ESP_OK) {
        nvs_commit(handle);
    }
    nvs_close(handle);
}

static int64_t load_last_attempt()
{
    nvs_handle_t handle = 0;
    int64_t value = 0;
    if (nvs_open(UPDATE_CACHE_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        nvs_get_i64(handle, UPDATE_CACHE_LAST_TRY_KEY, &value);
        nvs_close(handle);
    }
    return value;
}

static void save_last_attempt(int64_t value)
{
    nvs_handle_t handle = 0;
    if (nvs_open(UPDATE_CACHE_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) return;
    if (nvs_set_i64(handle, UPDATE_CACHE_LAST_TRY_KEY, value) == ESP_OK) {
        nvs_commit(handle);
    }
    nvs_close(handle);
}
'''
    text = replace_once(text, const_marker, const_marker + '\n' + constants, path)
text = text.replace('  uc->refresh();\n  uc->_evaluateReleaseInfo();', '  uc->refreshIfDue();\n  uc->_evaluateReleaseInfo();')
constructor_old = '''    if (!_stateMutex || !_fetchLock) {
        ESP_LOGE(TAG, "Failed to allocate UpdateCheck synchronization objects");
    }
}'''
constructor_new = '''    if (!_stateMutex || !_fetchLock) {
        ESP_LOGE(TAG, "Failed to allocate UpdateCheck synchronization objects");
    }

    ReleaseInfo cached = {};
    if (load_cached_release(&cached)) {
        _release = cached;
        snprintf(_latestVersion, sizeof(_latestVersion), "%s", cached.version);
        ESP_LOGI(TAG, "Loaded cached %s update manifest: %s",
                 cached.betaChannel ? "beta" : "stable", cached.version);
    }
}'''
text = replace_once(text, constructor_old, constructor_new, path)
text = text.replace('// One-shot timer: fire the first check 30 s after boot so the network /\n    // Ethernet link is up. Subsequent checks run on the periodic timer below.', '// One-shot timer: evaluate the persistent 24 h window 30 s after boot.\n    // It performs a fetch only when due; rebooting cannot force a new request.')

refresh_marker = 'bool UpdateCheck::refresh()\n{\n'
if 'bool UpdateCheck::refreshIfDue()' not in text:
    refresh_due = '''bool UpdateCheck::refreshIfDue()
{
    const int64_t now = current_epoch_seconds();
    const int64_t lastAttempt = load_last_attempt();
    const int64_t uptimeUs = esp_timer_get_time();

    bool due = lastAttempt == 0;
    if (!due && now >= VALID_EPOCH_THRESHOLD && lastAttempt >= VALID_EPOCH_THRESHOLD) {
        const int64_t age = now - lastAttempt;
        due = age >= UPDATE_INTERVAL_SECONDS;
        if (!due) {
            ESP_LOGI(TAG, "Update check skipped: next online check in %lld minutes",
                     (long long)((UPDATE_INTERVAL_SECONDS - age + 59) / 60));
        }
    } else if (!due) {
        // Clock is not reliable yet, or the previous attempt used the no-clock
        // sentinel. Do not let reboots bypass the 24 h policy. Once the device
        // has stayed up for 24 h the periodic timer is allowed to try again.
        due = uptimeUs >= UPDATE_INTERVAL_SECONDS * 1000000LL;
        if (!due) {
            ESP_LOGI(TAG, "Update check skipped: persistent 24 h window not due");
        }
    }

    if (!due) return false;

    // Mark the attempt before opening TLS. Failed DNS/TLS attempts are also
    // limited to once per 24 h, exactly like successful checks.
    save_last_attempt(now >= VALID_EPOCH_THRESHOLD ? now : 1);
    const bool ok = refresh();
    if (ok) {
        save_cached_release(getReleaseInfo());
    }
    return ok;
}

'''
    text = replace_once(text, refresh_marker, refresh_due + refresh_marker, path)
text = text.replace('    int64_t now = esp_timer_get_time() / 1000;', '    const int64_t epoch = current_epoch_seconds();\n    int64_t now = epoch ? epoch * 1000 : esp_timer_get_time() / 1000;')
# Parse nested WebUI block.
parse_anchor = '''        out->isPrerelease = json_bool(root, "isPrerelease", json_bool(root, "prerelease", false));
        out->valid = ok;
    }
'''
if 'out->webui.valid' not in text:
    parse_new = '''        out->isPrerelease = json_bool(root, "isPrerelease", json_bool(root, "prerelease", false));
        out->valid = ok;

        cJSON *webui = cJSON_GetObjectItemCaseSensitive(root, "webui");
        if (cJSON_IsObject(webui)) {
            const char *webuiVersion = json_string(webui, "version");
            const char *design = json_string(webui, "design");
            const char *minimumFirmware = json_string(webui, "minFirmwareVersion");
            const char *webuiUrl = json_string(webui, "downloadUrl");
            const char *webuiSha = json_string(webui, "sha256");
            const char *partition = json_string(webui, "partition");
            const char *webuiReleaseUrl = json_string(webui, "releaseUrl");
            const char *webuiPublishedAt = json_string(webui, "publishedAt");
            cJSON *apiVersion = cJSON_GetObjectItemCaseSensitive(webui, "apiVersion");
            cJSON *imageSize = cJSON_GetObjectItemCaseSensitive(webui, "size");
            cJSON *format = cJSON_GetObjectItemCaseSensitive(webui, "format");

            const bool webuiOk = webuiVersion && design && minimumFirmware &&
                webuiUrl && is_hex_sha256(webuiSha) &&
                cJSON_IsNumber(apiVersion) && cJSON_IsNumber(imageSize) &&
                cJSON_IsNumber(format);
            if (webuiOk) {
                out->webui.valid = true;
                snprintf(out->webui.version, sizeof(out->webui.version), "%s", webuiVersion);
                snprintf(out->webui.design, sizeof(out->webui.design), "%s", design);
                out->webui.apiVersion = apiVersion->valueint;
                snprintf(out->webui.minFirmwareVersion, sizeof(out->webui.minFirmwareVersion), "%s", minimumFirmware);
                snprintf(out->webui.downloadUrl, sizeof(out->webui.downloadUrl), "%s", webuiUrl);
                snprintf(out->webui.sha256, sizeof(out->webui.sha256), "%s", webuiSha);
                out->webui.size = static_cast<uint32_t>(imageSize->valuedouble);
                snprintf(out->webui.partition, sizeof(out->webui.partition), "%s", partition ? partition : "spiffs");
                out->webui.format = format->valueint;
                if (webuiReleaseUrl) snprintf(out->webui.releaseUrl, sizeof(out->webui.releaseUrl), "%s", webuiReleaseUrl);
                if (webuiPublishedAt) snprintf(out->webui.publishedAt, sizeof(out->webui.publishedAt), "%s", webuiPublishedAt);
            }
        }
    }
'''
    text = replace_once(text, parse_anchor, parse_new, path)
# Attach current channel after successful parse.
text = text.replace('        parsedOk = _parseUpdateManifest(resp.buf, out);\n', '        parsedOk = _parseUpdateManifest(resp.buf, out);\n        if (parsedOk) out->betaChannel = beta;\n')
write(path, text)

# API exposes one channel-matched cached firmware/WebUI snapshot. No POST refresh.
path = 'main/webui.cpp'
text = read(path)
old = '''    ReleaseInfo info = _updateCheck->getReleaseInfo();
    const char *currentVersion = _sysInfo->getCurrentVersion();

    bool updateAvailable = false;
    if (info.valid && currentVersion && strcmp(info.version, "n/a") != 0) {
        updateAvailable = (compareVersions(currentVersion, info.version) < 0);
    }
'''
new = '''    ReleaseInfo info = _updateCheck->getReleaseInfo();
    const char *currentVersion = _sysInfo->getCurrentVersion();
    const bool configuredBeta = _settings->getBetaChannel();
    const bool cacheMatchesChannel = info.valid && info.betaChannel == configuredBeta;

    bool updateAvailable = false;
    if (cacheMatchesChannel && currentVersion && strcmp(info.version, "n/a") != 0) {
        updateAvailable = (compareVersions(currentVersion, info.version) < 0);
    }
'''
text = replace_once(text, old, new, path)
text = text.replace('cJSON_AddStringToObject(root, "latestVersion", info.valid ? info.version : "n/a");', 'cJSON_AddStringToObject(root, "latestVersion", cacheMatchesChannel ? info.version : "n/a");')
text = text.replace('cJSON_AddStringToObject(root, "releaseNotes", info.valid ? info.body : "");', 'cJSON_AddStringToObject(root, "releaseNotes", cacheMatchesChannel ? info.body : "");')
text = text.replace('cJSON_AddStringToObject(root, "releaseUrl", info.releaseUrl);', 'cJSON_AddStringToObject(root, "releaseUrl", cacheMatchesChannel ? info.releaseUrl : "");')
text = text.replace('cJSON_AddStringToObject(root, "downloadUrl", info.downloadUrl);', 'cJSON_AddStringToObject(root, "downloadUrl", cacheMatchesChannel ? info.downloadUrl : "");')
text = text.replace('cJSON_AddStringToObject(root, "sha256", info.sha256);', 'cJSON_AddStringToObject(root, "sha256", cacheMatchesChannel ? info.sha256 : "");')
text = text.replace('cJSON_AddStringToObject(root, "publishedAt", info.publishedAt);', 'cJSON_AddStringToObject(root, "publishedAt", cacheMatchesChannel ? info.publishedAt : "");')
text = text.replace('cJSON_AddBoolToObject(root, "betaChannel", _settings->getBetaChannel());', 'cJSON_AddBoolToObject(root, "betaChannel", configuredBeta);\n    cJSON_AddNumberToObject(root, "checkIntervalSeconds", 24 * 60 * 60);')
webui_insert = '''    if (cacheMatchesChannel && info.webui.valid) {
        cJSON *webui = cJSON_AddObjectToObject(root, "webui");
        if (webui) {
            cJSON_AddStringToObject(webui, "version", info.webui.version);
            cJSON_AddStringToObject(webui, "design", info.webui.design);
            cJSON_AddNumberToObject(webui, "apiVersion", info.webui.apiVersion);
            cJSON_AddStringToObject(webui, "minFirmwareVersion", info.webui.minFirmwareVersion);
            cJSON_AddStringToObject(webui, "downloadUrl", info.webui.downloadUrl);
            cJSON_AddStringToObject(webui, "sha256", info.webui.sha256);
            cJSON_AddNumberToObject(webui, "size", info.webui.size);
            cJSON_AddStringToObject(webui, "partition", info.webui.partition);
            cJSON_AddNumberToObject(webui, "format", info.webui.format);
            cJSON_AddStringToObject(webui, "releaseUrl", info.webui.releaseUrl);
            cJSON_AddStringToObject(webui, "publishedAt", info.webui.publishedAt);
        }
    } else {
        cJSON_AddNullToObject(root, "webui");
    }
'''
anchor = '    cJSON_AddBoolToObject(root, "fetchInProgress", _updateCheck->isFetchInProgress());\n'
if webui_insert not in text:
    text = replace_once(text, anchor, anchor + webui_insert, path)
text = text.replace('    // GET returns the cached snapshot only - no network fetch. The WebUI\n    // uses POST /api/check_update to trigger a refresh; this keeps GET cheap\n    // for polling and avoids redundant manifest requests.', '    // GET returns the persistent cached snapshot only. No browser, MQTT\n    // command or page visit can trigger an online request; the backend timer\n    // enforces the single 24-hour update window.')
write(path, text)

# WebUI updater reads the shared ESP cache only, never GitHub directly.
path = 'webui/src/webuiupdate.vue'
text = read(path)
old = '''    const info = await axios.get('/api/check_update', { timeout: 15000, silent: true })
    const channel = info.data?.betaChannel ? 'beta' : 'latest'
    const manifest = await axios.get(`https://raw.githubusercontent.com/Xerolux/HB-RF-ETH-ng/main/${channel}.json?t=${Date.now()}`, { timeout: 15000, headers: { 'Cache-Control': 'no-cache' } })
    const item = manifest.data?.webui'''
new = '''    const info = await axios.get('/api/check_update', { timeout: 15000, silent: true })
    const item = info.data?.webui'''
text = replace_once(text, old, new, path)
text = text.replace("refresh: 'Neu prüfen'", "refresh: 'Status aktualisieren'")
text = text.replace("refresh: 'Check again'", "refresh: 'Refresh status'")
write(path, text)

# Firmware store displays the actual cached-fetch timestamp, not the time of a
# local GET request.
path = 'webui/src/stores.js'
text = read(path)
text = text.replace('        this.lastCheck = new Date().toISOString()', "        this.lastCheck = data.fetchedAt ? new Date(Number(data.fetchedAt)).toISOString() : null")
write(path, text)

# Document the strict policy in the hotfix note.
path = 'docs/HOTFIX_BETA3.md'
text = read(path)
if 'exactly one online update-manifest request per 24 hours' not in text:
    text += '''

## Update-check policy

Firmware and WebUI metadata come from one combined manifest cache owned by the
ESP32. The browser only reads `/api/check_update`; it never contacts GitHub for
update metadata. A persistent NVS timestamp limits online update-manifest
requests to exactly one attempt per 24 hours, including across device reboots.
Page visits, local refresh buttons and MQTT status publication only read the
cached snapshot.
'''
write(path, text)

print('Applied gzip-only WebUI, strict upload separation, and persistent 24 h update cache.')
