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
    no: 'Nie'
  },

  // Header Navigation
  nav: {
    home: 'Start',
    settings: 'Ustawienia',
    firmware: 'Oprogramowanie',
    monitoring: 'Monitorowanie',
    analyzer: 'Analyzer',
    about: 'O programie',
    logout: 'Wyloguj'
  },

  // Login Page
  login: {
    title: 'Logowanie',
    username: 'Nazwa użytkownika',
    password: 'Hasło',
    login: 'Zaloguj',
    loginFailed: 'Logowanie nie powiodło się',
    invalidCredentials: 'Nieprawidłowe dane logowania'
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
    language: 'Język',

    // Messages
    saveSuccess: 'Ustawienia zostały pomyślnie zapisane. Uruchom ponownie system, aby je zastosować.',
    saveError: 'Wystąpił błąd podczas zapisywania ustawień.'
  },

  // System Info
  sysinfo: {
    title: 'Informacje o systemie',
    serial: 'Numer seryjny',
    version: 'Wersja',
    latestVersion: 'Najnowsza wersja',
    uptime: 'Czas działania',
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
    title: 'Aktualizacja oprogramowania',
    currentVersion: 'Obecna wersja',
    selectFile: 'Wybierz plik',
    upload: 'Prześlij',
    uploading: 'Przesyłanie...',
    updateSuccess: 'Oprogramowanie zaktualizowane pomyślnie',
    updateError: 'Błąd podczas aktualizacji oprogramowania',
    warning: 'Ostrzeżenie: Nie odłączaj zasilania podczas aktualizacji!'
  },

  // Monitoring
  monitoring: {
    title: 'Monitorowanie',
    snmp: 'SNMP',
    checkmk: 'Check_MK',
    enable: 'Włącz',
    port: 'Port',
    community: 'Społeczność',
    location: 'Lokalizacja',
    contact: 'Kontakt',
    allowedHosts: 'Dozwolone hosty',
    saveSuccess: 'Ustawienia monitorowania zapisane pomyślnie',
    saveError: 'Błąd podczas zapisywania ustawień monitorowania'
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
    title: 'O HB-RF-ETH-ng',
    description: 'Brama LAN HomeMatic BidCoS/HmIP',
    version: 'Wersja',
    author: 'Autor',
    license: 'Licencja',
    website: 'Strona internetowa',
    documentation: 'Dokumentacja',
    support: 'Wsparcie'
  },

  // Change Password
  changePassword: {
    title: 'Zmień hasło',
    currentPassword: 'Obecne hasło',
    newPassword: 'Nowe hasło',
    confirmPassword: 'Potwierdź hasło',
    changeSuccess: 'Hasło zmienione pomyślnie',
    changeError: 'Błąd podczas zmiany hasła',
    passwordMismatch: 'Hasła nie są zgodne',
    passwordTooShort: 'Hasło jest za krótkie (minimum 5 znaków)'
  }
}
