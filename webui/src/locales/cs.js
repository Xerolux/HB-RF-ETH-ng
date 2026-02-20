export default {
  // Common
  common: {
    enabled: 'Povoleno',
    disabled: 'Zakázáno',
    save: 'Uložit',
    cancel: 'Zrušit',
    close: 'Zavřít',
    loading: 'Načítání...',
    changing: 'Měním...',
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
    monitoring: 'Monitorování',
    systemlog: 'Systémový protokol',
    about: 'O programu',
    login: 'Přihlásit',
    logout: 'Odhlásit',
    toggleTheme: 'Přepnout téma',
    language: 'Jazyk'
  },

  // Login Page
  login: {
    title: 'Prosím přihlašte se',
    subtitle: 'Zadejte své heslo pro pokračování',
    username: 'Uživatelské jméno',
    password: 'Heslo',
    passwordPlaceholder: 'Heslo',
    login: 'Přihlásit',
    loginFailed: 'Přihlášení se nezdařilo',
    invalidCredentials: 'Neplatné přihlašovací údaje',
    loginError: 'Přihlášení nebylo úspěšné.',
    passwordRequired: 'Heslo je vyžadováno',
    loggingIn: 'Přihlašování...'
  },

  // Settings Page
  settings: {
    title: 'Nastavení',
    ccuSettings: 'Připojení CCU',
    ccuIpAddress: 'IP adresa CCU',
    ccuIpHint: 'Zadejte prosím IP adresu vaší CCU, aby se zabránilo blokování připojení. Systém se po uložení restartuje.',
    tabGeneral: 'Obecné',
    tabNetwork: 'Síť',
    tabTime: 'Čas',
    tabBackup: 'Záloha',
    changePassword: 'Změnit heslo',
    repeatPassword: 'Opakovat heslo',
    hostname: 'Název hostitele',

    // Security Settings
    security: 'Zabezpečení',
    changePasswordHint: 'Změňte své administrátorské heslo pro bezpečný přístup',
    otaPassword: 'OTA Heslo',
    otaPasswordHint: 'Pro aktualizace firmwaru je vyžadováno samostatné heslo',
    changePasswordBtn: 'Změnit heslo',
    changeOtaPassword: 'Změnit',
    setOtaPassword: 'Nastavit heslo',
    clearOtaPassword: 'Vymazat',
    clearOtaPasswordConfirm: 'Jste si jisti, že chcete odstranit heslo OTA? Aktualizace firmwaru nebudou možné, dokud nebude nastaveno nové heslo.',
    clearOtaPasswordSuccess: 'Heslo OTA bylo odstraněno.',
    clearOtaPasswordError: 'Nepodařilo se odstranit heslo OTA',

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
    ipv6Static: 'Staticky',
    ipv6Address: 'IPv6 Adresa',
    ipv6PrefixLength: 'Délka prefixu',
    ipv6Gateway: 'IPv6 Brána',
    ipv6Dns1: 'Primární IPv6 DNS',
    ipv6Dns2: 'Sekundární IPv6 DNS',

    // Time Settings
    timeSettings: 'Nastavení času',
    timesource: 'Zdroj času',
    ntp: 'NTP',
    dcf: 'DCF',
    gps: 'GPS',
    ntpServer: 'NTP server',
    dcfOffset: 'DCF posun',
    gpsBaudrate: 'GPS Baudrate',

    // System Settings
    systemSettings: 'Nastavení systému',
    ledBrightness: 'Jas LED',
    updateLedBlink: 'Blikat LED při aktualizacích',
    checkUpdates: 'Zkontrolovat aktualizace',
    allowPrerelease: 'Povolit dřívější aktualizace (Beta/Alpha)',
    language: 'Jazyk',

    // Messages
    saveSuccess: 'Nastavení bylo úspěšně uloženo.',
    saveError: 'Při ukládání nastavení došlo k chybě.',
    restartTitle: 'Vyžadován restart',
    restartMessage: 'Nastavení bylo uloženo. Chcete nyní restartovat zařízení, aby se změny projevily?',
    restartNow: 'Restartovat nyní',
    restartLater: 'Restartovat později',

    // Backup & Restore
    backupRestore: 'Zálohování a obnovení',
    backupInfo: 'Stáhněte si zálohu nastavení pro pozdější obnovení.',
    restoreInfo: 'Nahrajte soubor zálohy pro obnovení nastavení. Systém se poté restartuje.',
    downloadBackup: 'Stáhnout zálohu',
    restore: 'Obnovit',
    restoreBtn: 'Obnovit',
    download: 'Stáhnout',
    noFileChosen: 'Nebyl vybrán žádný soubor',
    browse: 'Procházet',
    restoreConfirm: 'Jste si jisti? Současné nastavení bude přepsáno a systém se restartuje.',
    restoreSuccess: 'Nastavení úspěšně obnoveno. Restartování systému...',
    restoreError: 'Chyba při obnově nastavení',
    backupError: 'Chyba při stahování zálohy'
  },

  // System Info
  sysinfo: {
    title: 'Informace o systému',
    goodMorning: 'Dobré ráno',
    goodAfternoon: 'Dobré odpoledne',
    goodEvening: 'Dobrý večer',
    serial: 'Sériové číslo',
    boardRevision: 'Revize desky',
    uptime: 'Doba běhu',
    resetReason: 'Poslední restart',
    cpuUsage: 'Využití CPU',
    memoryUsage: 'Využití paměti',
    ethernetStatus: 'Připojení Ethernet',
    rawUartRemoteAddress: 'Připojeno s',
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
    hmipMAC: 'MAC rádia HmIP',
    system: 'Systém',
    network: 'Síť',
    down: 'Dolů',
    mbits: 'Mbit/s',
    online: 'Online',
    offline: 'Offline',
    dashboardTitle: 'Stav systému'
  },

  // Update
  update: {
    available: 'Aktualizace k dispozici',
    updateNow: 'Aktualizovat nyní'
  },

  // Firmware Update
  firmware: {
    title: 'Firmware',
    subtitle: 'Aktualizovat firmware zařízení',
    restarting: 'Restartování...',
    restartingText: 'Zařízení se restartuje. Stránka se automaticky obnoví.',
    fileUpload: 'Nahrání souboru',
    fileUploadHint: 'Nahrajte soubor firmwaru .bin',
    networkUpdate: 'Síťová aktualizace',
    networkUpdateHint: 'Stáhnout z URL',
    urlPlaceholder: 'https://priklad.cz/firmware.bin',
    downloading: 'Stahování...',
    downloadInstall: 'Stáhnout a nainstalovat',
    factoryReset: 'Obnovit',
    factoryResetHint: 'Obnovit do továrního nastavení',
    factoryResetConfirm: 'Opravdu chcete obnovit tovární nastavení? Všechna nastavení budou ztracena.',
    otaProgress: 'Stahování aktualizace firmwaru...',
    otaSuccess: 'Stažení úspěšné. Instalace...',
    currentVersion: 'Současná verze',
    installedVersion: 'Nainstalovaná verze',
    versionInfo: 'Modernizovaný fork v2.1.8 od Xerolux (2025) - Založeno na původní práci Alexandra Reinerta.',
    updateAvailable: 'Je k dispozici aktualizace na verzi {latestVersion}.',
    newVersionAvailable: 'Nová verze {version} je k dispozici!',
    viewUpdate: 'Zobrazit',
    onlineUpdate: 'Aktualizovat online',
    onlineUpdateConfirm: 'Opravdu chcete stáhnout a nainstalovat aktualizaci? Systém se automaticky restartuje.',
    onlineUpdateStarted: 'Aktualizace zahájena. Zařízení se po dokončení automaticky restartuje.',
    showReleaseNotes: 'Zobrazit poznámky k vydání',
    releaseNotesTitle: 'Poznámky k vydání pro v{version}',
    releaseNotesError: 'Nepodařilo se načíst poznámky k vydání z GitHubu.',
    updateFile: 'Soubor firmwaru',
    noFileChosen: 'Nebyl vybrán žádný soubor',
    browse: 'Procházet',
    selectFile: 'Vyberte nebo přetáhněte soubor .bin',
    upload: 'Nainstalovat firmware',
    restart: 'Restartovat',
    restartHint: 'Restartovat zařízení',
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
    title: 'Monitorování',
    description: 'Nakonfigurujte monitorování SNMP a CheckMK pro bránu HB-RF-ETH.',
    save: 'Uložit',
    saving: 'Ukládání...',
    saveSuccess: 'Konfigurace úspěšně uložena!',
    saveError: 'Chyba při ukládání konfigurace!',
    snmp: {
      title: 'SNMP Agent',
      enabled: 'Povolit SNMP',
      port: 'Port',
      portHelp: 'Výchozí: 161',
      community: 'Community String',
      communityHelp: 'Výchozí: "public" - Prosím změňte pro produkci!',
      location: 'Umístění',
      locationHelp: 'Volitelné: např. "Serverovna, Budova A"',
      contact: 'Kontakt',
      contactHelp: 'Volitelné: např. "admin@priklad.cz"'
    },
    checkmk: {
      title: 'CheckMK Agent',
      enabled: 'Povolit CheckMK',
      port: 'Port',
      portHelp: 'Výchozí: 6556',
      allowedHosts: 'Povolené IP klientů',
      allowedHostsHelp: 'IP adresy oddělené čárkami (např. "192.168.1.10,192.168.1.20") nebo "*" pro všechny'
    },
    mqtt: {
      title: 'MQTT Klient',
      enabled: 'Povolit MQTT',
      server: 'Server',
      serverHelp: 'Hostitelské jméno nebo IP brokera MQTT',
      port: 'Port',
      portHelp: 'Výchozí: 1883',
      user: 'Uživatel',
      userHelp: 'Volitelné: Uživatelské jméno MQTT',
      password: 'Heslo',
      passwordHelp: 'Volitelné: Heslo MQTT',
      topicPrefix: 'Prefix tématu',
      topicPrefixHelp: 'Výchozí: hb-rf-eth - Témata budou jako prefix/status/...',
      haDiscoveryEnabled: 'Home Assistant Discovery',
      haDiscoveryPrefix: 'Discovery Prefix',
      haDiscoveryPrefixHelp: 'Výchozí: homeassistant'
    },
    enable: 'Povolit',
    allowedHosts: 'Povolené hostitelé'
  },

  // About Page
  about: {
    title: 'O programu',
    version: 'Verze 2.1.8',
    fork: 'Modernizovaný Fork',
    forkDescription: 'Tato verze je modernizovaný fork od Xerolux (2025), založený na původním firmwaru HB-RF-ETH. Aktualizováno na ESP-IDF 5.3, moderní toolchainy a současné technologie WebUI (Vue 3, Vite, Pinia).',
    original: 'Původní Autor',
    firmwareLicense: 'Tento',
    hardwareLicense: 'Tento',
    under: 'je vydán pod',
    description: 'LAN brána HomeMatic BidCoS/HmIP',
    author: 'Autor',
    license: 'Licence',
    website: 'Webová stránka',
    documentation: 'Dokumentace',
    support: 'Podpora'
  },

  // Third Party
  thirdParty: {
    title: 'Software třetích stran',
    containsThirdPartySoftware: 'Tento software obsahuje bezplatné softwarové produkty třetích stran používané za různých licenčních podmínek.',
    providedAsIs: 'Software je poskytován "tak jak je" BEZ JAKÉKOLI ZÁRUKY.'
  },

  // Change Password
  changePassword: {
    title: 'Vyžadována změna hesla',
    subtitle: 'Zabezpečte svůj účet',
    warningTitle: 'Vyžadováno',
    requirementsTitle: 'Požadavky na heslo:',
    reqMinLength: 'Alespoň 6 znaků',
    reqLettersNumbers: 'Musí obsahovat písmena a čísla',
    currentPassword: 'Současné heslo',
    newPassword: 'Nové heslo',
    confirmPassword: 'Potvrdit heslo',
    changePassword: 'Změnit heslo',
    changeSuccess: 'Heslo úspěšně změněno',
    changeError: 'Chyba při změně hesla',
    passwordMismatch: 'Hesla se neshodují',
    passwordTooShort: 'Heslo musí mít alespoň 6 znaků a obsahovat písmena a čísla.',
    passwordRequirements: 'Musí obsahovat písmena a čísla',
    passwordsDoNotMatch: 'Hesla se neshodují',
    warningMessage: 'Toto je vaše první přihlášení nebo je heslo stále nastaveno na "admin". Z bezpečnostních důvodů musíte heslo změnit.',
    success: 'Heslo úspěšně změněno',
    newPasswordPlaceholder: 'Zadejte nové heslo',
    confirmPasswordPlaceholder: 'Potvrďte nové heslo'
  },

  // OTA Password Modal
  otaPassword: {
    title: 'Nastavit heslo OTA',
    warningMessage: 'Nastavte samostatné heslo pro aktualizace firmwaru. Toto je vyžadováno pro aktualizace OTA.',
    otaPassword: 'Heslo OTA',
    otaPasswordPlaceholder: 'Zadejte heslo OTA',
    confirmPassword: 'Potvrdit heslo',
    confirmPasswordPlaceholder: 'Potvrďte heslo OTA',
    passwordTooShort: 'Heslo musí mít alespoň 8 znaků',
    passwordRequirements: 'Musí obsahovat velká, malá písmena a čísla',
    passwordsDoNotMatch: 'Hesla se neshodují',
    requirementsTitle: 'Požadavky na heslo:',
    reqMinLength: 'Alespoň 8 znaků',
    reqMixedCase: 'Velká a malá písmena',
    reqNumbers: 'Alespoň jedno číslo',
    strengthWeak: 'Slabé',
    strengthMedium: 'Střední',
    strengthGood: 'Dobré',
    strengthStrong: 'Silné'
  },

  // Sponsor
  sponsor: {
    title: 'Podpořit tento projekt',
    description: 'Pokud se vám tento projekt líbí a chcete podpořit jeho vývoj, můžete využít jednu z níže uvedených možností.',
    thanks: 'Děkujeme za vaši podporu!'
  },

  // System Log
  systemlog: {
    title: 'Systémový protokol',
    description: 'Živý pohled na výstup systémového protokolu s možností stažení.',
    liveLog: 'Živý protokol',
    autoScroll: 'Automatické posouvání',
    enabled: 'Povoleno',
    disabledMessage: 'Prohlížeč protokolů je vypnutý. Zapněte přepínač pro zobrazení živých protokolů.',
    refresh: 'Obnovit',
    clear: 'Vymazat',
    download: 'Stáhnout',
    empty: 'Zatím žádné záznamy v protokolu.'
  },

  // Changelog
  changelog: {
    title: 'Seznam změn',
    loading: 'Načítání...',
    fetching: 'Načítání seznamu změn z GitHubu...',
    error: 'Nepodařilo se načíst seznam změn',
    fetchError: 'Nepodařilo se načíst seznam změn. Zkontrolujte prosím své připojení k internetu.',
    retry: 'Zkusit znovu',
    close: 'Zavřít',
    viewOnGithub: 'Zobrazit na GitHubu'
  }
}
