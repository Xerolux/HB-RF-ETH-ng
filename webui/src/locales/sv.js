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
    no: 'Nej'
  },

  // Header Navigation
  nav: {
    home: 'Hem',
    settings: 'Inställningar',
    firmware: 'Firmware',
    monitoring: 'Övervakning',
    about: 'Om',
    logout: 'Logga ut'
  },

  // Login Page
  login: {
    title: 'Logga in',
    username: 'Användarnamn',
    password: 'Lösenord',
    login: 'Logga in',
    loginFailed: 'Inloggning misslyckades',
    invalidCredentials: 'Ogiltiga inloggningsuppgifter'
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

    // DTLS Security Settings
    dtlsSettings: 'DTLS Kryptering',
    dtlsDescription: 'Säker transportkryptering för kommunikation mellan kort och CCU på port 3008.',
    dtlsMode: 'Krypteringsläge',
    dtlsModeDisabled: 'Inaktiverad (standard)',
    dtlsModePsk: 'Fördelad nyckel (PSK)',
    dtlsModeCertificate: 'X.509 Certifikat',
    dtlsCipherSuite: 'Chiffer Suite',
    dtlsCipherAes128: 'AES-128-GCM-SHA256',
    dtlsCipherAes256: 'AES-256-GCM-SHA384 (Rekommenderas)',
    dtlsCipherChacha: 'ChaCha20-Poly1305-SHA256',
    dtlsRequireClientCert: 'Kräv klientcertifikat',
    dtlsSessionResumption: 'Aktivera sessionsåterupptagning',
    dtlsPskManagement: 'PSK Hantering',
    dtlsPskIdentity: 'PSK Identitet',
    dtlsPskKey: 'PSK Nyckel (Hex)',
    dtlsPskGenerate: 'Generera ny PSK',
    dtlsPskGenerating: 'Genererar PSK...',
    dtlsPskGenerated: 'Ny PSK genererad',
    dtlsPskCopyWarning: 'VIKTIGT: Kopiera denna nyckel nu! Den visas bara en gång.',
    dtlsPskKeyLength: 'Nyckellängd',
    dtlsPsk128bit: '128 Bit',
    dtlsPsk256bit: '256 Bit (Rekommenderas)',
    dtlsPsk384bit: '384 Bit',
    dtlsPsk512bit: '512 Bit',
    dtlsPskStatus: 'PSK Status',
    dtlsPskConfigured: 'Konfigurerad',
    dtlsPskNotConfigured: 'Inte konfigurerad',
    dtlsWarningDisabled: 'Kommunikationen är OKRYPTERAD. Vem som helst på nätverket kan fånga upp trafiken.',
    dtlsWarningPsk: 'Se till att PSK lagras säkert på CCU.',
    dtlsInfo: 'DTLS 1.2 krypterar Raw-UART UDP-kommunikation end-to-end. CCU måste också stödja DTLS.',
    dtlsDocumentation: 'Dokumentation för CCU-utvecklare',
    dtlsViewDocs: 'Visa implementeringsguide',


    // Messages
    saveSuccess: 'Inställningarna sparades. Starta om systemet för att tillämpa dem.',
    saveError: 'Ett fel uppstod när inställningarna skulle sparas.'
  },

  // System Info
  sysinfo: {
    title: 'Systeminformation',
    serial: 'Serienummer',
    version: 'Version',
    latestVersion: 'Senaste version',
    uptime: 'Drifttid',
    memory: 'Minnesanvändning',
    cpu: 'CPU-användning',
    temperature: 'Temperatur',
    voltage: 'Matningsspänning',
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
    title: 'Firmware-uppdatering',
    currentVersion: 'Nuvarande version',
    selectFile: 'Välj fil',
    upload: 'Ladda upp',
    uploading: 'Laddar upp...',
    updateSuccess: 'Firmware uppdaterad',
    updateError: 'Fel vid uppdatering av firmware',
    warning: 'Varning: Koppla inte bort strömmen under uppdateringen!'
  },

  // Monitoring
  monitoring: {
    title: 'Övervakning',
    snmp: 'SNMP',
    checkmk: 'Check_MK',
    enable: 'Aktivera',
    port: 'Port',
    community: 'Community',
    location: 'Plats',
    contact: 'Kontakt',
    allowedHosts: 'Tillåtna värdar',
    saveSuccess: 'Övervakningsinställningar sparade',
    saveError: 'Fel vid sparande av övervakningsinställningar'
  },

  // About Page
  about: {
    title: 'Om HB-RF-ETH-ng',
    description: 'HomeMatic BidCoS/HmIP LAN Gateway',
    version: 'Version',
    author: 'Författare',
    license: 'Licens',
    website: 'Webbplats',
    documentation: 'Dokumentation',
    support: 'Support'
  },

  // Change Password
  changePassword: {
    title: 'Ändra lösenord',
    currentPassword: 'Nuvarande lösenord',
    newPassword: 'Nytt lösenord',
    confirmPassword: 'Bekräfta lösenord',
    changeSuccess: 'Lösenord ändrat',
    changeError: 'Fel vid ändring av lösenord',
    passwordMismatch: 'Lösenorden stämmer inte överens',
    passwordTooShort: 'Lösenordet är för kort (minst 5 tecken)'
  }
}
