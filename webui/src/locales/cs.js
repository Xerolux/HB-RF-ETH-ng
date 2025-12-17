export default {
  // Common
  common: {
    enabled: 'Povoleno',
    disabled: 'Zakázáno',
    save: 'Uložit',
    cancel: 'Zrušit',
    close: 'Zavřít',
    loading: 'Načítání...',
    error: 'Chyba',
    success: 'Úspěch',
    yes: 'Ano',
    no: 'Ne'
  },

  // Header Navigation
  nav: {
    home: 'Domů',
    settings: 'Nastavení',
    firmware: 'Firmware',
    monitoring: 'Monitoring',
    about: 'O aplikaci',
    logout: 'Odhlásit'
  },

  // Login Page
  login: {
    title: 'Přihlášení',
    username: 'Uživatelské jméno',
    password: 'Heslo',
    login: 'Přihlásit',
    loginFailed: 'Přihlášení selhalo',
    invalidCredentials: 'Neplatné přihlašovací údaje'
  },

  // Settings Page
  settings: {
    title: 'Nastavení',
    changePassword: 'Změnit heslo',
    repeatPassword: 'Opakovat heslo',
    hostname: 'Název hostitele',

    // Network Settings
    networkSettings: 'Nastavení sítě',
    dhcp: 'DHCP',
    ipAddress: 'IP adresa',
    netmask: 'Maska sítě',
    gateway: 'Brána',
    dns1: 'Primární DNS',
    dns2: 'Sekundární DNS',

    // IPv6 Settings
    ipv6Settings: 'Nastavení IPv6',
    enableIPv6: 'Povolit IPv6',
    ipv6Mode: 'Režim IPv6',
    ipv6Auto: 'Automaticky (SLAAC)',
    ipv6Static: 'Statický',
    ipv6Address: 'Adresa IPv6',
    ipv6PrefixLength: 'Délka prefixu',
    ipv6Gateway: 'Brána IPv6',
    ipv6Dns1: 'Primární DNS IPv6',
    ipv6Dns2: 'Sekundární DNS IPv6',

    // Time Settings
    timeSettings: 'Nastavení času',
    timesource: 'Zdroj času',
    ntp: 'NTP',
    dcf: 'DCF',
    gps: 'GPS',
    ntpServer: 'Server NTP',
    dcfOffset: 'Offset DCF',
    gpsBaudrate: 'Baudrate GPS',

    // System Settings
    systemSettings: 'Nastavení systému',
    ledBrightness: 'Jas LED',
    language: 'Jazyk',

    // DTLS Security Settings
    dtlsSettings: 'Šifrování DTLS',
    dtlsDescription: 'Bezpečné šifrování přenosu pro komunikaci mezi deskou a CCU na portu 3008.',
    dtlsMode: 'Režim šifrování',
    dtlsModeDisabled: 'Zakázáno (výchozí)',
    dtlsModePsk: 'Předsdílený klíč (PSK)',
    dtlsModeCertificate: 'Certifikát X.509',
    dtlsCipherSuite: 'Šifrovací sada',
    dtlsCipherAes128: 'AES-128-GCM-SHA256',
    dtlsCipherAes256: 'AES-256-GCM-SHA384 (Doporučeno)',
    dtlsCipherChacha: 'ChaCha20-Poly1305-SHA256',
    dtlsRequireClientCert: 'Vyžadovat klientský certifikát',
    dtlsSessionResumption: 'Povolit obnovení relace',
    dtlsPskManagement: 'Správa PSK',
    dtlsPskIdentity: 'Identita PSK',
    dtlsPskKey: 'Klíč PSK (Hex)',
    dtlsPskGenerate: 'Generovat nový PSK',
    dtlsPskGenerating: 'Generování PSK...',
    dtlsPskGenerated: 'Nový PSK vygenerován',
    dtlsPskCopyWarning: 'DŮLEŽITÉ: Zkopírujte tento klíč nyní! Bude zobrazen pouze jednou.',
    dtlsPskKeyLength: 'Délka klíče',
    dtlsPsk128bit: '128 Bit',
    dtlsPsk256bit: '256 Bit (Doporučeno)',
    dtlsPsk384bit: '384 Bit',
    dtlsPsk512bit: '512 Bit',
    dtlsPskStatus: 'Stav PSK',
    dtlsPskConfigured: 'Nakonfigurováno',
    dtlsPskNotConfigured: 'Nenakonfigurováno',
    dtlsWarningDisabled: 'Komunikace je NEŠIFROVANÁ. Kdokoli v síti může zachytit provoz.',
    dtlsWarningPsk: 'Ujistěte se, že PSK je bezpečně uložen na CCU.',
    dtlsInfo: 'DTLS 1.2 šifruje komunikaci Raw-UART UDP end-to-end. CCU musí také podporovat DTLS.',
    dtlsDocumentation: 'Dokumentace pro vývojáře CCU',
    dtlsViewDocs: 'Zobrazit průvodce implementací',


    // Messages
    saveSuccess: 'Nastavení byla úspěšně uložena. Restartujte systém, aby se projevila.',
    saveError: 'Při ukládání nastavení došlo k chybě.'
  },

  // System Info
  sysinfo: {
    title: 'Systémové informace',
    serial: 'Sériové číslo',
    version: 'Verze',
    latestVersion: 'Nejnovější verze',
    uptime: 'Doba provozu',
    memory: 'Využití paměti',
    cpu: 'Využití CPU',
    temperature: 'Teplota',
    voltage: 'Napájecí napětí',
    ethernet: 'Ethernet',
    connected: 'Připojeno',
    disconnected: 'Odpojeno',
    speed: 'Rychlost',
    duplex: 'Duplex',
    radioModule: 'Rádiový modul',
    moduleType: 'Typ modulu',
    firmwareVersion: 'Verze firmwaru',
    bidcosMAC: 'MAC rádia BidCoS',
    hmipMAC: 'MAC rádia HmIP'
  },

  // Firmware Update
  firmware: {
    title: 'Aktualizace firmwaru',
    currentVersion: 'Aktuální verze',
    selectFile: 'Vybrat soubor',
    upload: 'Nahrát',
    uploading: 'Nahrávání...',
    updateSuccess: 'Firmware úspěšně aktualizován',
    updateError: 'Chyba při aktualizaci firmwaru',
    warning: 'Varování: Během aktualizace neodpojujte napájení!'
  },

  // Monitoring
  monitoring: {
    title: 'Monitoring',
    snmp: 'SNMP',
    checkmk: 'Check_MK',
    enable: 'Povolit',
    port: 'Port',
    community: 'Komunita',
    location: 'Umístění',
    contact: 'Kontakt',
    allowedHosts: 'Povolení hostitelé',
    saveSuccess: 'Nastavení monitoringu úspěšně uloženo',
    saveError: 'Chyba při ukládání nastavení monitoringu'
  },

  // About Page
  about: {
    title: 'O HB-RF-ETH-ng',
    description: 'HomeMatic BidCoS/HmIP LAN brána',
    version: 'Verze',
    author: 'Autor',
    license: 'Licence',
    website: 'Webová stránka',
    documentation: 'Dokumentace',
    support: 'Podpora'
  },

  // Change Password
  changePassword: {
    title: 'Změnit heslo',
    currentPassword: 'Aktuální heslo',
    newPassword: 'Nové heslo',
    confirmPassword: 'Potvrdit heslo',
    changeSuccess: 'Heslo úspěšně změněno',
    changeError: 'Chyba při změně hesla',
    passwordMismatch: 'Hesla se neshodují',
    passwordTooShort: 'Heslo je příliš krátké (minimálně 5 znaků)'
  }
}
