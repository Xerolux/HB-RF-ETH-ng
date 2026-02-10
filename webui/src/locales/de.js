export default {
  // Common
  common: {
    enabled: 'Aktiv',
    disabled: 'Deaktiviert',
    save: 'Speichern',
    cancel: 'Abbrechen',
    close: 'Schließen',
    loading: 'Lädt...',
    changing: 'Ändert...',
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
    systemlog: 'System-Log',
    about: 'Über',
    login: 'Anmelden',
    logout: 'Abmelden',
    toggleTheme: 'Design wechseln',
    language: 'Sprache'
  },

  // Login Page
  login: {
    title: 'Bitte anmelden',
    subtitle: 'Bitte geben Sie Ihr Passwort ein',
    username: 'Benutzername',
    password: 'Passwort',
    passwordPlaceholder: 'Passwort',
    login: 'Anmelden',
    loginFailed: 'Anmeldung fehlgeschlagen',
    invalidCredentials: 'Ungültige Anmeldedaten',
    loginError: 'Anmelden war nicht erfolgreich.',
    passwordRequired: 'Passwort ist erforderlich',
    loggingIn: 'Anmeldung läuft...'
  },

  // Settings Page
  settings: {
    title: 'Einstellungen',
    ccuSettings: 'CCU Verbindung',
    ccuIpAddress: 'CCU IP-Adresse',
    ccuIpHint: 'Bitte geben Sie die IP-Adresse Ihrer CCU ein, um Verbindungsblockaden zu vermeiden. Das System startet nach dem Speichern neu.',
    tabGeneral: 'Allgemein',
    tabNetwork: 'Netzwerk',
    tabTime: 'Zeit',
    tabBackup: 'Backup',
    changePassword: 'Passwort ändern',
    repeatPassword: 'Passwort wiederholen',
    hostname: 'Hostname',

    // Security Settings
    security: 'Sicherheit',
    changePasswordHint: 'Ändern Sie Ihr Administrator-Passwort für sicheren Zugriff',
    otaPassword: 'OTA Passwort',
    otaPasswordHint: 'Separates Passwort für Firmware-Updates erforderlich',
    changePasswordBtn: 'Passwort ändern',
    changeOtaPassword: 'Ändern',
    setOtaPassword: 'Passwort setzen',
    clearOtaPassword: 'Löschen',
    clearOtaPasswordConfirm: 'Sind Sie sicher, dass Sie das OTA-Passwort entfernen möchten? Firmware-Updates sind nicht möglich, bis ein neues Passwort gesetzt wurde.',
    clearOtaPasswordSuccess: 'OTA-Passwort wurde entfernt.',
    clearOtaPasswordError: 'Fehler beim Entfernen des OTA-Passworts',

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

    // Messages
    saveSuccess: 'Einstellungen wurden erfolgreich gespeichert.',
    saveError: 'Beim Speichern der Einstellungen ist ein Fehler aufgetreten.',
    restartTitle: 'Neustart erforderlich',
    restartMessage: 'Die Einstellungen wurden gespeichert. Möchten Sie das Gerät jetzt neu starten, um die Änderungen zu übernehmen?',
    restartNow: 'Jetzt neu starten',
    restartLater: 'Später neu starten',

    // Backup & Restore
    backupRestore: 'Sichern & Wiederherstellen',
    backupInfo: 'Laden Sie eine Sicherung Ihrer Einstellungen herunter, um sie später wiederherzustellen.',
    restoreInfo: 'Laden Sie eine Sicherungsdatei hoch, um die Einstellungen wiederherzustellen. Das System wird danach neu gestartet.',
    downloadBackup: 'Sicherung herunterladen',
    restore: 'Wiederherstellen',
    restoreBtn: 'Wiederherstellen',
    download: 'Herunterladen',
    noFileChosen: 'Keine Datei ausgewählt',
    browse: 'Datei auswählen',
    restoreConfirm: 'Sind Sie sicher? Die aktuellen Einstellungen werden überschrieben und das System neu gestartet.',
    restoreSuccess: 'Einstellungen erfolgreich wiederhergestellt. System startet neu...',
    restoreError: 'Fehler beim Wiederherstellen der Einstellungen',
    backupError: 'Fehler beim Herunterladen der Sicherung'
  },

  // Update
  update: {
    available: 'Update verfügbar',
    updateNow: 'Jetzt aktualisieren'
  },

  // System Info
  sysinfo: {
    title: 'Systeminformationen',
    goodMorning: 'Guten Morgen',
    goodAfternoon: 'Guten Tag',
    goodEvening: 'Guten Abend',
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
    hmipMAC: 'HmIP Radio MAC',
    system: 'System',
    network: 'Netzwerk',
    down: 'Down',
    mbits: 'Mbit/s',
    online: 'Online',
    offline: 'Offline',
    dashboardTitle: 'Systemstatus'
  },

  // Firmware Update
  firmware: {
    title: 'Firmware',
    subtitle: 'Firmware aktualisieren',
    restarting: 'Startet neu...',
    restartingText: 'Gerät startet neu. Seite wird automatisch neu geladen.',
    fileUpload: 'Datei-Upload',
    fileUploadHint: 'Laden Sie eine .bin Firmware-Datei hoch',
    networkUpdate: 'Netzwerk-Update',
    networkUpdateHint: 'Download von URL',
    urlPlaceholder: 'https://beispiel.de/firmware.bin',
    downloading: 'Lädt herunter...',
    downloadInstall: 'Herunterladen & Installieren',
    factoryReset: 'Zurücksetzen',
    factoryResetHint: 'Auf Werkseinstellungen zurücksetzen',
    factoryResetConfirm: 'Möchten Sie wirklich auf Werkseinstellungen zurücksetzen? Alle Einstellungen gehen verloren.',
    otaProgress: 'Firmware-Update wird heruntergeladen...',
    otaSuccess: 'Download erfolgreich. Installation...',
    currentVersion: 'Aktuelle Version',
    installedVersion: 'Installierte Version',
    versionInfo: 'Modernisierte Fork v2.1.5 von Xerolux (2025) - Basierend auf der Original-Arbeit von Alexander Reinert.',
    updateAvailable: 'Ein Update auf Version {latestVersion} ist verfügbar.',
    newVersionAvailable: 'Neue Version {version} ist verfügbar!',
    viewUpdate: 'Ansehen',
    onlineUpdate: 'Online Update durchführen',
    onlineUpdateConfirm: 'Möchten Sie das Update wirklich herunterladen und installieren? Das System wird automatisch neu gestartet.',
    onlineUpdateStarted: 'Update gestartet. Das Gerät wird nach Abschluss automatisch neu gestartet.',
    showReleaseNotes: 'Release Notes anzeigen',
    releaseNotesTitle: 'Release Notes für v{version}',
    releaseNotesError: 'Release Notes konnten nicht von GitHub geladen werden.',
    updateFile: 'Firmware Datei',
    noFileChosen: 'Keine Datei ausgewählt',
    browse: 'Datei auswählen',
    selectFile: 'Datei auswählen oder hierher ziehen',
    upload: 'Firmware installieren',
    restart: 'Neustart',
    restartHint: 'Gerät neu starten',
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

  // About Page
  about: {
    title: 'Über',
    version: 'Version 2.1.5',
    fork: 'Modernisierte Fork',
    forkDescription: 'Diese Version ist eine modernisierte Fork von Xerolux (2025), basierend auf der originalen HB-RF-ETH Firmware. Aktualisiert auf ESP-IDF 5.3, moderne Toolchains und aktuelle WebUI-Technologien (Vue 3, Vite, Pinia).',
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
    subtitle: 'Sichern Sie Ihr Konto',
    warningTitle: 'Erforderlich',
    requirementsTitle: 'Passwort-Anforderungen:',
    reqMinLength: 'Mindestens 6 Zeichen',
    reqLettersNumbers: 'Muss Buchstaben und Zahlen enthalten',
    currentPassword: 'Aktuelles Passwort',
    newPassword: 'Neues Passwort',
    confirmPassword: 'Passwort bestätigen',
    changePassword: 'Passwort ändern',
    changeSuccess: 'Passwort erfolgreich geändert',
    changeError: 'Fehler beim Ändern des Passworts',
    passwordMismatch: 'Passwörter stimmen nicht überein',
    passwordTooShort: 'Das Passwort muss mindestens 6 Zeichen lang sein und Buchstaben und Zahlen enthalten.',
    passwordRequirements: 'Muss Buchstaben und Zahlen enthalten',
    passwordsDoNotMatch: 'Passwörter stimmen nicht überein',
    warningMessage: 'Dies ist Ihre erste Anmeldung oder das Passwort ist noch auf "admin". Aus Sicherheitsgründen müssen Sie das Passwort ändern.',
    success: 'Passwort erfolgreich geändert',
    newPasswordPlaceholder: 'Neues Passwort eingeben',
    confirmPasswordPlaceholder: 'Neues Passwort bestätigen'
  },

  // OTA Password Modal
  otaPassword: {
    title: 'OTA Passwort setzen',
    warningMessage: 'Setzen Sie ein separates Passwort für Firmware-Updates. Dies wird für OTA-Updates benötigt.',
    otaPassword: 'OTA Passwort',
    otaPasswordPlaceholder: 'OTA Passwort eingeben',
    confirmPassword: 'Passwort bestätigen',
    confirmPasswordPlaceholder: 'OTA Passwort bestätigen',
    passwordTooShort: 'Passwort muss mindestens 8 Zeichen lang sein',
    passwordRequirements: 'Muss Groß-, Kleinbuchstaben und Zahlen enthalten',
    passwordsDoNotMatch: 'Passwörter stimmen nicht überein',
    requirementsTitle: 'Passwort-Anforderungen:',
    reqMinLength: 'Mindestens 8 Zeichen',
    reqMixedCase: 'Groß- und Kleinbuchstaben',
    reqNumbers: 'Mindestens eine Zahl',
    strengthWeak: 'Schwach',
    strengthMedium: 'Mittel',
    strengthGood: 'Gut',
    strengthStrong: 'Stark'
  },

  // Sponsor
  sponsor: {
    title: 'Unterstützen Sie dieses Projekt',
    description: 'Wenn Ihnen dieses Projekt gefällt und Sie die Weiterentwicklung unterstützen möchten, können Sie eine der folgenden Optionen nutzen.',
    thanks: 'Vielen Dank für Ihre Unterstützung!'
  },

  // System Log
  systemlog: {
    title: 'System-Log',
    description: 'Live-Ansicht der Systemprotokollierung mit Download-Funktion.',
    liveLog: 'Live-Protokoll',
    autoScroll: 'Auto-Scroll',
    enabled: 'Aktiv',
    disabledMessage: 'Log-Anzeige ist deaktiviert. Schalten Sie den Schalter ein, um Live-Logs zu sehen.',
    refresh: 'Aktualisieren',
    clear: 'Leeren',
    download: 'Download',
    empty: 'Noch keine Log-Einträge vorhanden.'
  },

  // Changelog
  changelog: {
    title: 'Änderungsprotokoll',
    loading: 'Laden...',
    fetching: 'Lade Änderungsprotokoll von GitHub...',
    error: 'Fehler beim Laden des Änderungsprotokolls',
    fetchError: 'Änderungsprotokoll konnte nicht geladen werden. Bitte überprüfen Sie Ihre Internetverbindung.',
    retry: 'Wiederholen',
    close: 'Schließen',
    viewOnGithub: 'Auf GitHub anzeigen'
  }
}
