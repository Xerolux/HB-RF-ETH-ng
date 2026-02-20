export default {
  // Common
  common: {
    enabled: 'Ingeschakeld',
    disabled: 'Uitgeschakeld',
    save: 'Opslaan',
    cancel: 'Annuleren',
    close: 'Sluiten',
    loading: 'Laden...',
    changing: 'Wijzigen...',
    error: 'Fout',
    success: 'Succes',
    yes: 'Ja',
    no: 'Nee'
  },

  // Header Navigation
  nav: {
    home: 'Home',
    settings: 'Instellingen',
    firmware: 'Firmware',
    monitoring: 'Monitoring',
    systemlog: 'Systeemlog',
    about: 'Over',
    login: 'Inloggen',
    logout: 'Uitloggen',
    toggleTheme: 'Thema wisselen',
    language: 'Taal'
  },

  // Login Page
  login: {
    title: 'Gelieve in te loggen',
    subtitle: 'Voer uw wachtwoord in om door te gaan',
    username: 'Gebruikersnaam',
    password: 'Wachtwoord',
    passwordPlaceholder: 'Wachtwoord',
    login: 'Inloggen',
    loginFailed: 'Inloggen mislukt',
    invalidCredentials: 'Ongeldige inloggegevens',
    loginError: 'Inloggen was niet succesvol.',
    passwordRequired: 'Wachtwoord is vereist',
    loggingIn: 'Inloggen...'
  },

  // Settings Page
  settings: {
    title: 'Instellingen',
    ccuSettings: 'CCU Verbinding',
    ccuIpAddress: 'CCU IP-adres',
    ccuIpHint: 'Voer het IP-adres van uw CCU in om verbindingsblokkering te voorkomen. Het systeem zal na het opslaan opnieuw opstarten.',
    tabGeneral: 'Algemeen',
    tabNetwork: 'Netwerk',
    tabTime: 'Tijd',
    tabBackup: 'Back-up',
    changePassword: 'Wachtwoord wijzigen',
    repeatPassword: 'Wachtwoord herhalen',
    hostname: 'Hostnaam',

    // Security Settings
    security: 'Beveiliging',
    changePasswordHint: 'Wijzig uw beheerderswachtwoord voor veilige toegang',
    otaPassword: 'OTA Wachtwoord',
    otaPasswordHint: 'Apart wachtwoord vereist voor firmware-updates',
    changePasswordBtn: 'Wachtwoord wijzigen',
    changeOtaPassword: 'Wijzigen',
    setOtaPassword: 'Wachtwoord instellen',
    clearOtaPassword: 'Wissen',
    clearOtaPasswordConfirm: 'Weet u zeker dat u het OTA-wachtwoord wilt verwijderen? Firmware-updates zijn niet mogelijk totdat er een nieuw wachtwoord is ingesteld.',
    clearOtaPasswordSuccess: 'OTA-wachtwoord is verwijderd.',
    clearOtaPasswordError: 'Kon OTA-wachtwoord niet verwijderen',

    // Network Settings
    networkSettings: 'Netwerkinstellingen',
    dhcp: 'DHCP',
    ipAddress: 'IP-adres',
    netmask: 'Netmasker',
    gateway: 'Gateway',
    dns1: 'Primaire DNS',
    dns2: 'Secundaire DNS',

    // IPv6 Settings
    ipv6Settings: 'IPv6 Instellingen',
    enableIPv6: 'IPv6 inschakelen',
    ipv6Mode: 'IPv6 Modus',
    ipv6Auto: 'Automatisch (SLAAC)',
    ipv6Static: 'Statisch',
    ipv6Address: 'IPv6 Adres',
    ipv6PrefixLength: 'Prefixlengte',
    ipv6Gateway: 'IPv6 Gateway',
    ipv6Dns1: 'Primaire IPv6 DNS',
    ipv6Dns2: 'Secundaire IPv6 DNS',

    // Time Settings
    timeSettings: 'Tijdinstellingen',
    timesource: 'Tijdbron',
    ntp: 'NTP',
    dcf: 'DCF',
    gps: 'GPS',
    ntpServer: 'NTP Server',
    dcfOffset: 'DCF Offset',
    gpsBaudrate: 'GPS Baudrate',

    // System Settings
    systemSettings: 'Systeeminstellingen',
    ledBrightness: 'LED Helderheid',
    updateLedBlink: 'LED knipperen bij updates',
    checkUpdates: 'Controleren op updates',
    allowPrerelease: 'Vroege updates toestaan (Beta/Alpha)',
    language: 'Taal',

    // Messages
    saveSuccess: 'Instellingen zijn succesvol opgeslagen.',
    saveError: 'Er is een fout opgetreden bij het opslaan van de instellingen.',
    restartTitle: 'Herstart Vereist',
    restartMessage: 'Instellingen zijn opgeslagen. Wilt u het apparaat nu opnieuw opstarten om de wijzigingen toe te passen?',
    restartNow: 'Nu herstarten',
    restartLater: 'Later herstarten',

    // Backup & Restore
    backupRestore: 'Back-up & Herstel',
    backupInfo: 'Download een back-up van uw instellingen om ze later te herstellen.',
    restoreInfo: 'Upload een back-upbestand om instellingen te herstellen. Het systeem zal daarna opnieuw opstarten.',
    downloadBackup: 'Back-up downloaden',
    restore: 'Herstellen',
    restoreBtn: 'Herstellen',
    download: 'Downloaden',
    noFileChosen: 'Geen bestand gekozen',
    browse: 'Bladeren',
    restoreConfirm: 'Weet u het zeker? Huidige instellingen worden overschreven en het systeem wordt opnieuw opgestart.',
    restoreSuccess: 'Instellingen succesvol hersteld. Systeem herstart...',
    restoreError: 'Fout bij het herstellen van instellingen',
    backupError: 'Fout bij het downloaden van back-up'
  },

  // System Info
  sysinfo: {
    title: 'Systeeminformatie',
    goodMorning: 'Goedemorgen',
    goodAfternoon: 'Goedemiddag',
    goodEvening: 'Goedenavond',
    serial: 'Serienummer',
    boardRevision: 'Bord revisie',
    uptime: 'Uptime',
    resetReason: 'Laatste herstart',
    cpuUsage: 'CPU gebruik',
    memoryUsage: 'Geheugengebruik',
    ethernetStatus: 'Ethernet verbinding',
    rawUartRemoteAddress: 'Verbonden met',
    radioModuleType: 'Radio module type',
    radioModuleSerial: 'Serienummer',
    radioModuleFirmware: 'Firmware versie',
    radioModuleBidCosRadioMAC: 'Radio adres (BidCoS)',
    radioModuleHmIPRadioMAC: 'Radio adres (HmIP)',
    radioModuleSGTIN: 'SGTIN',
    version: 'Versie',
    latestVersion: 'Nieuwste Versie',
    memory: 'Geheugengebruik',
    cpu: 'CPU Gebruik',
    temperature: 'Temperatuur',
    voltage: 'Voedingsspanning',
    ethernet: 'Ethernet',
    connected: 'Verbonden',
    disconnected: 'Verbroken',
    speed: 'Snelheid',
    duplex: 'Duplex',
    radioModule: 'Radio Module',
    moduleType: 'Module Type',
    firmwareVersion: 'Firmware Versie',
    bidcosMAC: 'BidCoS Radio MAC',
    hmipMAC: 'HmIP Radio MAC',
    system: 'Systeem',
    network: 'Netwerk',
    down: 'Neer',
    mbits: 'Mbit/s',
    online: 'Online',
    offline: 'Offline',
    dashboardTitle: 'Systeemstatus'
  },

  // Update
  update: {
    available: 'Update Beschikbaar',
    updateNow: 'Nu Updaten'
  },

  // Firmware Update
  firmware: {
    title: 'Firmware',
    subtitle: 'Update uw apparaatfirmware',
    restarting: 'Opnieuw opstarten...',
    restartingText: 'Apparaat start opnieuw op. Pagina wordt automatisch herladen.',
    fileUpload: 'Bestand uploaden',
    fileUploadHint: 'Upload een .bin firmwarebestand',
    networkUpdate: 'Netwerk Update',
    networkUpdateHint: 'Download van URL',
    urlPlaceholder: 'https://voorbeeld.com/firmware.bin',
    downloading: 'Downloaden...',
    downloadInstall: 'Download & Installeer',
    factoryReset: 'Resetten',
    factoryResetHint: 'Terugzetten naar fabrieksinstellingen',
    factoryResetConfirm: 'Wilt u echt terugzetten naar fabrieksinstellingen? Alle instellingen gaan verloren.',
    otaProgress: 'Firmware-update downloaden...',
    otaSuccess: 'Download succesvol. Installeren...',
    currentVersion: 'Huidige Versie',
    installedVersion: 'Geïnstalleerde versie',
    versionInfo: 'Gemoderniseerde fork v2.1.7 door Xerolux (2025) - Gebaseerd op het originele werk van Alexander Reinert.',
    updateAvailable: 'Een update naar versie {latestVersion} is beschikbaar.',
    newVersionAvailable: 'Nieuwe versie {version} is beschikbaar!',
    viewUpdate: 'Bekijken',
    onlineUpdate: 'Online Updaten',
    onlineUpdateConfirm: 'Wilt u de update echt downloaden en installeren? Het systeem zal automatisch herstarten.',
    onlineUpdateStarted: 'Update gestart. Het apparaat zal automatisch herstarten zodra voltooid.',
    showReleaseNotes: 'Release Notes Tonen',
    releaseNotesTitle: 'Release Notes voor v{version}',
    releaseNotesError: 'Kon release notes niet laden van GitHub.',
    updateFile: 'Firmware bestand',
    noFileChosen: 'Geen bestand gekozen',
    browse: 'Bladeren',
    selectFile: 'Selecteer of sleep .bin bestand',
    upload: 'Firmware Installeren',
    restart: 'Herstarten',
    restartHint: 'Apparaat herstarten',
    uploading: 'Uploaden...',
    uploadSuccess: 'Firmware-update succesvol geüpload. Systeem herstart automatisch in 3 seconden...',
    uploadError: 'Er is een fout opgetreden.',
    updateSuccess: 'Firmware succesvol bijgewerkt',
    updateError: 'Fout bij bijwerken firmware',
    warning: 'Waarschuwing: Koppel de stroom niet los tijdens de update!',
    restartConfirm: 'Wilt u het systeem echt opnieuw opstarten?'
  },

  // Monitoring
  monitoring: {
    title: 'Monitoring',
    description: 'Configureer SNMP en CheckMK monitoring voor de HB-RF-ETH gateway.',
    save: 'Opslaan',
    saving: 'Opslaan...',
    saveSuccess: 'Configuratie succesvol opgeslagen!',
    saveError: 'Fout bij opslaan configuratie!',
    snmp: {
      title: 'SNMP Agent',
      enabled: 'SNMP inschakelen',
      port: 'Poort',
      portHelp: 'Standaard: 161',
      community: 'Community String',
      communityHelp: 'Standaard: "public" - Wijzig dit voor productie!',
      location: 'Locatie',
      locationHelp: 'Optioneel: bijv. "Serverruimte, Gebouw A"',
      contact: 'Contact',
      contactHelp: 'Optioneel: bijv. "admin@voorbeeld.com"'
    },
    checkmk: {
      title: 'CheckMK Agent',
      enabled: 'CheckMK inschakelen',
      port: 'Poort',
      portHelp: 'Standaard: 6556',
      allowedHosts: 'Toegestane Client IP\'s',
      allowedHostsHelp: 'Komma-gescheiden IP-adressen (bijv. "192.168.1.10,192.168.1.20") of "*" voor alles'
    },
    mqtt: {
      title: 'MQTT Client',
      enabled: 'MQTT inschakelen',
      server: 'Server',
      serverHelp: 'MQTT Broker Hostnaam of IP',
      port: 'Poort',
      portHelp: 'Standaard: 1883',
      user: 'Gebruiker',
      userHelp: 'Optioneel: MQTT Gebruikersnaam',
      password: 'Wachtwoord',
      passwordHelp: 'Optioneel: MQTT Wachtwoord',
      topicPrefix: 'Onderwerp Prefix',
      topicPrefixHelp: 'Standaard: hb-rf-eth - Onderwerpen zullen zijn als prefix/status/...',
      haDiscoveryEnabled: 'Home Assistant Discovery',
      haDiscoveryPrefix: 'Discovery Prefix',
      haDiscoveryPrefixHelp: 'Standaard: homeassistant'
    },
    enable: 'Inschakelen',
    allowedHosts: 'Toegestane Hosts'
  },

  // About Page
  about: {
    title: 'Over',
    version: 'Versie 2.1.7',
    fork: 'Gemoderniseerde Fork',
    forkDescription: 'Deze versie is een gemoderniseerde fork door Xerolux (2025), gebaseerd op de originele HB-RF-ETH firmware. Geüpdatet naar ESP-IDF 5.3, moderne toolchains en huidige WebUI technologieën (Vue 3, Vite, Pinia).',
    original: 'Originele Auteur',
    firmwareLicense: 'De',
    hardwareLicense: 'De',
    under: 'is uitgegeven onder',
    description: 'HomeMatic BidCoS/HmIP LAN Gateway',
    author: 'Auteur',
    license: 'Licentie',
    website: 'Website',
    documentation: 'Documentatie',
    support: 'Ondersteuning'
  },

  // Third Party
  thirdParty: {
    title: 'Software van derden',
    containsThirdPartySoftware: 'Deze software bevat gratis softwareproducten van derden die onder verschillende licentievoorwaarden worden gebruikt.',
    providedAsIs: 'De software wordt geleverd "zoals hij is" ZONDER ENIGE GARANTIE.'
  },

  // Change Password
  changePassword: {
    title: 'Wachtwoordwijziging vereist',
    subtitle: 'Beveilig uw account',
    warningTitle: 'Vereist',
    requirementsTitle: 'Wachtwoordvereisten:',
    reqMinLength: 'Minstens 6 tekens',
    reqLettersNumbers: 'Moet letters en cijfers bevatten',
    currentPassword: 'Huidig Wachtwoord',
    newPassword: 'Nieuw Wachtwoord',
    confirmPassword: 'Bevestig Wachtwoord',
    changePassword: 'Wachtwoord Wijzigen',
    changeSuccess: 'Wachtwoord succesvol gewijzigd',
    changeError: 'Fout bij wijzigen wachtwoord',
    passwordMismatch: 'Wachtwoorden komen niet overeen',
    passwordTooShort: 'Wachtwoord moet minstens 6 tekens lang zijn en letters en cijfers bevatten.',
    passwordRequirements: 'Moet letters en cijfers bevatten',
    passwordsDoNotMatch: 'Wachtwoorden komen niet overeen',
    warningMessage: 'Dit is uw eerste aanmelding of het wachtwoord is nog ingesteld op "admin". Om veiligheidsredenen moet u het wachtwoord wijzigen.',
    success: 'Wachtwoord succesvol gewijzigd',
    newPasswordPlaceholder: 'Voer nieuw wachtwoord in',
    confirmPasswordPlaceholder: 'Bevestig nieuw wachtwoord'
  },

  // OTA Password Modal
  otaPassword: {
    title: 'OTA Wachtwoord Instellen',
    warningMessage: 'Stel een apart wachtwoord in voor firmware-updates. Dit is vereist voor OTA-updates.',
    otaPassword: 'OTA Wachtwoord',
    otaPasswordPlaceholder: 'Voer OTA wachtwoord in',
    confirmPassword: 'Bevestig Wachtwoord',
    confirmPasswordPlaceholder: 'Bevestig OTA wachtwoord',
    passwordTooShort: 'Wachtwoord moet minstens 8 tekens lang zijn',
    passwordRequirements: 'Moet hoofdletters, kleine letters en cijfers bevatten',
    passwordsDoNotMatch: 'Wachtwoorden komen niet overeen',
    requirementsTitle: 'Wachtwoordvereisten:',
    reqMinLength: 'Minstens 8 tekens',
    reqMixedCase: 'Hoofdletters en kleine letters',
    reqNumbers: 'Minstens één cijfer',
    strengthWeak: 'Zwak',
    strengthMedium: 'Gemiddeld',
    strengthGood: 'Goed',
    strengthStrong: 'Sterk'
  },

  // Sponsor
  sponsor: {
    title: 'Steun dit Project',
    description: 'Als u dit project leuk vindt en de ontwikkeling ervan wilt steunen, kunt u een van de onderstaande opties gebruiken.',
    thanks: 'Bedankt voor uw steun!'
  },

  // System Log
  systemlog: {
    title: 'Systeemlog',
    description: 'Live weergave van systeemloguitvoer met downloadmogelijkheid.',
    liveLog: 'Live Log',
    autoScroll: 'Auto-scroll',
    enabled: 'Ingeschakeld',
    disabledMessage: 'Logboekviewer is uitgeschakeld. Zet de schakelaar aan om live logs te zien.',
    refresh: 'Vernieuwen',
    clear: 'Wissen',
    download: 'Downloaden',
    empty: 'Nog geen logboekvermeldingen.'
  },

  // Changelog
  changelog: {
    title: 'Wijzigingslogboek',
    loading: 'Laden...',
    fetching: 'Wijzigingslogboek ophalen van GitHub...',
    error: 'Kon wijzigingslogboek niet laden',
    fetchError: 'Kon wijzigingslogboek niet ophalen. Controleer uw internetverbinding.',
    retry: 'Opnieuw proberen',
    close: 'Sluiten',
    viewOnGithub: 'Bekijken op GitHub'
  }
}
