from pathlib import Path

storage = Path('main/webui_storage.cpp')
text = storage.read_text(encoding='utf-8')

for old, new in [
    ('{"/main.js", BASE_PATH "/main.js.gz", "application/javascript", "gzip"}',
     '{"/main.js", BASE_PATH "/main.js.br", "application/javascript", "br"}'),
    ('{"/main.css", BASE_PATH "/main.css.gz", "text/css", "gzip"}',
     '{"/main.css", BASE_PATH "/main.css.br", "text/css", "br"}'),
    ('strcmp(js_encoding->valuestring, "gzip") == 0',
     'strcmp(js_encoding->valuestring, "br") == 0'),
    ('strcmp(css_encoding->valuestring, "gzip") == 0',
     'strcmp(css_encoding->valuestring, "br") == 0'),
    ('BASE_PATH "/main.js.gz"', 'BASE_PATH "/main.js.br"'),
    ('BASE_PATH "/main.css.gz"', 'BASE_PATH "/main.css.br"'),
    ('// Only the New Design is accepted. Gzip works reliably on the device\n'
     '    // private-LAN HTTP origin.',
     '// Only the New Design is accepted. Brotli keeps the complete single-file\n'
     '    // Vue application inside the existing 320 KiB partition.'),
]:
    if old in text:
        text = text.replace(old, new)
    elif new not in text:
        raise RuntimeError(f'missing storage revision pattern: {old}')

marker = '''esp_err_t stream_external_file(httpd_req_t *req, const AssetSpec *asset)
{'''
helper = '''bool request_accepts_encoding(httpd_req_t *req, const char *encoding)
{
    if (!req || !encoding || !encoding[0]) return false;

    const size_t length = httpd_req_get_hdr_value_len(req, "Accept-Encoding");
    if (length == 0 || length >= 160) return false;

    char value[160] = {};
    if (httpd_req_get_hdr_value_str(req, "Accept-Encoding", value,
                                    sizeof(value)) != ESP_OK)
    {
        return false;
    }

    const size_t wanted = strlen(encoding);
    const char *cursor = value;
    while (*cursor)
    {
        while (*cursor == ' ' || *cursor == '\t' || *cursor == ',') ++cursor;
        const char *start = cursor;
        while (*cursor && *cursor != ',' && *cursor != ';') ++cursor;
        const char *end = cursor;
        while (end > start && (end[-1] == ' ' || end[-1] == '\t')) --end;

        if (static_cast<size_t>(end - start) == wanted &&
            strncasecmp(start, encoding, wanted) == 0)
        {
            return true;
        }

        while (*cursor && *cursor != ',') ++cursor;
    }
    return false;
}

esp_err_t stream_external_file(httpd_req_t *req, const AssetSpec *asset)
{'''
if helper not in text:
    if marker not in text:
        raise RuntimeError('stream_external_file marker missing')
    text = text.replace(marker, helper, 1)

old_handler = '''    if (webui_storage_is_valid())
    {
        const esp_err_t result = stream_external_file(req, route->asset);
        if (result != ESP_ERR_NOT_FOUND) return result;

        // Optional assets such as the large PWA icon intentionally remain in
        // firmware and fall through to the embedded New Design handler.
        ESP_LOGD(TAG, "Using embedded fallback asset for %s",
                 route->asset ? route->asset->uri : "unknown");
    }

    return route->original_handler(req);'''
new_handler = '''    if (webui_storage_is_valid())
    {
        const bool needs_brotli = route->asset &&
            strcmp(route->asset->content_encoding, "br") == 0;
        if (!needs_brotli || request_accepts_encoding(req, "br"))
        {
            const esp_err_t result = stream_external_file(req, route->asset);
            if (result != ESP_ERR_NOT_FOUND) return result;
        }
        else
        {
            ESP_LOGI(TAG,
                     "Client does not advertise Brotli; using embedded asset for %s",
                     route->asset ? route->asset->uri : "unknown");
        }

        // Optional or unsupported-encoding assets intentionally fall through
        // to the embedded New Design handler.
        ESP_LOGD(TAG, "Using embedded fallback asset for %s",
                 route->asset ? route->asset->uri : "unknown");
    }

    return route->original_handler(req);'''
if old_handler in text:
    text = text.replace(old_handler, new_handler, 1)
elif new_handler not in text:
    raise RuntimeError('wrapped_asset_handler pattern missing')

storage.write_text(text, encoding='utf-8')

packager = Path('rename_webui_files.py')
p = packager.read_text(encoding='utf-8')
p = p.replace('"main.js.gz",\n        "main.css.gz",', '"main.js.br",\n        "main.css.br",')
p = p.replace('"main.js": "gzip",\n            "main.css": "gzip",', '"main.js": "br",\n            "main.css": "br",')
if '"main.js.br"' not in p or '"main.js": "br"' not in p:
    raise RuntimeError('could not restore Brotli packaging')
packager.write_text(p, encoding='utf-8')

hotfix_doc = Path('docs/HOTFIX_BETA3_RECOVERY.md')
d = hotfix_doc.read_text(encoding='utf-8')
d = d.replace(
    'the external WebUI used Brotli assets on the device\'s private-LAN HTTP origin;',
    'the server sent Brotli assets even when the browser did not advertise Brotli support;',
)
d = d.replace(
    'The fixed external WebUI uses gzip and version `1.0.0-Beta.2`.',
    'The server now uses the embedded gzip assets whenever a browser does not advertise Brotli support. The updated WebUI version is `1.0.0-Beta.2`.',
)
hotfix_doc.write_text(d, encoding='utf-8')

Path(__file__).unlink(missing_ok=True)
