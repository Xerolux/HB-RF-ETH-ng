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
    no: 'Nee'
  },

  // Header Navigation
  nav: {
    home: 'Start',
    settings: 'Instellingen',
    firmware: 'Firmware',
    monitoring: 'Monitoring',
    about: 'Over',
    logout: 'Afmelden'
  },

  // Login Page
  login: {
    title: 'Aanmelden',
    username: 'Gebruikersnaam',
    password: 'Wachtwoord',
    login: 'Aanmelden',
    loginFailed: 'Aanmelding mislukt',
    invalidCredentials: 'Ongeldige inloggegevens'
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

    // DTLS Security Settings
    dtlsSettings: 'DTLS Versleuteling',
    dtlsDescription: 'Veilige transportversleuteling voor communicatie tussen board en CCU op poort 3008.',
    dtlsMode: 'Versleutelingsmodus',
    dtlsModeDisabled: 'Uitgeschakeld (standaard)',
    dtlsModePsk: 'Vooraf gedeelde sleutel (PSK)',
    dtlsModeCertificate: 'X.509 Certificaat',
    dtlsCipherSuite: 'Cipher Suite',
    dtlsCipherAes128: 'AES-128-GCM-SHA256',
    dtlsCipherAes256: 'AES-256-GCM-SHA384 (Aanbevolen)',
    dtlsCipherChacha: 'ChaCha20-Poly1305-SHA256',
    dtlsRequireClientCert: 'Clientcertificaat vereisen',
    dtlsSessionResumption: 'Sessiehervatting inschakelen',
    dtlsPskManagement: 'PSK Beheer',
    dtlsPskIdentity: 'PSK Identiteit',
    dtlsPskKey: 'PSK Sleutel (Hex)',
    dtlsPskGenerate: 'Nieuwe PSK genereren',
    dtlsPskGenerating: 'PSK genereren...',
    dtlsPskGenerated: 'Nieuwe PSK gegenereerd',
    dtlsPskCopyWarning: 'BELANGRIJK: Kopieer deze sleutel nu! Hij wordt slechts één keer getoond.',
    dtlsPskKeyLength: 'Sleutellengte',
    dtlsPsk128bit: '128 Bit',
    dtlsPsk256bit: '256 Bit (Aanbevolen)',
    dtlsPsk384bit: '384 Bit',
    dtlsPsk512bit: '512 Bit',
    dtlsPskStatus: 'PSK Status',
    dtlsPskConfigured: 'Geconfigureerd',
    dtlsPskNotConfigured: 'Niet geconfigureerd',
    dtlsWarningDisabled: 'Communicatie is ONVERSLEUTELD. Iedereen op het netwerk kan verkeer onderscheppen.',
    dtlsWarningPsk: 'Zorg ervoor dat de PSK veilig is opgeslagen op de CCU.',
    dtlsInfo: 'DTLS 1.2 versleutelt Raw-UART UDP-communicatie end-to-end. De CCU moet ook DTLS ondersteunen.',
    dtlsDocumentation: 'Documentatie voor CCU-ontwikkelaars',
    dtlsViewDocs: 'Implementatiegids bekijken',


    // Messages
    saveSuccess: 'Instellingen zijn succesvol opgeslagen. Start het systeem opnieuw op om ze toe te passen.',
    saveError: 'Er is een fout opgetreden bij het opslaan van de instellingen.'
  },

  // System Info
  sysinfo: {
    title: 'Systeeminformatie',
    serial: 'Serienummer',
    version: 'Versie',
    latestVersion: 'Laatste versie',
    uptime: 'Uptime',
    memory: 'Geheugengebruik',
    cpu: 'CPU Gebruik',
    temperature: 'Temperatuur',
    voltage: 'Voedingsspanning',
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
    title: 'Firmware Update',
    currentVersion: 'Huidige versie',
    selectFile: 'Selecteer bestand',
    upload: 'Uploaden',
    uploading: 'Uploaden...',
    updateSuccess: 'Firmware succesvol bijgewerkt',
    updateError: 'Fout bij het bijwerken van de firmware',
    warning: 'Waarschuwing: Onderbreek de voeding niet tijdens de update!'
  },

  // Monitoring
  monitoring: {
    title: 'Monitoring',
    snmp: 'SNMP',
    checkmk: 'Check_MK',
    enable: 'Inschakelen',
    port: 'Poort',
    community: 'Community',
    location: 'Locatie',
    contact: 'Contact',
    allowedHosts: 'Toegestane hosts',
    saveSuccess: 'Monitoring-instellingen succesvol opgeslagen',
    saveError: 'Fout bij het opslaan van monitoring-instellingen'
  },

  // About Page
  about: {
    title: 'Over HB-RF-ETH-ng',
    description: 'HomeMatic BidCoS/HmIP LAN Gateway',
    version: 'Versie',
    author: 'Auteur',
    license: 'Licentie',
    website: 'Website',
    documentation: 'Documentatie',
    support: 'Ondersteuning'
  },

  // Change Password
  changePassword: {
    title: 'Wachtwoord wijzigen',
    currentPassword: 'Huidig wachtwoord',
    newPassword: 'Nieuw wachtwoord',
    confirmPassword: 'Bevestig wachtwoord',
    changeSuccess: 'Wachtwoord succesvol gewijzigd',
    changeError: 'Fout bij het wijzigen van wachtwoord',
    passwordMismatch: 'Wachtwoorden komen niet overeen',
    passwordTooShort: 'Wachtwoord is te kort (minimaal 5 tekens)'
  }
}
