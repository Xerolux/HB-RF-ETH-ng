export default {
  // Common
  common: {
    enabled: 'Aktivert',
    disabled: 'Deaktivert',
    save: 'Lagre',
    cancel: 'Avbryt',
    close: 'Lukk',
    loading: 'Laster...',
    changing: 'Endrer...',
    error: 'Feil',
    success: 'Suksess',
    yes: 'Ja',
    no: 'Nei'
  },

  // Header Navigation
  nav: {
    home: 'Hjem',
    settings: 'Innstillinger',
    firmware: 'Fastvare',
    monitoring: 'Overvåking',
    systemlog: 'Systemlogg',
    about: 'Om',
    login: 'Logg inn',
    logout: 'Logg ut',
    toggleTheme: 'Bytt tema',
    language: 'Språk'
  },

  // Login Page
  login: {
    title: 'Vennligst logg inn',
    subtitle: 'Skriv inn passordet ditt for å fortsette',
    username: 'Brukernavn',
    password: 'Passord',
    passwordPlaceholder: 'Passord',
    login: 'Logg inn',
    loginFailed: 'Innlogging mislyktes',
    invalidCredentials: 'Ugyldig brukernavn eller passord',
    loginError: 'Innlogging var ikke vellykket.',
    passwordRequired: 'Passord er påkrevd',
    loggingIn: 'Logger inn...'
  },

  // Settings Page
  settings: {
    title: 'Innstillinger',
    ccuSettings: 'CCU-tilkobling',
    ccuIpAddress: 'CCU IP-adresse',
    ccuIpHint: 'Skriv inn IP-adressen til din CCU for å forhindre tilkoblingsblokkering. Systemet starter på nytt etter lagring.',
    tabGeneral: 'Generelt',
    tabNetwork: 'Nettverk',
    tabTime: 'Tid',
    tabBackup: 'Sikkerhetskopi',
    changePassword: 'Endre Passord',
    repeatPassword: 'Gjenta Passord',
    hostname: 'Vertsnavn',

    // Security Settings
    security: 'Sikkerhet',
    changePasswordHint: 'Endre administratorpassordet for sikker tilgang',
    otaPassword: 'OTA-passord',
    otaPasswordHint: 'Separat passord kreves for fastvareoppdateringer',
    changePasswordBtn: 'Endre Passord',
    changeOtaPassword: 'Endre',
    setOtaPassword: 'Angi Passord',
    clearOtaPassword: 'Slett',
    clearOtaPasswordConfirm: 'Er du sikker på at du vil fjerne OTA-passordet? Fastvareoppdateringer vil ikke være mulig før et nytt passord er angitt.',
    clearOtaPasswordSuccess: 'OTA-passordet er fjernet.',
    clearOtaPasswordError: 'Kunne ikke fjerne OTA-passord',

    // Network Settings
    networkSettings: 'Nettverksinnstillinger',
    dhcp: 'DHCP',
    ipAddress: 'IP-adresse',
    netmask: 'Nettmaske',
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
    checkUpdates: 'Se etter oppdateringer',
    allowPrerelease: 'Tillat tidlige oppdateringer (Beta/Alpha)',
    language: 'Språk',

    // Messages
    saveSuccess: 'Innstillingene ble lagret.',
    saveError: 'Det oppsto en feil under lagring av innstillingene.',
    restartTitle: 'Omstart Kreves',
    restartMessage: 'Innstillingene er lagret. Vil du starte enheten på nytt nå for at endringene skal tre i kraft?',
    restartNow: 'Start på nytt nå',
    restartLater: 'Start på nytt senere',

    // Backup & Restore
    backupRestore: 'Sikkerhetskopiering og Gjenoppretting',
    backupInfo: 'Last ned en sikkerhetskopi av innstillingene dine for å gjenopprette dem senere.',
    restoreInfo: 'Last opp en sikkerhetskopifil for å gjenopprette innstillinger. Systemet vil starte på nytt etterpå.',
    downloadBackup: 'Last ned sikkerhetskopi',
    restore: 'Gjenopprett',
    restoreBtn: 'Gjenopprett',
    download: 'Last ned',
    noFileChosen: 'Ingen fil valgt',
    browse: 'Bla gjennom',
    restoreConfirm: 'Er du sikker? Gjeldende innstillinger vil bli overskrevet og systemet vil starte på nytt.',
    restoreSuccess: 'Innstillinger gjenopprettet. Starter systemet på nytt...',
    restoreError: 'Feil ved gjenoppretting av innstillinger',
    backupError: 'Feil ved nedlasting av sikkerhetskopi'
  },

  // System Info
  sysinfo: {
    title: 'Systeminformasjon',
    goodMorning: 'God morgen',
    goodAfternoon: 'God ettermiddag',
    goodEvening: 'God kveld',
    serial: 'Serienummer',
    boardRevision: 'Kortrevisjon',
    uptime: 'oppetid',
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
    latestVersion: 'Siste Versjon',
    memory: 'Minnebruk',
    cpu: 'CPU-bruk',
    temperature: 'Temperatur',
    voltage: 'Forsyningsspenning',
    ethernet: 'Ethernet',
    connected: 'Tilkoblet',
    disconnected: 'Frakoblet',
    speed: 'Hastighet',
    duplex: 'Dupleks',
    radioModule: 'Radiomodul',
    moduleType: 'Modultype',
    firmwareVersion: 'Fastvareversjon',
    bidcosMAC: 'BidCoS Radio MAC',
    hmipMAC: 'HmIP Radio MAC',
    system: 'System',
    network: 'Nettverk',
    down: 'Ned',
    mbits: 'Mbit/s',
    online: 'På nett',
    offline: 'Avslått',
    dashboardTitle: 'Systemstatus'
  },

  // Update
  update: {
    available: 'Oppdatering Tilgjengelig',
    updateNow: 'Oppdater Nå'
  },

  // Firmware Update
  firmware: {
    title: 'Fastvare',
    subtitle: 'Oppdater enhetens fastvare',
    restarting: 'Starter på nytt...',
    restartingText: 'Enheten starter på nytt. Siden lastes inn på nytt automatisk.',
    fileUpload: 'Filopplasting',
    fileUploadHint: 'Last opp en .bin fastvarefil',
    networkUpdate: 'Nettverksoppdatering',
    networkUpdateHint: 'Last ned fra URL',
    urlPlaceholder: 'https://eksempel.no/firmware.bin',
    downloading: 'Laster ned...',
    downloadInstall: 'Last ned og Installer',
    factoryReset: 'Tilbakestill',
    factoryResetHint: 'Tilbakestill til fabrikkinnstillinger',
    factoryResetConfirm: 'Vil du virkelig tilbakestille til fabrikkinnstillinger? Alle innstillinger vil gå tapt.',
    otaProgress: 'Laster ned fastvareoppdatering...',
    otaSuccess: 'Nedlasting vellykket. Installerer...',
    currentVersion: 'Nåværende Versjon',
    installedVersion: 'Installert versjon',
    versionInfo: 'Modernisert fork v2.1.6 av Xerolux (2025) - Basert på originalarbeidet til Alexander Reinert.',
    updateAvailable: 'En oppdatering til versjon {latestVersion} er tilgjengelig.',
    newVersionAvailable: 'Ny versjon {version} er tilgjengelig!',
    viewUpdate: 'Vis',
    onlineUpdate: 'Oppdater Online',
    onlineUpdateConfirm: 'Vil du virkelig laste ned og installere oppdateringen? Systemet starter på nytt automatisk.',
    onlineUpdateStarted: 'Oppdatering startet. Enheten starter på nytt automatisk når den er ferdig.',
    showReleaseNotes: 'Vis utgivelsesnotater',
    releaseNotesTitle: 'Utgivelsesnotater for v{version}',
    releaseNotesError: 'Kunne ikke laste utgivelsesnotater fra GitHub.',
    updateFile: 'Fastvarefil',
    noFileChosen: 'Ingen fil valgt',
    browse: 'Bla gjennom',
    selectFile: 'Velg eller slipp .bin-fil',
    upload: 'Installer Fastvare',
    restart: 'Start på nytt',
    restartHint: 'Start enhet på nytt',
    uploading: 'Laster opp...',
    uploadSuccess: 'Fastvareoppdatering lastet opp. Systemet starter på nytt automatisk om 3 sekunder...',
    uploadError: 'Det oppsto en feil.',
    updateSuccess: 'Fastvare oppdatert',
    updateError: 'Feil ved oppdatering av fastvare',
    warning: 'Advarsel: Ikke koble fra strømmen under oppdateringen!',
    restartConfirm: 'Vil du virkelig starte systemet på nytt?'
  },

  // Monitoring
  monitoring: {
    title: 'Overvåking',
    description: 'Konfigurer SNMP- og CheckMK-overvåking for HB-RF-ETH-gatewayen.',
    save: 'Lagre',
    saving: 'Lagrer...',
    saveSuccess: 'Konfigurasjon lagret!',
    saveError: 'Feil ved lagring av konfigurasjon!',
    snmp: {
      title: 'SNMP-agent',
      enabled: 'Aktiver SNMP',
      port: 'Port',
      portHelp: 'Standard: 161',
      community: 'Community String',
      communityHelp: 'Standard: "public" - Vennligst endre for produksjon!',
      location: 'Plassering',
      locationHelp: 'Valgfritt: f.eks. "Serverrom, Bygg A"',
      contact: 'Kontakt',
      contactHelp: 'Valgfritt: f.eks. "admin@eksempel.no"'
    },
    checkmk: {
      title: 'CheckMK-agent',
      enabled: 'Aktiver CheckMK',
      port: 'Port',
      portHelp: 'Standard: 6556',
      allowedHosts: 'Tillatte Klient-IP-er',
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
      userHelp: 'Valgfritt: MQTT Brukernavn',
      password: 'Passord',
      passwordHelp: 'Valgfritt: MQTT Passord',
      topicPrefix: 'Emne-prefiks',
      topicPrefixHelp: 'Standard: hb-rf-eth - Emner vil være som prefiks/status/...',
      haDiscoveryEnabled: 'Home Assistant Discovery',
      haDiscoveryPrefix: 'Discovery-prefiks',
      haDiscoveryPrefixHelp: 'Standard: homeassistant'
    },
    enable: 'Aktiver',
    allowedHosts: 'Tillatte verter'
  },

  // About Page
  about: {
    title: 'Om',
    version: 'Versjon 2.1.6',
    fork: 'Modernisert Fork',
    forkDescription: 'Denne versjonen er en modernisert fork av Xerolux (2025), basert på den originale HB-RF-ETH-fastvaren. Oppdatert til ESP-IDF 5.3, moderne verktøykjeder og nåværende WebUI-teknologier (Vue 3, Vite, Pinia).',
    original: 'Original Forfatter',
    firmwareLicense: 'Den',
    hardwareLicense: 'Den',
    under: 'er utgitt under',
    description: 'HomeMatic BidCoS/HmIP LAN Gateway',
    author: 'Forfatter',
    license: 'Lisens',
    website: 'Nettsted',
    documentation: 'Dokumentasjon',
    support: 'Støtte'
  },

  // Third Party
  thirdParty: {
    title: 'Tredjepartsprogramvare',
    containsThirdPartySoftware: 'Denne programvaren inneholder gratis tredjepartsprogramvareprodukter som brukes under ulike lisensbetingelser.',
    providedAsIs: 'Programvaren leveres "som den er" UTEN NOEN GARANTI.'
  },

  // Change Password
  changePassword: {
    title: 'Passordbytte kreves',
    subtitle: 'Sikre kontoen din',
    warningTitle: 'Påkrevd',
    requirementsTitle: 'Passordkrav:',
    reqMinLength: 'Minst 6 tegn',
    reqLettersNumbers: 'Må inneholde bokstaver og tall',
    currentPassword: 'Nåværende Passord',
    newPassword: 'Nytt Passord',
    confirmPassword: 'Bekreft Passord',
    changePassword: 'Endre Passord',
    changeSuccess: 'Passord endret',
    changeError: 'Feil ved endring av passord',
    passwordMismatch: 'Passordene samsvarer ikke',
    passwordTooShort: 'Passordet må være minst 6 tegn langt og inneholde bokstaver og tall.',
    passwordRequirements: 'Må inneholde bokstaver og tall',
    passwordsDoNotMatch: 'Passordene samsvarer ikke',
    warningMessage: 'Dette er din første innlogging eller passordet er fortsatt satt til "admin". Av sikkerhetsgrunner må du endre passordet.',
    success: 'Passord endret',
    newPasswordPlaceholder: 'Skriv inn nytt passord',
    confirmPasswordPlaceholder: 'Bekreft nytt passord'
  },

  // OTA Password Modal
  otaPassword: {
    title: 'Angi OTA-passord',
    warningMessage: 'Angi et separat passord for fastvareoppdateringer. Dette er nødvendig for OTA-oppdateringer.',
    otaPassword: 'OTA-passord',
    otaPasswordPlaceholder: 'Angi OTA-passord',
    confirmPassword: 'Bekreft Passord',
    confirmPasswordPlaceholder: 'Bekreft OTA-passord',
    passwordTooShort: 'Passordet må være minst 8 tegn langt',
    passwordRequirements: 'Må inneholde store og små bokstaver og tall',
    passwordsDoNotMatch: 'Passordene samsvarer ikke',
    requirementsTitle: 'Passordkrav:',
    reqMinLength: 'Minst 8 tegn',
    reqMixedCase: 'Store og små bokstaver',
    reqNumbers: 'Minst ett tall',
    strengthWeak: 'Svakt',
    strengthMedium: 'Middels',
    strengthGood: 'Bra',
    strengthStrong: 'Sterkt'
  },

  // Sponsor
  sponsor: {
    title: 'Støtt dette prosjektet',
    description: 'Hvis du liker dette prosjektet og ønsker å støtte utviklingen, kan du bruke ett av alternativene nedenfor.',
    thanks: 'Takk for støtten!'
  },

  // System Log
  systemlog: {
    title: 'Systemlogg',
    description: 'Live visning av systemloggutdata med nedlastingsmulighet.',
    liveLog: 'Live Logg',
    autoScroll: 'Auto-scroll',
    enabled: 'Aktivert',
    disabledMessage: 'Loggvisning er deaktivert. Slå på bryteren for å se live logger.',
    refresh: 'Oppdater',
    clear: 'Tøm',
    download: 'Last ned',
    empty: 'Ingen loggoppføringer ennå.'
  },

  // Changelog
  changelog: {
    title: 'Endringslogg',
    loading: 'Laster...',
    fetching: 'Henter endringslogg fra GitHub...',
    error: 'Kunne ikke laste endringslogg',
    fetchError: 'Kunne ikke hente endringslogg. Vennligst sjekk internettforbindelsen din.',
    retry: 'Prøv igjen',
    close: 'Lukk',
    viewOnGithub: 'Vis på GitHub'
  }
}
