export default {
  // Common
  common: {
    enabled: 'Enabled',
    disabled: 'Disabled',
    save: 'Save',
    cancel: 'Cancel',
    close: 'Close',
    loading: 'Loading...',
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
    about: 'About',
    login: 'Login',
    logout: 'Logout'
  },

  // Login Page
  login: {
    title: 'Please log in',
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
    changePassword: 'Change Password',
    repeatPassword: 'Repeat Password',
    hostname: 'Hostname',

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
    hmipMAC: 'HmIP Radio MAC'
  },

  // Firmware Update
  firmware: {
    title: 'Firmware',
    currentVersion: 'Current Version',
    installedVersion: 'Installed version',
    versionInfo: 'Modernized fork v2.1 by Xerolux (2025) - Based on the original work by Alexander Reinert.',
    updateAvailable: 'An update to version {latestVersion} is available.',
    onlineUpdate: 'Update Online',
    onlineUpdateConfirm: 'Do you really want to download and install the update? The system will restart automatically.',
    onlineUpdateStarted: 'Update started. The device will restart automatically once finished.',
    updateFile: 'Firmware file',
    noFileChosen: 'No file chosen',
    browse: 'Browse',
    selectFile: 'Select File',
    upload: 'Upload',
    restart: 'Restart system',
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
      topicPrefixHelp: 'Default: hb-rf-eth - Topics will be like prefix/status/...'
    },
    enable: 'Enable',
    allowedHosts: 'Allowed Hosts'
  },

  // About Page
  about: {
    title: 'About',
    version: 'Version 2.1.0',
    fork: 'Modernized Fork',
    forkDescription: 'This version is a modernized fork by Xerolux (2025), based on the original HB-RF-ETH firmware. Updated to ESP-IDF 5.3, modern toolchains (GCC 13.2.0) and current WebUI technologies (Vue 3, Parcel 2, Pinia).',
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
    success: 'Password changed successfully'
  }
}
