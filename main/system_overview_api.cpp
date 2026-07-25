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
:root{color-scheme:light;--color-primary:#f26a3d;--color-primary-strong:#c04018;--color-primary-soft:rgba(242,106,61,.12);--color-success:#43a047;--color-success-soft:rgba(67,160,71,.14);--color-danger:#e05247;--color-danger-soft:#fff0ee;--color-bg:#f6f7f8;--color-text:#222;--color-text-soft:#666;--color-text-muted:#999;--newdesign-panel:#fff;--newdesign-panel-soft:#ecedee;--newdesign-border:#e3e5e7;--newdesign-border-strong:#d2d4d7;--newdesign-sidebar:#fff;--newdesign-header:#fff;--newdesign-shadow:0 1px 2px rgba(15,23,42,.05);--newdesign-shadow-lg:0 10px 24px rgba(15,23,42,.08);--newdesign-sidebar-width:360px;--newdesign-header-height:88px;--newdesign-radius-card:4px;--newdesign-radius-input:4px;--newdesign-radius-button:4px;--content-gap:24px;font-family:Inter,"Segoe UI Variable Text","Segoe UI",system-ui,-apple-system,sans-serif}
*{box-sizing:border-box}html{min-height:100%;background:var(--color-bg)}body{margin:0;min-height:100vh;background:var(--color-bg);color:var(--color-text);font-size:16px;line-height:1.5;-webkit-font-smoothing:antialiased}button,input{font:inherit}.recovery-shell{min-height:100vh}.brand,.mobile-brand{display:flex;align-items:center;gap:14px;color:var(--color-text);text-decoration:none;min-width:0}.brand-logo{display:block;width:48px;height:48px;flex:0 0 auto}.brand-copy{display:flex;flex-direction:column;min-width:0}.brand-copy strong{font-size:2.375rem;line-height:1.2;font-weight:800;letter-spacing:0;white-space:nowrap}.brand-copy small{color:var(--color-text-soft);font-size:1rem;max-width:230px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.desktop-sidebar{position:fixed;inset:0 auto 0 0;width:var(--newdesign-sidebar-width);z-index:20;padding:24px 12px 20px;background:var(--newdesign-sidebar);border-right:1px solid var(--newdesign-border);display:flex;flex-direction:column;gap:28px;overflow-y:auto}.side-nav{display:flex;flex-direction:column;gap:18px}.side-nav-group{display:flex;flex-direction:column;gap:6px}.side-nav-heading{padding:0 14px;color:var(--color-text-muted);font-size:.8125rem;font-weight:800;letter-spacing:.08em;text-transform:uppercase}.nav-item,.back-link{min-height:42px;display:flex;align-items:center;gap:12px;padding:0 14px;border:1px solid transparent;border-radius:6px;color:var(--color-text-soft);text-decoration:none;font-weight:600}.nav-icon{width:20px;text-align:center}.nav-item.active{color:#fff;background:var(--color-primary)}.back-link{border-color:var(--newdesign-border-strong);justify-content:center;color:var(--color-text)}.back-link:hover,.nav-item:hover{background:var(--newdesign-panel-soft);color:var(--color-text)}.sidebar-footer{margin-top:auto;display:grid;gap:8px}.sidebar-note{padding:12px 14px;border:1px solid var(--newdesign-border);border-radius:6px;color:var(--color-text-soft);font-size:.875rem}
.header-nav{position:fixed;top:0;left:var(--newdesign-sidebar-width);right:0;height:var(--newdesign-header-height);z-index:20;display:flex;align-items:center;justify-content:space-between;gap:18px;padding:0 var(--content-gap);background:var(--newdesign-header);border-bottom:1px solid var(--newdesign-border);min-width:0}.mobile-brand{display:none}.top-status{display:flex;align-items:center;justify-content:flex-end;gap:10px;flex:1;min-width:0;color:var(--color-text-soft)}.top-status strong{color:var(--color-text)}.status-pill{display:inline-flex;align-items:center;min-height:26px;padding:0 10px;border-radius:6px;border:1px solid currentColor;color:var(--color-success);background:var(--color-success-soft);font-size:.875rem;font-weight:800}.top-separator{width:1px;height:22px;background:var(--newdesign-border-strong)}.icon-button{width:50px;height:50px;display:inline-grid;place-items:center;border:1px solid var(--newdesign-border-strong);border-radius:6px;background:var(--newdesign-panel);color:var(--color-text);cursor:pointer}.icon-button:hover{background:var(--newdesign-panel-soft)}
main{min-height:100vh;padding:calc(var(--newdesign-header-height) + var(--content-gap)) var(--content-gap) var(--content-gap) calc(var(--newdesign-sidebar-width) + var(--content-gap))}.content{max-width:1320px}.page-hero,.card{background:var(--newdesign-panel);border:1px solid var(--newdesign-border);border-radius:var(--newdesign-radius-card);box-shadow:none}.page-hero{padding:20px;margin-bottom:16px;display:flex;align-items:center;justify-content:space-between;gap:20px}.hero-copy{display:grid;gap:8px;min-width:0}.hero-eyebrow{display:inline-flex;align-items:center;gap:8px;width:max-content;padding:7px 12px;border-radius:4px;color:var(--color-primary-strong);background:var(--color-primary-soft);font-size:.8125rem;font-weight:800;letter-spacing:.08em;text-transform:uppercase}.page-hero h1{margin:0;font-size:clamp(1.875rem,4vw,2.375rem);line-height:1.2}.page-subtitle{max-width:76ch;margin:0;color:var(--color-text-soft)}.mode-chip{display:inline-flex;align-items:center;gap:8px;padding:8px 12px;border-radius:4px;background:var(--newdesign-panel-soft);color:var(--color-text-soft);white-space:nowrap}
.status{min-height:48px;margin:0 0 16px;padding:12px 14px;border:1px solid var(--newdesign-border);border-left:4px solid var(--color-primary);border-radius:var(--newdesign-radius-card);background:var(--newdesign-panel);white-space:pre-wrap;overflow-wrap:anywhere}.card{padding:20px;margin-bottom:14px}.card-head{display:flex;align-items:flex-start;gap:12px;margin-bottom:16px;padding-bottom:14px;border-bottom:1px solid var(--newdesign-border)}.card-icon{width:46px;height:46px;display:grid;place-items:center;flex:0 0 auto;border-radius:4px;color:var(--color-primary);background:var(--color-primary-soft);font-size:1.25rem}.card h2{margin:0;font-size:1.25rem}.card-head p,.muted{margin:3px 0 0;color:var(--color-text-soft);font-size:.875rem}.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:14px}#loginCard{max-width:640px}.form-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:14px}label{display:block;margin:0 0 7px;color:var(--color-text-soft);font-size:.8125rem;font-weight:800;letter-spacing:.08em;text-transform:uppercase}input{width:100%;min-height:46px;padding:11px 13px;border:1px solid var(--newdesign-border);border-radius:var(--newdesign-radius-input);background:var(--newdesign-panel-soft);color:var(--color-text)}input:focus{border-color:var(--color-primary);outline:3px solid var(--color-primary-soft);background:var(--newdesign-panel)}input[type=file]{padding:8px}input[type=file]::file-selector-button{min-height:30px;margin-right:10px;padding:5px 10px;border:1px solid var(--newdesign-border-strong);border-radius:4px;background:var(--newdesign-panel);color:var(--color-text);cursor:pointer}
.actions{display:flex;gap:10px;flex-wrap:wrap;margin-top:14px}button.action{min-height:44px;padding:10px 15px;border:1px solid transparent;border-radius:var(--newdesign-radius-button);background:var(--color-primary);color:#fff;font-weight:700;cursor:pointer}button.action:hover,button.action:focus-visible{background:var(--color-primary-strong);outline:3px solid var(--color-primary-soft)}button.action.secondary{background:var(--newdesign-panel);border-color:var(--newdesign-border-strong);color:var(--color-text)}button.action.secondary:hover{background:var(--newdesign-panel-soft)}button.action.danger{background:var(--color-danger)}button.action:disabled{opacity:.55;cursor:not-allowed}.actions .action{flex:1 1 180px}.facts{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:8px}.fact{padding:11px;border:1px solid var(--newdesign-border);border-radius:4px;background:var(--newdesign-panel-soft)}.fact span{display:block;color:var(--color-text-soft);font-size:.8125rem}.fact strong{display:block;margin-top:3px;overflow-wrap:anywhere}.diag-output{min-height:96px;margin-top:12px;padding:12px;border:1px solid var(--newdesign-border);border-radius:4px;background:var(--newdesign-panel-soft);white-space:pre-wrap;overflow:auto}progress{width:100%;height:8px;margin-top:14px;border:0;border-radius:999px;accent-color:var(--color-primary)}code{color:var(--color-primary-strong)}[hidden]{display:none!important}
[data-theme=dark]{color-scheme:dark;--color-success:#55c271;--color-success-soft:rgba(85,194,113,.16);--color-danger-soft:rgba(224,82,71,.14);--color-bg:#2e3136;--color-text:#f2f2f2;--color-text-soft:#c5c8cc;--color-text-muted:#90959c;--newdesign-panel:#2c2f33;--newdesign-panel-soft:#3a3f45;--newdesign-border:#474c51;--newdesign-border-strong:#565c62;--newdesign-sidebar:#24272b;--newdesign-header:#25282c}
@media(prefers-color-scheme:dark){:root:not([data-theme=light]){color-scheme:dark;--color-success:#55c271;--color-success-soft:rgba(85,194,113,.16);--color-danger-soft:rgba(224,82,71,.14);--color-bg:#2e3136;--color-text:#f2f2f2;--color-text-soft:#c5c8cc;--color-text-muted:#90959c;--newdesign-panel:#2c2f33;--newdesign-panel-soft:#3a3f45;--newdesign-border:#474c51;--newdesign-border-strong:#565c62;--newdesign-sidebar:#24272b;--newdesign-header:#25282c}}
@media(max-width:991px){:root{--newdesign-header-height:72px}.desktop-sidebar{display:none}.header-nav{left:0;padding:0 12px}.mobile-brand{display:flex}.mobile-brand .brand-logo{width:38px;height:38px}.mobile-title{font-weight:800}.top-status{display:none}main{padding:calc(var(--newdesign-header-height) + 16px) 8px 24px}.page-hero{align-items:flex-start}.mode-chip{display:none}.grid,.form-grid,.facts{grid-template-columns:1fr}.card{padding:16px}}
@media(max-width:520px){.page-hero{padding:16px}.page-hero h1{font-size:1.75rem}.mobile-title{font-size:.9rem}.icon-button{width:44px;height:44px}.actions{display:grid}.actions .action{width:100%}}
</style>
</head>
<body><div class="recovery-shell">
<aside class="desktop-sidebar">
<a class="brand" href="/">
<svg class="brand-logo" viewBox="0 0 512 512" aria-hidden="true"><defs><linearGradient id="recovery-brand" x1="0" y1="0" x2="1" y2="1"><stop offset="0%" stop-color="#D96A5A"/><stop offset="100%" stop-color="#EAA08E"/></linearGradient></defs><g fill="url(#recovery-brand)" transform="translate(15,-7)"><path d="M256 186C320.6 123.3 374.3 165.6 320.7 233.1C289.1 197.3 272.8 175.2 256 186Z"/><path d="M316.6 291C338.6 378.3 275.2 403.6 243.5 323.5C290.3 314 317.6 311 316.6 291Z"/><path d="M195.4 291C108.8 266.4 118.6 198.8 203.8 211.4C188.6 256.7 177.6 281.8 195.4 291Z"/></g></svg>
<span class="brand-copy"><strong>HB-RF-ETH-ng</strong><small id="deviceName">System Recovery</small></span>
</a>
<nav class="side-nav" aria-label="Recovery-Navigation"><div class="side-nav-group"><div class="side-nav-heading">System</div><span class="nav-item active"><span class="nav-icon">✚</span><span>Recovery</span></span><a class="nav-item" href="/"><span class="nav-icon">⌂</span><span>Status</span></a></div></nav>
<div class="sidebar-footer"><a class="back-link" href="/">← Zur normalen WebUI</a><div class="sidebar-note"><strong>Autarker Modus</strong><br>Diese Oberfläche funktioniert unabhängig vom installierten WebUI-Bundle.</div></div>
</aside>
<header class="header-nav">
<a class="mobile-brand" href="/"><svg class="brand-logo" viewBox="0 0 512 512" aria-hidden="true"><g fill="#df7e6e" transform="translate(15,-7)"><path d="M256 186C320.6 123.3 374.3 165.6 320.7 233.1C289.1 197.3 272.8 175.2 256 186Z"/><path d="M316.6 291C338.6 378.3 275.2 403.6 243.5 323.5C290.3 314 317.6 311 316.6 291Z"/><path d="M195.4 291C108.8 266.4 118.6 198.8 203.8 211.4C188.6 256.7 177.6 281.8 195.4 291Z"/></g></svg><span class="mobile-title">System Recovery</span></a>
<div class="top-status"><span>Notfallmodus</span><span class="status-pill">Bereit</span><span class="top-separator"></span><span>Firmware: <strong id="topFirmware">—</strong></span><span class="top-separator"></span><span>WebUI-unabhängig</span></div>
<button class="icon-button" id="themeToggle" type="button" title="Hell/Dunkel umschalten" aria-label="Farbschema umschalten">☼</button>
</header>
<main><div class="content">
<section class="page-hero"><div class="hero-copy"><span class="hero-eyebrow">✚ System Recovery</span><h1>Notfall-Wiederherstellung</h1><p class="page-subtitle">Diagnose, WebUI- und Firmware-Wiederherstellung im aktuellen New Design – weiterhin vollständig autark, falls das reguläre WebUI beschädigt ist.</p></div><span class="mode-chip">● Unabhängige Recovery</span></section>
<div class="status" id="status" role="status">Bereit. Bitte anmelden, um Diagnose und Wiederherstellung zu öffnen.</div>
<section class="card" id="loginCard"><div class="card-head"><span class="card-icon">↪</span><div><h2>Anmelden</h2><p>Verwende die Zugangsdaten der normalen WebUI.</p></div></div><div class="form-grid"><div><label for="user">Benutzer</label><input id="user" value="admin" autocomplete="username"></div><div><label for="pass">Passwort</label><input id="pass" type="password" autocomplete="current-password"></div></div><div class="actions"><button class="action" id="login">Anmelden</button></div></section>
<section id="tools" hidden>
<div class="grid">
<section class="card"><div class="card-head"><span class="card-icon">◎</span><div><h2>Systemstatus</h2><p>Live-Daten werden nur auf Anforderung gelesen.</p></div></div><div class="facts" id="facts"></div><div class="actions"><button class="action secondary" id="refresh">Status aktualisieren</button></div></section>
<section class="card"><div class="card-head"><span class="card-icon">⌁</span><div><h2>Diagnose</h2><p>Crash-Tail und aktuelles Gerätelog abrufen.</p></div></div><div class="actions"><button class="action secondary" id="crash">Crash-Tail laden</button><button class="action secondary" id="logs">Log herunterladen</button></div><div class="diag-output" id="diag">Noch keine Diagnose geladen.</div></section>
<section class="card"><div class="card-head"><span class="card-icon">▣</span><div><h2>WebUI wiederherstellen</h2><p>Nur ein passendes <code>webui_*.bin</code> beziehungsweise <code>spiffs.bin</code> verwenden.</p></div></div><input id="wwwFile" type="file" accept=".bin,application/octet-stream"><div class="actions"><button class="action" id="wwwUpload">WebUI hochladen</button></div><progress id="wwwProgress" max="100" value="0"></progress></section>
<section class="card"><div class="card-head"><span class="card-icon">⇧</span><div><h2>Firmware wiederherstellen</h2><p>Das Gerät startet nach erfolgreichem Firmware-Upload automatisch neu.</p></div></div><input id="fwFile" type="file" accept=".bin,application/octet-stream"><div class="actions"><button class="action" id="fwUpload">Firmware hochladen</button></div><progress id="fwProgress" max="100" value="0"></progress></section>
</div>
<section class="card"><div class="card-head"><span class="card-icon">↻</span><div><h2>Gerätesteuerung</h2><p>Ein Neustart verändert keine gespeicherten Einstellungen.</p></div></div><div class="actions"><button class="action danger" id="restart">Gerät neu starten</button><a class="back-link" href="/">Zur normalen WebUI</a></div></section>
</section>
<script>
let token='',busy=false;const $=id=>document.getElementById(id);const status=m=>$('status').textContent=m;
const headers=(extra={})=>Object.assign({'Authorization':'Token '+token},extra);
const bytes=n=>{n=Number(n)||0;if(n<1024)return n+' B';if(n<1048576)return(n/1024).toFixed(1)+' KB';return(n/1048576).toFixed(2)+' MB'};
const esc=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const shift=(hex,n)=>{const v=parseInt(hex.slice(1),16),p=x=>Math.max(0,Math.min(255,x+n)),a=[p(v>>16&255),p(v>>8&255),p(v&255)];return'#'+a.map(x=>x.toString(16).padStart(2,'0')).join('')};
function setTheme(scheme,color){const dark=scheme==='dark'||(scheme==='system'&&matchMedia('(prefers-color-scheme:dark)').matches);document.documentElement.dataset.theme=dark?'dark':'light';if(/^#[0-9a-f]{6}$/i.test(color||'')){const root=document.documentElement.style;root.setProperty('--color-primary',color);root.setProperty('--color-primary-strong',shift(color,-42));root.setProperty('--color-primary-soft',`${color}${dark?'29':'1f'}`)}$('themeToggle').textContent=dark?'☾':'☼'}
async function loadTheme(){try{const r=await fetch('/api/theme',{cache:'no-store'}),d=await r.json();setTheme(d.colorScheme,d.primaryColor)}catch{setTheme('system','#f26a3d')}}
async function api(url,opt={}){opt.headers=headers(opt.headers||{});const r=await fetch(url,opt);const text=await r.text();if(!r.ok)throw new Error(text||('HTTP '+r.status));try{return JSON.parse(text)}catch{return text}}
async function login(){status('Anmeldung läuft …');const r=await fetch('/login.json',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({username:$('user').value,password:$('pass').value})});const d=await r.json();if(!r.ok||!d.isAuthenticated||!d.token)throw new Error('Anmeldung fehlgeschlagen');token=d.token;$('loginCard').hidden=true;$('tools').hidden=false;status('Angemeldet. Recovery-Werkzeuge sind bereit.');await refresh()}
async function refresh(){const [s,o]=await Promise.all([api('/sysinfo.json?t='+Date.now()),api('/api/system/overview')]);const i=s.sysInfo||{};const used=Number(o.usedInternalHeap)||0,total=Number(o.totalInternalHeap)||0;const rows=[['Firmware',i.currentVersion||'—'],['Laufzeit',Math.floor((i.uptimeSeconds||0)/60)+' min'],['Resetgrund',o.resetReasonText||i.resetReason||'—'],['RAM gesamt',bytes(total)],['RAM belegt',bytes(used)+' ('+(Number(o.internalHeapUsagePercent)||0).toFixed(1)+' %)'],['RAM frei',bytes(o.freeInternalHeap)],['RAM-Minimum',bytes(o.minimumFreeHeap)],['Größter Block',bytes(o.largestFreeBlock)],['PSRAM',o.psramAvailable?bytes(o.totalPsram):'nicht vorhanden'],['WebUI',o.webui?.version||'embedded'],['Crash-Tail',o.logs?.crashTailAvailable?'vorhanden':'nicht vorhanden'],['Logpuffer',o.logs?.enabled?bytes(o.logs.bufferBytes):'inaktiv']];$('facts').innerHTML=rows.map(x=>'<div class="fact"><span>'+esc(x[0])+'</span><strong>'+esc(x[1])+'</strong></div>').join('');$('topFirmware').textContent=i.currentVersion||'—';$('deviceName').textContent=i.hostname||'System Recovery';status('Status aktualisiert.')}
function upload(fileId,url,progressId,label){if(busy)return status('Ein Update läuft bereits.');const f=$(fileId).files[0];if(!f)return status('Bitte zuerst eine BIN-Datei auswählen.');if(!confirm(label+' wirklich starten?'))return;busy=true;const x=new XMLHttpRequest();x.open('POST',url);x.setRequestHeader('Authorization','Token '+token);x.setRequestHeader('Content-Type','application/octet-stream');x.upload.onprogress=e=>{if(e.lengthComputable)$(progressId).value=Math.round(e.loaded*100/e.total)};x.onload=()=>{busy=false;status(x.status>=200&&x.status<300?label+' erfolgreich.':label+' fehlgeschlagen: '+x.responseText);$(progressId).value=x.status>=200&&x.status<300?100:0};x.onerror=()=>{busy=false;status(label+' fehlgeschlagen: Netzwerkfehler')};status(label+' läuft …');x.send(f)}
$('login').onclick=()=>login().catch(e=>status(e.message));$('refresh').onclick=()=>refresh().catch(e=>status(e.message));$('wwwUpload').onclick=()=>upload('wwwFile','/api/webui/update','wwwProgress','WebUI-Upload');$('fwUpload').onclick=()=>upload('fwFile','/ota_update','fwProgress','Firmware-Upload');
$('pass').onkeydown=e=>{if(e.key==='Enter')$('login').click()};$('themeToggle').onclick=()=>{const dark=document.documentElement.dataset.theme==='dark';setTheme(dark?'light':'dark',getComputedStyle(document.documentElement).getPropertyValue('--color-primary').trim())};
$('crash').onclick=async()=>{try{const d=await api('/api/crash_log');$('diag').textContent=d.available?(d.tail||'Crash-Tail leer'):'Kein Crash-Tail vorhanden.';await refresh()}catch(e){$('diag').textContent=e.message}};
$('logs').onclick=async()=>{try{const r=await fetch('/api/log/download',{headers:headers()});if(!r.ok)throw new Error(await r.text());const a=document.createElement('a');a.href=URL.createObjectURL(await r.blob());a.download='hb-rf-eth-log.txt';a.click();setTimeout(()=>URL.revokeObjectURL(a.href),1000)}catch(e){status(e.message)}};
$('restart').onclick=async()=>{if(busy)return status('Ein Update läuft bereits.');if(!confirm('Gerät wirklich neu starten?'))return;try{await api('/api/restart',{method:'POST'});status('Neustart wurde ausgelöst.')}catch(e){status('Neustart ausgelöst; Verbindung wird getrennt.')}};
loadTheme();
</script></div></main></div></body></html>)HTML";

esp_err_t get_recovery_page(httpd_req_t *req)
{
    // Recovery is a static self-contained document. Its entire script body is
    // embedded inline, so it needs a CSP that permits inline scripts. Use the
    // dedicated variant — do NOT also call add_security_headers(), because
    // httpd_resp_set_hdr() appends rather than overwrites, and two CSP headers
    // (one strict, one permissive) would block the inline script again.
    add_security_headers_inline_script(req);
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
    char webui_effective_version[32] = {};
    webui_storage_get_effective_version(webui_effective_version, sizeof(webui_effective_version));
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
                                webui_effective_version);
        cJSON_AddBoolToObject(webui_object, "valid", webui.valid);
        cJSON_AddBoolToObject(webui_object, "mounted", webui.mounted);
        cJSON_AddBoolToObject(webui_object, "manifestValid",
                              webui.manifestValid);
        cJSON_AddBoolToObject(webui_object, "compatible",
                              webui.compatible);
        cJSON_AddStringToObject(webui_object, "compatibilityStatus",
                                webui.compatibilityStatus);
        cJSON_AddNumberToObject(webui_object, "apiVersion",
                                webui.apiVersion);
        cJSON_AddNumberToObject(webui_object, "supportedApiVersion",
                                webui.supportedApiVersion);
        cJSON_AddStringToObject(webui_object, "minFirmwareVersion",
                                webui.minFirmwareVersion);
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
