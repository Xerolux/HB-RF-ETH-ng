export default {
  // Common
  common: {
    enabled: 'Enabled',
    disabled: 'Disabled',
    save: 'Save',
    cancel: 'Cancel',
    close: 'Close',
    loading: 'Loading...',
    changing: 'Changing...',
    error: 'Error',
    success: 'Success',
    yes: 'Yes',
    no: 'No'
  },

  // Header Navigation
  nav: {
    home: 'Home',
    settings: 'Settings',
    firmware: 'Firmware',
    monitoring: 'Monitoring',
    systemlog: 'System Log',
    about: 'About',
    login: 'Login',
    logout: 'Logout',
    toggleTheme: 'Toggle theme'
  },

  // Login Page
  login: {
    title: 'Please log in',
    subtitle: 'Please enter your password to continue',
    username: 'Username',
    password: 'Password',
    login: 'Login',
    loginFailed: 'Login failed',
    invalidCredentials: 'Invalid credentials',
    loginError: 'Login was not successful.'
  },

  // Settings Page
  settings: {
    title: 'Settings',
    tabGeneral: 'General',
    tabNetwork: 'Network',
    tabTime: 'Time',
    tabBackup: 'Backup',
    changePassword: 'Change Password',
    repeatPassword: 'Repeat Password',
    hostname: 'Hostname',

    // Security Settings
    security: 'Security',
    changePasswordHint: 'Change your administrator password for secure access',
    otaPassword: 'OTA Password',
    otaPasswordHint: 'Separate password required for firmware updates',
    changePasswordBtn: 'Change Password',
    changeOtaPassword: 'Change',
    setOtaPassword: 'Set Password',
    clearOtaPassword: 'Clear',
    clearOtaPasswordConfirm: 'Are you sure you want to remove the OTA password? Firmware updates will not be possible until a new password is set.',
    clearOtaPasswordSuccess: 'OTA password has been removed.',
    clearOtaPasswordError: 'Failed to remove OTA password',

    // Network Settings
    networkSettings: 'Network Settings',
    dhcp: 'DHCP',
    ipAddress: 'IP Address',
    netmask: 'Netmask',
    gateway: 'Gateway',
    dns1: 'Primary DNS',
    dns2: 'Secondary DNS',

    // IPv6 Settings
    ipv6Settings: 'IPv6 Settings',
    enableIPv6: 'Enable IPv6',
    ipv6Mode: 'IPv6 Mode',
    ipv6Auto: 'Automatic (SLAAC)',
    ipv6Static: 'Static',
    ipv6Address: 'IPv6 Address',
    ipv6PrefixLength: 'Prefix Length',
    ipv6Gateway: 'IPv6 Gateway',
    ipv6Dns1: 'Primary IPv6 DNS',
    ipv6Dns2: 'Secondary IPv6 DNS',

    // Time Settings
    timeSettings: 'Time Settings',
    timesource: 'Time Source',
    ntp: 'NTP',
    dcf: 'DCF',
    gps: 'GPS',
    ntpServer: 'NTP Server',
    dcfOffset: 'DCF Offset',
    gpsBaudrate: 'GPS Baudrate',

    // System Settings
    systemSettings: 'System Settings',
    ledBrightness: 'LED Brightness',
    checkUpdates: 'Check for updates',
    allowPrerelease: 'Allow Early Updates (Beta/Alpha)',
    language: 'Language',

    // Messages
    saveSuccess: 'Settings were successfully saved. Please restart to take them effect.',
    saveError: 'An error occurred while saving the settings.',

    // Backup & Restore
    backupRestore: 'Backup & Restore',
    backupInfo: 'Download a backup of your settings to restore them later.',
    restoreInfo: 'Upload a backup file to restore settings. The system will restart afterwards.',
    downloadBackup: 'Download Backup',
    restore: 'Restore',
    restoreBtn: 'Restore',
    download: 'Download',
    noFileChosen: 'No file chosen',
    browse: 'Browse',
    restoreConfirm: 'Are you sure? Current settings will be overwritten and the system will restart.',
    restoreSuccess: 'Settings successfully restored. System restarting...',
    restoreError: 'Error restoring settings',
    backupError: 'Error downloading backup'
  },

  // System Info
  sysinfo: {
    title: 'System information',
    goodMorning: 'Good Morning',
    goodAfternoon: 'Good Afternoon',
    goodEvening: 'Good Evening',
    serial: 'Serial number',
    boardRevision: 'Board revision',
    uptime: 'Uptime',
    resetReason: 'Last reboot',
    cpuUsage: 'CPU usage',
    memoryUsage: 'Memory usage',
    ethernetStatus: 'Ethernet connection',
    rawUartRemoteAddress: 'Connected with',
    radioModuleType: 'Radio module type',
    radioModuleSerial: 'Serial number',
    radioModuleFirmware: 'Firmware version',
    radioModuleBidCosRadioMAC: 'Radio address (BidCoS)',
    radioModuleHmIPRadioMAC: 'Radio address (HmIP)',
    radioModuleSGTIN: 'SGTIN',
    version: 'Version',
    latestVersion: 'Latest Version',
    memory: 'Memory Usage',
    cpu: 'CPU Usage',
    temperature: 'Temperature',
    voltage: 'Supply Voltage',
    ethernet: 'Ethernet',
    connected: 'Connected',
    disconnected: 'Disconnected',
    speed: 'Speed',
    duplex: 'Duplex',
    radioModule: 'Radio Module',
    moduleType: 'Module Type',
    firmwareVersion: 'Firmware Version',
    bidcosMAC: 'BidCoS Radio MAC',
    hmipMAC: 'HmIP Radio MAC',
    system: 'System',
    network: 'Network',
    down: 'Down',
    mbits: 'Mbit/s',
    online: 'Online',
    offline: 'Offline',
    dashboardTitle: 'System Status'
  },

  // Firmware Update
  firmware: {
    title: 'Firmware',
    subtitle: 'Update your device firmware',
    restarting: 'Restarting...',
    restartingText: 'Device is restarting. Page will reload automatically.',
    fileUpload: 'File Upload',
    fileUploadHint: 'Upload a .bin firmware file',
    networkUpdate: 'Network Update',
    networkUpdateHint: 'Download from URL',
    urlPlaceholder: 'https://example.com/firmware.bin',
    downloading: 'Downloading...',
    downloadInstall: 'Download & Install',
    factoryReset: 'Reset',
    factoryResetHint: 'Reset to factory defaults',
    factoryResetConfirm: 'Do you really want to reset to factory defaults? All settings will be lost.',
    otaProgress: 'Downloading firmware update...',
    otaSuccess: 'Download successful. Installing...',
    currentVersion: 'Current Version',
    installedVersion: 'Installed version',
    versionInfo: 'Modernized fork v2.1.4 by Xerolux (2025) - Based on the original work by Alexander Reinert.',
    updateAvailable: 'An update to version {latestVersion} is available.',
    newVersionAvailable: 'New version {version} is available!',
    viewUpdate: 'View',
    onlineUpdate: 'Update Online',
    onlineUpdateConfirm: 'Do you really want to download and install the update? The system will restart automatically.',
    onlineUpdateStarted: 'Update started. The device will restart automatically once finished.',
    showReleaseNotes: 'Show Release Notes',
    releaseNotesTitle: 'Release Notes for v{version}',
    releaseNotesError: 'Failed to load release notes from GitHub.',
    updateFile: 'Firmware file',
    noFileChosen: 'No file chosen',
    browse: 'Browse',
    selectFile: 'Select or drop .bin file',
    upload: 'Install Firmware',
    restart: 'Restart',
    restartHint: 'Reboot device',
    uploading: 'Uploading...',
    uploadSuccess: 'Firmware update successfully uploaded. System will restart automatically in 3 seconds...',
    uploadError: 'An error occured.',
    updateSuccess: 'Firmware updated successfully',
    updateError: 'Error updating firmware',
    warning: 'Warning: Do not disconnect power during update!',
    restartConfirm: 'Do you really want to restart the system?'
  },

  // Monitoring
  monitoring: {
    title: 'Monitoring',
    description: 'Configure SNMP and CheckMK monitoring for the HB-RF-ETH gateway.',
    save: 'Save',
    saving: 'Saving...',
    saveSuccess: 'Configuration saved successfully!',
    saveError: 'Error saving configuration!',
    snmp: {
      title: 'SNMP Agent',
      enabled: 'Enable SNMP',
      port: 'Port',
      portHelp: 'Default: 161',
      community: 'Community String',
      communityHelp: 'Default: "public" - Please change for production!',
      location: 'Location',
      locationHelp: 'Optional: e.g. "Server room, Building A"',
      contact: 'Contact',
      contactHelp: 'Optional: e.g. "admin@example.com"'
    },
    checkmk: {
      title: 'CheckMK Agent',
      enabled: 'Enable CheckMK',
      port: 'Port',
      portHelp: 'Default: 6556',
      allowedHosts: 'Allowed Client IPs',
      allowedHostsHelp: 'Comma-separated IP addresses (e.g. "192.168.1.10,192.168.1.20") or "*" for all'
    },
    mqtt: {
      title: 'MQTT Client',
      enabled: 'Enable MQTT',
      server: 'Server',
      serverHelp: 'MQTT Broker Hostname or IP',
      port: 'Port',
      portHelp: 'Default: 1883',
      user: 'User',
      userHelp: 'Optional: MQTT Username',
      password: 'Password',
      passwordHelp: 'Optional: MQTT Password',
      topicPrefix: 'Topic Prefix',
      topicPrefixHelp: 'Default: hb-rf-eth - Topics will be like prefix/status/...',
      haDiscoveryEnabled: 'Home Assistant Discovery',
      haDiscoveryPrefix: 'Discovery Prefix',
      haDiscoveryPrefixHelp: 'Default: homeassistant'
    },
    enable: 'Enable',
    allowedHosts: 'Allowed Hosts'
  },

  // About Page
  about: {
    title: 'About',
    version: 'Version 2.1.4',
    fork: 'Modernized Fork',
    forkDescription: 'This version is a modernized fork by Xerolux (2025), based on the original HB-RF-ETH firmware. Updated to ESP-IDF 5.3, modern toolchains and current WebUI technologies (Vue 3, Vite, Pinia).',
    original: 'Original Author',
    firmwareLicense: 'The',
    hardwareLicense: 'The',
    under: 'is released under',
    description: 'HomeMatic BidCoS/HmIP LAN Gateway',
    author: 'Author',
    license: 'License',
    website: 'Website',
    documentation: 'Documentation',
    support: 'Support'
  },

  // Third Party
  thirdParty: {
    title: 'Third party software',
    containsThirdPartySoftware: 'This software contains free third party software products used under various license conditions.',
    providedAsIs: 'The software is provided "as is" WITHOUT ANY WARRANTY.'
  },

  // Change Password
  changePassword: {
    title: 'Password change required',
    currentPassword: 'Current Password',
    newPassword: 'New Password',
    confirmPassword: 'Confirm Password',
    changePassword: 'Change Password',
    changeSuccess: 'Password changed successfully',
    changeError: 'Error changing password',
    passwordMismatch: 'Passwords do not match',
    passwordTooShort: 'Password must be at least 6 characters long and contain letters and numbers.',
    passwordsDoNotMatch: 'Passwords do not match',
    warningMessage: 'This is your first login or the password is still set to "admin". For security reasons, you must change the password.',
    success: 'Password changed successfully',
    newPasswordPlaceholder: 'Enter new password',
    confirmPasswordPlaceholder: 'Confirm new password'
  },

  // OTA Password Modal
  otaPassword: {
    title: 'Set OTA Password',
    warningMessage: 'Set a separate password for firmware updates. This is required for OTA updates.',
    otaPassword: 'OTA Password',
    otaPasswordPlaceholder: 'Enter OTA password',
    confirmPassword: 'Confirm Password',
    confirmPasswordPlaceholder: 'Confirm OTA password',
    passwordTooShort: 'Password must be at least 8 characters long',
    passwordRequirements: 'Must contain uppercase, lowercase, and numbers',
    passwordsDoNotMatch: 'Passwords do not match',
    requirementsTitle: 'Password requirements:',
    reqMinLength: 'At least 8 characters',
    reqMixedCase: 'Uppercase and lowercase letters',
    reqNumbers: 'At least one number',
    strengthWeak: 'Weak',
    strengthMedium: 'Medium',
    strengthGood: 'Good',
    strengthStrong: 'Strong'
  },

  // Sponsor
  sponsor: {
    title: 'Support this Project',
    description: 'If you like this project and want to support its development, you can use one of the options below.',
    thanks: 'Thank you for your support!'
  },

  // System Log
  systemlog: {
    title: 'System Log',
    description: 'Live view of system log output with download capability.',
    liveLog: 'Live Log',
    autoScroll: 'Auto-scroll',
    enabled: 'Enabled',
    disabledMessage: 'Log viewer is disabled. Turn on the switch to see live logs.',
    clear: 'Clear',
    download: 'Download',
    empty: 'No log entries yet.'
  },

  // Changelog
  changelog: {
    title: 'Changelog',
    loading: 'Loading...',
    fetching: 'Fetching changelog from GitHub...',
    error: 'Failed to load changelog',
    fetchError: 'Could not fetch changelog. Please check your internet connection.',
    retry: 'Retry',
    close: 'Close',
    viewOnGithub: 'View on GitHub'
  }
}
