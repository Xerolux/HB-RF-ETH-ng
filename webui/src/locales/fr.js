export default {
  // Common
  common: {
    enabled: 'Activé',
    disabled: 'Désactivé',
    save: 'Enregistrer',
    cancel: 'Annuler',
    close: 'Fermer',
    loading: 'Chargement...',
    error: 'Erreur',
    success: 'Succès',
    yes: 'Oui',
    no: 'Non'
  },

  // Header Navigation
  nav: {
    home: 'Accueil',
    settings: 'Paramètres',
    firmware: 'Micrologiciel',
    monitoring: 'Surveillance',
    about: 'À propos',
    logout: 'Déconnexion'
  },

  // Login Page
  login: {
    title: 'Connexion',
    username: 'Nom d\'utilisateur',
    password: 'Mot de passe',
    login: 'Se connecter',
    loginFailed: 'Échec de la connexion',
    invalidCredentials: 'Identifiants invalides'
  },

  // Settings Page
  settings: {
    title: 'Paramètres',
    changePassword: 'Changer le mot de passe',
    repeatPassword: 'Répéter le mot de passe',
    hostname: 'Nom d\'hôte',

    // Network Settings
    networkSettings: 'Paramètres réseau',
    dhcp: 'DHCP',
    ipAddress: 'Adresse IP',
    netmask: 'Masque de sous-réseau',
    gateway: 'Passerelle',
    dns1: 'DNS primaire',
    dns2: 'DNS secondaire',

    // IPv6 Settings
    ipv6Settings: 'Paramètres IPv6',
    enableIPv6: 'Activer IPv6',
    ipv6Mode: 'Mode IPv6',
    ipv6Auto: 'Automatique (SLAAC)',
    ipv6Static: 'Statique',
    ipv6Address: 'Adresse IPv6',
    ipv6PrefixLength: 'Longueur du préfixe',
    ipv6Gateway: 'Passerelle IPv6',
    ipv6Dns1: 'DNS IPv6 primaire',
    ipv6Dns2: 'DNS IPv6 secondaire',

    // Time Settings
    timeSettings: 'Paramètres de l\'heure',
    timesource: 'Source de temps',
    ntp: 'NTP',
    dcf: 'DCF',
    gps: 'GPS',
    ntpServer: 'Serveur NTP',
    dcfOffset: 'Décalage DCF',
    gpsBaudrate: 'Débit GPS',

    // System Settings
    systemSettings: 'Paramètres système',
    ledBrightness: 'Luminosité LED',
    language: 'Langue',

    // Messages
    saveSuccess: 'Les paramètres ont été enregistrés avec succès. Veuillez redémarrer le système pour les appliquer.',
    saveError: 'Une erreur s\'est produite lors de l\'enregistrement des paramètres.'
  },

  // System Info
  sysinfo: {
    title: 'Informations système',
    serial: 'Numéro de série',
    version: 'Version',
    latestVersion: 'Dernière version',
    uptime: 'Temps de fonctionnement',
    memory: 'Utilisation de la mémoire',
    cpu: 'Utilisation du CPU',
    temperature: 'Température',
    voltage: 'Tension d\'alimentation',
    ethernet: 'Ethernet',
    connected: 'Connecté',
    disconnected: 'Déconnecté',
    speed: 'Vitesse',
    duplex: 'Duplex',
    radioModule: 'Module radio',
    moduleType: 'Type de module',
    firmwareVersion: 'Version du micrologiciel',
    bidcosMAC: 'MAC radio BidCoS',
    hmipMAC: 'MAC radio HmIP'
  },

  // Firmware Update
  firmware: {
    title: 'Mise à jour du micrologiciel',
    currentVersion: 'Version actuelle',
    selectFile: 'Sélectionner un fichier',
    upload: 'Télécharger',
    uploading: 'Téléchargement en cours...',
    updateSuccess: 'Micrologiciel mis à jour avec succès',
    updateError: 'Erreur lors de la mise à jour du micrologiciel',
    warning: 'Attention: Ne débranchez pas l\'alimentation pendant la mise à jour!'
  },

  // Monitoring
  monitoring: {
    title: 'Surveillance',
    snmp: 'SNMP',
    checkmk: 'Check_MK',
    enable: 'Activer',
    port: 'Port',
    community: 'Communauté',
    location: 'Emplacement',
    contact: 'Contact',
    allowedHosts: 'Hôtes autorisés',
    saveSuccess: 'Paramètres de surveillance enregistrés avec succès',
    saveError: 'Erreur lors de l\'enregistrement des paramètres de surveillance'
  },

  // About Page
  about: {
    title: 'À propos de HB-RF-ETH-ng',
    description: 'Passerelle LAN HomeMatic BidCoS/HmIP',
    version: 'Version',
    author: 'Auteur',
    license: 'Licence',
    website: 'Site web',
    documentation: 'Documentation',
    support: 'Support'
  },

  // Change Password
  changePassword: {
    title: 'Changer le mot de passe',
    currentPassword: 'Mot de passe actuel',
    newPassword: 'Nouveau mot de passe',
    confirmPassword: 'Confirmer le mot de passe',
    changeSuccess: 'Mot de passe changé avec succès',
    changeError: 'Erreur lors du changement de mot de passe',
    passwordMismatch: 'Les mots de passe ne correspondent pas',
    passwordTooShort: 'Le mot de passe est trop court (minimum 5 caractères)'
  }
}
