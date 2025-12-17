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

    // DTLS Security Settings
    dtlsSettings: 'Cifrado DTLS',
    dtlsDescription: 'Cifrado de transporte seguro para la comunicación entre la placa y CCU en el puerto 3008.',
    dtlsMode: 'Modo de cifrado',
    dtlsModeDisabled: 'Deshabilitado (predeterminado)',
    dtlsModePsk: 'Clave precompartida (PSK)',
    dtlsModeCertificate: 'Certificado X.509',
    dtlsCipherSuite: 'Suite de cifrado',
    dtlsCipherAes128: 'AES-128-GCM-SHA256',
    dtlsCipherAes256: 'AES-256-GCM-SHA384 (Recomendado)',
    dtlsCipherChacha: 'ChaCha20-Poly1305-SHA256',
    dtlsRequireClientCert: 'Requerir certificado de cliente',
    dtlsSessionResumption: 'Habilitar reanudación de sesión',
    dtlsPskManagement: 'Gestión PSK',
    dtlsPskIdentity: 'Identidad PSK',
    dtlsPskKey: 'Clave PSK (Hex)',
    dtlsPskGenerate: 'Generar nueva PSK',
    dtlsPskGenerating: 'Generando PSK...',
    dtlsPskGenerated: 'Nueva PSK generada',
    dtlsPskCopyWarning: 'IMPORTANTE: ¡Copie esta clave ahora! Solo se mostrará una vez.',
    dtlsPskKeyLength: 'Longitud de clave',
    dtlsPsk128bit: '128 Bit',
    dtlsPsk256bit: '256 Bit (Recomendado)',
    dtlsPsk384bit: '384 Bit',
    dtlsPsk512bit: '512 Bit',
    dtlsPskStatus: 'Estado PSK',
    dtlsPskConfigured: 'Configurado',
    dtlsPskNotConfigured: 'No configurado',
    dtlsWarningDisabled: 'La comunicación NO ESTÁ CIFRADA. Cualquiera en la red puede interceptar el tráfico.',
    dtlsWarningPsk: 'Asegúrese de que el PSK se almacene de forma segura en la CCU.',
    dtlsInfo: 'DTLS 1.2 cifra la comunicación Raw-UART UDP de extremo a extremo. La CCU también debe admitir DTLS.',
    dtlsDocumentation: 'Documentación para desarrolladores de CCU',
    dtlsViewDocs: 'Ver guía de implementación',


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
