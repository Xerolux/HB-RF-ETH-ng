export default {
  // Common
  common: {
    enabled: 'Aktiverad',
    disabled: 'Inaktiverad',
    save: 'Spara',
    cancel: 'Avbryt',
    close: 'Stäng',
    loading: 'Laddar...',
    changing: 'Ändrar...',
    error: 'Fel',
    success: 'Framgång',
    yes: 'Ja',
    no: 'Nej'
  },

  // Header Navigation
  nav: {
    home: 'Hem',
    settings: 'Inställningar',
    firmware: 'Firmware',
    monitoring: 'Övervakning',
    systemlog: 'Systemlogg',
    about: 'Om',
    login: 'Logga in',
    logout: 'Logga ut',
    toggleTheme: 'Växla tema',
    language: 'Språk'
  },

  // Login Page
  login: {
    title: 'Vänligen logga in',
    subtitle: 'Vänligen ange ditt lösenord för att fortsätta',
    username: 'Användarnamn',
    password: 'Lösenord',
    passwordPlaceholder: 'Lösenord',
    login: 'Logga in',
    loginFailed: 'Inloggning misslyckades',
    invalidCredentials: 'Ogiltiga inloggningsuppgifter',
    loginError: 'Inloggningen lyckades inte.',
    passwordRequired: 'Lösenord krävs',
    loggingIn: 'Loggar in...'
  },

  // Settings Page
  settings: {
    title: 'Inställningar',
    ccuSettings: 'CCU Anslutning',
    ccuIpAddress: 'CCU IP-adress',
    ccuIpHint: 'Ange IP-adressen till din CCU för att förhindra anslutningsblockering. Systemet startar om efter att ha sparat.',
    tabGeneral: 'Allmänt',
    tabNetwork: 'Nätverk',
    tabTime: 'Tid',
    tabBackup: 'Säkerhetskopia',
    changePassword: 'Byt Lösenord',
    repeatPassword: 'Upprepa Lösenord',
    hostname: 'Värdnamn',

    // Security Settings
    security: 'Säkerhet',
    changePasswordHint: 'Ändra ditt administratörslösenord för säker åtkomst',
    otaPassword: 'OTA Lösenord',
    otaPasswordHint: 'Separat lösenord krävs för firmware-uppdateringar',
    changePasswordBtn: 'Byt Lösenord',
    changeOtaPassword: 'Ändra',
    setOtaPassword: 'Ange Lösenord',
    clearOtaPassword: 'Rensa',
    clearOtaPasswordConfirm: 'Är du säker på att du vill ta bort OTA-lösenordet? Firmware-uppdateringar kommer inte att vara möjliga förrän ett nytt lösenord har angetts.',
    clearOtaPasswordSuccess: 'OTA-lösenordet har tagits bort.',
    clearOtaPasswordError: 'Misslyckades med att ta bort OTA-lösenord',

    // Network Settings
    networkSettings: 'Nätverksinställningar',
    dhcp: 'DHCP',
    ipAddress: 'IP-adress',
    netmask: 'Nätmask',
    gateway: 'Gateway',
    dns1: 'Primär DNS',
    dns2: 'Sekundär DNS',

    // IPv6 Settings
    ipv6Settings: 'IPv6 Inställningar',
    enableIPv6: 'Aktivera IPv6',
    ipv6Mode: 'IPv6 Läge',
    ipv6Auto: 'Automatisk (SLAAC)',
    ipv6Static: 'Statisk',
    ipv6Address: 'IPv6 Adress',
    ipv6PrefixLength: 'Prefixlängd',
    ipv6Gateway: 'IPv6 Gateway',
    ipv6Dns1: 'Primär IPv6 DNS',
    ipv6Dns2: 'Sekundär IPv6 DNS',

    // Time Settings
    timeSettings: 'Tidsinställningar',
    timesource: 'Tidskälla',
    ntp: 'NTP',
    dcf: 'DCF',
    gps: 'GPS',
    ntpServer: 'NTP Server',
    dcfOffset: 'DCF Offset',
    gpsBaudrate: 'GPS Baudrate',

    // System Settings
    systemSettings: 'Systeminställningar',
    ledBrightness: 'LED Ljusstyrka',
    updateLedBlink: 'Blinka LED vid uppdateringar',
    checkUpdates: 'Sök efter uppdateringar',
    allowPrerelease: 'Tillåt tidiga uppdateringar (Beta/Alpha)',
    language: 'Språk',

    // Messages
    saveSuccess: 'Inställningarna sparades framgångsrikt.',
    saveError: 'Ett fel uppstod när inställningarna sparades.',
    restartTitle: 'Omstart Krävs',
    restartMessage: 'Inställningarna har sparats. Vill du starta om enheten nu för att ändringarna ska träda i kraft?',
    restartNow: 'Starta Om Nu',
    restartLater: 'Starta Om Senare',

    // Backup & Restore
    backupRestore: 'Säkerhetskopiering & Återställning',
    backupInfo: 'Ladda ner en säkerhetskopia av dina inställningar för att återställa dem senare.',
    restoreInfo: 'Ladda upp en säkerhetskopia för att återställa inställningar. Systemet kommer att starta om efteråt.',
    downloadBackup: 'Ladda Ner Säkerhetskopia',
    restore: 'Återställ',
    restoreBtn: 'Återställ',
    download: 'Ladda Ner',
    noFileChosen: 'Ingen fil vald',
    browse: 'Bläddra',
    restoreConfirm: 'Är du säker? Nuvarande inställningar kommer att skrivas över och systemet kommer att starta om.',
    restoreSuccess: 'Inställningar återställda framgångsrikt. Startar om systemet...',
    restoreError: 'Fel vid återställning av inställningar',
    backupError: 'Fel vid nedladdning av säkerhetskopia'
  },

  // System Info
  sysinfo: {
    title: 'Systeminformation',
    goodMorning: 'God morgon',
    goodAfternoon: 'God eftermiddag',
    goodEvening: 'God kväll',
    serial: 'Serienummer',
    boardRevision: 'Kortrevision',
    uptime: 'Drifttid',
    resetReason: 'Senaste omstart',
    cpuUsage: 'CPU-användning',
    memoryUsage: 'Minnesanvändning',
    ethernetStatus: 'Ethernet-anslutning',
    rawUartRemoteAddress: 'Ansluten med',
    radioModuleType: 'Radiomodultyp',
    radioModuleSerial: 'Serienummer',
    radioModuleFirmware: 'Firmware-version',
    radioModuleBidCosRadioMAC: 'Radioadress (BidCoS)',
    radioModuleHmIPRadioMAC: 'Radioadress (HmIP)',
    radioModuleSGTIN: 'SGTIN',
    version: 'Version',
    latestVersion: 'Senaste Version',
    memory: 'Minnesanvändning',
    cpu: 'CPU-användning',
    temperature: 'Temperatur',
    voltage: 'Matningsspänning',
    ethernet: 'Ethernet',
    connected: 'Ansluten',
    disconnected: 'Frånkopplad',
    speed: 'Hastighet',
    duplex: 'Duplex',
    radioModule: 'Radiomodul',
    moduleType: 'Modultyp',
    firmwareVersion: 'Firmware Version',
    bidcosMAC: 'BidCoS Radio MAC',
    hmipMAC: 'HmIP Radio MAC',
    system: 'System',
    network: 'Nätverk',
    down: 'Nere',
    mbits: 'Mbit/s',
    online: 'Online',
    offline: 'Offline',
    dashboardTitle: 'Systemstatus'
  },

  // Update
  update: {
    available: 'Uppdatering Tillgänglig',
    updateNow: 'Uppdatera Nu'
  },

  // Firmware Update
  firmware: {
    title: 'Firmware',
    subtitle: 'Uppdatera din enhets firmware',
    restarting: 'Startar om...',
    restartingText: 'Enheten startar om. Sidan laddas om automatiskt.',
    fileUpload: 'Filuppladdning',
    fileUploadHint: 'Ladda upp en .bin firmware-fil',
    networkUpdate: 'Nätverksuppdatering',
    networkUpdateHint: 'Ladda ner från URL',
    urlPlaceholder: 'https://exempel.com/firmware.bin',
    downloading: 'Laddar ner...',
    downloadInstall: 'Ladda Ner & Installera',
    factoryReset: 'Återställ',
    factoryResetHint: 'Återställ till fabriksinställningar',
    factoryResetConfirm: 'Vill du verkligen återställa till fabriksinställningarna? Alla inställningar kommer att gå förlorade.',
    otaProgress: 'Laddar ner firmware-uppdatering...',
    otaSuccess: 'Nedladdning lyckades. Installerar...',
    currentVersion: 'Nuvarande Version',
    installedVersion: 'Installerad version',
    versionInfo: 'Moderniserad fork v2.1.8 av Xerolux (2025) - Baserad på originalarbetet av Alexander Reinert.',
    updateAvailable: 'En uppdatering till version {latestVersion} finns tillgänglig.',
    newVersionAvailable: 'Ny version {version} finns tillgänglig!',
    viewUpdate: 'Visa',
    onlineUpdate: 'Uppdatera Online',
    onlineUpdateConfirm: 'Vill du verkligen ladda ner och installera uppdateringen? Systemet startar om automatiskt.',
    onlineUpdateStarted: 'Uppdatering startad. Enheten startar om automatiskt när den är klar.',
    showReleaseNotes: 'Visa Release Notes',
    releaseNotesTitle: 'Release Notes för v{version}',
    releaseNotesError: 'Misslyckades med att ladda release notes från GitHub.',
    updateFile: 'Firmware-fil',
    noFileChosen: 'Ingen fil vald',
    browse: 'Bläddra',
    selectFile: 'Välj eller släpp .bin-fil',
    upload: 'Installera Firmware',
    restart: 'Starta Om',
    restartHint: 'Starta om enhet',
    uploading: 'Laddar upp...',
    uploadSuccess: 'Firmware-uppdatering uppladdad framgångsrikt. Systemet startar om automatiskt om 3 sekunder...',
    uploadError: 'Ett fel inträffade.',
    updateSuccess: 'Firmware uppdaterad framgångsrikt',
    updateError: 'Fel vid uppdatering av firmware',
    warning: 'Varning: Koppla inte bort strömmen under uppdateringen!',
    restartConfirm: 'Vill du verkligen starta om systemet?'
  },

  // Monitoring
  monitoring: {
    title: 'Övervakning',
    description: 'Konfigurera SNMP- och CheckMK-övervakning för HB-RF-ETH-gatewayen.',
    save: 'Spara',
    saving: 'Sparar...',
    saveSuccess: 'Konfiguration sparad framgångsrikt!',
    saveError: 'Fel vid sparande av konfiguration!',
    snmp: {
      title: 'SNMP Agent',
      enabled: 'Aktivera SNMP',
      port: 'Port',
      portHelp: 'Standard: 161',
      community: 'Community String',
      communityHelp: 'Standard: "public" - Vänligen ändra för produktion!',
      location: 'Plats',
      locationHelp: 'Valfritt: t.ex. "Serverrum, Byggnad A"',
      contact: 'Kontakt',
      contactHelp: 'Valfritt: t.ex. "admin@exempel.com"'
    },
    checkmk: {
      title: 'CheckMK Agent',
      enabled: 'Aktivera CheckMK',
      port: 'Port',
      portHelp: 'Standard: 6556',
      allowedHosts: 'Tillåtna Klient-IP:er',
      allowedHostsHelp: 'Kommaseparerade IP-adresser (t.ex. "192.168.1.10,192.168.1.20") eller "*" för alla'
    },
    mqtt: {
      title: 'MQTT Klient',
      enabled: 'Aktivera MQTT',
      server: 'Server',
      serverHelp: 'MQTT Broker Värdnamn eller IP',
      port: 'Port',
      portHelp: 'Standard: 1883',
      user: 'Användare',
      userHelp: 'Valfritt: MQTT Användarnamn',
      password: 'Lösenord',
      passwordHelp: 'Valfritt: MQTT Lösenord',
      topicPrefix: 'Ämnesprefix',
      topicPrefixHelp: 'Standard: hb-rf-eth - Ämnen kommer att vara som prefix/status/...',
      haDiscoveryEnabled: 'Home Assistant Discovery',
      haDiscoveryPrefix: 'Discovery Prefix',
      haDiscoveryPrefixHelp: 'Standard: homeassistant'
    },
    enable: 'Aktivera',
    allowedHosts: 'Tillåtna Värdar'
  },

  // About Page
  about: {
    title: 'Om',
    version: 'Version 2.1.8',
    fork: 'Moderniserad Fork',
    forkDescription: 'Denna version är en moderniserad fork av Xerolux (2025), baserad på den ursprungliga HB-RF-ETH-firmwaren. Uppdaterad till ESP-IDF 5.3, moderna verktygskedjor och nuvarande WebUI-teknologier (Vue 3, Vite, Pinia).',
    original: 'Ursprunglig Författare',
    firmwareLicense: 'Den',
    hardwareLicense: 'Den',
    under: 'är utgiven under',
    description: 'HomeMatic BidCoS/HmIP LAN Gateway',
    author: 'Författare',
    license: 'Licens',
    website: 'Webbplats',
    documentation: 'Dokumentation',
    support: 'Support'
  },

  // Third Party
  thirdParty: {
    title: 'Tredjepartsprogramvara',
    containsThirdPartySoftware: 'Denna programvara innehåller gratis tredjepartsprogramvaruprodukter som används under olika licensvillkor.',
    providedAsIs: 'Programvaran tillhandahålls "som den är" UTAN NÅGON GARANTI.'
  },

  // Change Password
  changePassword: {
    title: 'Lösenordsbyte krävs',
    subtitle: 'Säkra ditt konto',
    warningTitle: 'Krävs',
    requirementsTitle: 'Lösenordskrav:',
    reqMinLength: 'Minst 6 tecken',
    reqLettersNumbers: 'Måste innehålla bokstäver och siffror',
    currentPassword: 'Nuvarande Lösenord',
    newPassword: 'Nytt Lösenord',
    confirmPassword: 'Bekräfta Lösenord',
    changePassword: 'Byt Lösenord',
    changeSuccess: 'Lösenord bytt framgångsrikt',
    changeError: 'Fel vid byte av lösenord',
    passwordMismatch: 'Lösenorden matchar inte',
    passwordTooShort: 'Lösenordet måste vara minst 6 tecken långt och innehålla bokstäver och siffror.',
    passwordRequirements: 'Måste innehålla bokstäver och siffror',
    passwordsDoNotMatch: 'Lösenorden matchar inte',
    warningMessage: 'Detta är din första inloggning eller lösenordet är fortfarande satt till "admin". Av säkerhetsskäl måste du byta lösenordet.',
    success: 'Lösenord bytt framgångsrikt',
    newPasswordPlaceholder: 'Ange nytt lösenord',
    confirmPasswordPlaceholder: 'Bekräfta nytt lösenord'
  },

  // OTA Password Modal
  otaPassword: {
    title: 'Ange OTA Lösenord',
    warningMessage: 'Ange ett separat lösenord för firmware-uppdateringar. Detta krävs för OTA-uppdateringar.',
    otaPassword: 'OTA Lösenord',
    otaPasswordPlaceholder: 'Ange OTA lösenord',
    confirmPassword: 'Bekräfta Lösenord',
    confirmPasswordPlaceholder: 'Bekräfta OTA lösenord',
    passwordTooShort: 'Lösenordet måste vara minst 8 tecken långt',
    passwordRequirements: 'Måste innehålla versaler, gemener och siffror',
    passwordsDoNotMatch: 'Lösenorden matchar inte',
    requirementsTitle: 'Lösenordskrav:',
    reqMinLength: 'Minst 8 tecken',
    reqMixedCase: 'Versaler och gemener',
    reqNumbers: 'Minst en siffra',
    strengthWeak: 'Svag',
    strengthMedium: 'Medel',
    strengthGood: 'Bra',
    strengthStrong: 'Stark'
  },

  // Sponsor
  sponsor: {
    title: 'Stöd detta Projekt',
    description: 'Om du gillar detta projekt och vill stödja dess utveckling kan du använda ett av alternativen nedan.',
    thanks: 'Tack för ditt stöd!'
  },

  // System Log
  systemlog: {
    title: 'Systemlogg',
    description: 'Live-visning av systemloggutdata med nedladdningsmöjlighet.',
    liveLog: 'Live Logg',
    autoScroll: 'Auto-scroll',
    enabled: 'Aktiverad',
    disabledMessage: 'Loggvisaren är inaktiverad. Slå på strömbrytaren för att se live-loggar.',
    refresh: 'Uppdatera',
    clear: 'Rensa',
    download: 'Ladda Ner',
    empty: 'Inga loggposter ännu.'
  },

  // Changelog
  changelog: {
    title: 'Ändringslogg',
    loading: 'Laddar...',
    fetching: 'Hämtar ändringslogg från GitHub...',
    error: 'Misslyckades med att ladda ändringslogg',
    fetchError: 'Kunde inte hämta ändringslogg. Kontrollera din internetanslutning.',
    retry: 'Försök igen',
    close: 'Stäng',
    viewOnGithub: 'Visa på GitHub'
  }
}
