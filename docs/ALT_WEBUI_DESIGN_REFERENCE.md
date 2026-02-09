# HB-RF-ETH-ng ALT WebUI - Design Reference

> **Optional:** Diese Datei dokumentiert das Design der alternativen WebUI aus `alt/webui/`.
> Falls das schlichtere Bootstrap-Design gewuenscht wird, kann diese Referenz als Vorlage dienen.

## Design-Philosophie

Die ALT WebUI ist ein **vereinfachtes, Bootstrap-basiertes** Interface:
- **Minimalistisch** - nur Bootstrap 5 + Bootstrap-Vue-Next, kein custom CSS
- **Hash-basiertes Routing** (`#/` URLs) statt History API
- **Card-basierte Layouts** mit farbigen Headers
- **Standard-Formular-Elemente** - keine custom Controls
- **Funktional statt dekorativ**

## Vergleich: ALT vs MAIN WebUI

| Feature | ALT WebUI | MAIN WebUI |
|---------|-----------|-----------|
| Routing | Hash-basiert (#/) | History API (clean URLs) |
| Header | Bootstrap Navbar (dark primary) | Custom modern Header mit Update-Banner |
| Settings | Eine lange Seite | Tabbed Interface (4 Tabs) |
| Custom CSS | Keines - nur Bootstrap | 500+ Zeilen custom CSS (Apple/OneUI-inspiriert) |
| Design System | Bootstrap Defaults | Custom Farb-Variablen, Schatten, Radius |
| Cards | Standard Bootstrap | Custom rounded mit Schatten |
| Buttons | Bootstrap Varianten | Custom pill-shaped mit Schatten |
| Mobile | Basic responsive | Enhanced mit safe area, notch support |

## Technologie-Stack

- Vue 3 mit Composition API (`<script setup>`)
- Bootstrap 5 + Bootstrap-Vue-Next (explizite Imports)
- Pinia State Management
- Vue Router (Hash History)
- Vue I18n
- Vuelidate
- Axios

## CSS-Imports (nur Standard-Bootstrap)

```javascript
import 'bootstrap/dist/css/bootstrap.css'
import 'bootstrap-vue-next/dist/bootstrap-vue-next.css'
// Kein custom CSS!
```

## Layout-Pattern

### App Container
```vue
<div id="app" class="container" style="max-width: 800px">
  <Header />
  <RouterView />
</div>
```

### Navigation Header
```vue
<BNavbar type="dark" variant="primary" toggleable="md" class="mb-3 mt-3">
  <BNavbarBrand tag="h1" class="font-weight-bold">HB-RF-ETH-ng</BNavbarBrand>
  <BNavbarToggle target="nav-collapse" />
  <BCollapse id="nav-collapse" is-nav>
    <BNavbarNav>
      <BNavItem to="/">Home</BNavItem>
      <BNavItemDropdown v-if="loginStore.isLoggedIn" text="Settings">
        <BDropdownItem to="/settings">Settings</BDropdownItem>
        <BDropdownItem to="/firmware">Firmware</BDropdownItem>
        <BDropdownItem to="/monitoring">Monitoring</BDropdownItem>
      </BNavItemDropdown>
      <BNavItem to="/about">About</BNavItem>
    </BNavbarNav>
    <BNavbarNav class="ms-auto">
      <!-- Login/Logout, Language, Theme -->
    </BNavbarNav>
  </BCollapse>
</BNavbar>
```

### Card-Muster (fuer alle Seiten)
```vue
<BCard
  :header="title"
  header-tag="h6"
  header-bg-variant="secondary"   <!-- secondary=grau, danger=rot, info=blau -->
  header-text-variant="white"
  class="mb-3"
>
  <BForm>
    <BFormGroup :label="label" label-cols-sm="4">
      <BFormInput v-model="value" />
    </BFormGroup>
  </BForm>
</BCard>
```

### Formular-Layout
- **Label-Breite:** `label-cols-sm="4"` (4 Spalten auf kleinen Screens)
- **Sections:** `<h6 class="text-secondary">Abschnittstitel</h6>` + `<hr />`
- **Toggle:** `<BFormCheckbox v-model="enabled" switch />`
- **Radio-Buttons:** `<BFormRadioGroup buttons v-model="value">`
- **Dropdown:** `<BFormSelect v-model="value">`
- **Datei-Upload:** `<BFormFile v-model="file" accept=".bin" />`

### Login-Seite
```vue
<BCard header-bg-variant="secondary">
  <BForm>
    <BFormGroup :label="'Passwort'" label-cols-sm="4">
      <BFormInput type="password" v-model="password" />
    </BFormGroup>
    <BAlert variant="danger" v-if="error">Fehler</BAlert>
    <BButton type="submit" variant="primary" block>Anmelden</BButton>
  </BForm>
</BCard>
```

### System Info (mit Progress-Bars)
```vue
<BFormGroup :label="'CPU'" label-cols-sm="4">
  <BProgress :max="100" height="2.25rem" class="form-control p-0">
    <BProgressBar :value="cpuUsage">
      <span class="position-absolute w-100 text-dark d-flex justify-content-center">
        {{ cpuUsage.toFixed(2) }}%
      </span>
    </BProgressBar>
  </BProgress>
</BFormGroup>
```

### Firmware-Update
```vue
<BFormGroup :label="'Firmware-Datei'" label-cols-sm="4">
  <BFormFile v-model="file" accept=".bin" />
</BFormGroup>
<BProgress :value="progress" :max="100" animated v-if="progress > 0" />
<BButton variant="primary" block @click="upload" :disabled="!file">
  <BSpinner small v-if="uploading" class="me-2" />
  Hochladen
</BButton>
```

### Modals (Bestaetigungen)
```vue
<BModal v-model="showModal" :title="'Neustart'" @ok="confirmAction" ok-variant="warning">
  <p>Wirklich neu starten?</p>
</BModal>
```

## Bootstrap-Vue Komponenten-Liste

### Navigation
`BNavbar`, `BNavbarBrand`, `BNavbarToggle`, `BCollapse`, `BNavbarNav`,
`BNavItem`, `BNavItemDropdown`, `BDropdownItem`

### Formulare
`BForm`, `BFormGroup`, `BFormInput`, `BFormSelect`, `BFormSelectOption`,
`BFormRadio`, `BFormRadioGroup`, `BFormCheckbox`, `BFormFile`,
`BFormText`, `BFormInvalidFeedback`, `BInputGroup`

### Layout & Feedback
`BCard`, `BAlert`, `BProgress`, `BProgressBar`, `BSpinner`,
`BListGroup`, `BListGroupItem`, `BModal`, `BButton`

## Haeufig verwendete Bootstrap-Klassen

```
mb-3, mt-3, mb-2, ms-auto, me-2    -- Spacing
d-flex, align-items-center           -- Flexbox
form-control, form-check, form-switch -- Forms
badge, bg-warning, bg-success        -- Badges
text-dark, text-secondary, text-warning -- Text
container                            -- Layout
```

## Seiten-Struktur

| Seite | Beschreibung | Auth |
|-------|-------------|------|
| `/` | Home → SysInfo anzeigen | Nein |
| `/settings` | Alle Einstellungen (eine lange Seite) | Ja |
| `/firmware` | Firmware-Upload + Neustart | Ja |
| `/monitoring` | SNMP, CheckMK, MQTT Config | Ja |
| `/log` | System-Log (nicht implementiert) | Ja |
| `/analyzer` | Paket-Analyzer (nicht implementiert) | Ja |
| `/change-password` | Passwort aendern | Ja |
| `/about` | Info + Lizenzen | Nein |
| `/login` | Anmeldung | Nein |

## Store-Struktur (Pinia)

```
useThemeStore     - theme (light/dark), localStorage
useLoginStore     - isLoggedIn, token, passwordChanged, sessionStorage
useSysInfoStore   - serial, versions, uptime, cpu, memory, ethernet, radio
useSettingsStore  - hostname, DHCP, IPs, time, LED, HMLGW, DTLS
useFirmwareUpdateStore - progress
useMonitoringStore - snmp, checkmk, mqtt
```

## Wichtig: Fehlende Komponenten in ALT

- **PasswordInput.vue** - Referenziert aber existiert nicht
- **analyzer.vue** - Route vorhanden, Datei fehlt
- **log.vue** - Route vorhanden, Datei fehlt
