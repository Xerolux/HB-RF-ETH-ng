#include "system_overview_api.h"

#include <stdlib.h>

#include "cJSON.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "nvs.h"

#include "log_manager.h"
#include "reset_info.h"
#include "security_headers.h"
#include "webui_storage.h"

extern esp_err_t validate_auth(httpd_req_t *req);

namespace
{
constexpr const char *TAG = "SystemOverview";
constexpr const char *CRASH_TAIL_NVS_NAMESPACE = "reset_info";
constexpr const char *CRASH_TAIL_NVS_KEY = "clog";

bool crash_tail_available()
{
    nvs_handle_t handle = 0;
    if (nvs_open(CRASH_TAIL_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK)
    {
        return false;
    }

    size_t length = 0;
    const esp_err_t result = nvs_get_blob(handle, CRASH_TAIL_NVS_KEY, nullptr,
                                          &length);
    nvs_close(handle);
    return result == ESP_OK && length > 0;
}

// Deliberately self-contained: no Vue, no external files, no timer and no
// automatic polling. The page allocates no firmware resources until requested.
constexpr char RECOVERY_PAGE[] = R"HTML(<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>HB-RF-ETH-ng Recovery</title>
<style>
:root{color-scheme:dark;font-family:system-ui,-apple-system,"Segoe UI",sans-serif}*{box-sizing:border-box}body{margin:0;background:#0f131a;color:#f4f7fb}main{max-width:820px;margin:auto;padding:24px}.hero,.card{background:#171d26;border:1px solid #303846;border-radius:18px;padding:20px;margin-bottom:16px}.hero h1{margin:0 0 6px}.muted,small{color:#a9b0be}.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:16px}label{display:block;margin:10px 0 5px}input,button{width:100%;min-height:44px;border-radius:10px;border:1px solid #3c4657;padding:10px 12px;background:#10161f;color:#f4f7fb}button{background:#f26a3d;border:0;font-weight:700;cursor:pointer;margin-top:10px}button.secondary{background:#303846}button.danger{background:#b83f38}button:disabled{opacity:.55;cursor:not-allowed}.status{white-space:pre-wrap;overflow-wrap:anywhere;background:#0c1118;border-radius:10px;padding:12px;min-height:48px}.facts{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:8px}.fact{background:#10161f;border-radius:10px;padding:10px}.fact span{display:block;color:#a9b0be;font-size:.78rem}.fact strong{display:block;margin-top:3px}progress{width:100%;height:14px;margin-top:10px}@media(max-width:640px){.grid,.facts{grid-template-columns:1fr}main{padding:14px}}
</style>
</head>
<body><main>
<section class="hero"><h1>HB-RF-ETH-ng Recovery</h1><p class="muted">Minimale Notfalloberfläche ohne New-Design-Bundle. Alle Aktionen verwenden die normale Geräteanmeldung.</p></section>
<section class="card" id="loginCard"><h2>Anmelden</h2><label>Benutzer</label><input id="user" value="admin" autocomplete="username"><label>Passwort</label><input id="pass" type="password" autocomplete="current-password"><button id="login">Anmelden</button></section>
<section id="tools" hidden>
<div class="grid">
<section class="card"><h2>Systemstatus</h2><div class="facts" id="facts"></div><button class="secondary" id="refresh">Status aktualisieren</button></section>
<section class="card"><h2>Diagnose</h2><button class="secondary" id="crash">Crash-Tail laden</button><button class="secondary" id="logs">Aktuelles Log herunterladen</button><div class="status" id="diag">Noch keine Diagnose geladen.</div></section>
<section class="card"><h2>WebUI wiederherstellen</h2><p class="muted">Nur ein passendes <code>webui_*.bin</code> beziehungsweise <code>spiffs.bin</code> verwenden.</p><input id="wwwFile" type="file" accept=".bin,application/octet-stream"><button id="wwwUpload">WebUI hochladen</button><progress id="wwwProgress" max="100" value="0"></progress></section>
<section class="card"><h2>Firmware wiederherstellen</h2><p class="muted">Das Gerät startet nach erfolgreichem Firmware-Upload automatisch neu.</p><input id="fwFile" type="file" accept=".bin,application/octet-stream"><button id="fwUpload">Firmware hochladen</button><progress id="fwProgress" max="100" value="0"></progress></section>
</div>
<section class="card"><h2>Gerätesteuerung</h2><button class="danger" id="restart">Gerät neu starten</button><div class="status" id="status">Bereit.</div></section>
</section>
<script>
let token='',busy=false;const $=id=>document.getElementById(id);const status=m=>$('status').textContent=m;
const headers=(extra={})=>Object.assign({'Authorization':'Token '+token},extra);
const bytes=n=>{n=Number(n)||0;if(n<1024)return n+' B';if(n<1048576)return(n/1024).toFixed(1)+' KB';return(n/1048576).toFixed(2)+' MB'};
const esc=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
async function api(url,opt={}){opt.headers=headers(opt.headers||{});const r=await fetch(url,opt);const text=await r.text();if(!r.ok)throw new Error(text||('HTTP '+r.status));try{return JSON.parse(text)}catch{return text}}
async function login(){status('Anmeldung läuft …');const r=await fetch('/login.json',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({username:$('user').value,password:$('pass').value})});const d=await r.json();if(!r.ok||!d.isAuthenticated||!d.token)throw new Error('Anmeldung fehlgeschlagen');token=d.token;$('loginCard').hidden=true;$('tools').hidden=false;status('Angemeldet.');await refresh()}
async function refresh(){const [s,o]=await Promise.all([api('/sysinfo.json?t='+Date.now()),api('/api/system/overview')]);const i=s.sysInfo||{};const used=Number(o.usedInternalHeap)||0,total=Number(o.totalInternalHeap)||0;const rows=[['Firmware',i.currentVersion||'—'],['Laufzeit',Math.floor((i.uptimeSeconds||0)/60)+' min'],['Resetgrund',o.resetReasonText||i.resetReason||'—'],['RAM gesamt',bytes(total)],['RAM belegt',bytes(used)+' ('+(Number(o.internalHeapUsagePercent)||0).toFixed(1)+' %)'],['RAM frei',bytes(o.freeInternalHeap)],['RAM-Minimum',bytes(o.minimumFreeHeap)],['Größter Block',bytes(o.largestFreeBlock)],['PSRAM',o.psramAvailable?bytes(o.totalPsram):'nicht vorhanden'],['WebUI',o.webui?.version||'embedded'],['Crash-Tail',o.logs?.crashTailAvailable?'vorhanden':'nicht vorhanden'],['Logpuffer',o.logs?.enabled?bytes(o.logs.bufferBytes):'inaktiv']];$('facts').innerHTML=rows.map(x=>'<div class="fact"><span>'+esc(x[0])+'</span><strong>'+esc(x[1])+'</strong></div>').join('');status('Status aktualisiert.')}
function upload(fileId,url,progressId,label){if(busy)return status('Ein Update läuft bereits.');const f=$(fileId).files[0];if(!f)return status('Bitte zuerst eine BIN-Datei auswählen.');if(!confirm(label+' wirklich starten?'))return;busy=true;const x=new XMLHttpRequest();x.open('POST',url);x.setRequestHeader('Authorization','Token '+token);x.setRequestHeader('Content-Type','application/octet-stream');x.upload.onprogress=e=>{if(e.lengthComputable)$(progressId).value=Math.round(e.loaded*100/e.total)};x.onload=()=>{busy=false;status(x.status>=200&&x.status<300?label+' erfolgreich.':label+' fehlgeschlagen: '+x.responseText);$(progressId).value=x.status>=200&&x.status<300?100:0};x.onerror=()=>{busy=false;status(label+' fehlgeschlagen: Netzwerkfehler')};status(label+' läuft …');x.send(f)}
$('login').onclick=()=>login().catch(e=>status(e.message));$('refresh').onclick=()=>refresh().catch(e=>status(e.message));$('wwwUpload').onclick=()=>upload('wwwFile','/api/webui/update','wwwProgress','WebUI-Upload');$('fwUpload').onclick=()=>upload('fwFile','/ota_update','fwProgress','Firmware-Upload');
$('crash').onclick=async()=>{try{const d=await api('/api/crash_log');$('diag').textContent=d.available?(d.tail||'Crash-Tail leer'):'Kein Crash-Tail vorhanden.';await refresh()}catch(e){$('diag').textContent=e.message}};
$('logs').onclick=async()=>{try{const r=await fetch('/api/log/download',{headers:headers()});if(!r.ok)throw new Error(await r.text());const a=document.createElement('a');a.href=URL.createObjectURL(await r.blob());a.download='hb-rf-eth-log.txt';a.click();setTimeout(()=>URL.revokeObjectURL(a.href),1000)}catch(e){status(e.message)}};
$('restart').onclick=async()=>{if(busy)return status('Ein Update läuft bereits.');if(!confirm('Gerät wirklich neu starten?'))return;try{await api('/api/restart',{method:'POST'});status('Neustart wurde ausgelöst.')}catch(e){status('Neustart ausgelöst; Verbindung wird getrennt.')}};
</script></main></body></html>)HTML";

esp_err_t get_recovery_page(httpd_req_t *req)
{
    add_security_headers(req);
    // Recovery is a static self-contained document. Permit only this route's
    // own inline script; the normal New Design retains the strict global CSP.
    httpd_resp_set_hdr(req, "Content-Security-Policy",
                       "default-src 'self'; style-src 'self' 'unsafe-inline'; "
                       "script-src 'self' 'unsafe-inline'; img-src 'self' data:; "
                       "connect-src 'self'; font-src 'self' data:; object-src 'none'; "
                       "base-uri 'self'; form-action 'self'; frame-ancestors 'self';");
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, RECOVERY_PAGE, HTTPD_RESP_USE_STRLEN);
}

