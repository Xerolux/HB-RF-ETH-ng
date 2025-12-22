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
    no: 'No',
    showPassword: 'Mostrar contraseña',
    hidePassword: 'Ocultar contraseña',
    rebootingWait: 'El sistema se está reiniciando. Por favor espere aproximadamente 10 segundos...',
    factoryResettingWait: 'El sistema se está restableciendo a los valores de fábrica y reiniciando. Por favor espere...',
    confirmReboot: '¿Está seguro de que desea reiniciar el sistema?',
    confirmFactoryReset: '¿Está seguro? ¡Se perderán todos los ajustes!'
  },

  // Header Navigation
  nav: {
    home: 'Inicio',
    settings: 'Configuración',
    firmware: 'Firmware',
    monitoring: 'Monitoreo',
    analyzer: 'Analyzer',
    about: 'Acerca de',
    login: 'Iniciar sesión',
    logout: 'Cerrar sesión',
    toggleTheme: 'Cambiar tema'
  },

  // Login Page
  login: {
    title: 'Iniciar sesión',
    username: 'Nombre de usuario',
    password: 'Contraseña',
    login: 'Acceder',
    loginFailed: 'Error al iniciar sesión',
    invalidCredentials: 'Credenciales inválidas',
    loginError: 'El inicio de sesión no fue exitoso.'
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
    checkUpdates: 'Buscar actualizaciones automáticamente',
    allowPrerelease: 'Permitir actualizaciones anticipadas (Beta/Alpha)',
    language: 'Idioma',
    analyzerSettings: 'Configuración Analyzer Light',
    enableAnalyzer: 'Habilitar Analyzer Light',
    systemMaintenance: 'Mantenimiento del sistema',
    reboot: 'Reiniciar',
    factoryReset: 'Restablecer valores de fábrica',

    // HMLGW
    hmlgwSettings: 'Configuración de la puerta de enlace HomeMatic LAN (HMLGW)',
    enableHmlgw: 'Habilitar modo HMLGW',
    hmlgwPort: 'Puerto de datos (Predeterminado: 2000)',
    hmlgwKeepAlivePort: 'Puerto KeepAlive (Predeterminado: 2001)',

    // DTLS Security Settings
    dtls: {
      title: 'Cifrado DTLS',
      description: 'Cifrado de transporte seguro para la comunicación entre la placa y CCU en el puerto 3008.',
      mode: 'Modo de cifrado',
      modeDisabled: 'Deshabilitado (predeterminado)',
      modePsk: 'Clave precompartida (PSK)',
      modeCert: 'Certificado X.509',
      cipherSuite: 'Suite de cifrado',
      cipherAes128: 'AES-128-GCM-SHA256',
      cipherAes256: 'AES-256-GCM-SHA384 (Recomendado)',
      cipherChacha: 'ChaCha20-Poly1305-SHA256',
      requireClientCert: 'Requerir certificado de cliente',
      sessionResumption: 'Habilitar reanudación de sesión',
      pskManagement: 'Gestión PSK',
      pskIdentity: 'Identidad PSK',
      pskKey: 'Clave PSK (Hex)',
      pskGenerate: 'Generar nueva PSK',
      pskGenerating: 'Generando PSK...',
      pskGenerated: 'Nueva PSK generada',
      pskCopyWarning: 'IMPORTANTE: ¡Copie esta clave ahora! Solo se mostrará una vez.',
      pskKeyLength: 'Longitud de clave',
      psk128bit: '128 Bit',
      psk256bit: '256 Bit (Recomendado)',
      psk384bit: '384 Bit',
      psk512bit: '512 Bit',
      pskStatus: 'Estado PSK',
      pskConfigured: 'Configurado',
      pskNotConfigured: 'No configurado',
      warningDisabled: 'La comunicación NO ESTÁ CIFRADA. Cualquiera en la red puede interceptar el tráfico.',
      warningPsk: 'Asegúrese de que el PSK se almacene de forma segura en la CCU.',
      info: 'DTLS 1.2 cifra la comunicación Raw-UART UDP de extremo a extremo. La CCU también debe admitir DTLS.',
      documentation: 'Documentación para desarrolladores de CCU',
      viewDocs: 'Ver guía de implementación',
      restartNote: 'Los cambios en la configuración DTLS requieren un reinicio del sistema.'
    },


    // Messages
    saveSuccess: 'La configuración se guardó correctamente. Reinicie el sistema para aplicarla.',
    saveError: 'Se produjo un error al guardar la configuración.',

    // Backup & Restore
    backupRestore: 'Copia de seguridad y restauración',
    backupInfo: 'Descargue una copia de seguridad de su configuración para restaurarla más tarde.',
    restoreInfo: 'Cargue un archivo de copia de seguridad para restaurar la configuración. El sistema se reiniciará después.',
    downloadBackup: 'Descargar copia de seguridad',
    restore: 'Restaurar',
    noFileChosen: 'Ningún archivo seleccionado',
    browse: 'Explorar',
    restoreConfirm: '¿Está seguro? La configuración actual se sobrescribirá y el sistema se reiniciará.',
    restoreSuccess: 'Configuración restaurada correctamente. Reiniciando el sistema...',
    restoreError: 'Error al restaurar la configuración',
    backupError: 'Error al descargar la copia de seguridad'
  },

  // System Info
  sysinfo: {
    title: 'Información del sistema',
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
    hmipMAC: 'MAC de radio HmIP'
  },

  // Firmware Update
  firmware: {
    title: 'Firmware',
    currentVersion: 'Versión actual',
    installedVersion: 'Versión instalada',
    latestVersion: 'Última versión disponible',
    versionInfo: 'Fork modernizado v2.1 por Xerolux (2025) - Basado en el trabajo original de Alexander Reinert.',
    upToDate: 'Su firmware está actualizado.',
    updateAvailable: 'Actualización disponible: Versión {currentVersion} → {latestVersion}',
    versionCheckFailed: 'Error al verificar la versión. Verifique su conexión a Internet e intente nuevamente.',
    onlineUpdate: 'Actualización en línea',
    checkUpdate: 'Buscar actualizaciones ahora',
    noUpdateAvailable: 'No hay actualizaciones disponibles. Ya está ejecutando la última versión.',
    onlineUpdateConfirm: '¿Realmente desea descargar e instalar la actualización? El sistema se reiniciará automáticamente.',
    onlineUpdateStarted: 'Actualización iniciada. El dispositivo se reiniciará automáticamente una vez finalizado.',
    showReleaseNotes: 'Mostrar notas de la versión',
    releaseNotesTitle: 'Notas de la versión para v{version}',
    releaseNotesError: 'Error al cargar las notas de la versión.',
    downloadFirmware: 'Descargar firmware',
    updateFile: 'Archivo de firmware',
    noFileChosen: 'Ningún archivo seleccionado',
    browse: 'Explorar',
    selectFile: 'Seleccionar archivo',
    upload: 'Cargar',
    restart: 'Reiniciar sistema',
    uploading: 'Cargando...',
    uploadSuccess: 'Actualización de firmware cargada correctamente. El sistema se reiniciará automáticamente en 3 segundos...',
    uploadError: 'Ocurrió un error.',
    updateSuccess: 'Firmware actualizado correctamente',
    updateError: 'Error al actualizar el firmware',
    warning: '¡Advertencia: No desconecte la alimentación durante la actualización!',
    restartConfirm: '¿Realmente desea reiniciar el sistema?'
  },

  // Monitoring
  monitoring: {
    title: 'Monitoreo',
    description: 'Configure el monitoreo SNMP y CheckMK para la puerta de enlace HB-RF-ETH.',
    save: 'Guardar',
    saving: 'Guardando...',
    saveSuccess: 'Configuración guardada correctamente',
    saveError: 'Error al guardar la configuración',
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
      contactHelp: 'Opcional: p. ej. "admin@example.com"'
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
      topicPrefixHelp: 'Predeterminado: hb-rf-eth - Los temas serán como prefix/status/...',
      haDiscoveryEnabled: 'Descubrimiento de Home Assistant',
      haDiscoveryPrefix: 'Prefijo de descubrimiento',
      haDiscoveryPrefixHelp: 'Predeterminado: homeassistant'
    },
    enable: 'Habilitar',
    allowedHosts: 'Hosts permitidos'
  },

  // Analyzer
  analyzer: {
    title: 'Analyzer Light',
    disabled: 'La función Analyzer Light está deshabilitada. Por favor habilítela en Configuración.',
    connected: 'Conectado',
    disconnected: 'Desconectado',
    clear: 'Limpiar',
    autoScroll: 'Desplazamiento automático',
    time: 'Tiempo',
    len: 'Long.',
    cnt: 'Cont',
    type: 'Tipo',
    src: 'Origen',
    dst: 'Destino',
    payload: 'Datos',
    rssi: 'RSSI',
    deviceNames: 'Nombres de dispositivos',
    address: 'Dirección',
    name: 'Nombre',
    storedNames: 'Nombres almacenados'
  },

  // About Page
  about: {
    title: 'Acerca de',
    version: 'Versión 2.1.0',
    fork: 'Fork modernizado',
    forkDescription: 'Esta versión es un fork modernizado por Xerolux (2025), basado en el firmware original HB-RF-ETH. Actualizado a ESP-IDF 5.3, cadenas de herramientas modernas (GCC 13.2.0) y tecnologías WebUI actuales (Vue 3, Parcel 2, Pinia).',
    original: 'Autor original',
    firmwareLicense: 'El',
    hardwareLicense: 'El',
    under: 'está publicado bajo',
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
    containsThirdPartySoftware: 'Este software contiene productos de software de terceros gratuitos utilizados bajo diversas condiciones de licencia.',
    providedAsIs: 'El software se proporciona "tal cual" SIN NINGUNA GARANTÍA.'
  },

  // Change Password
  changePassword: {
    title: 'Cambio de contraseña requerido',
    currentPassword: 'Contraseña actual',
    newPassword: 'Nueva contraseña',
    confirmPassword: 'Confirmar contraseña',
    changePassword: 'Cambiar contraseña',
    changeSuccess: 'Contraseña cambiada correctamente',
    changeError: 'Error al cambiar la contraseña',
    passwordMismatch: 'Las contraseñas no coinciden',
    passwordTooShort: 'La contraseña debe tener al menos 6 caracteres y contener letras y números.',
    passwordsDoNotMatch: 'Las contraseñas no coinciden',
    warningMessage: 'Este es su primer inicio de sesión o la contraseña aún está configurada como "admin". Por razones de seguridad, debe cambiar la contraseña.',
    success: 'Contraseña cambiada correctamente'
  }
}
