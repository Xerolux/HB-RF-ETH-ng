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
    no: 'No'
  },

  // Header Navigation
  nav: {
    home: 'Home',
    settings: 'Impostazioni',
    firmware: 'Firmware',
    monitoring: 'Monitoraggio',
    analyzer: 'Analyzer',
    about: 'Informazioni',
    logout: 'Disconnetti'
  },

  // Login Page
  login: {
    title: 'Accesso',
    username: 'Nome utente',
    password: 'Password',
    login: 'Accedi',
    loginFailed: 'Accesso fallito',
    invalidCredentials: 'Credenziali non valide'
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

    // Messages
    saveSuccess: 'Le impostazioni sono state salvate con successo. Riavviare il sistema per applicarle.',
    saveError: 'Si è verificato un errore durante il salvataggio delle impostazioni.'
  },

  // System Info
  sysinfo: {
    title: 'Informazioni di sistema',
    serial: 'Numero di serie',
    version: 'Versione',
    latestVersion: 'Ultima versione',
    uptime: 'Tempo di attività',
    memory: 'Utilizzo memoria',
    cpu: 'Utilizzo CPU',
    temperature: 'Temperatura',
    voltage: 'Tensione di alimentazione',
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
    title: 'Aggiornamento firmware',
    currentVersion: 'Versione corrente',
    selectFile: 'Seleziona file',
    upload: 'Carica',
    uploading: 'Caricamento in corso...',
    updateSuccess: 'Firmware aggiornato con successo',
    updateError: 'Errore durante l\'aggiornamento del firmware',
    warning: 'Attenzione: Non scollegare l\'alimentazione durante l\'aggiornamento!'
  },

  // Monitoring
  monitoring: {
    title: 'Monitoraggio',
    snmp: 'SNMP',
    checkmk: 'Check_MK',
    enable: 'Abilita',
    port: 'Porta',
    community: 'Community',
    location: 'Posizione',
    contact: 'Contatto',
    allowedHosts: 'Host consentiti',
    saveSuccess: 'Impostazioni di monitoraggio salvate con successo',
    saveError: 'Errore durante il salvataggio delle impostazioni di monitoraggio'
  },

  // Analyzer
  analyzer: {
    title: 'Analyzer Light',
    connected: 'Connected',
    disconnected: 'Disconnected',
    clear: 'Clear',
    autoScroll: 'Auto Scroll',
    time: 'Time',
    len: 'Len',
    cnt: 'Cnt',
    type: 'Type',
    src: 'Source',
    dst: 'Destination',
    payload: 'Payload',
    rssi: 'RSSI',
    deviceNames: 'Device Names',
    address: 'Address',
    name: 'Name',
    storedNames: 'Stored Names'
  },

  // About Page
  about: {
    title: 'Informazioni su HB-RF-ETH-ng',
    description: 'Gateway LAN HomeMatic BidCoS/HmIP',
    version: 'Versione',
    author: 'Autore',
    license: 'Licenza',
    website: 'Sito web',
    documentation: 'Documentazione',
    support: 'Supporto'
  },

  // Change Password
  changePassword: {
    title: 'Cambia password',
    currentPassword: 'Password corrente',
    newPassword: 'Nuova password',
    confirmPassword: 'Conferma password',
    changeSuccess: 'Password cambiata con successo',
    changeError: 'Errore durante il cambio della password',
    passwordMismatch: 'Le password non corrispondono',
    passwordTooShort: 'La password è troppo corta (minimo 5 caratteri)'
  }
}
