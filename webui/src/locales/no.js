export default {
  // Common
  common: {
    enabled: 'Aktivert',
    disabled: 'Deaktivert',
    save: 'Lagre',
    cancel: 'Avbryt',
    close: 'Lukk',
    loading: 'Laster...',
    error: 'Feil',
    success: 'Suksess',
    yes: 'Ja',
    no: 'Nei',
    showPassword: 'Vis passord',
    hidePassword: 'Skjul passord',
    rebootingWait: 'Systemet starter på nytt. Vennligst vent ca. 10 sekunder...',
    factoryResettingWait: 'Systemet tilbakestilles til fabrikkinnstillinger og starter på nytt. Vennligst vent...',
    confirmReboot: 'Er du sikker på at du vil starte systemet på nytt?',
    confirmFactoryReset: 'Er du sikker? Alle innstillinger vil gå tapt!'
  },

  // Header Navigation
  nav: {
    home: 'Hjem',
    settings: 'Innstillinger',
    firmware: 'Fastvare',
    monitoring: 'Overvåkning',
    analyzer: 'Analyzer',
    about: 'Om',
    login: 'Logg inn',
    logout: 'Logg ut',
    toggleTheme: 'Bytt tema'
  },

  // Login Page
  login: {
    title: 'Logg inn',
    username: 'Brukernavn',
    password: 'Passord',
    login: 'Logg inn',
    loginFailed: 'Pålogging mislyktes',
    invalidCredentials: 'Ugyldige påloggingsdetaljer',
    loginError: 'Pålogging var ikke vellykket.'
  },

  // Settings Page
  settings: {
    title: 'Innstillinger',
    changePassword: 'Endre passord',
    repeatPassword: 'Gjenta passord',
    hostname: 'Vertsnavn',

    // Network Settings
    networkSettings: 'Nettverksinnstillinger',
    dhcp: 'DHCP',
    ipAddress: 'IP-adresse',
    netmask: 'Nettverksmaske',
    gateway: 'Gateway',
    dns1: 'Primær DNS',
    dns2: 'Sekundær DNS',

    // IPv6 Settings
    ipv6Settings: 'IPv6-innstillinger',
    enableIPv6: 'Aktiver IPv6',
    ipv6Mode: 'IPv6-modus',
    ipv6Auto: 'Automatisk (SLAAC)',
    ipv6Static: 'Statisk',
    ipv6Address: 'IPv6-adresse',
    ipv6PrefixLength: 'Prefikslengde',
    ipv6Gateway: 'IPv6 Gateway',
    ipv6Dns1: 'Primær IPv6 DNS',
    ipv6Dns2: 'Sekundær IPv6 DNS',

    // Time Settings
    timeSettings: 'Tidsinnstillinger',
    timesource: 'Tidskilde',
    ntp: 'NTP',
    dcf: 'DCF',
    gps: 'GPS',
    ntpServer: 'NTP-server',
    dcfOffset: 'DCF-offset',
    gpsBaudrate: 'GPS Baudrate',

    // System Settings
    systemSettings: 'Systeminnstillinger',
    ledBrightness: 'LED-lysstyrke',
    language: 'Språk',
    analyzerSettings: 'Analyzer Light-innstillinger',
    enableAnalyzer: 'Aktiver Analyzer Light',
    systemMaintenance: 'Systemvedlikehold',
    reboot: 'Omstart',
    factoryReset: 'Fabrikkinnstillinger',

    // HMLGW
    hmlgwSettings: 'HomeMatic LAN Gateway (HMLGW) innstillinger',
    enableHmlgw: 'Aktiver HMLGW-modus',
    hmlgwPort: 'Dataport (Standard: 2000)',
    hmlgwKeepAlivePort: 'KeepAlive-port (Standard: 2001)',

    // DTLS Security Settings
    dtls: {
      title: 'DTLS Kryptering',
      description: 'Sikker transportkryptering for kommunikasjon mellom kort og CCU på port 3008.',
      mode: 'Krypteringsmodus',
      modeDisabled: 'Deaktivert (Standard)',
      modePsk: 'Forhåndsdelt nøkkel (PSK)',
      modeCert: 'X.509 Sertifikat',
      cipherSuite: 'Cipher Suite',
      cipherAes128: 'AES-128-GCM-SHA256',
      cipherAes256: 'AES-256-GCM-SHA384 (Anbefalt)',
      cipherChacha: 'ChaCha20-Poly1305-SHA256',
      requireClientCert: 'Krev klientsertifikat',
      sessionResumption: 'Aktiver øktgjenopptagelse',
      pskManagement: 'PSK Administrasjon',
      pskIdentity: 'PSK Identitet',
      pskKey: 'PSK Nøkkel (Hex)',
      pskGenerate: 'Generer ny PSK',
      pskGenerating: 'Genererer PSK...',
      pskGenerated: 'Ny PSK generert',
      pskCopyWarning: 'VIKTIG: Kopier denne nøkkelen nå! Den vises bare én gang.',
      pskKeyLength: 'Nøkkellengde',
      psk128bit: '128 Bit',
      psk256bit: '256 Bit (Anbefalt)',
      psk384bit: '384 Bit',
      psk512bit: '512 Bit',
      pskStatus: 'PSK Status',
      pskConfigured: 'Konfigurert',
      pskNotConfigured: 'Ikke konfigurert',
      warningDisabled: 'Kommunikasjonen er UKRYPTERT. Alle på nettverket kan fange opp trafikken.',
      warningPsk: 'Sørg for at PSK lagres sikkert på CCU.',
      info: 'DTLS 1.2 krypterer Raw-UART UDP-kommunikasjon ende-til-ende. CCU må også støtte DTLS.',
      documentation: 'Dokumentasjon for CCU-utviklere',
      viewDocs: 'Vis implementeringsveiledning',
      restartNote: 'Endringer i DTLS-innstillinger krever en systemomstart.'
    },

    // Messages
    saveSuccess: 'Innstillingene ble lagret. Start systemet på nytt for å ta dem i bruk.',
    saveError: 'Det oppstod en feil under lagring av innstillingene.',

    // Backup & Restore
    backupRestore: 'Sikkerhetskopi og gjenoppretting',
    backupInfo: 'Last ned en sikkerhetskopi av innstillingene dine for å gjenopprette dem senere.',
    restoreInfo: 'Last opp en sikkerhetskopifil for å gjenopprette innstillinger. Systemet vil starte på nytt etterpå.',
    downloadBackup: 'Last ned sikkerhetskopi',
    restore: 'Gjenopprett',
    noFileChosen: 'Ingen fil valgt',
    browse: 'Bla gjennom',
    restoreConfirm: 'Er du sikker? Nåværende innstillinger vil bli overskrevet og systemet vil starte på nytt.',
    restoreSuccess: 'Innstillinger gjenopprettet. Systemet starter på nytt...',
    restoreError: 'Feil ved gjenoppretting av innstillinger',
    backupError: 'Feil ved nedlasting av sikkerhetskopi'
  },

  // System Info
  sysinfo: {
    title: 'Systeminformasjon',
    serial: 'Serienummer',
    boardRevision: 'Kortrevisjon',
    uptime: 'Oppetid',
    resetReason: 'Siste omstart',
    cpuUsage: 'CPU-bruk',
    memoryUsage: 'Minnebruk',
    ethernetStatus: 'Ethernet-tilkobling',
    rawUartRemoteAddress: 'Tilkoblet med',
    radioModuleType: 'Radiomodultype',
    radioModuleSerial: 'Serienummer',
    radioModuleFirmware: 'Fastvareversjon',
    radioModuleBidCosRadioMAC: 'Radioadresse (BidCoS)',
    radioModuleHmIPRadioMAC: 'Radioadresse (HmIP)',
    radioModuleSGTIN: 'SGTIN',
    version: 'Versjon',
    memory: 'Minnebruk',
    cpu: 'CPU-bruk',
    ethernet: 'Ethernet',
    connected: 'Tilkoblet',
    disconnected: 'Frakoblet',
    speed: 'Hastighet',
    duplex: 'Duplex',
    radioModule: 'Radiomodul',
    moduleType: 'Modultype',
    firmwareVersion: 'Fastvareversjon',
    bidcosMAC: 'BidCoS Radio MAC',
    hmipMAC: 'HmIP Radio MAC'
  },

  // Firmware Update
  firmware: {
    title: 'Fastvare',
    currentVersion: 'Nåværende versjon',
    installedVersion: 'Installert versjon',
    versionInfo: 'Modernisert fork v2.1 av Xerolux (2025) - Basert på det opprinnelige arbeidet til Alexander Reinert.',
    updateFile: 'Fastvarefil',
    noFileChosen: 'Ingen fil valgt',
    browse: 'Bla gjennom',
    selectFile: 'Velg fil',
    upload: 'Last opp',
    restart: 'Start systemet på nytt',
    uploading: 'Laster opp...',
    uploadSuccess: 'Fastvareoppdatering lastet opp. Systemet starter på nytt automatisk om 3 sekunder...',
    uploadError: 'En feil oppstod.',
    updateSuccess: 'Fastvare oppdatert',
    updateError: 'Feil ved oppdatering av fastvare',
    warning: 'Advarsel: Ikke koble fra strømmen under oppdateringen!',
    restartConfirm: 'Vil du virkelig starte systemet på nytt?'
  },

  // Monitoring
  monitoring: {
    title: 'Overvåkning',
    description: 'Konfigurer SNMP og CheckMK overvåking for HB-RF-ETH gateway.',
    save: 'Lagre',
    saving: 'Lagrer...',
    saveSuccess: 'Konfigurasjon lagret!',
    saveError: 'Feil ved lagring av konfigurasjon!',
    snmp: {
      title: 'SNMP Agent',
      enabled: 'Aktiver SNMP',
      port: 'Port',
      portHelp: 'Standard: 161',
      community: 'Community String',
      communityHelp: 'Standard: "public" - Vennligst endre for produksjon!',
      location: 'Plassering',
      locationHelp: 'Valgfritt: f.eks. "Serverrom, Bygning A"',
      contact: 'Kontakt',
      contactHelp: 'Valgfritt: f.eks. "admin@example.com"'
    },
    checkmk: {
      title: 'CheckMK Agent',
      enabled: 'Aktiver CheckMK',
      port: 'Port',
      portHelp: 'Standard: 6556',
      allowedHosts: 'Tillatte klient-IP-er',
      allowedHostsHelp: 'Kommaseparerte IP-adresser (f.eks. "192.168.1.10,192.168.1.20") eller "*" for alle'
    },
    mqtt: {
      title: 'MQTT-klient',
      enabled: 'Aktiver MQTT',
      server: 'Server',
      serverHelp: 'MQTT Broker vertsnavn eller IP',
      port: 'Port',
      portHelp: 'Standard: 1883',
      user: 'Bruker',
      userHelp: 'Valgfritt: MQTT-brukernavn',
      password: 'Passord',
      passwordHelp: 'Valgfritt: MQTT-passord',
      topicPrefix: 'Emne-prefiks',
      topicPrefixHelp: 'Standard: hb-rf-eth - Emner vil være som prefiks/status/...',
      haDiscoveryEnabled: 'Home Assistant Discovery',
      haDiscoveryPrefix: 'Discovery-prefiks',
      haDiscoveryPrefixHelp: 'Standard: homeassistant'
    },
    enable: 'Aktiver',
    allowedHosts: 'Tillatte verter'
  },

  // Analyzer
  analyzer: {
    title: 'Analyzer Light',
    disabled: 'Analyzer Light-funksjonen er deaktivert. Vennligst aktiver den i Innstillinger.',
    connected: 'Tilkoblet',
    disconnected: 'Frakoblet',
    clear: 'Tøm',
    autoScroll: 'Automatisk rulling',
    time: 'Tid',
    len: 'Lengde',
    cnt: 'Antall',
    type: 'Type',
    src: 'Kilde',
    dst: 'Destinasjon',
    payload: 'Payload',
    rssi: 'RSSI',
    deviceNames: 'Enhetsnavn',
    address: 'Adresse',
    name: 'Navn',
    storedNames: 'Lagrede navn'
  },

  // About Page
  about: {
    title: 'Om',
    version: 'Versjon 2.1.0',
    fork: 'Modernisert fork',
    forkDescription: 'Denne versjonen er en modernisert fork av Xerolux (2025), basert på den opprinnelige HB-RF-ETH fastvaren. Oppdatert til ESP-IDF 5.3, moderne verktøykjeder (GCC 13.2.0) og nåværende WebUI-teknologier (Vue 3, Parcel 2, Pinia).',
    original: 'Opprinnelig forfatter',
    firmwareLicense: '',
    hardwareLicense: '',
    under: 'er utgitt under',
    description: 'HomeMatic BidCoS/HmIP LAN Gateway',
    author: 'Forfatter',
    license: 'Lisens',
    website: 'Nettside',
    documentation: 'Dokumentasjon',
    support: 'Støtte'
  },

  // Third Party
  thirdParty: {
    title: 'Tredjepartsprogramvare',
    containsThirdPartySoftware: 'Denne programvaren inneholder gratis tredjepartsprogramvare brukt under forskjellige lisensvilkår.',
    providedAsIs: 'Programvaren leveres "SOM DEN ER" UTEN NOEN GARANTI.'
  },

  // Change Password
  changePassword: {
    title: 'Passordendring kreves',
    currentPassword: 'Nåværende passord',
    newPassword: 'Nytt passord',
    confirmPassword: 'Bekreft passord',
    changePassword: 'Endre passord',
    changeSuccess: 'Passord endret',
    changeError: 'Feil ved endring av passord',
    passwordMismatch: 'Passordene samsvarer ikke',
    passwordTooShort: 'Passordet må være minst 6 tegn langt og inneholde bokstaver og tall.',
    passwordsDoNotMatch: 'Passordene samsvarer ikke',
    warningMessage: 'Dette er din første pålogging eller passordet er fortsatt satt til "admin". Av sikkerhetsgrunner må du endre passordet.',
    success: 'Passord endret'
  }
}
