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
    about: 'Über',
    logout: 'Abmelden'
  },

  // Login Page
  login: {
    title: 'Anmeldung',
    username: 'Benutzername',
    password: 'Passwort',
    login: 'Anmelden',
    loginFailed: 'Anmeldung fehlgeschlagen',
    invalidCredentials: 'Ungültige Anmeldedaten'
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
    language: 'Sprache',

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
    restoreError: 'Fehler beim Wiederherstellen der Einstellungen'
  },

  // System Info
  sysinfo: {
    title: 'Systeminformationen',
    serial: 'Seriennummer',
    version: 'Version',
    latestVersion: 'Neueste Version',
    uptime: 'Betriebszeit',
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
    title: 'Firmware Update',
    currentVersion: 'Aktuelle Version',
    selectFile: 'Datei auswählen',
    upload: 'Hochladen',
    uploading: 'Wird hochgeladen...',
    updateSuccess: 'Firmware wurde erfolgreich aktualisiert',
    updateError: 'Fehler beim Aktualisieren der Firmware',
    warning: 'Warnung: Unterbrechen Sie nicht die Stromversorgung während des Updates!'
  },

  // Monitoring
  monitoring: {
    title: 'Monitoring',
    snmp: 'SNMP',
    checkmk: 'Check_MK',
    enable: 'Aktivieren',
    port: 'Port',
    community: 'Community',
    location: 'Standort',
    contact: 'Kontakt',
    allowedHosts: 'Erlaubte Hosts',
    saveSuccess: 'Monitoring-Einstellungen erfolgreich gespeichert',
    saveError: 'Fehler beim Speichern der Monitoring-Einstellungen'
  },

  // About Page
  about: {
    title: 'Über HB-RF-ETH-ng',
    description: 'HomeMatic BidCoS/HmIP LAN Gateway',
    version: 'Version',
    author: 'Autor',
    license: 'Lizenz',
    website: 'Webseite',
    documentation: 'Dokumentation',
    support: 'Support'
  },

  // Change Password
  changePassword: {
    title: 'Passwort ändern',
    currentPassword: 'Aktuelles Passwort',
    newPassword: 'Neues Passwort',
    confirmPassword: 'Passwort bestätigen',
    changeSuccess: 'Passwort erfolgreich geändert',
    changeError: 'Fehler beim Ändern des Passworts',
    passwordMismatch: 'Passwörter stimmen nicht überein',
    passwordTooShort: 'Passwort ist zu kurz (mindestens 5 Zeichen)'
  }
}
