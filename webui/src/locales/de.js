export default {
  // Common
  common: {
    enabled: 'Aktiv',
    disabled: 'Deaktiviert',
    save: 'Speichern',
    cancel: 'Abbrechen',
    close: 'Schließen',
    loading: 'Lädt...',
    error: 'Fehler',
    success: 'Erfolgreich',
    yes: 'Ja',
    no: 'Nein'
  },

  // Header Navigation
  nav: {
    home: 'Status',
    settings: 'Einstellungen',
    firmware: 'Firmware',
    monitoring: 'Monitoring',
    analyzer: 'Analyzer',
    about: 'Über',
    login: 'Anmelden',
    logout: 'Abmelden',
    toggleTheme: 'Design wechseln'
  },

  // Login Page
  login: {
    title: 'Bitte anmelden',
    username: 'Benutzername',
    password: 'Passwort',
    login: 'Anmelden',
    loginFailed: 'Anmeldung fehlgeschlagen',
    invalidCredentials: 'Ungültige Anmeldedaten',
    loginError: 'Anmelden war nicht erfolgreich.'
  },

  // Settings Page
  settings: {
    title: 'Einstellungen',
    changePassword: 'Passwort ändern',
    repeatPassword: 'Passwort wiederholen',
    hostname: 'Hostname',

    // Network Settings
    networkSettings: 'Netzwerkeinstellungen',
    dhcp: 'DHCP',
    ipAddress: 'IP-Adresse',
    netmask: 'Netzmaske',
    gateway: 'Gateway',
    dns1: 'Primärer DNS Server',
    dns2: 'Sekundärer DNS Server',

    // IPv6 Settings
    ipv6Settings: 'IPv6 Einstellungen',
    enableIPv6: 'IPv6 aktivieren',
    ipv6Mode: 'IPv6 Modus',
    ipv6Auto: 'Automatisch (SLAAC)',
    ipv6Static: 'Statisch',
    ipv6Address: 'IPv6 Adresse',
    ipv6PrefixLength: 'Präfixlänge',
    ipv6Gateway: 'IPv6 Gateway',
    ipv6Dns1: 'Primärer IPv6 DNS',
    ipv6Dns2: 'Sekundärer IPv6 DNS',

    // Time Settings
    timeSettings: 'Zeiteinstellungen',
    timesource: 'Zeitquelle',
    ntp: 'NTP',
    dcf: 'DCF',
    gps: 'GPS',
    ntpServer: 'NTP Server',
    dcfOffset: 'DCF Versatz',
    gpsBaudrate: 'GPS Baudrate',

    // System Settings
    systemSettings: 'Systemeinstellungen',
    ledBrightness: 'LED Helligkeit',
    checkUpdates: 'Nach Updates suchen',
    allowPrerelease: 'Frühe Updates erlauben (Beta/Alpha)',
    language: 'Sprache',

    // HMLGW
    hmlgwSettings: 'HomeMatic LAN Gateway (HMLGW) Einstellungen',
    enableHmlgw: 'HMLGW-Modus aktivieren',
    hmlgwPort: 'Daten-Port (Standard: 2000)',
    hmlgwKeepAlivePort: 'KeepAlive-Port (Standard: 2001)',

    // Messages
    saveSuccess: 'Einstellungen wurden erfolgreich gespeichert. Bitte starten Sie das System neu, um sie zu übernehmen.',
    saveError: 'Beim Speichern der Einstellungen ist ein Fehler aufgetreten.',

    // Backup & Restore
    backupRestore: 'Sichern & Wiederherstellen',
    backupInfo: 'Laden Sie eine Sicherung Ihrer Einstellungen herunter, um sie später wiederherzustellen.',
    restoreInfo: 'Laden Sie eine Sicherungsdatei hoch, um die Einstellungen wiederherzustellen. Das System wird danach neu gestartet.',
    downloadBackup: 'Sicherung herunterladen',
    restore: 'Wiederherstellen',
    noFileChosen: 'Keine Datei ausgewählt',
    browse: 'Datei auswählen',
    restoreConfirm: 'Sind Sie sicher? Die aktuellen Einstellungen werden überschrieben und das System neu gestartet.',
    restoreSuccess: 'Einstellungen erfolgreich wiederhergestellt. System startet neu...',
    restoreError: 'Fehler beim Wiederherstellen der Einstellungen',
    backupError: 'Fehler beim Herunterladen der Sicherung'
  },

  // System Info
  sysinfo: {
    title: 'Systeminformationen',
    serial: 'Seriennummer',
    boardRevision: 'Board-Revision',
    uptime: 'Laufzeit',
    resetReason: 'Letzter Neustart',
    cpuUsage: 'CPU Auslastung',
    memoryUsage: 'Speicherauslastung',
    ethernetStatus: 'Ethernet-Verbindung',
    rawUartRemoteAddress: 'Verbunden mit',
    radioModuleType: 'Funkmodultyp',
    radioModuleSerial: 'Seriennummer',
    radioModuleFirmware: 'Firmware-Version',
    radioModuleBidCosRadioMAC: 'Funkadresse (BidCoS)',
    radioModuleHmIPRadioMAC: 'Funkadresse (HmIP)',
    radioModuleSGTIN: 'SGTIN',
    version: 'Version',
    latestVersion: 'Neueste Version',
    memory: 'Speichernutzung',
    cpu: 'CPU Auslastung',
    temperature: 'Temperatur',
    voltage: 'Versorgungsspannung',
    ethernet: 'Ethernet',
    connected: 'Verbunden',
    disconnected: 'Getrennt',
    speed: 'Geschwindigkeit',
    duplex: 'Duplex',
    radioModule: 'Funkmodul',
    moduleType: 'Modultyp',
    firmwareVersion: 'Firmware Version',
    bidcosMAC: 'BidCoS Radio MAC',
    hmipMAC: 'HmIP Radio MAC'
  },

  // Firmware Update
  firmware: {
    title: 'Firmware',
    currentVersion: 'Aktuelle Version',
    installedVersion: 'Installierte Version',
    versionInfo: 'Modernisierte Fork v2.1 von Xerolux (2025) - Basierend auf der Original-Arbeit von Alexander Reinert.',
    updateAvailable: 'Ein Update auf Version {latestVersion} ist verfügbar.',
    onlineUpdate: 'Online Update durchführen',
    onlineUpdateConfirm: 'Möchten Sie das Update wirklich herunterladen und installieren? Das System wird automatisch neu gestartet.',
    onlineUpdateStarted: 'Update gestartet. Das Gerät wird nach Abschluss automatisch neu gestartet.',
    showReleaseNotes: 'Release Notes anzeigen',
    releaseNotesTitle: 'Release Notes für v{version}',
    releaseNotesError: 'Release Notes konnten nicht von GitHub geladen werden.',
    updateFile: 'Firmware Datei',
    noFileChosen: 'Keine Datei ausgewählt',
    browse: 'Datei auswählen',
    selectFile: 'Datei auswählen',
    upload: 'Hochladen',
    restart: 'System neu starten',
    uploading: 'Wird hochgeladen...',
    uploadSuccess: 'Die Firmware wurde erfolgreich hochgeladen. System startet in 3 Sekunden automatisch neu...',
    uploadError: 'Es ist ein Fehler aufgetreten.',
    updateSuccess: 'Firmware wurde erfolgreich aktualisiert',
    updateError: 'Fehler beim Aktualisieren der Firmware',
    warning: 'Warnung: Unterbrechen Sie nicht die Stromversorgung während des Updates!',
    restartConfirm: 'Möchten Sie das System wirklich neu starten?'
  },

  // Monitoring
  monitoring: {
    title: 'Monitoring',
    description: 'Konfigurieren Sie SNMP und CheckMK Monitoring für das HB-RF-ETH Gateway.',
    save: 'Speichern',
    saving: 'Speichern...',
    saveSuccess: 'Konfiguration erfolgreich gespeichert!',
    saveError: 'Fehler beim Speichern der Konfiguration!',
    snmp: {
      title: 'SNMP Agent',
      enabled: 'SNMP aktivieren',
      port: 'Port',
      portHelp: 'Standard: 161',
      community: 'Community String',
      communityHelp: 'Standard: "public" - Bitte ändern für Produktivumgebung!',
      location: 'Standort (Location)',
      locationHelp: 'Optional: z.B. "Serverraum, Gebäude A"',
      contact: 'Kontakt',
      contactHelp: 'Optional: z.B. "admin@example.com"'
    },
    checkmk: {
      title: 'CheckMK Agent',
      enabled: 'CheckMK aktivieren',
      port: 'Port',
      portHelp: 'Standard: 6556',
      allowedHosts: 'Erlaubte Client-IPs',
      allowedHostsHelp: 'Komma-getrennte IP-Adressen (z.B. "192.168.1.10,192.168.1.20") oder "*" für alle'
    },
    mqtt: {
      title: 'MQTT Client',
      enabled: 'MQTT aktivieren',
      server: 'Server',
      serverHelp: 'MQTT Broker Hostname oder IP',
      port: 'Port',
      portHelp: 'Standard: 1883',
      user: 'Benutzer',
      userHelp: 'Optional: MQTT Benutzername',
      password: 'Passwort',
      passwordHelp: 'Optional: MQTT Passwort',
      topicPrefix: 'Topic Präfix',
      topicPrefixHelp: 'Standard: hb-rf-eth - Topics lauten präfix/status/...',
      haDiscoveryEnabled: 'Home Assistant Discovery',
      haDiscoveryPrefix: 'Discovery Präfix',
      haDiscoveryPrefixHelp: 'Standard: homeassistant'
    },
    enable: 'Aktivieren',
    allowedHosts: 'Erlaubte Hosts'
  },

  // Analyzer
  analyzer: {
    title: 'Analyzer Light',
    connected: 'Verbunden',
    disconnected: 'Getrennt',
    clear: 'Leeren',
    autoScroll: 'Auto-Scroll',
    time: 'Zeit',
    len: 'Länge',
    cnt: 'Zähler',
    type: 'Typ',
    src: 'Quelle',
    dst: 'Ziel',
    payload: 'Daten',
    rssi: 'RSSI',
    deviceNames: 'Gerätenamen',
    address: 'Adresse',
    name: 'Name',
    storedNames: 'Gespeicherte Namen'
  },

  // About Page
  about: {
    title: 'Über',
    version: 'Version 2.1.0',
    fork: 'Modernisierte Fork',
    forkDescription: 'Diese Version ist eine modernisierte Fork von Xerolux (2025), basierend auf der originalen HB-RF-ETH Firmware. Aktualisiert auf ESP-IDF 5.3, moderne Toolchains (GCC 13.2.0) und aktuelle WebUI-Technologien (Vue 3, Parcel 2, Pinia).',
    original: 'Original-Autor',
    firmwareLicense: 'Die',
    hardwareLicense: 'Die',
    under: 'ist veröffentlicht unter',
    description: 'HomeMatic BidCoS/HmIP LAN Gateway',
    author: 'Autor',
    license: 'Lizenz',
    website: 'Webseite',
    documentation: 'Dokumentation',
    support: 'Support'
  },

  // Third Party
  thirdParty: {
    title: 'Software Dritter',
    containsThirdPartySoftware: 'Diese Software enthält freie Software Dritter, die unter verschiedenen Lizenzbedingungen weitergegeben wird.',
    providedAsIs: 'Die Veröffentlichung der freien Software erfolgt, „wie es ist", OHNE IRGENDEINE GARANTIE.'
  },

  // Change Password
  changePassword: {
    title: 'Passwort ändern erforderlich',
    currentPassword: 'Aktuelles Passwort',
    newPassword: 'Neues Passwort',
    confirmPassword: 'Passwort bestätigen',
    changePassword: 'Passwort ändern',
    changeSuccess: 'Passwort erfolgreich geändert',
    changeError: 'Fehler beim Ändern des Passworts',
    passwordMismatch: 'Passwörter stimmen nicht überein',
    passwordTooShort: 'Das Passwort muss mindestens 6 Zeichen lang sein und Buchstaben und Zahlen enthalten.',
    passwordsDoNotMatch: 'Passwörter stimmen nicht überein',
    warningMessage: 'Dies ist Ihre erste Anmeldung oder das Passwort ist noch auf "admin". Aus Sicherheitsgründen müssen Sie das Passwort ändern.',
    success: 'Passwort erfolgreich geändert'
  }
}
