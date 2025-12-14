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
    logout: 'Logout'
  },

  // Login Page
  login: {
    title: 'Login',
    username: 'Username',
    password: 'Password',
    login: 'Login',
    loginFailed: 'Login failed',
    invalidCredentials: 'Invalid credentials'
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
    restoreError: 'Error restoring settings'
  },

  // System Info
  sysinfo: {
    title: 'System Information',
    serial: 'Serial Number',
    version: 'Version',
    latestVersion: 'Latest Version',
    uptime: 'Uptime',
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
    title: 'Firmware Update',
    currentVersion: 'Current Version',
    selectFile: 'Select File',
    upload: 'Upload',
    uploading: 'Uploading...',
    updateSuccess: 'Firmware updated successfully',
    updateError: 'Error updating firmware',
    warning: 'Warning: Do not disconnect power during update!'
  },

  // Monitoring
  monitoring: {
    title: 'Monitoring',
    snmp: 'SNMP',
    checkmk: 'Check_MK',
    enable: 'Enable',
    port: 'Port',
    community: 'Community',
    location: 'Location',
    contact: 'Contact',
    allowedHosts: 'Allowed Hosts',
    saveSuccess: 'Monitoring settings saved successfully',
    saveError: 'Error saving monitoring settings'
  },

  // About Page
  about: {
    title: 'About HB-RF-ETH-ng',
    description: 'HomeMatic BidCoS/HmIP LAN Gateway',
    version: 'Version',
    author: 'Author',
    license: 'License',
    website: 'Website',
    documentation: 'Documentation',
    support: 'Support'
  },

  // Change Password
  changePassword: {
    title: 'Change Password',
    currentPassword: 'Current Password',
    newPassword: 'New Password',
    confirmPassword: 'Confirm Password',
    changeSuccess: 'Password changed successfully',
    changeError: 'Error changing password',
    passwordMismatch: 'Passwords do not match',
    passwordTooShort: 'Password is too short (minimum 5 characters)'
  }
}
