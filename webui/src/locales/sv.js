export default {
  // Common
  common: {
    enabled: 'Aktiverad',
    disabled: 'Inaktiverad',
    save: 'Spara',
    cancel: 'Avbryt',
    close: 'Stäng',
    loading: 'Laddar...',
    error: 'Fel',
    success: 'Framgång',
    yes: 'Ja',
    no: 'Nej',
    showPassword: 'Visa lösenord',
    hidePassword: 'Dölj lösenord',
    rebootingWait: 'Systemet startar om. Vänta cirka 10 sekunder...',
    factoryResettingWait: 'Systemet återställs till fabriksinställningar och startar om. Vänligen vänta...',
    confirmReboot: 'Är du säker på att du vill starta om systemet?',
    confirmFactoryReset: 'Är du säker? Alla inställningar kommer att förloras!'
  },

  // Header Navigation
  nav: {
    home: 'Hem',
    settings: 'Inställningar',
    firmware: 'Firmware',
    monitoring: 'Övervakning',
    analyzer: 'Analyzer',
    about: 'Om',
    login: 'Logga in',
    logout: 'Logga ut',
    toggleTheme: 'Växla tema'
  },

  // Login Page
  login: {
    title: 'Logga in',
    username: 'Användarnamn',
    password: 'Lösenord',
    login: 'Logga in',
    loginFailed: 'Inloggning misslyckades',
    invalidCredentials: 'Ogiltiga inloggningsuppgifter',
    loginError: 'Inloggningen lyckades inte.'
  },

  // Settings Page
  settings: {
    title: 'Inställningar',
    changePassword: 'Ändra lösenord',
    repeatPassword: 'Upprepa lösenord',
    hostname: 'Värdnamn',

    // Network Settings
    networkSettings: 'Nätverksinställningar',
    dhcp: 'DHCP',
    ipAddress: 'IP-adress',
    netmask: 'Nätmask',
    gateway: 'Gateway',
    dns1: 'Primär DNS',
    dns2: 'Sekundär DNS',

    // IPv6 Settings
    ipv6Settings: 'IPv6-inställningar',
    enableIPv6: 'Aktivera IPv6',
    ipv6Mode: 'IPv6-läge',
    ipv6Auto: 'Automatisk (SLAAC)',
    ipv6Static: 'Statisk',
    ipv6Address: 'IPv6-adress',
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
    ntpServer: 'NTP-server',
    dcfOffset: 'DCF-offset',
    gpsBaudrate: 'GPS Baudrate',

    // System Settings
    systemSettings: 'Systeminställningar',
    ledBrightness: 'LED-ljusstyrka',
    language: 'Språk',
    analyzerSettings: 'Analyzer Light-inställningar',
    enableAnalyzer: 'Aktivera Analyzer Light',
    systemMaintenance: 'Systemunderhåll',
    reboot: 'Omstart',
    factoryReset: 'Fabriksåterställning',

    // HMLGW
    hmlgwSettings: 'HomeMatic LAN Gateway (HMLGW) inställningar',
    enableHmlgw: 'Aktivera HMLGW-läge',
    hmlgwPort: 'Dataport (Standard: 2000)',
    hmlgwKeepAlivePort: 'KeepAlive-port (Standard: 2001)',

    // DTLS Security Settings
    dtls: {
      title: 'DTLS Kryptering',
      description: 'Säker transportkryptering för kommunikation mellan kort och CCU på port 3008.',
      mode: 'Krypteringsläge',
      modeDisabled: 'Inaktiverad (Standard)',
      modePsk: 'Fördelad nyckel (PSK)',
      modeCert: 'X.509 Certifikat',
      cipherSuite: 'Chiffer Suite',
      cipherAes128: 'AES-128-GCM-SHA256',
      cipherAes256: 'AES-256-GCM-SHA384 (Rekommenderas)',
      cipherChacha: 'ChaCha20-Poly1305-SHA256',
      requireClientCert: 'Kräv klientcertifikat',
      sessionResumption: 'Aktivera sessionsåterupptagning',
      pskManagement: 'PSK Hantering',
      pskIdentity: 'PSK Identitet',
      pskKey: 'PSK Nyckel (Hex)',
      pskGenerate: 'Generera ny PSK',
      pskGenerating: 'Genererar PSK...',
      pskGenerated: 'Ny PSK genererad',
      pskCopyWarning: 'VIKTIGT: Kopiera denna nyckel nu! Den visas bara en gång.',
      pskKeyLength: 'Nyckellängd',
      psk128bit: '128 Bit',
      psk256bit: '256 Bit (Rekommenderas)',
      psk384bit: '384 Bit',
      psk512bit: '512 Bit',
      pskStatus: 'PSK Status',
      pskConfigured: 'Konfigurerad',
      pskNotConfigured: 'Inte konfigurerad',
      warningDisabled: 'Kommunikationen är OKRYPTERAD. Vem som helst på nätverket kan fånga upp trafiken.',
      warningPsk: 'Se till att PSK lagras säkert på CCU.',
      info: 'DTLS 1.2 krypterar Raw-UART UDP-kommunikation end-to-end. CCU måste också stödja DTLS.',
      documentation: 'Dokumentation för CCU-utvecklare',
      viewDocs: 'Visa implementeringsguide',
      restartNote: 'Ändringar av DTLS-inställningar kräver en systemomstart.'
    },

    // Messages
    saveSuccess: 'Inställningarna sparades. Starta om systemet för att tillämpa dem.',
    saveError: 'Ett fel uppstod när inställningarna skulle sparas.',

    // Backup & Restore
    backupRestore: 'Säkerhetskopiering och återställning',
    backupInfo: 'Ladda ner en säkerhetskopia av dina inställningar för att återställa dem senare.',
    restoreInfo: 'Ladda upp en säkerhetskopieringsfil för att återställa inställningar. Systemet startar om efteråt.',
    downloadBackup: 'Ladda ner säkerhetskopia',
    restore: 'Återställ',
    noFileChosen: 'Ingen fil vald',
    browse: 'Bläddra',
    restoreConfirm: 'Är du säker? Nuvarande inställningar kommer att skrivas över och systemet startar om.',
    restoreSuccess: 'Inställningar återställda. Systemet startar om...',
    restoreError: 'Fel vid återställning av inställningar',
    backupError: 'Fel vid nedladdning av säkerhetskopia'
  },

  // System Info
  sysinfo: {
    title: 'Systeminformation',
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
    memory: 'Minnesanvändning',
    cpu: 'CPU-användning',
    ethernet: 'Ethernet',
    connected: 'Ansluten',
    disconnected: 'Frånkopplad',
    speed: 'Hastighet',
    duplex: 'Duplex',
    radioModule: 'Radiomodul',
    moduleType: 'Modultyp',
    firmwareVersion: 'Firmware-version',
    bidcosMAC: 'BidCoS Radio MAC',
    hmipMAC: 'HmIP Radio MAC'
  },

  // Firmware Update
  firmware: {
    title: 'Firmware',
    currentVersion: 'Nuvarande version',
    installedVersion: 'Installerad version',
    versionInfo: 'Moderniserad fork v2.1 av Xerolux (2025) - Baserad på det ursprungliga arbetet av Alexander Reinert.',
    updateFile: 'Firmware-fil',
    noFileChosen: 'Ingen fil vald',
    browse: 'Bläddra',
    selectFile: 'Välj fil',
    upload: 'Ladda upp',
    restart: 'Starta om systemet',
    uploading: 'Laddar upp...',
    uploadSuccess: 'Firmware-uppdatering uppladdad. Systemet startar om automatiskt om 3 sekunder...',
    uploadError: 'Ett fel uppstod.',
    updateSuccess: 'Firmware uppdaterad',
    updateError: 'Fel vid uppdatering av firmware',
    warning: 'Varning: Koppla inte bort strömmen under uppdateringen!',
    restartConfirm: 'Vill du verkligen starta om systemet?'
  },

  // Monitoring
  monitoring: {
    title: 'Övervakning',
    description: 'Konfigurera SNMP och CheckMK övervakning för HB-RF-ETH gateway.',
    save: 'Spara',
    saving: 'Sparar...',
    saveSuccess: 'Konfiguration sparad!',
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
      contactHelp: 'Valfritt: t.ex. "admin@example.com"'
    },
    checkmk: {
      title: 'CheckMK Agent',
      enabled: 'Aktivera CheckMK',
      port: 'Port',
      portHelp: 'Standard: 6556',
      allowedHosts: 'Tillåtna klient-IP:er',
      allowedHostsHelp: 'Kommaseparerade IP-adresser (t.ex. "192.168.1.10,192.168.1.20") eller "*" för alla'
    },
    mqtt: {
      title: 'MQTT-klient',
      enabled: 'Aktivera MQTT',
      server: 'Server',
      serverHelp: 'MQTT Broker värdnamn eller IP',
      port: 'Port',
      portHelp: 'Standard: 1883',
      user: 'Användare',
      userHelp: 'Valfritt: MQTT användarnamn',
      password: 'Lösenord',
      passwordHelp: 'Valfritt: MQTT lösenord',
      topicPrefix: 'Ämnesprefix',
      topicPrefixHelp: 'Standard: hb-rf-eth - Ämnen kommer att vara som prefix/status/...',
      haDiscoveryEnabled: 'Home Assistant Discovery',
      haDiscoveryPrefix: 'Discovery-prefix',
      haDiscoveryPrefixHelp: 'Standard: homeassistant'
    },
    enable: 'Aktivera',
    allowedHosts: 'Tillåtna värdar'
  },

  // Analyzer
  analyzer: {
    title: 'Analyzer Light',
    disabled: 'Analyzer Light-funktionen är inaktiverad. Aktivera den i Inställningar.',
    connected: 'Ansluten',
    disconnected: 'Frånkopplad',
    clear: 'Rensa',
    autoScroll: 'Automatisk rullning',
    time: 'Tid',
    len: 'Längd',
    cnt: 'Antal',
    type: 'Typ',
    src: 'Källa',
    dst: 'Destination',
    payload: 'Payload',
    rssi: 'RSSI',
    deviceNames: 'Enhetsnamn',
    address: 'Adress',
    name: 'Namn',
    storedNames: 'Sparade namn'
  },

  // About Page
  about: {
    title: 'Om',
    version: 'Version 2.1.0',
    fork: 'Moderniserad fork',
    forkDescription: 'Denna version är en moderniserad fork av Xerolux (2025), baserad på den ursprungliga HB-RF-ETH firmware. Uppdaterad till ESP-IDF 5.3, moderna verktygskedjor (GCC 13.2.0) och nuvarande WebUI-teknologier (Vue 3, Parcel 2, Pinia).',
    original: 'Ursprunglig författare',
    firmwareLicense: '',
    hardwareLicense: '',
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
    containsThirdPartySoftware: 'Denna programvara innehåller gratis tredjepartsprogramvara som används under olika licensvillkor.',
    providedAsIs: 'Programvaran tillhandahålls "SOM DEN ÄR" UTAN NÅGON GARANTI.'
  },

  // Change Password
  changePassword: {
    title: 'Lösenordsändring krävs',
    currentPassword: 'Nuvarande lösenord',
    newPassword: 'Nytt lösenord',
    confirmPassword: 'Bekräfta lösenord',
    changePassword: 'Ändra lösenord',
    changeSuccess: 'Lösenord ändrat',
    changeError: 'Fel vid ändring av lösenord',
    passwordMismatch: 'Lösenorden stämmer inte överens',
    passwordTooShort: 'Lösenordet måste vara minst 6 tecken långt och innehålla bokstäver och siffror.',
    passwordsDoNotMatch: 'Lösenorden stämmer inte överens',
    warningMessage: 'Detta är din första inloggning eller lösenordet är fortfarande inställt på "admin". Av säkerhetsskäl måste du ändra lösenordet.',
    success: 'Lösenord ändrat'
  }
}
