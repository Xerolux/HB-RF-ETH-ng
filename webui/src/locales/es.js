export default {
  // Common
  common: {
    enabled: 'Habilitado',
    disabled: 'Deshabilitado',
    save: 'Guardar',
    cancel: 'Cancelar',
    close: 'Cerrar',
    loading: 'Cargando...',
    error: 'Error',
    success: 'Éxito',
    yes: 'Sí',
    no: 'No'
  },

  // Header Navigation
  nav: {
    home: 'Inicio',
    settings: 'Configuración',
    firmware: 'Firmware',
    monitoring: 'Monitoreo',
    analyzer: 'Analyzer',
    about: 'Acerca de',
    logout: 'Cerrar sesión'
  },

  // Login Page
  login: {
    title: 'Iniciar sesión',
    username: 'Nombre de usuario',
    password: 'Contraseña',
    login: 'Acceder',
    loginFailed: 'Error al iniciar sesión',
    invalidCredentials: 'Credenciales inválidas'
  },

  // Settings Page
  settings: {
    title: 'Configuración',
    changePassword: 'Cambiar contraseña',
    repeatPassword: 'Repetir contraseña',
    hostname: 'Nombre de host',

    // Network Settings
    networkSettings: 'Configuración de red',
    dhcp: 'DHCP',
    ipAddress: 'Dirección IP',
    netmask: 'Máscara de red',
    gateway: 'Puerta de enlace',
    dns1: 'DNS primario',
    dns2: 'DNS secundario',

    // IPv6 Settings
    ipv6Settings: 'Configuración IPv6',
    enableIPv6: 'Habilitar IPv6',
    ipv6Mode: 'Modo IPv6',
    ipv6Auto: 'Automático (SLAAC)',
    ipv6Static: 'Estático',
    ipv6Address: 'Dirección IPv6',
    ipv6PrefixLength: 'Longitud del prefijo',
    ipv6Gateway: 'Puerta de enlace IPv6',
    ipv6Dns1: 'DNS IPv6 primario',
    ipv6Dns2: 'DNS IPv6 secundario',

    // Time Settings
    timeSettings: 'Configuración de hora',
    timesource: 'Fuente de tiempo',
    ntp: 'NTP',
    dcf: 'DCF',
    gps: 'GPS',
    ntpServer: 'Servidor NTP',
    dcfOffset: 'Desplazamiento DCF',
    gpsBaudrate: 'Baudrate GPS',

    // System Settings
    systemSettings: 'Configuración del sistema',
    ledBrightness: 'Brillo del LED',
    language: 'Idioma',

    // Messages
    saveSuccess: 'La configuración se guardó correctamente. Reinicie el sistema para aplicarla.',
    saveError: 'Se produjo un error al guardar la configuración.'
  },

  // System Info
  sysinfo: {
    title: 'Información del sistema',
    serial: 'Número de serie',
    version: 'Versión',
    latestVersion: 'Última versión',
    uptime: 'Tiempo de actividad',
    memory: 'Uso de memoria',
    cpu: 'Uso de CPU',
    temperature: 'Temperatura',
    voltage: 'Voltaje de alimentación',
    ethernet: 'Ethernet',
    connected: 'Conectado',
    disconnected: 'Desconectado',
    speed: 'Velocidad',
    duplex: 'Dúplex',
    radioModule: 'Módulo de radio',
    moduleType: 'Tipo de módulo',
    firmwareVersion: 'Versión de firmware',
    bidcosMAC: 'MAC de radio BidCoS',
    hmipMAC: 'MAC de radio HmIP'
  },

  // Firmware Update
  firmware: {
    title: 'Actualización de firmware',
    currentVersion: 'Versión actual',
    selectFile: 'Seleccionar archivo',
    upload: 'Cargar',
    uploading: 'Cargando...',
    updateSuccess: 'Firmware actualizado correctamente',
    updateError: 'Error al actualizar el firmware',
    warning: '¡Advertencia: No desconecte la alimentación durante la actualización!'
  },

  // Monitoring
  monitoring: {
    title: 'Monitoreo',
    snmp: 'SNMP',
    checkmk: 'Check_MK',
    enable: 'Habilitar',
    port: 'Puerto',
    community: 'Comunidad',
    location: 'Ubicación',
    contact: 'Contacto',
    allowedHosts: 'Hosts permitidos',
    saveSuccess: 'Configuración de monitoreo guardada correctamente',
    saveError: 'Error al guardar la configuración de monitoreo'
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
    title: 'Acerca de HB-RF-ETH-ng',
    description: 'Gateway LAN HomeMatic BidCoS/HmIP',
    version: 'Versión',
    author: 'Autor',
    license: 'Licencia',
    website: 'Sitio web',
    documentation: 'Documentación',
    support: 'Soporte'
  },

  // Change Password
  changePassword: {
    title: 'Cambiar contraseña',
    currentPassword: 'Contraseña actual',
    newPassword: 'Nueva contraseña',
    confirmPassword: 'Confirmar contraseña',
    changeSuccess: 'Contraseña cambiada correctamente',
    changeError: 'Error al cambiar la contraseña',
    passwordMismatch: 'Las contraseñas no coinciden',
    passwordTooShort: 'La contraseña es demasiado corta (mínimo 5 caracteres)'
  }
}
