export default {
  // Common
  common: {
    enabled: 'Abilitato',
    disabled: 'Disabilitato',
    save: 'Salva',
    cancel: 'Annulla',
    close: 'Chiudi',
    loading: 'Caricamento...',
    changing: 'Modifica in corso...',
    error: 'Errore',
    success: 'Successo',
    yes: 'Sì',
    no: 'No'
  },

  // Header Navigation
  nav: {
    home: 'Home',
    settings: 'Impostazioni',
    firmware: 'Firmware',
    monitoring: 'Monitoraggio',
    systemlog: 'System Log',
    about: 'Informazioni',
    login: 'Accedi',
    logout: 'Esci',
    toggleTheme: 'Cambia tema',
    language: 'Lingua'
  },

  // Login Page
  login: {
    title: 'Effettua l\'accesso',
    subtitle: 'Inserisci la tua password per continuare',
    username: 'Nome utente',
    password: 'Password',
    passwordPlaceholder: 'Password',
    login: 'Accedi',
    loginFailed: 'Accesso fallito',
    invalidCredentials: 'Credenziali non valide',
    loginError: 'L\'accesso non è riuscito.',
    passwordRequired: 'La password è richiesta',
    loggingIn: 'Accesso in corso...'
  },

  // Settings Page
  settings: {
    title: 'Impostazioni',
    ccuSettings: 'Connessione CCU',
    ccuIpAddress: 'Indirizzo IP CCU',
    ccuIpHint: 'Inserisci l\'indirizzo IP della tua CCU per evitare il blocco della connessione. Il sistema si riavvierà dopo il salvataggio.',
    tabGeneral: 'Generale',
    tabNetwork: 'Rete',
    tabTime: 'Ora',
    tabBackup: 'Backup',
    changePassword: 'Cambia Password',
    repeatPassword: 'Ripeti Password',
    hostname: 'Nome Host',

    // Security Settings
    security: 'Sicurezza',
    changePasswordHint: 'Cambia la password di amministratore per un accesso sicuro',
    otaPassword: 'Password OTA',
    otaPasswordHint: 'Password separata richiesta per gli aggiornamenti del firmware',
    changePasswordBtn: 'Cambia Password',
    changeOtaPassword: 'Cambia',
    setOtaPassword: 'Imposta Password',
    clearOtaPassword: 'Rimuovi',
    clearOtaPasswordConfirm: 'Sei sicuro di voler rimuovere la password OTA? Gli aggiornamenti firmware non saranno possibili finché non verrà impostata una nuova password.',
    clearOtaPasswordSuccess: 'La password OTA è stata rimossa.',
    clearOtaPasswordError: 'Impossibile rimuovere la password OTA',

    // Network Settings
    networkSettings: 'Impostazioni di Rete',
    dhcp: 'DHCP',
    ipAddress: 'Indirizzo IP',
    netmask: 'Maschera di rete',
    gateway: 'Gateway',
    dns1: 'DNS Primario',
    dns2: 'DNS Secondario',

    // IPv6 Settings
    ipv6Settings: 'Impostazioni IPv6',
    enableIPv6: 'Abilita IPv6',
    ipv6Mode: 'Modalità IPv6',
    ipv6Auto: 'Automatico (SLAAC)',
    ipv6Static: 'Statico',
    ipv6Address: 'Indirizzo IPv6',
    ipv6PrefixLength: 'Lunghezza Prefisso',
    ipv6Gateway: 'Gateway IPv6',
    ipv6Dns1: 'DNS IPv6 Primario',
    ipv6Dns2: 'DNS IPv6 Secondario',

    // Time Settings
    timeSettings: 'Impostazioni Ora',
    timesource: 'Fonte Oraria',
    ntp: 'NTP',
    dcf: 'DCF',
    gps: 'GPS',
    ntpServer: 'Server NTP',
    dcfOffset: 'Offset DCF',
    gpsBaudrate: 'Baudrate GPS',

    // System Settings
    systemSettings: 'Impostazioni di Sistema',
    ledBrightness: 'Luminosità LED',
    updateLedBlink: 'LED lampeggiante per aggiornamenti',
    checkUpdates: 'Controlla aggiornamenti',
    allowPrerelease: 'Consenti aggiornamenti anticipati (Beta/Alpha)',
    language: 'Lingua',

    // Messages
    saveSuccess: 'Le impostazioni sono state salvate con successo.',
    saveError: 'Si è verificato un errore durante il salvataggio delle impostazioni.',
    restartTitle: 'Riavvio Richiesto',
    restartMessage: 'Le impostazioni sono state salvate. Vuoi riavviare il dispositivo ora per applicare le modifiche?',
    restartNow: 'Riavvia Ora',
    restartLater: 'Riavvia Più Tardi',

    // Backup & Restore
    backupRestore: 'Backup e Ripristino',
    backupInfo: 'Scarica un backup delle tue impostazioni per ripristinarle in seguito.',
    restoreInfo: 'Carica un file di backup per ripristinare le impostazioni. Il sistema si riavvierà successivamente.',
    downloadBackup: 'Scarica Backup',
    restore: 'Ripristina',
    restoreBtn: 'Ripristina',
    download: 'Scarica',
    noFileChosen: 'Nessun file selezionato',
    browse: 'Sfoglia',
    restoreConfirm: 'Sei sicuro? Le impostazioni attuali verranno sovrascritte e il sistema si riavvierà.',
    restoreSuccess: 'Impostazioni ripristinate con successo. Riavvio del sistema...',
    restoreError: 'Errore durante il ripristino delle impostazioni',
    backupError: 'Errore durante il download del backup'
  },

  // System Info
  sysinfo: {
    title: 'Informazioni di Sistema',
    goodMorning: 'Buongiorno',
    goodAfternoon: 'Buon pomeriggio',
    goodEvening: 'Buonasera',
    serial: 'Numero di Serie',
    boardRevision: 'Revisione Scheda',
    uptime: 'Tempo di Attività',
    resetReason: 'Ultimo Riavvio',
    cpuUsage: 'Uso CPU',
    memoryUsage: 'Uso Memoria',
    ethernetStatus: 'Connessione Ethernet',
    rawUartRemoteAddress: 'Connesso con',
    radioModuleType: 'Tipo Modulo Radio',
    radioModuleSerial: 'Numero di Serie',
    radioModuleFirmware: 'Versione Firmware',
    radioModuleBidCosRadioMAC: 'Indirizzo Radio (BidCoS)',
    radioModuleHmIPRadioMAC: 'Indirizzo Radio (HmIP)',
    radioModuleSGTIN: 'SGTIN',
    version: 'Versione',
    latestVersion: 'Ultima Versione',
    memory: 'Uso Memoria',
    cpu: 'Uso CPU',
    temperature: 'Temperatura',
    voltage: 'Tensione di Alimentazione',
    ethernet: 'Ethernet',
    connected: 'Connesso',
    disconnected: 'Disconnesso',
    speed: 'Velocità',
    duplex: 'Duplex',
    radioModule: 'Modulo Radio',
    moduleType: 'Tipo Modulo',
    firmwareVersion: 'Versione Firmware',
    bidcosMAC: 'MAC Radio BidCoS',
    hmipMAC: 'MAC Radio HmIP',
    system: 'Sistema',
    network: 'Rete',
    down: 'Giù',
    mbits: 'Mbit/s',
    online: 'Online',
    offline: 'Offline',
    dashboardTitle: 'Stato del Sistema'
  },

  // Update
  update: {
    available: 'Aggiornamento Disponibile',
    updateNow: 'Aggiorna Ora'
  },

  // Firmware Update
  firmware: {
    title: 'Firmware',
    subtitle: 'Aggiorna il firmware del dispositivo',
    restarting: 'Riavvio in corso...',
    restartingText: 'Il dispositivo si sta riavviando. La pagina si ricaricherà automaticamente.',
    fileUpload: 'Caricamento File',
    fileUploadHint: 'Carica un file firmware .bin',
    networkUpdate: 'Aggiornamento di Rete',
    networkUpdateHint: 'Scarica da URL',
    urlPlaceholder: 'https://esempio.com/firmware.bin',
    downloading: 'Download in corso...',
    downloadInstall: 'Scarica e Installa',
    factoryReset: 'Ripristina',
    factoryResetHint: 'Ripristina alle impostazioni di fabbrica',
    factoryResetConfirm: 'Vuoi davvero ripristinare le impostazioni di fabbrica? Tutte le impostazioni andranno perse.',
    otaProgress: 'Download aggiornamento firmware...',
    otaSuccess: 'Download riuscito. Installazione...',
    currentVersion: 'Versione Attuale',
    installedVersion: 'Versione Installata',
    versionInfo: 'Fork modernizzato v2.1.8 di Xerolux (2025) - Basato sul lavoro originale di Alexander Reinert.',
    updateAvailable: 'È disponibile un aggiornamento alla versione {latestVersion}.',
    newVersionAvailable: 'Nuova versione {version} disponibile!',
    viewUpdate: 'Vedi',
    onlineUpdate: 'Aggiorna Online',
    onlineUpdateConfirm: 'Vuoi davvero scaricare e installare l\'aggiornamento? Il sistema si riavvierà automaticamente.',
    onlineUpdateStarted: 'Aggiornamento avviato. Il dispositivo si riavvierà automaticamente una volta terminato.',
    showReleaseNotes: 'Mostra Note di Rilascio',
    releaseNotesTitle: 'Note di Rilascio per v{version}',
    releaseNotesError: 'Impossibile caricare le note di rilascio da GitHub.',
    updateFile: 'File Firmware',
    noFileChosen: 'Nessun file scelto',
    browse: 'Sfoglia',
    selectFile: 'Seleziona o trascina file .bin',
    upload: 'Installa Firmware',
    restart: 'Riavvia',
    restartHint: 'Riavvia dispositivo',
    uploading: 'Caricamento...',
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
    saveSuccess: 'Configurazione salvata con successo!',
    saveError: 'Errore durante il salvataggio della configurazione!',
    snmp: {
      title: 'Agente SNMP',
      enabled: 'Abilita SNMP',
      port: 'Porta',
      portHelp: 'Predefinito: 161',
      community: 'Stringa Community',
      communityHelp: 'Predefinito: "public" - Si prega di cambiare per la produzione!',
      location: 'Posizione',
      locationHelp: 'Opzionale: es. "Sala Server, Edificio A"',
      contact: 'Contatto',
      contactHelp: 'Opzionale: es. "admin@esempio.com"'
    },
    checkmk: {
      title: 'Agente CheckMK',
      enabled: 'Abilita CheckMK',
      port: 'Porta',
      portHelp: 'Predefinito: 6556',
      allowedHosts: 'IP Client Consentiti',
      allowedHostsHelp: 'Indirizzi IP separati da virgola (es. "192.168.1.10,192.168.1.20") o "*" per tutti'
    },
    mqtt: {
      title: 'Client MQTT',
      enabled: 'Abilita MQTT',
      server: 'Server',
      serverHelp: 'Hostname o IP del Broker MQTT',
      port: 'Porta',
      portHelp: 'Predefinito: 1883',
      user: 'Utente',
      userHelp: 'Opzionale: Nome Utente MQTT',
      password: 'Password',
      passwordHelp: 'Opzionale: Password MQTT',
      topicPrefix: 'Prefisso Topic',
      topicPrefixHelp: 'Predefinito: hb-rf-eth - I topic saranno come prefisso/stato/...',
      haDiscoveryEnabled: 'Home Assistant Discovery',
      haDiscoveryPrefix: 'Prefisso Discovery',
      haDiscoveryPrefixHelp: 'Predefinito: homeassistant'
    },
    enable: 'Abilita',
    allowedHosts: 'Host Consentiti'
  },

  // About Page
  about: {
    title: 'Informazioni',
    version: 'Versione 2.1.8',
    fork: 'Fork Modernizzato',
    forkDescription: 'Questa versione è un fork modernizzato da Xerolux (2025), basato sul firmware originale HB-RF-ETH. Aggiornato a ESP-IDF 5.3, toolchain moderne e tecnologie WebUI attuali (Vue 3, Vite, Pinia).',
    original: 'Autore Originale',
    firmwareLicense: 'Il',
    hardwareLicense: 'Il',
    under: 'è rilasciato sotto',
    description: 'Gateway LAN HomeMatic BidCoS/HmIP',
    author: 'Autore',
    license: 'Licenza',
    website: 'Sito Web',
    documentation: 'Documentazione',
    support: 'Supporto'
  },

  // Third Party
  thirdParty: {
    title: 'Software di Terze Parti',
    containsThirdPartySoftware: 'Questo software contiene prodotti software gratuiti di terze parti utilizzati secondo varie condizioni di licenza.',
    providedAsIs: 'Il software è fornito "così com\'è" SENZA ALCUNA GARANZIA.'
  },

  // Change Password
  changePassword: {
    title: 'Cambio Password Richiesto',
    subtitle: 'Proteggi il tuo account',
    warningTitle: 'Richiesto',
    requirementsTitle: 'Requisiti Password:',
    reqMinLength: 'Almeno 6 caratteri',
    reqLettersNumbers: 'Deve contenere lettere e numeri',
    currentPassword: 'Password Attuale',
    newPassword: 'Nuova Password',
    confirmPassword: 'Conferma Password',
    changePassword: 'Cambia Password',
    changeSuccess: 'Password cambiata con successo',
    changeError: 'Errore durante il cambio password',
    passwordMismatch: 'Le password non corrispondono',
    passwordTooShort: 'La password deve essere lunga almeno 6 caratteri e contenere lettere e numeri.',
    passwordRequirements: 'Deve contenere lettere e numeri',
    passwordsDoNotMatch: 'Le password non corrispondono',
    warningMessage: 'Questo è il tuo primo accesso o la password è ancora impostata su "admin". Per motivi di sicurezza, devi cambiare la password.',
    success: 'Password cambiata con successo',
    newPasswordPlaceholder: 'Inserisci nuova password',
    confirmPasswordPlaceholder: 'Conferma nuova password'
  },

  // OTA Password Modal
  otaPassword: {
    title: 'Imposta Password OTA',
    warningMessage: 'Imposta una password separata per gli aggiornamenti del firmware. Questo è richiesto per gli aggiornamenti OTA.',
    otaPassword: 'Password OTA',
    otaPasswordPlaceholder: 'Inserisci password OTA',
    confirmPassword: 'Conferma Password',
    confirmPasswordPlaceholder: 'Conferma password OTA',
    passwordTooShort: 'La password deve essere lunga almeno 8 caratteri',
    passwordRequirements: 'Deve contenere maiuscole, minuscole e numeri',
    passwordsDoNotMatch: 'Le password non corrispondono',
    requirementsTitle: 'Requisiti Password:',
    reqMinLength: 'Almeno 8 caratteri',
    reqMixedCase: 'Lettere maiuscole e minuscole',
    reqNumbers: 'Almeno un numero',
    strengthWeak: 'Debole',
    strengthMedium: 'Media',
    strengthGood: 'Buona',
    strengthStrong: 'Forte'
  },

  // Sponsor
  sponsor: {
    title: 'Supporta questo Progetto',
    description: 'Se ti piace questo progetto e vuoi supportare il suo sviluppo, puoi usare una delle opzioni qui sotto.',
    thanks: 'Grazie per il tuo supporto!'
  },

  // System Log
  systemlog: {
    title: 'System Log',
    description: 'Visualizzazione in tempo reale dell\'output del registro di sistema con funzionalità di download.',
    liveLog: 'Log in Diretta',
    autoScroll: 'Scorrimento Automatico',
    enabled: 'Abilitato',
    disabledMessage: 'Il visualizzatore di log è disabilitato. Accendi l\'interruttore per vedere i log in diretta.',
    refresh: 'Aggiorna',
    clear: 'Pulisci',
    download: 'Scarica',
    empty: 'Nessuna voce di log ancora.'
  },

  // Changelog
  changelog: {
    title: 'Registro delle Modifiche',
    loading: 'Caricamento...',
    fetching: 'Recupero registro modifiche da GitHub...',
    error: 'Impossibile caricare il registro delle modifiche',
    fetchError: 'Impossibile recuperare il registro delle modifiche. Controlla la tua connessione internet.',
    retry: 'Riprova',
    close: 'Chiudi',
    viewOnGithub: 'Vedi su GitHub'
  }
}
