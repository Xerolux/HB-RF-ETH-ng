export default {
  // Common
  common: {
    enabled: 'Aktivert',
    disabled: 'Deaktivert',
    save: 'Lagre',
    cancel: 'Avbryt',
    close: 'Lukk',
    loading: 'Laster...',
    error: 'Feil',
    success: 'Suksess',
    yes: 'Ja',
    no: 'Nei'
  },

  // Header Navigation
  nav: {
    home: 'Hjem',
    settings: 'Innstillinger',
    firmware: 'Fastvare',
    monitoring: 'Overvåkning',
    about: 'Om',
    logout: 'Logg ut'
  },

  // Login Page
  login: {
    title: 'Logg inn',
    username: 'Brukernavn',
    password: 'Passord',
    login: 'Logg inn',
    loginFailed: 'Pålogging mislyktes',
    invalidCredentials: 'Ugyldige påloggingsdetaljer'
  },

  // Settings Page
  settings: {
    title: 'Innstillinger',
    changePassword: 'Endre passord',
    repeatPassword: 'Gjenta passord',
    hostname: 'Vertsnavn',

    // Network Settings
    networkSettings: 'Nettverksinnstillinger',
    dhcp: 'DHCP',
    ipAddress: 'IP-adresse',
    netmask: 'Nettverksmaske',
    gateway: 'Gateway',
    dns1: 'Primær DNS',
    dns2: 'Sekundær DNS',

    // IPv6 Settings
    ipv6Settings: 'IPv6-innstillinger',
    enableIPv6: 'Aktiver IPv6',
    ipv6Mode: 'IPv6-modus',
    ipv6Auto: 'Automatisk (SLAAC)',
    ipv6Static: 'Statisk',
    ipv6Address: 'IPv6-adresse',
    ipv6PrefixLength: 'Prefikslengde',
    ipv6Gateway: 'IPv6 Gateway',
    ipv6Dns1: 'Primær IPv6 DNS',
    ipv6Dns2: 'Sekundær IPv6 DNS',

    // Time Settings
    timeSettings: 'Tidsinnstillinger',
    timesource: 'Tidskilde',
    ntp: 'NTP',
    dcf: 'DCF',
    gps: 'GPS',
    ntpServer: 'NTP-server',
    dcfOffset: 'DCF-offset',
    gpsBaudrate: 'GPS Baudrate',

    // System Settings
    systemSettings: 'Systeminnstillinger',
    ledBrightness: 'LED-lysstyrke',
    language: 'Språk',

    // DTLS Security Settings
    dtlsSettings: 'DTLS Kryptering',
    dtlsDescription: 'Sikker transportkryptering for kommunikasjon mellom kort og CCU på port 3008.',
    dtlsMode: 'Krypteringsmodus',
    dtlsModeDisabled: 'Deaktivert (standard)',
    dtlsModePsk: 'Forhåndsdelt nøkkel (PSK)',
    dtlsModeCertificate: 'X.509 Sertifikat',
    dtlsCipherSuite: 'Cipher Suite',
    dtlsCipherAes128: 'AES-128-GCM-SHA256',
    dtlsCipherAes256: 'AES-256-GCM-SHA384 (Anbefalt)',
    dtlsCipherChacha: 'ChaCha20-Poly1305-SHA256',
    dtlsRequireClientCert: 'Krev klientsertifikat',
    dtlsSessionResumption: 'Aktiver øktgjenopptagelse',
    dtlsPskManagement: 'PSK Administrasjon',
    dtlsPskIdentity: 'PSK Identitet',
    dtlsPskKey: 'PSK Nøkkel (Hex)',
    dtlsPskGenerate: 'Generer ny PSK',
    dtlsPskGenerating: 'Genererer PSK...',
    dtlsPskGenerated: 'Ny PSK generert',
    dtlsPskCopyWarning: 'VIKTIG: Kopier denne nøkkelen nå! Den vises bare én gang.',
    dtlsPskKeyLength: 'Nøkkellengde',
    dtlsPsk128bit: '128 Bit',
    dtlsPsk256bit: '256 Bit (Anbefalt)',
    dtlsPsk384bit: '384 Bit',
    dtlsPsk512bit: '512 Bit',
    dtlsPskStatus: 'PSK Status',
    dtlsPskConfigured: 'Konfigurert',
    dtlsPskNotConfigured: 'Ikke konfigurert',
    dtlsWarningDisabled: 'Kommunikasjonen er UKRYPTERT. Alle på nettverket kan fange opp trafikken.',
    dtlsWarningPsk: 'Sørg for at PSK lagres sikkert på CCU.',
    dtlsInfo: 'DTLS 1.2 krypterer Raw-UART UDP-kommunikasjon ende-til-ende. CCU må også støtte DTLS.',
    dtlsDocumentation: 'Dokumentasjon for CCU-utviklere',
    dtlsViewDocs: 'Vis implementeringsveiledning',


    // Messages
    saveSuccess: 'Innstillingene ble lagret. Start systemet på nytt for å ta dem i bruk.',
    saveError: 'Det oppstod en feil under lagring av innstillingene.'
  },

  // System Info
  sysinfo: {
    title: 'Systeminformasjon',
    serial: 'Serienummer',
    version: 'Versjon',
    latestVersion: 'Nyeste versjon',
    uptime: 'Oppetid',
    memory: 'Minnebruk',
    cpu: 'CPU-bruk',
    temperature: 'Temperatur',
    voltage: 'Forsyningsspenning',
    ethernet: 'Ethernet',
    connected: 'Tilkoblet',
    disconnected: 'Frakoblet',
    speed: 'Hastighet',
    duplex: 'Duplex',
    radioModule: 'Radiomodul',
    moduleType: 'Modultype',
    firmwareVersion: 'Fastvareversjon',
    bidcosMAC: 'BidCoS Radio MAC',
    hmipMAC: 'HmIP Radio MAC'
  },

  // Firmware Update
  firmware: {
    title: 'Fastvareoppdatering',
    currentVersion: 'Nåværende versjon',
    selectFile: 'Velg fil',
    upload: 'Last opp',
    uploading: 'Laster opp...',
    updateSuccess: 'Fastvare oppdatert',
    updateError: 'Feil ved oppdatering av fastvare',
    warning: 'Advarsel: Ikke koble fra strømmen under oppdateringen!'
  },

  // Monitoring
  monitoring: {
    title: 'Overvåkning',
    snmp: 'SNMP',
    checkmk: 'Check_MK',
    enable: 'Aktiver',
    port: 'Port',
    community: 'Community',
    location: 'Plassering',
    contact: 'Kontakt',
    allowedHosts: 'Tillatte verter',
    saveSuccess: 'Overvåkningsinnstillinger lagret',
    saveError: 'Feil ved lagring av overvåkningsinnstillinger'
  },

  // About Page
  about: {
    title: 'Om HB-RF-ETH-ng',
    description: 'HomeMatic BidCoS/HmIP LAN Gateway',
    version: 'Versjon',
    author: 'Forfatter',
    license: 'Lisens',
    website: 'Nettside',
    documentation: 'Dokumentasjon',
    support: 'Støtte'
  },

  // Change Password
  changePassword: {
    title: 'Endre passord',
    currentPassword: 'Nåværende passord',
    newPassword: 'Nytt passord',
    confirmPassword: 'Bekreft passord',
    changeSuccess: 'Passord endret',
    changeError: 'Feil ved endring av passord',
    passwordMismatch: 'Passordene samsvarer ikke',
    passwordTooShort: 'Passordet er for kort (minimum 5 tegn)'
  }
}
