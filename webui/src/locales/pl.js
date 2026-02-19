export default {
  // Common
  common: {
    enabled: 'Włączony',
    disabled: 'Wyłączony',
    save: 'Zapisz',
    cancel: 'Anuluj',
    close: 'Zamknij',
    loading: 'Ładowanie...',
    changing: 'Zmienianie...',
    error: 'Błąd',
    success: 'Sukces',
    yes: 'Tak',
    no: 'Nie'
  },

  // Header Navigation
  nav: {
    home: 'Strona główna',
    settings: 'Ustawienia',
    firmware: 'Firmware',
    monitoring: 'Monitoring',
    systemlog: 'Dziennik systemowy',
    about: 'O programie',
    login: 'Zaloguj',
    logout: 'Wyloguj',
    toggleTheme: 'Zmień motyw',
    language: 'Język'
  },

  // Login Page
  login: {
    title: 'Proszę się zalogować',
    subtitle: 'Wprowadź hasło, aby kontynuować',
    username: 'Nazwa użytkownika',
    password: 'Hasło',
    passwordPlaceholder: 'Hasło',
    login: 'Zaloguj',
    loginFailed: 'Logowanie nieudane',
    invalidCredentials: 'Nieprawidłowe dane logowania',
    loginError: 'Logowanie nie powiodło się.',
    passwordRequired: 'Hasło jest wymagane',
    loggingIn: 'Logowanie...'
  },

  // Settings Page
  settings: {
    title: 'Ustawienia',
    ccuSettings: 'Połączenie CCU',
    ccuIpAddress: 'Adres IP CCU',
    ccuIpHint: 'Wprowadź adres IP swojego CCU, aby zapobiec blokowaniu połączenia. System uruchomi się ponownie po zapisaniu.',
    tabGeneral: 'Ogólne',
    tabNetwork: 'Sieć',
    tabTime: 'Czas',
    tabBackup: 'Kopia zapasowa',
    changePassword: 'Zmień hasło',
    repeatPassword: 'Powtórz hasło',
    hostname: 'Nazwa hosta',

    // Security Settings
    security: 'Bezpieczeństwo',
    changePasswordHint: 'Zmień hasło administratora dla bezpiecznego dostępu',
    otaPassword: 'Hasło OTA',
    otaPasswordHint: 'Wymagane osobne hasło do aktualizacji firmware',
    changePasswordBtn: 'Zmień hasło',
    changeOtaPassword: 'Zmień',
    setOtaPassword: 'Ustaw hasło',
    clearOtaPassword: 'Wyczyść',
    clearOtaPasswordConfirm: 'Czy na pewno chcesz usunąć hasło OTA? Aktualizacje firmware nie będą możliwe do czasu ustawienia nowego hasła.',
    clearOtaPasswordSuccess: 'Hasło OTA zostało usunięte.',
    clearOtaPasswordError: 'Nie udało się usunąć hasła OTA',

    // Network Settings
    networkSettings: 'Ustawienia sieci',
    dhcp: 'DHCP',
    ipAddress: 'Adres IP',
    netmask: 'Maska sieci',
    gateway: 'Brama',
    dns1: 'Podstawowy DNS',
    dns2: 'Zapasowy DNS',

    // IPv6 Settings
    ipv6Settings: 'Ustawienia IPv6',
    enableIPv6: 'Włącz IPv6',
    ipv6Mode: 'Tryb IPv6',
    ipv6Auto: 'Automatyczny (SLAAC)',
    ipv6Static: 'Statyczny',
    ipv6Address: 'Adres IPv6',
    ipv6PrefixLength: 'Długość prefiksu',
    ipv6Gateway: 'Brama IPv6',
    ipv6Dns1: 'Podstawowy DNS IPv6',
    ipv6Dns2: 'Zapasowy DNS IPv6',

    // Time Settings
    timeSettings: 'Ustawienia czasu',
    timesource: 'Źródło czasu',
    ntp: 'NTP',
    dcf: 'DCF',
    gps: 'GPS',
    ntpServer: 'Serwer NTP',
    dcfOffset: 'Przesunięcie DCF',
    gpsBaudrate: 'Prędkość GPS',

    // System Settings
    systemSettings: 'Ustawienia systemu',
    ledBrightness: 'Jasność LED',
    updateLedBlink: 'Migaj LED podczas aktualizacji',
    checkUpdates: 'Sprawdź aktualizacje',
    allowPrerelease: 'Zezwalaj na wczesne aktualizacje (Beta/Alpha)',
    language: 'Język',

    // Messages
    saveSuccess: 'Ustawienia zostały pomyślnie zapisane.',
    saveError: 'Wystąpił błąd podczas zapisywania ustawień.',
    restartTitle: 'Wymagane ponowne uruchomienie',
    restartMessage: 'Ustawienia zostały zapisane. Czy chcesz teraz uruchomić urządzenie ponownie, aby zastosować zmiany?',
    restartNow: 'Uruchom ponownie teraz',
    restartLater: 'Uruchom ponownie później',

    // Backup & Restore
    backupRestore: 'Kopia zapasowa i przywracanie',
    backupInfo: 'Pobierz kopię zapasową swoich ustawień, aby przywrócić je później.',
    restoreInfo: 'Prześlij plik kopii zapasowej, aby przywrócić ustawienia. System uruchomi się ponownie po zakończeniu.',
    downloadBackup: 'Pobierz kopię zapasową',
    restore: 'Przywróć',
    restoreBtn: 'Przywróć',
    download: 'Pobierz',
    noFileChosen: 'Nie wybrano pliku',
    browse: 'Przeglądaj',
    restoreConfirm: 'Jesteś pewien? Obecne ustawienia zostaną nadpisane, a system uruchomi się ponownie.',
    restoreSuccess: 'Ustawienia przywrócone pomyślnie. Restartowanie systemu...',
    restoreError: 'Błąd przywracania ustawień',
    backupError: 'Błąd pobierania kopii zapasowej'
  },

  // System Info
  sysinfo: {
    title: 'Informacje o systemie',
    goodMorning: 'Dzień dobry',
    goodAfternoon: 'Dzień dobry',
    goodEvening: 'Dobry wieczór',
    serial: 'Numer seryjny',
    boardRevision: 'Rewizja płytki',
    uptime: 'Czas pracy',
    resetReason: 'Ostatni restart',
    cpuUsage: 'Użycie CPU',
    memoryUsage: 'Użycie pamięci',
    ethernetStatus: 'Połączenie Ethernet',
    rawUartRemoteAddress: 'Połączony z',
    radioModuleType: 'Typ modułu radiowego',
    radioModuleSerial: 'Numer seryjny',
    radioModuleFirmware: 'Wersja firmware',
    radioModuleBidCosRadioMAC: 'Adres radiowy (BidCoS)',
    radioModuleHmIPRadioMAC: 'Adres radiowy (HmIP)',
    radioModuleSGTIN: 'SGTIN',
    version: 'Wersja',
    latestVersion: 'Najnowsza Wersja',
    memory: 'Użycie Pamięci',
    cpu: 'Użycie CPU',
    temperature: 'Temperatura',
    voltage: 'Napięcie Zasilania',
    ethernet: 'Ethernet',
    connected: 'Połączony',
    disconnected: 'Rozłączony',
    speed: 'Prędkość',
    duplex: 'Dupleks',
    radioModule: 'Moduł Radiowy',
    moduleType: 'Typ Modułu',
    firmwareVersion: 'Wersja Firmware',
    bidcosMAC: 'MAC Radia BidCoS',
    hmipMAC: 'MAC Radia HmIP',
    system: 'System',
    network: 'Sieć',
    down: 'Dół',
    mbits: 'Mbit/s',
    online: 'Online',
    offline: 'Offline',
    dashboardTitle: 'Status Systemu'
  },

  // Update
  update: {
    available: 'Aktualizacja Dostępna',
    updateNow: 'Aktualizuj Teraz'
  },

  // Firmware Update
  firmware: {
    title: 'Firmware',
    subtitle: 'Zaktualizuj firmware urządzenia',
    restarting: 'Restartowanie...',
    restartingText: 'Urządzenie jest restartowane. Strona odświeży się automatycznie.',
    fileUpload: 'Przesyłanie pliku',
    fileUploadHint: 'Prześlij plik firmware .bin',
    networkUpdate: 'Aktualizacja sieciowa',
    networkUpdateHint: 'Pobierz z adresu URL',
    urlPlaceholder: 'https://przyklad.com/firmware.bin',
    downloading: 'Pobieranie...',
    downloadInstall: 'Pobierz i Zainstaluj',
    factoryReset: 'Reset',
    factoryResetHint: 'Przywróć ustawienia fabryczne',
    factoryResetConfirm: 'Czy na pewno chcesz przywrócić ustawienia fabryczne? Wszystkie ustawienia zostaną utracone.',
    otaProgress: 'Pobieranie aktualizacji firmware...',
    otaSuccess: 'Pobieranie udane. Instalowanie...',
    currentVersion: 'Obecna Wersja',
    installedVersion: 'Zainstalowana wersja',
    versionInfo: 'Zmodernizowany fork v2.1.6 autorstwa Xerolux (2025) - Na podstawie oryginalnej pracy Alexandra Reinerta.',
    updateAvailable: 'Dostępna jest aktualizacja do wersji {latestVersion}.',
    newVersionAvailable: 'Nowa wersja {version} jest dostępna!',
    viewUpdate: 'Zobacz',
    onlineUpdate: 'Aktualizuj Online',
    onlineUpdateConfirm: 'Czy na pewno chcesz pobrać i zainstalować aktualizację? System uruchomi się ponownie automatycznie.',
    onlineUpdateStarted: 'Aktualizacja rozpoczęta. Urządzenie uruchomi się ponownie automatycznie po zakończeniu.',
    showReleaseNotes: 'Pokaż notatki wydania',
    releaseNotesTitle: 'Notatki wydania dla v{version}',
    releaseNotesError: 'Nie udało się załadować notatek wydania z GitHub.',
    updateFile: 'Plik Firmware',
    noFileChosen: 'Nie wybrano pliku',
    browse: 'Przeglądaj',
    selectFile: 'Wybierz lub upuść plik .bin',
    upload: 'Zainstaluj Firmware',
    restart: 'Uruchom ponownie',
    restartHint: 'Zrestartuj urządzenie',
    uploading: 'Przesyłanie...',
    uploadSuccess: 'Aktualizacja firmware pomyślnie przesłana. System uruchomi się ponownie automatycznie za 3 sekundy...',
    uploadError: 'Wystąpił błąd.',
    updateSuccess: 'Firmware zaktualizowany pomyślnie',
    updateError: 'Błąd aktualizacji firmware',
    warning: 'Ostrzeżenie: Nie odłączaj zasilania podczas aktualizacji!',
    restartConfirm: 'Czy na pewno chcesz zrestartować system?'
  },

  // Monitoring
  monitoring: {
    title: 'Monitoring',
    description: 'Skonfiguruj monitoring SNMP i CheckMK dla bramki HB-RF-ETH.',
    save: 'Zapisz',
    saving: 'Zapisywanie...',
    saveSuccess: 'Konfiguracja zapisana pomyślnie!',
    saveError: 'Błąd zapisywania konfiguracji!',
    snmp: {
      title: 'Agent SNMP',
      enabled: 'Włącz SNMP',
      port: 'Port',
      portHelp: 'Domyślnie: 161',
      community: 'Community String',
      communityHelp: 'Domyślnie: "public" - Proszę zmienić w produkcji!',
      location: 'Lokalizacja',
      locationHelp: 'Opcjonalnie: np. "Serwerownia, Budynek A"',
      contact: 'Kontakt',
      contactHelp: 'Opcjonalnie: np. "admin@przyklad.com"'
    },
    checkmk: {
      title: 'Agent CheckMK',
      enabled: 'Włącz CheckMK',
      port: 'Port',
      portHelp: 'Domyślnie: 6556',
      allowedHosts: 'Dozwolone IP klienta',
      allowedHostsHelp: 'Adresy IP oddzielone przecinkami (np. "192.168.1.10,192.168.1.20") lub "*" dla wszystkich'
    },
    mqtt: {
      title: 'Klient MQTT',
      enabled: 'Włącz MQTT',
      server: 'Serwer',
      serverHelp: 'Nazwa hosta lub IP brokera MQTT',
      port: 'Port',
      portHelp: 'Domyślnie: 1883',
      user: 'Użytkownik',
      userHelp: 'Opcjonalnie: Nazwa użytkownika MQTT',
      password: 'Hasło',
      passwordHelp: 'Opcjonalnie: Hasło MQTT',
      topicPrefix: 'Prefiks Tematu',
      topicPrefixHelp: 'Domyślnie: hb-rf-eth - Tematy będą wyglądać jak prefiks/status/...',
      haDiscoveryEnabled: 'Wykrywanie Home Assistant',
      haDiscoveryPrefix: 'Prefiks Wykrywania',
      haDiscoveryPrefixHelp: 'Domyślnie: homeassistant'
    },
    enable: 'Włącz',
    allowedHosts: 'Dozwolone Hosty'
  },

  // About Page
  about: {
    title: 'O programie',
    version: 'Wersja 2.1.6',
    fork: 'Zmodernizowany Fork',
    forkDescription: 'Ta wersja to zmodernizowany fork autorstwa Xerolux (2025), oparty na oryginalnym firmware HB-RF-ETH. Zaktualizowany do ESP-IDF 5.3, nowoczesnych łańcuchów narzędzi i obecnych technologii WebUI (Vue 3, Vite, Pinia).',
    original: 'Oryginalny Autor',
    firmwareLicense: 'Oprogramowanie',
    hardwareLicense: 'Sprzęt',
    under: 'jest wydany na licencji',
    description: 'Bramka LAN HomeMatic BidCoS/HmIP',
    author: 'Autor',
    license: 'Licencja',
    website: 'Strona internetowa',
    documentation: 'Dokumentacja',
    support: 'Wsparcie'
  },

  // Third Party
  thirdParty: {
    title: 'Oprogramowanie stron trzecich',
    containsThirdPartySoftware: 'To oprogramowanie zawiera darmowe produkty oprogramowania stron trzecich używane na różnych warunkach licencyjnych.',
    providedAsIs: 'Oprogramowanie jest dostarczane "tak jak jest" BEZ ŻADNEJ GWARANCJI.'
  },

  // Change Password
  changePassword: {
    title: 'Wymagana zmiana hasła',
    subtitle: 'Zabezpiecz swoje konto',
    warningTitle: 'Wymagane',
    requirementsTitle: 'Wymagania dotyczące hasła:',
    reqMinLength: 'Co najmniej 6 znaków',
    reqLettersNumbers: 'Musi zawierać litery i cyfry',
    currentPassword: 'Obecne hasło',
    newPassword: 'Nowe hasło',
    confirmPassword: 'Potwierdź hasło',
    changePassword: 'Zmień hasło',
    changeSuccess: 'Hasło zmienione pomyślnie',
    changeError: 'Błąd zmiany hasła',
    passwordMismatch: 'Hasła nie pasują do siebie',
    passwordTooShort: 'Hasło musi mieć co najmniej 6 znaków i zawierać litery oraz cyfry.',
    passwordRequirements: 'Musi zawierać litery i cyfry',
    passwordsDoNotMatch: 'Hasła nie pasują do siebie',
    warningMessage: 'To twoje pierwsze logowanie lub hasło jest nadal ustawione na "admin". Ze względów bezpieczeństwa musisz zmienić hasło.',
    success: 'Hasło zmienione pomyślnie',
    newPasswordPlaceholder: 'Wprowadź nowe hasło',
    confirmPasswordPlaceholder: 'Potwierdź nowe hasło'
  },

  // OTA Password Modal
  otaPassword: {
    title: 'Ustaw hasło OTA',
    warningMessage: 'Ustaw osobne hasło dla aktualizacji firmware. Jest to wymagane dla aktualizacji OTA.',
    otaPassword: 'Hasło OTA',
    otaPasswordPlaceholder: 'Wprowadź hasło OTA',
    confirmPassword: 'Potwierdź hasło',
    confirmPasswordPlaceholder: 'Potwierdź hasło OTA',
    passwordTooShort: 'Hasło musi mieć co najmniej 8 znaków',
    passwordRequirements: 'Musi zawierać wielkie, małe litery i cyfry',
    passwordsDoNotMatch: 'Hasła nie pasują do siebie',
    requirementsTitle: 'Wymagania dotyczące hasła:',
    reqMinLength: 'Co najmniej 8 znaków',
    reqMixedCase: 'Wielkie i małe litery',
    reqNumbers: 'Co najmniej jedna cyfra',
    strengthWeak: 'Słabe',
    strengthMedium: 'Średnie',
    strengthGood: 'Dobre',
    strengthStrong: 'Silne'
  },

  // Sponsor
  sponsor: {
    title: 'Wesprzyj ten Projekt',
    description: 'Jeśli podoba Ci się ten projekt i chcesz wesprzeć jego rozwój, możesz skorzystać z jednej z poniższych opcji.',
    thanks: 'Dziękujemy za wsparcie!'
  },

  // System Log
  systemlog: {
    title: 'Dziennik Systemowy',
    description: 'Widok na żywo wyjścia dziennika systemowego z możliwością pobrania.',
    liveLog: 'Log na Żywo',
    autoScroll: 'Automatyczne przewijanie',
    enabled: 'Włączony',
    disabledMessage: 'Przeglądarka logów jest wyłączona. Włącz przełącznik, aby zobaczyć logi na żywo.',
    refresh: 'Odśwież',
    clear: 'Wyczyść',
    download: 'Pobierz',
    empty: 'Brak wpisów w dzienniku.'
  },

  // Changelog
  changelog: {
    title: 'Dziennik Zmian',
    loading: 'Ładowanie...',
    fetching: 'Pobieranie dziennika zmian z GitHub...',
    error: 'Nie udało się załadować dziennika zmian',
    fetchError: 'Nie udało się pobrać dziennika zmian. Sprawdź połączenie z internetem.',
    retry: 'Ponów',
    close: 'Zamknij',
    viewOnGithub: 'Zobacz na GitHub'
  }
}
