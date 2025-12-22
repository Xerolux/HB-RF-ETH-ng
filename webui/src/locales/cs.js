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
    no: 'Ne',
    showPassword: 'Zobrazit heslo',
    hidePassword: 'Skrýt heslo',
    rebootingWait: 'Systém se restartuje. Počkejte prosím přibližně 10 sekund...',
    factoryResettingWait: 'Systém se resetuje na tovární nastavení a restartuje se. Počkejte prosím...',
    confirmReboot: 'Opravdu chcete restartovat systém?',
    confirmFactoryReset: 'Jste si jisti? Všechna nastavení budou ztracena!'
  },

  // Header Navigation
  nav: {
    home: 'Domů',
    settings: 'Nastavení',
    firmware: 'Firmware',
    monitoring: 'Monitoring',
    analyzer: 'Analyzer',
    about: 'O aplikaci',
    login: 'Přihlásit',
    logout: 'Odhlásit',
    toggleTheme: 'Přepnout téma'
  },

  // Login Page
  login: {
    title: 'Přihlášení',
    username: 'Uživatelské jméno',
    password: 'Heslo',
    login: 'Přihlásit',
    loginFailed: 'Přihlášení selhalo',
    invalidCredentials: 'Neplatné přihlašovací údaje',
    loginError: 'Přihlášení nebylo úspěšné.'
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
    checkUpdates: 'Automaticky kontrolovat aktualizace',
    allowPrerelease: 'Povolit předběžné aktualizace (Beta/Alpha)',
    language: 'Jazyk',
    analyzerSettings: 'Nastavení Analyzer Light',
    enableAnalyzer: 'Povolit Analyzer Light',
    systemMaintenance: 'Údržba systému',
    reboot: 'Restart',
    factoryReset: 'Tovární reset',

    // HMLGW
    hmlgwSettings: 'Nastavení HomeMatic LAN Gateway (HMLGW)',
    enableHmlgw: 'Povolit režim HMLGW',
    hmlgwPort: 'Datový port (Výchozí: 2000)',
    hmlgwKeepAlivePort: 'Port KeepAlive (Výchozí: 2001)',

    // DTLS Security Settings
    dtls: {
      title: 'Šifrování DTLS',
      description: 'Bezpečné šifrování přenosu pro komunikaci mezi deskou a CCU na portu 3008.',
      mode: 'Režim šifrování',
      modeDisabled: 'Zakázáno (Výchozí)',
      modePsk: 'Předsdílený klíč (PSK)',
      modeCert: 'Certifikát X.509',
      cipherSuite: 'Šifrovací sada',
      cipherAes128: 'AES-128-GCM-SHA256',
      cipherAes256: 'AES-256-GCM-SHA384 (Doporučeno)',
      cipherChacha: 'ChaCha20-Poly1305-SHA256',
      requireClientCert: 'Vyžadovat klientský certifikát',
      sessionResumption: 'Povolit obnovení relace',
      pskManagement: 'Správa PSK',
      pskIdentity: 'Identita PSK',
      pskKey: 'Klíč PSK (Hex)',
      pskGenerate: 'Generovat nový PSK',
      pskGenerating: 'Generování PSK...',
      pskGenerated: 'Nový PSK vygenerován',
      pskCopyWarning: 'DŮLEŽITÉ: Zkopírujte tento klíč nyní! Bude zobrazen pouze jednou.',
      pskKeyLength: 'Délka klíče',
      psk128bit: '128 Bit',
      psk256bit: '256 Bit (Doporučeno)',
      psk384bit: '384 Bit',
      psk512bit: '512 Bit',
      pskStatus: 'Stav PSK',
      pskConfigured: 'Nakonfigurováno',
      pskNotConfigured: 'Nenakonfigurováno',
      warningDisabled: 'Komunikace je NEŠIFROVANÁ. Kdokoli v síti může zachytit provoz.',
      warningPsk: 'Ujistěte se, že PSK je bezpečně uložen na CCU.',
      info: 'DTLS 1.2 šifruje komunikaci Raw-UART UDP end-to-end. CCU musí také podporovat DTLS.',
      documentation: 'Dokumentace pro vývojáře CCU',
      viewDocs: 'Zobrazit průvodce implementací',
      restartNote: 'Změny nastavení DTLS vyžadují restart systému.'
    },

    // Messages
    saveSuccess: 'Nastavení byla úspěšně uložena. Restartujte systém, aby se projevila.',
    saveError: 'Při ukládání nastavení došlo k chybě.',

    // Backup & Restore
    backupRestore: 'Záloha a obnovení',
    backupInfo: 'Stáhněte si zálohu svých nastavení pro pozdější obnovení.',
    restoreInfo: 'Nahrajte záložní soubor pro obnovení nastavení. Systém se následně restartuje.',
    downloadBackup: 'Stáhnout zálohu',
    restore: 'Obnovit',
    noFileChosen: 'Nebyl vybrán žádný soubor',
    browse: 'Procházet',
    restoreConfirm: 'Jste si jisti? Aktuální nastavení budou přepsána a systém se restartuje.',
    restoreSuccess: 'Nastavení úspěšně obnovena. Systém se restartuje...',
    restoreError: 'Chyba při obnovování nastavení',
    backupError: 'Chyba při stahování zálohy'
  },

  // System Info
  sysinfo: {
    title: 'Systémové informace',
    serial: 'Sériové číslo',
    boardRevision: 'Revize desky',
    uptime: 'Doba provozu',
    resetReason: 'Poslední restart',
    cpuUsage: 'Využití CPU',
    memoryUsage: 'Využití paměti',
    ethernetStatus: 'Připojení Ethernet',
    rawUartRemoteAddress: 'Připojeno k',
    radioModuleType: 'Typ rádiového modulu',
    radioModuleSerial: 'Sériové číslo',
    radioModuleFirmware: 'Verze firmwaru',
    radioModuleBidCosRadioMAC: 'Rádiová adresa (BidCoS)',
    radioModuleHmIPRadioMAC: 'Rádiová adresa (HmIP)',
    radioModuleSGTIN: 'SGTIN',
    version: 'Verze',
    latestVersion: 'Nejnovější verze',
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
    title: 'Firmware',
    currentVersion: 'Aktuální verze',
    installedVersion: 'Instalovaná verze',
    latestVersion: 'Nejnovější dostupná verze',
    versionInfo: 'Modernizovaný fork v2.1 od Xerolux (2025) - Založeno na původní práci Alexandra Reinerta.',
    upToDate: 'Váš firmware je aktuální.',
    updateAvailable: 'Dostupná aktualizace: Verze {currentVersion} → {latestVersion}',
    versionCheckFailed: 'Kontrola verze selhala. Zkontrolujte připojení k internetu a zkuste to znovu.',
    onlineUpdate: 'Online aktualizace',
    checkUpdate: 'Zkontrolovat aktualizace nyní',
    noUpdateAvailable: 'Není k dispozici žádná aktualizace. Již používáte nejnovější verzi.',
    onlineUpdateConfirm: 'Opravdu chcete stáhnout a nainstalovat aktualizaci? Systém se automaticky restartuje.',
    onlineUpdateStarted: 'Aktualizace zahájena. Zařízení se automaticky restartuje po dokončení.',
    showReleaseNotes: 'Zobrazit poznámky k vydání',
    releaseNotesTitle: 'Poznámky k vydání pro v{version}',
    releaseNotesError: 'Nepodařilo se načíst poznámky k vydání.',
    downloadFirmware: 'Stáhnout firmware',
    updateFile: 'Soubor firmwaru',
    noFileChosen: 'Nebyl vybrán žádný soubor',
    browse: 'Procházet',
    selectFile: 'Vybrat soubor',
    upload: 'Nahrát',
    restart: 'Restartovat systém',
    uploading: 'Nahrávání...',
    uploadSuccess: 'Aktualizace firmwaru úspěšně nahrána. Systém se automaticky restartuje za 3 sekundy...',
    uploadError: 'Došlo k chybě.',
    updateSuccess: 'Firmware úspěšně aktualizován',
    updateError: 'Chyba při aktualizaci firmwaru',
    warning: 'Varování: Během aktualizace neodpojujte napájení!',
    restartConfirm: 'Opravdu chcete restartovat systém?'
  },

  // Monitoring
  monitoring: {
    title: 'Monitoring',
    description: 'Konfigurace monitorování SNMP a CheckMK pro bránu HB-RF-ETH.',
    save: 'Uložit',
    saving: 'Ukládání...',
    saveSuccess: 'Konfigurace úspěšně uložena!',
    saveError: 'Chyba při ukládání konfigurace!',
    snmp: {
      title: 'Agent SNMP',
      enabled: 'Povolit SNMP',
      port: 'Port',
      portHelp: 'Výchozí: 161',
      community: 'Community String',
      communityHelp: 'Výchozí: "public" - Prosím změňte pro produkci!',
      location: 'Umístění',
      locationHelp: 'Volitelné: např. "Serverovna, Budova A"',
      contact: 'Kontakt',
      contactHelp: 'Volitelné: např. "admin@example.com"'
    },
    checkmk: {
      title: 'Agent CheckMK',
      enabled: 'Povolit CheckMK',
      port: 'Port',
      portHelp: 'Výchozí: 6556',
      allowedHosts: 'Povolené IP adresy klientů',
      allowedHostsHelp: 'IP adresy oddělené čárkami (např. "192.168.1.10,192.168.1.20") nebo "*" pro všechny'
    },
    mqtt: {
      title: 'Klient MQTT',
      enabled: 'Povolit MQTT',
      server: 'Server',
      serverHelp: 'Hostname nebo IP MQTT Brokeru',
      port: 'Port',
      portHelp: 'Výchozí: 1883',
      user: 'Uživatel',
      userHelp: 'Volitelné: Uživatelské jméno MQTT',
      password: 'Heslo',
      passwordHelp: 'Volitelné: Heslo MQTT',
      topicPrefix: 'Prefix tématu',
      topicPrefixHelp: 'Výchozí: hb-rf-eth - Témata budou jako prefix/status/...',
      haDiscoveryEnabled: 'Detekce Home Assistant',
      haDiscoveryPrefix: 'Prefix detekce',
      haDiscoveryPrefixHelp: 'Výchozí: homeassistant'
    },
    enable: 'Povolit',
    allowedHosts: 'Povolení hostitelé'
  },

  // Analyzer
  analyzer: {
    title: 'Analyzer Light',
    disabled: 'Funkce Analyzer Light je zakázána. Povolte ji v Nastavení.',
    connected: 'Připojeno',
    disconnected: 'Odpojeno',
    clear: 'Vymazat',
    autoScroll: 'Automatické rolování',
    time: 'Čas',
    len: 'Délka',
    cnt: 'Počet',
    type: 'Typ',
    src: 'Zdroj',
    dst: 'Cíl',
    payload: 'Payload',
    rssi: 'RSSI',
    deviceNames: 'Názvy zařízení',
    address: 'Adresa',
    name: 'Název',
    storedNames: 'Uložené názvy'
  },

  // About Page
  about: {
    title: 'O aplikaci',
    version: 'Verze 2.1.0',
    fork: 'Modernizovaný fork',
    forkDescription: 'Tato verze je modernizovaný fork od Xerolux (2025), založený na původním firmwaru HB-RF-ETH. Aktualizováno na ESP-IDF 5.3, moderní toolchainy (GCC 13.2.0) a aktuální technologie WebUI (Vue 3, Parcel 2, Pinia).',
    original: 'Původní autor',
    firmwareLicense: '',
    hardwareLicense: '',
    under: 'je vydáno pod',
    description: 'HomeMatic BidCoS/HmIP LAN brána',
    author: 'Autor',
    license: 'Licence',
    website: 'Webová stránka',
    documentation: 'Dokumentace',
    support: 'Podpora'
  },

  // Third Party
  thirdParty: {
    title: 'Software třetích stran',
    containsThirdPartySoftware: 'Tento software obsahuje bezplatné softwarové produkty třetích stran používané pod různými licenčními podmínkami.',
    providedAsIs: 'Software je poskytován "JAK JE" BEZ JAKÝCHKOLI ZÁRUK.'
  },

  // Change Password
  changePassword: {
    title: 'Vyžadována změna hesla',
    currentPassword: 'Aktuální heslo',
    newPassword: 'Nové heslo',
    confirmPassword: 'Potvrdit heslo',
    changePassword: 'Změnit heslo',
    changeSuccess: 'Heslo úspěšně změněno',
    changeError: 'Chyba při změně hesla',
    passwordMismatch: 'Hesla se neshodují',
    passwordTooShort: 'Heslo musí mít alespoň 6 znaků a obsahovat písmena a čísla.',
    passwordsDoNotMatch: 'Hesla se neshodují',
    warningMessage: 'Toto je vaše první přihlášení nebo heslo je stále nastaveno na "admin". Z bezpečnostních důvodů musíte heslo změnit.',
    success: 'Heslo úspěšně změněno'
  }
}
