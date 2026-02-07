export default {
  // Common
  common: {
    enabled: 'Abilitato',
    disabled: 'Disabilitato',
    save: 'Salva',
    cancel: 'Annulla',
    close: 'Chiudi',
    loading: 'Caricamento...',
    error: 'Errore',
    success: 'Successo',
    yes: 'Sì',
    no: 'No',
    showPassword: 'Mostra password',
    hidePassword: 'Nascondi password',
    rebootingWait: 'Il sistema si sta riavviando. Attendere circa 10 secondi...',
    factoryResettingWait: 'Il sistema si sta ripristinando alle impostazioni di fabbrica e riavviando. Attendere...',
    confirmReboot: 'Sei sicuro di voler riavviare il sistema?',
    confirmFactoryReset: 'Sei sicuro? Tutte le impostazioni andranno perse!'
  },

  // Header Navigation
  nav: {
    home: 'Home',
    settings: 'Impostazioni',
    firmware: 'Firmware',
    monitoring: 'Monitoraggio',
    analyzer: 'Analyzer',
    about: 'Informazioni',
    login: 'Accedi',
    logout: 'Disconnetti',
    toggleTheme: 'Cambia tema'
  },

  // Login Page
  login: {
    title: 'Accesso',
    username: 'Nome utente',
    password: 'Password',
    login: 'Accedi',
    loginFailed: 'Accesso fallito',
    invalidCredentials: 'Credenziali non valide',
    loginError: 'L\'accesso non è riuscito.'
  },

  // Settings Page
  settings: {
    title: 'Impostazioni',
    changePassword: 'Cambia password',
    repeatPassword: 'Ripeti password',
    hostname: 'Nome host',

    // Network Settings
    networkSettings: 'Impostazioni di rete',
    dhcp: 'DHCP',
    ipAddress: 'Indirizzo IP',
    netmask: 'Maschera di rete',
    gateway: 'Gateway',
    dns1: 'DNS primario',
    dns2: 'DNS secondario',

    // IPv6 Settings
    ipv6Settings: 'Impostazioni IPv6',
    enableIPv6: 'Abilita IPv6',
    ipv6Mode: 'Modalità IPv6',
    ipv6Auto: 'Automatico (SLAAC)',
    ipv6Static: 'Statico',
    ipv6Address: 'Indirizzo IPv6',
    ipv6PrefixLength: 'Lunghezza prefisso',
    ipv6Gateway: 'Gateway IPv6',
    ipv6Dns1: 'DNS IPv6 primario',
    ipv6Dns2: 'DNS IPv6 secondario',

    // Time Settings
    timeSettings: 'Impostazioni ora',
    timesource: 'Fonte dell\'ora',
    ntp: 'NTP',
    dcf: 'DCF',
    gps: 'GPS',
    ntpServer: 'Server NTP',
    dcfOffset: 'Offset DCF',
    gpsBaudrate: 'Baudrate GPS',

    // System Settings
    systemSettings: 'Impostazioni di sistema',
    ledBrightness: 'Luminosità LED',
    language: 'Lingua',
    analyzerSettings: 'Impostazioni Analyzer Light',
    enableAnalyzer: 'Abilita Analyzer Light',
    systemMaintenance: 'Manutenzione del sistema',
    reboot: 'Riavvia',
    factoryReset: 'Ripristino di fabbrica',

    // HMLGW
    hmlgwSettings: 'Impostazioni gateway HomeMatic LAN (HMLGW)',
    enableHmlgw: 'Abilita modalità HMLGW',
    hmlgwPort: 'Porta dati (Predefinita: 2000)',
    hmlgwKeepAlivePort: 'Porta KeepAlive (Predefinita: 2001)',

    // DTLS Security Settings
    dtls: {
      title: 'Crittografia DTLS',
      description: 'Crittografia di trasporto sicura per la comunicazione tra scheda e CCU sulla porta 3008.',
      mode: 'Modalità di crittografia',
      modeDisabled: 'Disabilitato (predefinito)',
      modePsk: 'Chiave pre-condivisa (PSK)',
      modeCert: 'Certificato X.509',
      cipherSuite: 'Suite di cifratura',
      cipherAes128: 'AES-128-GCM-SHA256',
      cipherAes256: 'AES-256-GCM-SHA384 (Consigliato)',
      cipherChacha: 'ChaCha20-Poly1305-SHA256',
      requireClientCert: 'Richiedere certificato client',
      sessionResumption: 'Abilita ripresa sessione',
      pskManagement: 'Gestione PSK',
      pskIdentity: 'Identità PSK',
      pskKey: 'Chiave PSK (Hex)',
      pskGenerate: 'Genera nuova PSK',
      pskGenerating: 'Generazione PSK...',
      pskGenerated: 'Nuova PSK generata',
      pskCopyWarning: 'IMPORTANTE: Copia questa chiave ora! Verrà mostrata solo una volta.',
      pskKeyLength: 'Lunghezza chiave',
      psk128bit: '128 Bit',
      psk256bit: '256 Bit (Consigliato)',
      psk384bit: '384 Bit',
      psk512bit: '512 Bit',
      pskStatus: 'Stato PSK',
      pskConfigured: 'Configurato',
      pskNotConfigured: 'Non configurato',
      warningDisabled: 'La comunicazione NON È CRITTOGRAFATA. Chiunque sulla rete può intercettare il traffico.',
      warningPsk: 'Assicurati che il PSK sia memorizzato in modo sicuro sulla CCU.',
      info: 'DTLS 1.2 crittografa la comunicazione Raw-UART UDP end-to-end. Anche la CCU deve supportare DTLS.',
      documentation: 'Documentazione per sviluppatori CCU',
      viewDocs: 'Visualizza guida all\'implementazione',
      restartNote: 'Le modifiche alle impostazioni DTLS richiedono un riavvio del sistema.'
    },

    // Messages
    saveSuccess: 'Le impostazioni sono state salvate con successo. Riavviare il sistema per applicarle.',
    saveError: 'Si è verificato un errore durante il salvataggio delle impostazioni.',

    // Backup & Restore
    backupRestore: 'Backup e ripristino',
    backupInfo: 'Scarica un backup delle tue impostazioni per ripristinarle in seguito.',
    restoreInfo: 'Carica un file di backup per ripristinare le impostazioni. Il sistema si riavvierà in seguito.',
    downloadBackup: 'Scarica backup',
    restore: 'Ripristina',
    noFileChosen: 'Nessun file selezionato',
    browse: 'Sfoglia',
    restoreConfirm: 'Sei sicuro? Le impostazioni correnti verranno sovrascritte e il sistema si riavvierà.',
    restoreSuccess: 'Impostazioni ripristinate con successo. Riavvio del sistema...',
    restoreError: 'Errore durante il ripristino delle impostazioni',
    backupError: 'Errore durante il download del backup'
  },

  // System Info
  sysinfo: {
    title: 'Informazioni di sistema',
    serial: 'Numero di serie',
    boardRevision: 'Revisione scheda',
    uptime: 'Tempo di attività',
    resetReason: 'Ultimo riavvio',
    cpuUsage: 'Utilizzo CPU',
    memoryUsage: 'Utilizzo memoria',
    ethernetStatus: 'Connessione Ethernet',
    rawUartRemoteAddress: 'Connesso con',
    radioModuleType: 'Tipo di modulo radio',
    radioModuleSerial: 'Numero di serie',
    radioModuleFirmware: 'Versione firmware',
    radioModuleBidCosRadioMAC: 'Indirizzo radio (BidCoS)',
    radioModuleHmIPRadioMAC: 'Indirizzo radio (HmIP)',
    radioModuleSGTIN: 'SGTIN',
    version: 'Versione',
    memory: 'Utilizzo memoria',
    cpu: 'Utilizzo CPU',
    ethernet: 'Ethernet',
    connected: 'Connesso',
    disconnected: 'Disconnesso',
    speed: 'Velocità',
    duplex: 'Duplex',
    radioModule: 'Modulo radio',
    moduleType: 'Tipo di modulo',
    firmwareVersion: 'Versione firmware',
    bidcosMAC: 'MAC radio BidCoS',
    hmipMAC: 'MAC radio HmIP'
  },

  // Firmware Update
  firmware: {
    title: 'Firmware',
    currentVersion: 'Versione corrente',
    installedVersion: 'Versione installata',
    versionInfo: 'Fork modernizzato v2.1 di Xerolux (2025) - Basato sul lavoro originale di Alexander Reinert.',
    updateFile: 'File firmware',
    noFileChosen: 'Nessun file selezionato',
    browse: 'Sfoglia',
    selectFile: 'Seleziona file',
    upload: 'Carica',
    restart: 'Riavvia sistema',
    uploading: 'Caricamento in corso...',
    uploadSuccess: 'Aggiornamento firmware caricato con successo. Il sistema si riavvierà automaticamente tra 3 secondi...',
    uploadError: 'Si è verificato un errore.',
    updateSuccess: 'Firmware aggiornato con successo',
    updateError: 'Errore durante l\'aggiornamento del firmware',
    warning: 'Attenzione: Non scollegare l\'alimentazione durante l\'aggiornamento!',
    restartConfirm: 'Vuoi davvero riavviare il sistema?'
  },

  // Monitoring
  monitoring: {
    title: 'Monitoraggio',
    description: 'Configura il monitoraggio SNMP e CheckMK per il gateway HB-RF-ETH.',
    save: 'Salva',
    saving: 'Salvataggio...',
    saveSuccess: 'Configurazione salvata con successo',
    saveError: 'Errore durante il salvataggio della configurazione',
    snmp: {
      title: 'Agente SNMP',
      enabled: 'Abilita SNMP',
      port: 'Porta',
      portHelp: 'Predefinito: 161',
      community: 'Stringa community',
      communityHelp: 'Predefinito: "public" - Si prega di cambiare per la produzione!',
      location: 'Posizione',
      locationHelp: 'Opzionale: es. "Sala server, Edificio A"',
      contact: 'Contatto',
      contactHelp: 'Opzionale: es. "admin@example.com"'
    },
    checkmk: {
      title: 'Agente CheckMK',
      enabled: 'Abilita CheckMK',
      port: 'Porta',
      portHelp: 'Predefinito: 6556',
      allowedHosts: 'IP client consentiti',
      allowedHostsHelp: 'Indirizzi IP separati da virgola (es. "192.168.1.10,192.168.1.20") o "*" per tutti'
    },
    mqtt: {
      title: 'Client MQTT',
      enabled: 'Abilita MQTT',
      server: 'Server',
      serverHelp: 'Nome host o IP del broker MQTT',
      port: 'Porta',
      portHelp: 'Predefinito: 1883',
      user: 'Utente',
      userHelp: 'Opzionale: Nome utente MQTT',
      password: 'Password',
      passwordHelp: 'Opzionale: Password MQTT',
      topicPrefix: 'Prefisso topic',
      topicPrefixHelp: 'Predefinito: hb-rf-eth - I topic saranno come prefix/status/...',
      haDiscoveryEnabled: 'Scoperta Home Assistant',
      haDiscoveryPrefix: 'Prefisso scoperta',
      haDiscoveryPrefixHelp: 'Predefinito: homeassistant'
    },
    enable: 'Abilita',
    allowedHosts: 'Host consentiti'
  },

  // Analyzer
  analyzer: {
    title: 'Analyzer Light',
    disabled: 'La funzione Analyzer Light è disabilitata. Abilitala nelle Impostazioni.',
    connected: 'Connesso',
    disconnected: 'Disconnesso',
    clear: 'Cancella',
    autoScroll: 'Scorrimento automatico',
    time: 'Tempo',
    len: 'Lung.',
    cnt: 'Cont',
    type: 'Tipo',
    src: 'Origine',
    dst: 'Destinazione',
    payload: 'Dati',
    rssi: 'RSSI',
    deviceNames: 'Nomi dispositivi',
    address: 'Indirizzo',
    name: 'Nome',
    storedNames: 'Nomi memorizzati'
  },

  // About Page
  about: {
    title: 'Informazioni',
    version: 'Versione 2.1.0',
    fork: 'Fork modernizzato',
    forkDescription: 'Questa versione è un fork modernizzato di Xerolux (2025), basato sul firmware originale HB-RF-ETH. Aggiornato a ESP-IDF 5.3, toolchain moderne (GCC 13.2.0) e tecnologie WebUI attuali (Vue 3, Parcel 2, Pinia).',
    original: 'Autore originale',
    firmwareLicense: 'Il',
    hardwareLicense: 'Il',
    under: 'è rilasciato sotto',
    description: 'Gateway LAN HomeMatic BidCoS/HmIP',
    author: 'Autore',
    license: 'Licenza',
    website: 'Sito web',
    documentation: 'Documentazione',
    support: 'Supporto'
  },

  // Third Party
  thirdParty: {
    title: 'Software di terze parti',
    containsThirdPartySoftware: 'Questo software contiene prodotti software di terze parti gratuiti utilizzati con varie condizioni di licenza.',
    providedAsIs: 'Il software viene fornito "così com\'è" SENZA ALCUNA GARANZIA.'
  },

  // Change Password
  changePassword: {
    title: 'Cambio password richiesto',
    currentPassword: 'Password corrente',
    newPassword: 'Nuova password',
    confirmPassword: 'Conferma password',
    changePassword: 'Cambia password',
    changeSuccess: 'Password cambiata con successo',
    changeError: 'Errore durante il cambio della password',
    passwordMismatch: 'Le password non corrispondono',
    passwordTooShort: 'La password deve contenere almeno 6 caratteri e includere lettere e numeri.',
    passwordsDoNotMatch: 'Le password non corrispondono',
    warningMessage: 'Questo è il tuo primo accesso o la password è ancora impostata su "admin". Per motivi di sicurezza, devi cambiare la password.',
    success: 'Password cambiata con successo'
  }
}
