export default {
  // Common
  common: {
    enabled: 'Habilitado',
    disabled: 'Deshabilitado',
    save: 'Guardar',
    cancel: 'Cancelar',
    close: 'Cerrar',
    loading: 'Cargando...',
    changing: 'Cambiando...',
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
    systemlog: 'Registro del sistema',
    about: 'Acerca de',
    login: 'Iniciar sesión',
    logout: 'Cerrar sesión',
    toggleTheme: 'Cambiar tema',
    language: 'Idioma'
  },

  // Login Page
  login: {
    title: 'Por favor inicie sesión',
    subtitle: 'Por favor ingrese su contraseña para continuar',
    username: 'Nombre de usuario',
    password: 'Contraseña',
    passwordPlaceholder: 'Contraseña',
    login: 'Acceder',
    loginFailed: 'Inicio de sesión fallido',
    invalidCredentials: 'Credenciales inválidas',
    loginError: 'El inicio de sesión no fue exitoso.',
    passwordRequired: 'Se requiere contraseña',
    loggingIn: 'Iniciando sesión...'
  },

  // Settings Page
  settings: {
    title: 'Configuración',
    ccuSettings: 'Conexión CCU',
    ccuIpAddress: 'Dirección IP de CCU',
    ccuIpHint: 'Por favor ingrese la dirección IP de su CCU para evitar el bloqueo de la conexión. El sistema se reiniciará después de guardar.',
    tabGeneral: 'General',
    tabNetwork: 'Red',
    tabTime: 'Hora',
    tabBackup: 'Copia de seguridad',
    changePassword: 'Cambiar contraseña',
    repeatPassword: 'Repetir contraseña',
    hostname: 'Nombre de host',

    // Security Settings
    security: 'Seguridad',
    changePasswordHint: 'Cambie su contraseña de administrador para un acceso seguro',
    otaPassword: 'Contraseña OTA',
    otaPasswordHint: 'Se requiere contraseña separada para actualizaciones de firmware',
    changePasswordBtn: 'Cambiar contraseña',
    changeOtaPassword: 'Cambiar',
    setOtaPassword: 'Establecer contraseña',
    clearOtaPassword: 'Borrar',
    clearOtaPasswordConfirm: '¿Está seguro de que desea eliminar la contraseña OTA? Las actualizaciones de firmware no serán posibles hasta que se establezca una nueva contraseña.',
    clearOtaPasswordSuccess: 'La contraseña OTA ha sido eliminada.',
    clearOtaPasswordError: 'Error al eliminar la contraseña OTA',

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
    updateLedBlink: 'Parpadear LED en actualizaciones',
    checkUpdates: 'Buscar actualizaciones',
    allowPrerelease: 'Permitir actualizaciones tempranas (Beta/Alpha)',
    language: 'Idioma',

    // Messages
    saveSuccess: 'La configuración se guardó correctamente.',
    saveError: 'Se produjo un error al guardar la configuración.',
    restartTitle: 'Reinicio requerido',
    restartMessage: 'La configuración se ha guardado. ¿Desea reiniciar el dispositivo ahora para que los cambios surtan efecto?',
    restartNow: 'Reiniciar ahora',
    restartLater: 'Reiniciar más tarde',

    // Backup & Restore
    backupRestore: 'Copia de seguridad y restauración',
    backupInfo: 'Descargue una copia de seguridad de su configuración para restaurarla más tarde.',
    restoreInfo: 'Cargue un archivo de copia de seguridad para restaurar la configuración. El sistema se reiniciará después.',
    downloadBackup: 'Descargar copia de seguridad',
    restore: 'Restaurar',
    restoreBtn: 'Restaurar',
    download: 'Descargar',
    noFileChosen: 'Ningún archivo seleccionado',
    browse: 'Examinar',
    restoreConfirm: '¿Está seguro? La configuración actual se sobrescribirá y el sistema se reiniciará.',
    restoreSuccess: 'Configuración restaurada correctamente. Reiniciando el sistema...',
    restoreError: 'Error al restaurar la configuración',
    backupError: 'Error al descargar la copia de seguridad'
  },

  // System Info
  sysinfo: {
    title: 'Información del sistema',
    goodMorning: 'Buenos días',
    goodAfternoon: 'Buenas tardes',
    goodEvening: 'Buenas noches',
    serial: 'Número de serie',
    boardRevision: 'Revisión de la placa',
    uptime: 'Tiempo de actividad',
    resetReason: 'Último reinicio',
    cpuUsage: 'Uso de CPU',
    memoryUsage: 'Uso de memoria',
    ethernetStatus: 'Conexión Ethernet',
    rawUartRemoteAddress: 'Conectado con',
    radioModuleType: 'Tipo de módulo de radio',
    radioModuleSerial: 'Número de serie',
    radioModuleFirmware: 'Versión de firmware',
    radioModuleBidCosRadioMAC: 'Dirección de radio (BidCoS)',
    radioModuleHmIPRadioMAC: 'Dirección de radio (HmIP)',
    radioModuleSGTIN: 'SGTIN',
    version: 'Versión',
    latestVersion: 'Última versión',
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
    hmipMAC: 'MAC de radio HmIP',
    system: 'Sistema',
    network: 'Red',
    down: 'Abajo',
    mbits: 'Mbit/s',
    online: 'En línea',
    offline: 'Fuera de línea',
    dashboardTitle: 'Estado del sistema'
  },

  // Update
  update: {
    available: 'Actualización disponible',
    updateNow: 'Actualizar ahora'
  },

  // Firmware Update
  firmware: {
    title: 'Firmware',
    subtitle: 'Actualizar el firmware del dispositivo',
    restarting: 'Reiniciando...',
    restartingText: 'El dispositivo se está reiniciando. La página se recargará automáticamente.',
    fileUpload: 'Carga de archivo',
    fileUploadHint: 'Cargar un archivo de firmware .bin',
    networkUpdate: 'Actualización de red',
    networkUpdateHint: 'Descargar desde URL',
    urlPlaceholder: 'https://ejemplo.com/firmware.bin',
    downloading: 'Descargando...',
    downloadInstall: 'Descargar e instalar',
    factoryReset: 'Restablecer',
    factoryResetHint: 'Restablecer a valores de fábrica',
    factoryResetConfirm: '¿Realmente desea restablecer a los valores de fábrica? Se perderán todas las configuraciones.',
    otaProgress: 'Descargando actualización de firmware...',
    otaSuccess: 'Descarga exitosa. Instalando...',
    currentVersion: 'Versión actual',
    installedVersion: 'Versión instalada',
    versionInfo: 'Fork modernizado v2.1.6 por Xerolux (2025) - Basado en el trabajo original de Alexander Reinert.',
    updateAvailable: 'Hay una actualización disponible para la versión {latestVersion}.',
    newVersionAvailable: '¡Nueva versión {version} disponible!',
    viewUpdate: 'Ver',
    onlineUpdate: 'Actualizar en línea',
    onlineUpdateConfirm: '¿Realmente desea descargar e instalar la actualización? El sistema se reiniciará automáticamente.',
    onlineUpdateStarted: 'Actualización iniciada. El dispositivo se reiniciará automáticamente una vez finalizado.',
    showReleaseNotes: 'Mostrar notas de la versión',
    releaseNotesTitle: 'Notas de la versión para v{version}',
    releaseNotesError: 'No se pudieron cargar las notas de la versión desde GitHub.',
    updateFile: 'Archivo de firmware',
    noFileChosen: 'Ningún archivo seleccionado',
    browse: 'Examinar',
    selectFile: 'Seleccionar o soltar archivo .bin',
    upload: 'Instalar firmware',
    restart: 'Reiniciar',
    restartHint: 'Reiniciar dispositivo',
    uploading: 'Cargando...',
    uploadSuccess: 'Actualización de firmware cargada correctamente. El sistema se reiniciará automáticamente en 3 segundos...',
    uploadError: 'Ocurrió un error.',
    updateSuccess: 'Firmware actualizado correctamente',
    updateError: 'Error al actualizar el firmware',
    warning: 'Advertencia: ¡No desconecte la alimentación durante la actualización!',
    restartConfirm: '¿Realmente desea reiniciar el sistema?'
  },

  // Monitoring
  monitoring: {
    title: 'Monitoreo',
    description: 'Configure el monitoreo SNMP y CheckMK para la puerta de enlace HB-RF-ETH.',
    save: 'Guardar',
    saving: 'Guardando...',
    saveSuccess: '¡Configuración guardada correctamente!',
    saveError: '¡Error al guardar la configuración!',
    snmp: {
      title: 'Agente SNMP',
      enabled: 'Habilitar SNMP',
      port: 'Puerto',
      portHelp: 'Predeterminado: 161',
      community: 'Cadena de comunidad',
      communityHelp: 'Predeterminado: "public" - ¡Por favor cambie para producción!',
      location: 'Ubicación',
      locationHelp: 'Opcional: p. ej. "Sala de servidores, Edificio A"',
      contact: 'Contacto',
      contactHelp: 'Opcional: p. ej. "admin@ejemplo.com"'
    },
    checkmk: {
      title: 'Agente CheckMK',
      enabled: 'Habilitar CheckMK',
      port: 'Puerto',
      portHelp: 'Predeterminado: 6556',
      allowedHosts: 'IPs de clientes permitidas',
      allowedHostsHelp: 'Direcciones IP separadas por comas (p. ej. "192.168.1.10,192.168.1.20") o "*" para todos'
    },
    mqtt: {
      title: 'Cliente MQTT',
      enabled: 'Habilitar MQTT',
      server: 'Servidor',
      serverHelp: 'Nombre de host o IP del broker MQTT',
      port: 'Puerto',
      portHelp: 'Predeterminado: 1883',
      user: 'Usuario',
      userHelp: 'Opcional: Nombre de usuario MQTT',
      password: 'Contraseña',
      passwordHelp: 'Opcional: Contraseña MQTT',
      topicPrefix: 'Prefijo del tema',
      topicPrefixHelp: 'Predeterminado: hb-rf-eth - Los temas serán como prefijo/estado/...',
      haDiscoveryEnabled: 'Descubrimiento de Home Assistant',
      haDiscoveryPrefix: 'Prefijo de descubrimiento',
      haDiscoveryPrefixHelp: 'Predeterminado: homeassistant'
    },
    enable: 'Habilitar',
    allowedHosts: 'Hosts permitidos'
  },

  // About Page
  about: {
    title: 'Acerca de',
    version: 'Versión 2.1.6',
    fork: 'Fork Modernizado',
    forkDescription: 'Esta versión es un fork modernizado por Xerolux (2025), basado en el firmware original HB-RF-ETH. Actualizado a ESP-IDF 5.3, cadenas de herramientas modernas y tecnologías WebUI actuales (Vue 3, Vite, Pinia).',
    original: 'Autor original',
    firmwareLicense: 'El',
    hardwareLicense: 'El',
    under: 'se publica bajo',
    description: 'Gateway LAN HomeMatic BidCoS/HmIP',
    author: 'Autor',
    license: 'Licencia',
    website: 'Sitio web',
    documentation: 'Documentación',
    support: 'Soporte'
  },

  // Third Party
  thirdParty: {
    title: 'Software de terceros',
    containsThirdPartySoftware: 'Este software contiene productos de software de terceros gratuitos utilizados bajo varias condiciones de licencia.',
    providedAsIs: 'El software se proporciona "tal cual" SIN NINGUNA GARANTÍA.'
  },

  // Change Password
  changePassword: {
    title: 'Cambio de contraseña requerido',
    subtitle: 'Asegure su cuenta',
    warningTitle: 'Requerido',
    requirementsTitle: 'Requisitos de contraseña:',
    reqMinLength: 'Al menos 6 caracteres',
    reqLettersNumbers: 'Debe contener letras y números',
    currentPassword: 'Contraseña actual',
    newPassword: 'Nueva contraseña',
    confirmPassword: 'Confirmar contraseña',
    changePassword: 'Cambiar contraseña',
    changeSuccess: 'Contraseña cambiada correctamente',
    changeError: 'Error al cambiar la contraseña',
    passwordMismatch: 'Las contraseñas no coinciden',
    passwordTooShort: 'La contraseña debe tener al menos 6 caracteres y contener letras y números.',
    passwordRequirements: 'Debe contener letras y números',
    passwordsDoNotMatch: 'Las contraseñas no coinciden',
    warningMessage: 'Este es su primer inicio de sesión o la contraseña todavía está establecida en "admin". Por razones de seguridad, debe cambiar la contraseña.',
    success: 'Contraseña cambiada correctamente',
    newPasswordPlaceholder: 'Ingrese nueva contraseña',
    confirmPasswordPlaceholder: 'Confirmar nueva contraseña'
  },

  // OTA Password Modal
  otaPassword: {
    title: 'Establecer contraseña OTA',
    warningMessage: 'Establezca una contraseña separada para las actualizaciones de firmware. Esto es necesario para las actualizaciones OTA.',
    otaPassword: 'Contraseña OTA',
    otaPasswordPlaceholder: 'Ingrese contraseña OTA',
    confirmPassword: 'Confirmar contraseña',
    confirmPasswordPlaceholder: 'Confirmar contraseña OTA',
    passwordTooShort: 'La contraseña debe tener al menos 8 caracteres',
    passwordRequirements: 'Debe contener mayúsculas, minúsculas y números',
    passwordsDoNotMatch: 'Las contraseñas no coinciden',
    requirementsTitle: 'Requisitos de contraseña:',
    reqMinLength: 'Al menos 8 caracteres',
    reqMixedCase: 'Mayúsculas y minúsculas',
    reqNumbers: 'Al menos un número',
    strengthWeak: 'Débil',
    strengthMedium: 'Medio',
    strengthGood: 'Bueno',
    strengthStrong: 'Fuerte'
  },

  // Sponsor
  sponsor: {
    title: 'Apoyar este proyecto',
    description: 'Si le gusta este proyecto y desea apoyar su desarrollo, puede utilizar una de las siguientes opciones.',
    thanks: '¡Gracias por su apoyo!'
  },

  // System Log
  systemlog: {
    title: 'Registro del sistema',
    description: 'Vista en vivo de la salida del registro del sistema con capacidad de descarga.',
    liveLog: 'Registro en vivo',
    autoScroll: 'Desplazamiento automático',
    enabled: 'Habilitado',
    disabledMessage: 'El visor de registros está deshabilitado. Encienda el interruptor para ver los registros en vivo.',
    refresh: 'Actualizar',
    clear: 'Borrar',
    download: 'Descargar',
    empty: 'Aún no hay entradas de registro.'
  },

  // Changelog
  changelog: {
    title: 'Registro de cambios',
    loading: 'Cargando...',
    fetching: 'Obteniendo registro de cambios de GitHub...',
    error: 'Error al cargar el registro de cambios',
    fetchError: 'No se pudo obtener el registro de cambios. Por favor verifique su conexión a internet.',
    retry: 'Reintentar',
    close: 'Cerrar',
    viewOnGithub: 'Ver en GitHub'
  }
}