esp_err_t get_system_overview(httpd_req_t *req)
{
    add_security_headers(req);
    if (validate_auth(req) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, nullptr);
    }

    esp_chip_info_t chip = {};
    esp_chip_info(&chip);

    uint32_t flash_size = 0;
    const esp_err_t flash_result = esp_flash_get_size(nullptr, &flash_size);
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *next_update = esp_ota_get_next_update_partition(nullptr);
    const WebUIStorageStatus webui = webui_storage_get_status();
    LogManager &logs = LogManager::instance();

    const size_t total_internal = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    const size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const size_t used_internal = total_internal > free_internal
        ? total_internal - free_internal
        : 0;
    const double internal_usage = total_internal > 0
        ? static_cast<double>(used_internal) * 100.0 /
              static_cast<double>(total_internal)
        : 0.0;

    const size_t total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    const size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    const size_t used_psram = total_psram > free_psram
        ? total_psram - free_psram
        : 0;

    cJSON *root = cJSON_CreateObject();
    if (!root)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Out of memory");
    }

    cJSON_AddStringToObject(root, "idfVersion", esp_get_idf_version());
    cJSON_AddStringToObject(root, "target", CONFIG_IDF_TARGET);
    cJSON_AddNumberToObject(root, "chipModel", static_cast<int>(chip.model));
    cJSON_AddNumberToObject(root, "chipRevision", chip.revision);
    cJSON_AddNumberToObject(root, "chipCores", chip.cores);
    cJSON_AddNumberToObject(root, "chipFeatures", chip.features);
    cJSON_AddNumberToObject(root, "resetReason", static_cast<int>(esp_reset_reason()));
    cJSON_AddStringToObject(root, "resetReasonText", ResetInfo::getResetDetails());

    cJSON_AddNumberToObject(root, "flashBytes",
                            flash_result == ESP_OK ? flash_size : 0);
    cJSON_AddNumberToObject(root, "freeHeap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(root, "minimumFreeHeap",
                            esp_get_minimum_free_heap_size());
    cJSON_AddNumberToObject(root, "largestFreeBlock",
                            heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    cJSON_AddNumberToObject(root, "totalInternalHeap", total_internal);
    cJSON_AddNumberToObject(root, "freeInternalHeap", free_internal);
    cJSON_AddNumberToObject(root, "usedInternalHeap", used_internal);
    cJSON_AddNumberToObject(root, "internalHeapUsagePercent", internal_usage);
    cJSON_AddBoolToObject(root, "psramAvailable", total_psram > 0);
    cJSON_AddNumberToObject(root, "totalPsram", total_psram);
    cJSON_AddNumberToObject(root, "freePsram", free_psram);
    cJSON_AddNumberToObject(root, "usedPsram", used_psram);

    cJSON_AddStringToObject(root, "runningPartition",
                            running ? running->label : "unknown");
    cJSON_AddNumberToObject(root, "runningPartitionAddress",
                            running ? running->address : 0);
    cJSON_AddNumberToObject(root, "runningPartitionSize",
                            running ? running->size : 0);
    cJSON_AddStringToObject(root, "nextUpdatePartition",
                            next_update ? next_update->label : "unknown");
    cJSON_AddNumberToObject(root, "nextUpdatePartitionSize",
                            next_update ? next_update->size : 0);

    cJSON *webui_object = cJSON_AddObjectToObject(root, "webui");
    if (webui_object)
    {
        cJSON_AddStringToObject(webui_object, "source",
                                webui.valid ? "spiffs" : "embedded");
        cJSON_AddStringToObject(webui_object, "version",
                                webui.version[0] ? webui.version : "embedded");
        cJSON_AddBoolToObject(webui_object, "valid", webui.valid);
        cJSON_AddBoolToObject(webui_object, "mounted", webui.mounted);
        cJSON_AddNumberToObject(webui_object, "partitionBytes",
                                webui.partitionSize);
        cJSON_AddNumberToObject(webui_object, "usedBytes", webui.usedBytes);
    }

    cJSON *log_object = cJSON_AddObjectToObject(root, "logs");
    if (log_object)
    {
        cJSON_AddBoolToObject(log_object, "enabled", logs.isEnabled());
        cJSON_AddNumberToObject(log_object, "bufferBytes",
                                logs.getBufferSize());
        cJSON_AddNumberToObject(log_object, "availableBytes",
                                logs.getBufferedBytes());
        cJSON_AddNumberToObject(log_object, "totalWritten",
                                static_cast<double>(logs.getTotalWritten()));
        cJSON_AddNumberToObject(log_object, "subscribers",
                                logs.subscriberCount());
        cJSON_AddBoolToObject(log_object, "crashTailAvailable",
                              crash_tail_available());
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "JSON allocation failed");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control",
                       "no-store, no-cache, must-revalidate, max-age=0");
    const esp_err_t result = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return result;
}

httpd_uri_t system_overview_uri = {
    .uri = "/api/system/overview",
    .method = HTTP_GET,
    .handler = get_system_overview,
    .user_ctx = nullptr,
};

httpd_uri_t recovery_page_uri = {
    .uri = "/recovery",
    .method = HTTP_GET,
    .handler = get_recovery_page,
    .user_ctx = nullptr,
};
} // namespace

esp_err_t system_overview_api_register(httpd_handle_t server)
{
    if (!server) return ESP_ERR_INVALID_ARG;

    esp_err_t result = httpd_register_uri_handler(server, &system_overview_uri);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not register system overview API: %s",
                 esp_err_to_name(result));
        return result;
    }

    result = httpd_register_uri_handler(server, &recovery_page_uri);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not register recovery page: %s",
                 esp_err_to_name(result));
    }
    return result;
}
