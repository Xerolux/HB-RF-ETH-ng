export default {
  // Common
  common: {
    enabled: 'Włączone',
    disabled: 'Wyłączone',
    save: 'Zapisz',
    cancel: 'Anuluj',
    close: 'Zamknij',
    loading: 'Ładowanie...',
    error: 'Błąd',
    success: 'Sukces',
    yes: 'Tak',
    no: 'Nie',
    showPassword: 'Pokaż hasło',
    hidePassword: 'Ukryj hasło',
    rebootingWait: 'System jest restartowany. Proszę czekać około 10 sekund...',
    factoryResettingWait: 'System jest resetowany do ustawień fabrycznych i restartowany. Proszę czekać...',
    confirmReboot: 'Czy na pewno chcesz ponownie uruchomić system?',
    confirmFactoryReset: 'Czy jesteś pewien? Wszystkie ustawienia zostaną utracone!'
  },

  // Header Navigation
  nav: {
    home: 'Start',
    settings: 'Ustawienia',
    firmware: 'Oprogramowanie',
    monitoring: 'Monitorowanie',
    analyzer: 'Analyzer',
    about: 'O programie',
    login: 'Zaloguj',
    logout: 'Wyloguj',
    toggleTheme: 'Przełącz motyw'
  },

  // Login Page
  login: {
    title: 'Logowanie',
    username: 'Nazwa użytkownika',
    password: 'Hasło',
    login: 'Zaloguj',
    loginFailed: 'Logowanie nie powiodło się',
    invalidCredentials: 'Nieprawidłowe dane logowania',
    loginError: 'Logowanie nie powiodło się.'
  },

  // Settings Page
  settings: {
    title: 'Ustawienia',
    changePassword: 'Zmień hasło',
    repeatPassword: 'Powtórz hasło',
    hostname: 'Nazwa hosta',

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
    systemSettings: 'Ustawienia systemowe',
    ledBrightness: 'Jasność LED',
    checkUpdates: 'Automatycznie sprawdzaj aktualizacje',
    allowPrerelease: 'Zezwalaj na wczesne aktualizacje (Beta/Alpha)',
    language: 'Język',
    analyzerSettings: 'Ustawienia Analyzer Light',
    enableAnalyzer: 'Włącz Analyzer Light',
    systemMaintenance: 'Konserwacja systemu',
    reboot: 'Restart',
    factoryReset: 'Reset fabryczny',

    // HMLGW
    hmlgwSettings: 'Ustawienia HomeMatic LAN Gateway (HMLGW)',
    enableHmlgw: 'Włącz tryb HMLGW',
    hmlgwPort: 'Port danych (Domyślny: 2000)',
    hmlgwKeepAlivePort: 'Port KeepAlive (Domyślny: 2001)',

    // DTLS Security Settings
    dtls: {
      title: 'Szyfrowanie DTLS',
      description: 'Bezpieczne szyfrowanie transportu dla komunikacji między płytką a CCU na porcie 3008.',
      mode: 'Tryb szyfrowania',
      modeDisabled: 'Wyłączony (Domyślny)',
      modePsk: 'Klucz współdzielony (PSK)',
      modeCert: 'Certyfikat X.509',
      cipherSuite: 'Zestaw szyfrów',
      cipherAes128: 'AES-128-GCM-SHA256',
      cipherAes256: 'AES-256-GCM-SHA384 (Zalecane)',
      cipherChacha: 'ChaCha20-Poly1305-SHA256',
      requireClientCert: 'Wymagaj certyfikatu klienta',
      sessionResumption: 'Włącz wznowienie sesji',
      pskManagement: 'Zarządzanie PSK',
      pskIdentity: 'Tożsamość PSK',
      pskKey: 'Klucz PSK (Hex)',
      pskGenerate: 'Wygeneruj nowy PSK',
      pskGenerating: 'Generowanie PSK...',
      pskGenerated: 'Nowy PSK wygenerowany',
      pskCopyWarning: 'WAŻNE: Skopiuj ten klucz teraz! Zostanie wyświetlony tylko raz.',
      pskKeyLength: 'Długość klucza',
      psk128bit: '128 Bit',
      psk256bit: '256 Bit (Zalecane)',
      psk384bit: '384 Bit',
      psk512bit: '512 Bit',
      pskStatus: 'Status PSK',
      pskConfigured: 'Skonfigurowany',
      pskNotConfigured: 'Nieskonfigurowany',
      warningDisabled: 'Komunikacja jest NIESZYFROWANA. Każdy w sieci może przechwycić ruch.',
      warningPsk: 'Upewnij się, że PSK jest bezpiecznie przechowywany na CCU.',
      info: 'DTLS 1.2 szyfruje komunikację Raw-UART UDP end-to-end. CCU musi również obsługiwać DTLS.',
      documentation: 'Dokumentacja dla programistów CCU',
      viewDocs: 'Zobacz przewodnik implementacji',
      restartNote: 'Zmiany w ustawieniach DTLS wymagają ponownego uruchomienia systemu.'
    },

    // Messages
    saveSuccess: 'Ustawienia zostały pomyślnie zapisane. Uruchom ponownie system, aby je zastosować.',
    saveError: 'Wystąpił błąd podczas zapisywania ustawień.',

    // Backup & Restore
    backupRestore: 'Kopia zapasowa i przywracanie',
    backupInfo: 'Pobierz kopię zapasową swoich ustawień, aby przywrócić je później.',
    restoreInfo: 'Prześlij plik kopii zapasowej, aby przywrócić ustawienia. System zostanie uruchomiony ponownie.',
    downloadBackup: 'Pobierz kopię zapasową',
    restore: 'Przywróć',
    noFileChosen: 'Nie wybrano pliku',
    browse: 'Przeglądaj',
    restoreConfirm: 'Czy jesteś pewien? Obecne ustawienia zostaną nadpisane, a system zostanie ponownie uruchomiony.',
    restoreSuccess: 'Ustawienia przywrócone pomyślnie. System jest restartowany...',
    restoreError: 'Błąd podczas przywracania ustawień',
    backupError: 'Błąd podczas pobierania kopii zapasowej'
  },

  // System Info
  sysinfo: {
    title: 'Informacje o systemie',
    serial: 'Numer seryjny',
    boardRevision: 'Rewizja płyty',
    uptime: 'Czas działania',
    resetReason: 'Ostatni restart',
    cpuUsage: 'Użycie CPU',
    memoryUsage: 'Użycie pamięci',
    ethernetStatus: 'Połączenie Ethernet',
    rawUartRemoteAddress: 'Połączony z',
    radioModuleType: 'Typ modułu radiowego',
    radioModuleSerial: 'Numer seryjny',
    radioModuleFirmware: 'Wersja oprogramowania',
    radioModuleBidCosRadioMAC: 'Adres radiowy (BidCoS)',
    radioModuleHmIPRadioMAC: 'Adres radiowy (HmIP)',
    radioModuleSGTIN: 'SGTIN',
    version: 'Wersja',
    latestVersion: 'Najnowsza wersja',
    memory: 'Użycie pamięci',
    cpu: 'Użycie CPU',
    temperature: 'Temperatura',
    voltage: 'Napięcie zasilania',
    ethernet: 'Ethernet',
    connected: 'Połączony',
    disconnected: 'Rozłączony',
    speed: 'Prędkość',
    duplex: 'Dupleks',
    radioModule: 'Moduł radiowy',
    moduleType: 'Typ modułu',
    firmwareVersion: 'Wersja oprogramowania',
    bidcosMAC: 'MAC radia BidCoS',
    hmipMAC: 'MAC radia HmIP'
  },

  // Firmware Update
  firmware: {
    title: 'Oprogramowanie',
    currentVersion: 'Obecna wersja',
    installedVersion: 'Zainstalowana wersja',
    latestVersion: 'Najnowsza dostępna wersja',
    versionInfo: 'Zmodernizowany fork v2.1 przez Xerolux (2025) - Oparty na oryginalnej pracy Alexandra Reinerta.',
    upToDate: 'Twoje oprogramowanie jest aktualne.',
    updateAvailable: 'Dostępna aktualizacja: Wersja {currentVersion} → {latestVersion}',
    versionCheckFailed: 'Sprawdzanie wersji nie powiodło się. Sprawdź połączenie internetowe i spróbuj ponownie.',
    onlineUpdate: 'Aktualizacja online',
    checkUpdate: 'Sprawdź aktualizacje teraz',
    noUpdateAvailable: 'Brak dostępnych aktualizacji. Masz już najnowszą wersję.',
    onlineUpdateConfirm: 'Czy naprawdę chcesz pobrać i zainstalować aktualizację? System zostanie automatycznie uruchomiony ponownie.',
    onlineUpdateStarted: 'Aktualizacja rozpoczęta. Urządzenie uruchomi się ponownie automatycznie po zakończeniu.',
    showReleaseNotes: 'Pokaż informacje o wydaniu',
    releaseNotesTitle: 'Informacje o wydaniu dla v{version}',
    releaseNotesError: 'Nie udało się załadować informacji o wydaniu.',
    downloadFirmware: 'Pobierz oprogramowanie',
    updateFile: 'Plik oprogramowania',
    noFileChosen: 'Nie wybrano pliku',
    browse: 'Przeglądaj',
    selectFile: 'Wybierz plik',
    upload: 'Prześlij',
    restart: 'Uruchom ponownie system',
    uploading: 'Przesyłanie...',
    uploadSuccess: 'Aktualizacja oprogramowania została pomyślnie przesłana. System uruchomi się ponownie automatycznie za 3 sekundy...',
    uploadError: 'Wystąpił błąd.',
    updateSuccess: 'Oprogramowanie zaktualizowane pomyślnie',
    updateError: 'Błąd podczas aktualizacji oprogramowania',
    warning: 'Ostrzeżenie: Nie odłączaj zasilania podczas aktualizacji!',
    restartConfirm: 'Czy naprawdę chcesz ponownie uruchomić system?'
  },

  // Monitoring
  monitoring: {
    title: 'Monitorowanie',
    description: 'Skonfiguruj monitorowanie SNMP i CheckMK dla bramy HB-RF-ETH.',
    save: 'Zapisz',
    saving: 'Zapisywanie...',
    saveSuccess: 'Konfiguracja zapisana pomyślnie!',
    saveError: 'Błąd podczas zapisywania konfiguracji!',
    snmp: {
      title: 'Agent SNMP',
      enabled: 'Włącz SNMP',
      port: 'Port',
      portHelp: 'Domyślny: 161',
      community: 'Ciąg społeczności',
      communityHelp: 'Domyślny: "public" - Zmień dla produkcji!',
      location: 'Lokalizacja',
      locationHelp: 'Opcjonalnie: np. "Serwerownia, Budynek A"',
      contact: 'Kontakt',
      contactHelp: 'Opcjonalnie: np. "admin@example.com"'
    },
    checkmk: {
      title: 'Agent CheckMK',
      enabled: 'Włącz CheckMK',
      port: 'Port',
      portHelp: 'Domyślny: 6556',
      allowedHosts: 'Dozwolone adresy IP klientów',
      allowedHostsHelp: 'Adresy IP oddzielone przecinkami (np. "192.168.1.10,192.168.1.20") lub "*" dla wszystkich'
    },
    mqtt: {
      title: 'Klient MQTT',
      enabled: 'Włącz MQTT',
      server: 'Serwer',
      serverHelp: 'Nazwa hosta lub IP brokera MQTT',
      port: 'Port',
      portHelp: 'Domyślny: 1883',
      user: 'Użytkownik',
      userHelp: 'Opcjonalnie: Nazwa użytkownika MQTT',
      password: 'Hasło',
      passwordHelp: 'Opcjonalnie: Hasło MQTT',
      topicPrefix: 'Prefiks tematu',
      topicPrefixHelp: 'Domyślny: hb-rf-eth - Tematy będą w formacie prefix/status/...',
      haDiscoveryEnabled: 'Wykrywanie Home Assistant',
      haDiscoveryPrefix: 'Prefiks wykrywania',
      haDiscoveryPrefixHelp: 'Domyślny: homeassistant'
    },
    enable: 'Włącz',
    allowedHosts: 'Dozwolone hosty'
  },

  // Analyzer
  analyzer: {
    title: 'Analyzer Light',
    disabled: 'Funkcja Analyzer Light jest wyłączona. Włącz ją w Ustawieniach.',
    connected: 'Połączony',
    disconnected: 'Rozłączony',
    clear: 'Wyczyść',
    autoScroll: 'Auto przewijanie',
    time: 'Czas',
    len: 'Długość',
    cnt: 'Licznik',
    type: 'Typ',
    src: 'Źródło',
    dst: 'Miejsce docelowe',
    payload: 'Payload',
    rssi: 'RSSI',
    deviceNames: 'Nazwy urządzeń',
    address: 'Adres',
    name: 'Nazwa',
    storedNames: 'Zapisane nazwy'
  },

  // About Page
  about: {
    title: 'O programie',
    version: 'Wersja 2.1.0',
    fork: 'Zmodernizowany fork',
    forkDescription: 'Ta wersja jest zmodernizowanym forkiem przez Xerolux (2025), opartym na oryginalnym oprogramowaniu HB-RF-ETH. Zaktualizowano do ESP-IDF 5.3, nowoczesnych narzędzi (GCC 13.2.0) i aktualnych technologii WebUI (Vue 3, Parcel 2, Pinia).',
    original: 'Oryginalny autor',
    firmwareLicense: '',
    hardwareLicense: '',
    under: 'jest wydany na licencji',
    description: 'Brama LAN HomeMatic BidCoS/HmIP',
    author: 'Autor',
    license: 'Licencja',
    website: 'Strona internetowa',
    documentation: 'Dokumentacja',
    support: 'Wsparcie'
  },

  // Third Party
  thirdParty: {
    title: 'Oprogramowanie firm trzecich',
    containsThirdPartySoftware: 'To oprogramowanie zawiera darmowe produkty oprogramowania firm trzecich używane na różnych warunkach licencyjnych.',
    providedAsIs: 'Oprogramowanie jest dostarczane "TAK JAK JEST" BEZ ŻADNEJ GWARANCJI.'
  },

  // Change Password
  changePassword: {
    title: 'Wymagana zmiana hasła',
    currentPassword: 'Obecne hasło',
    newPassword: 'Nowe hasło',
    confirmPassword: 'Potwierdź hasło',
    changePassword: 'Zmień hasło',
    changeSuccess: 'Hasło zmienione pomyślnie',
    changeError: 'Błąd podczas zmiany hasła',
    passwordMismatch: 'Hasła nie są zgodne',
    passwordTooShort: 'Hasło musi mieć co najmniej 6 znaków i zawierać litery i cyfry.',
    passwordsDoNotMatch: 'Hasła nie są zgodne',
    warningMessage: 'To jest twoje pierwsze logowanie lub hasło jest nadal ustawione na "admin". Ze względów bezpieczeństwa musisz zmienić hasło.',
    success: 'Hasło zmienione pomyślnie'
  }
}
