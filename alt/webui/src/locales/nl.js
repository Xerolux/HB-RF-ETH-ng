export default {
  // Common
  common: {
    enabled: 'Ingeschakeld',
    disabled: 'Uitgeschakeld',
    save: 'Opslaan',
    cancel: 'Annuleren',
    close: 'Sluiten',
    loading: 'Laden...',
    error: 'Fout',
    success: 'Succes',
    yes: 'Ja',
    no: 'Nee',
    showPassword: 'Wachtwoord tonen',
    hidePassword: 'Wachtwoord verbergen',
    rebootingWait: 'Systeem wordt opnieuw opgestart. Een ogenblik geduld, ongeveer 10 seconden...',
    factoryResettingWait: 'Systeem wordt teruggezet naar fabrieksinstellingen en opnieuw opgestart. Een ogenblik geduld...',
    confirmReboot: 'Weet u zeker dat u het systeem opnieuw wilt opstarten?',
    confirmFactoryReset: 'Weet u het zeker? Alle instellingen gaan verloren!'
  },

  // Header Navigation
  nav: {
    home: 'Start',
    settings: 'Instellingen',
    firmware: 'Firmware',
    monitoring: 'Monitoring',
    analyzer: 'Analyzer',
    about: 'Over',
    login: 'Aanmelden',
    logout: 'Afmelden',
    toggleTheme: 'Thema wisselen'
  },

  // Login Page
  login: {
    title: 'Aanmelden',
    username: 'Gebruikersnaam',
    password: 'Wachtwoord',
    login: 'Aanmelden',
    loginFailed: 'Aanmelding mislukt',
    invalidCredentials: 'Ongeldige inloggegevens',
    loginError: 'Aanmelding was niet succesvol.'
  },

  // Settings Page
  settings: {
    title: 'Instellingen',
    changePassword: 'Wachtwoord wijzigen',
    repeatPassword: 'Herhaal wachtwoord',
    hostname: 'Hostnaam',

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
    ipv6PrefixLength: 'Prefix Lengte',
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
    language: 'Taal',
    analyzerSettings: 'Analyzer Light Instellingen',
    enableAnalyzer: 'Analyzer Light inschakelen',
    systemMaintenance: 'Systeemonderhoud',
    reboot: 'Opnieuw opstarten',
    factoryReset: 'Fabrieksinstellingen herstellen',

    // HMLGW
    hmlgwSettings: 'HomeMatic LAN Gateway (HMLGW) Instellingen',
    enableHmlgw: 'HMLGW Modus inschakelen',
    hmlgwPort: 'Data Poort (Standaard: 2000)',
    hmlgwKeepAlivePort: 'KeepAlive Poort (Standaard: 2001)',

    // DTLS Security Settings
    dtls: {
      title: 'DTLS Versleuteling',
      description: 'Veilige transportversleuteling voor communicatie tussen board en CCU op poort 3008.',
      mode: 'Versleutelingsmodus',
      modeDisabled: 'Uitgeschakeld (Standaard)',
      modePsk: 'Vooraf gedeelde sleutel (PSK)',
      modeCert: 'X.509 Certificaat',
      cipherSuite: 'Cipher Suite',
      cipherAes128: 'AES-128-GCM-SHA256',
      cipherAes256: 'AES-256-GCM-SHA384 (Aanbevolen)',
      cipherChacha: 'ChaCha20-Poly1305-SHA256',
      requireClientCert: 'Clientcertificaat vereisen',
      sessionResumption: 'Sessiehervatting inschakelen',
      pskManagement: 'PSK Beheer',
      pskIdentity: 'PSK Identiteit',
      pskKey: 'PSK Sleutel (Hex)',
      pskGenerate: 'Nieuwe PSK genereren',
      pskGenerating: 'PSK genereren...',
      pskGenerated: 'Nieuwe PSK gegenereerd',
      pskCopyWarning: 'BELANGRIJK: Kopieer deze sleutel nu! Hij wordt slechts één keer getoond.',
      pskKeyLength: 'Sleutellengte',
      psk128bit: '128 Bit',
      psk256bit: '256 Bit (Aanbevolen)',
      psk384bit: '384 Bit',
      psk512bit: '512 Bit',
      pskStatus: 'PSK Status',
      pskConfigured: 'Geconfigureerd',
      pskNotConfigured: 'Niet geconfigureerd',
      warningDisabled: 'Communicatie is ONVERSLEUTELD. Iedereen op het netwerk kan verkeer onderscheppen.',
      warningPsk: 'Zorg ervoor dat de PSK veilig is opgeslagen op de CCU.',
      info: 'DTLS 1.2 versleutelt Raw-UART UDP-communicatie end-to-end. De CCU moet ook DTLS ondersteunen.',
      documentation: 'Documentatie voor CCU-ontwikkelaars',
      viewDocs: 'Implementatiegids bekijken',
      restartNote: 'Wijzigingen in DTLS-instellingen vereisen een herstart van het systeem.'
    },

    // Messages
    saveSuccess: 'Instellingen zijn succesvol opgeslagen. Start het systeem opnieuw op om ze toe te passen.',
    saveError: 'Er is een fout opgetreden bij het opslaan van de instellingen.',

    // Backup & Restore
    backupRestore: 'Backup & Herstel',
    backupInfo: 'Download een backup van uw instellingen om deze later te herstellen.',
    restoreInfo: 'Upload een backupbestand om instellingen te herstellen. Het systeem wordt daarna opnieuw opgestart.',
    downloadBackup: 'Backup downloaden',
    restore: 'Herstellen',
    noFileChosen: 'Geen bestand gekozen',
    browse: 'Bladeren',
    restoreConfirm: 'Weet u het zeker? Huidige instellingen worden overschreven en het systeem wordt opnieuw opgestart.',
    restoreSuccess: 'Instellingen succesvol hersteld. Systeem wordt opnieuw opgestart...',
    restoreError: 'Fout bij het herstellen van instellingen',
    backupError: 'Fout bij het downloaden van backup'
  },

  // System Info
  sysinfo: {
    title: 'Systeeminformatie',
    serial: 'Serienummer',
    boardRevision: 'Board revisie',
    uptime: 'Uptime',
    resetReason: 'Laatste herstart',
    cpuUsage: 'CPU gebruik',
    memoryUsage: 'Geheugengebruik',
    ethernetStatus: 'Ethernet verbinding',
    rawUartRemoteAddress: 'Verbonden met',
    radioModuleType: 'Radiomoduletype',
    radioModuleSerial: 'Serienummer',
    radioModuleFirmware: 'Firmwareversie',
    radioModuleBidCosRadioMAC: 'Radioadres (BidCoS)',
    radioModuleHmIPRadioMAC: 'Radioadres (HmIP)',
    radioModuleSGTIN: 'SGTIN',
    version: 'Versie',
    memory: 'Geheugengebruik',
    cpu: 'CPU Gebruik',
    ethernet: 'Ethernet',
    connected: 'Verbonden',
    disconnected: 'Niet verbonden',
    speed: 'Snelheid',
    duplex: 'Duplex',
    radioModule: 'Radiomodule',
    moduleType: 'Moduletype',
    firmwareVersion: 'Firmware Versie',
    bidcosMAC: 'BidCoS Radio MAC',
    hmipMAC: 'HmIP Radio MAC'
  },

  // Firmware Update
  firmware: {
    title: 'Firmware',
    currentVersion: 'Huidige versie',
    installedVersion: 'Geïnstalleerde versie',
    versionInfo: 'Gemoderniseerde fork v2.1 door Xerolux (2025) - Gebaseerd op het oorspronkelijke werk van Alexander Reinert.',
    updateFile: 'Firmwarebestand',
    noFileChosen: 'Geen bestand gekozen',
    browse: 'Bladeren',
    selectFile: 'Selecteer bestand',
    upload: 'Uploaden',
    restart: 'Systeem opnieuw opstarten',
    uploading: 'Uploaden...',
    uploadSuccess: 'Firmware-update succesvol geüpload. Systeem wordt automatisch opnieuw opgestart over 3 seconden...',
    uploadError: 'Er is een fout opgetreden.',
    updateSuccess: 'Firmware succesvol bijgewerkt',
    updateError: 'Fout bij het bijwerken van de firmware',
    warning: 'Waarschuwing: Onderbreek de voeding niet tijdens de update!',
    restartConfirm: 'Wilt u het systeem echt opnieuw opstarten?'
  },

  // Monitoring
  monitoring: {
    title: 'Monitoring',
    description: 'Configureer SNMP en CheckMK monitoring voor de HB-RF-ETH gateway.',
    save: 'Opslaan',
    saving: 'Bezig met opslaan...',
    saveSuccess: 'Configuratie succesvol opgeslagen!',
    saveError: 'Fout bij opslaan van configuratie!',
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
      contactHelp: 'Optioneel: bijv. "admin@example.com"'
    },
    checkmk: {
      title: 'CheckMK Agent',
      enabled: 'CheckMK inschakelen',
      port: 'Poort',
      portHelp: 'Standaard: 6556',
      allowedHosts: 'Toegestane client-IP\'s',
      allowedHostsHelp: 'Komma-gescheiden IP-adressen (bijv. "192.168.1.10,192.168.1.20") of "*" voor alle'
    },
    mqtt: {
      title: 'MQTT Client',
      enabled: 'MQTT inschakelen',
      server: 'Server',
      serverHelp: 'MQTT Broker hostnaam of IP',
      port: 'Poort',
      portHelp: 'Standaard: 1883',
      user: 'Gebruiker',
      userHelp: 'Optioneel: MQTT gebruikersnaam',
      password: 'Wachtwoord',
      passwordHelp: 'Optioneel: MQTT wachtwoord',
      topicPrefix: 'Topic Prefix',
      topicPrefixHelp: 'Standaard: hb-rf-eth - Topics zijn zoals prefix/status/...',
      haDiscoveryEnabled: 'Home Assistant Discovery',
      haDiscoveryPrefix: 'Discovery Prefix',
      haDiscoveryPrefixHelp: 'Standaard: homeassistant'
    },
    enable: 'Inschakelen',
    allowedHosts: 'Toegestane hosts'
  },

  // Analyzer
  analyzer: {
    title: 'Analyzer Light',
    disabled: 'Analyzer Light functie is uitgeschakeld. Schakel het in bij Instellingen.',
    connected: 'Verbonden',
    disconnected: 'Niet verbonden',
    clear: 'Wissen',
    autoScroll: 'Auto scrollen',
    time: 'Tijd',
    len: 'Lengte',
    cnt: 'Aantal',
    type: 'Type',
    src: 'Bron',
    dst: 'Bestemming',
    payload: 'Payload',
    rssi: 'RSSI',
    deviceNames: 'Apparaatnamen',
    address: 'Adres',
    name: 'Naam',
    storedNames: 'Opgeslagen namen'
  },

  // About Page
  about: {
    title: 'Over',
    version: 'Versie 2.1.0',
    fork: 'Gemoderniseerde Fork',
    forkDescription: 'Deze versie is een gemoderniseerde fork door Xerolux (2025), gebaseerd op de originele HB-RF-ETH firmware. Bijgewerkt naar ESP-IDF 5.3, moderne toolchains (GCC 13.2.0) en actuele WebUI-technologieën (Vue 3, Parcel 2, Pinia).',
    original: 'Oorspronkelijke auteur',
    firmwareLicense: 'De',
    hardwareLicense: 'De',
    under: 'is uitgebracht onder',
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
    providedAsIs: 'De software wordt geleverd "zoals het is" ZONDER ENIGE GARANTIE.'
  },

  // Change Password
  changePassword: {
    title: 'Wachtwoordwijziging vereist',
    currentPassword: 'Huidig wachtwoord',
    newPassword: 'Nieuw wachtwoord',
    confirmPassword: 'Bevestig wachtwoord',
    changePassword: 'Wachtwoord wijzigen',
    changeSuccess: 'Wachtwoord succesvol gewijzigd',
    changeError: 'Fout bij het wijzigen van wachtwoord',
    passwordMismatch: 'Wachtwoorden komen niet overeen',
    passwordTooShort: 'Wachtwoord moet minimaal 6 tekens lang zijn en letters en cijfers bevatten.',
    passwordsDoNotMatch: 'Wachtwoorden komen niet overeen',
    warningMessage: 'Dit is uw eerste aanmelding of het wachtwoord is nog steeds ingesteld op "admin". Om veiligheidsredenen moet u het wachtwoord wijzigen.',
    success: 'Wachtwoord succesvol gewijzigd'
  }
}
