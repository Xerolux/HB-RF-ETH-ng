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
    login: 'Connexion',
    logout: 'Déconnexion'
  },

  // Login Page
  login: {
    title: 'Veuillez vous connecter',
    username: "Nom d'utilisateur",
    password: 'Mot de passe',
    login: 'Connexion',
    loginFailed: 'Échec de la connexion',
    invalidCredentials: 'Identifiants invalides',
    loginError: 'La connexion a échoué.'
  },

  // Settings Page
  settings: {
    title: 'Paramètres',
    changePassword: 'Changer le mot de passe',
    repeatPassword: 'Répéter le mot de passe',
    hostname: "Nom d'hôte",

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
    timeSettings: "Paramètres de l'heure",
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
    checkUpdates: 'Vérifier les mises à jour',
    language: 'Langue',

    // Messages
    saveSuccess:
      'Les paramètres ont été enregistrés avec succès. Veuillez redémarrer le système pour les appliquer.',
    saveError: "Une erreur s'est produite lors de l'enregistrement des paramètres.",

    // Backup & Restore
    backupRestore: 'Sauvegarde et restauration',
    backupInfo: 'Téléchargez une sauvegarde de vos paramètres pour les restaurer plus tard.',
    restoreInfo:
      'Téléchargez un fichier de sauvegarde pour restaurer les paramètres. Le système redémarrera ensuite.',
    downloadBackup: 'Télécharger la sauvegarde',
    restore: 'Restaurer',
    noFileChosen: 'Aucun fichier choisi',
    browse: 'Parcourir',
    restoreConfirm:
      'Êtes-vous sûr ? Les paramètres actuels seront écrasés et le système redémarrera.',
    restoreSuccess: 'Paramètres restaurés avec succès. Redémarrage du système...',
    restoreError: 'Erreur lors de la restauration des paramètres',
    backupError: 'Erreur lors du téléchargement de la sauvegarde'
  },

  // System Info
  sysinfo: {
    title: 'Informations système',
    serial: 'Numéro de série',
    boardRevision: 'Révision de la carte',
    uptime: 'Temps de fonctionnement',
    resetReason: 'Dernier redémarrage',
    cpuUsage: 'Utilisation du CPU',
    memoryUsage: 'Utilisation de la mémoire',
    ethernetStatus: 'Connexion Ethernet',
    rawUartRemoteAddress: 'Connecté avec',
    radioModuleType: 'Type de module radio',
    radioModuleSerial: 'Numéro de série',
    radioModuleFirmware: 'Version du micrologiciel',
    radioModuleBidCosRadioMAC: 'Adresse radio (BidCoS)',
    radioModuleHmIPRadioMAC: 'Adresse radio (HmIP)',
    radioModuleSGTIN: 'SGTIN',
    version: 'Version',
    latestVersion: 'Dernière version',
    memory: 'Utilisation mémoire',
    cpu: 'Utilisation CPU',
    temperature: 'Température',
    voltage: "Tension d'alimentation",
    ethernet: 'Ethernet',
    connected: 'Connecté',
    disconnected: 'Déconnecté',
    speed: 'Vitesse',
    duplex: 'Duplex',
    radioModule: 'Module Radio',
    moduleType: 'Type de module',
    firmwareVersion: 'Version du micrologiciel',
    bidcosMAC: 'MAC Radio BidCoS',
    hmipMAC: 'MAC Radio HmIP'
  },

  // Firmware Update
  firmware: {
    title: 'Micrologiciel',
    currentVersion: 'Version actuelle',
    installedVersion: 'Version installée',
    versionInfo:
      "Fork modernisé v2.1 par Xerolux (2025) - Basé sur le travail original d'Alexander Reinert.",
    updateAvailable: 'Une mise à jour vers la version {latestVersion} est disponible.',
    onlineUpdate: 'Mise à jour en ligne',
    onlineUpdateConfirm:
      'Voulez-vous vraiment télécharger et installer la mise à jour ? Le système redémarrera automatiquement.',
    onlineUpdateStarted:
      "Mise à jour commencée. L'appareil redémarrera automatiquement une fois terminé.",
    updateFile: 'Fichier du micrologiciel',
    noFileChosen: 'Aucun fichier choisi',
    browse: 'Parcourir',
    selectFile: 'Sélectionner un fichier',
    upload: 'Télécharger',
    restart: 'Redémarrer le système',
    uploading: 'Téléchargement...',
    uploadSuccess:
      'Mise à jour du micrologiciel téléchargée avec succès. Le système redémarrera automatiquement dans 3 secondes...',
    uploadError: 'Une erreur est survenue.',
    updateSuccess: 'Micrologiciel mis à jour avec succès',
    updateError: 'Erreur lors de la mise à jour du micrologiciel',
    warning: "Attention : Ne débranchez pas l'alimentation pendant la mise à jour !",
    restartConfirm: 'Voulez-vous vraiment redémarrer le système ?'
  },

  // Monitoring
  monitoring: {
    title: 'Surveillance',
    description: 'Configurer la surveillance SNMP et CheckMK pour la passerelle HB-RF-ETH.',
    save: 'Enregistrer',
    saving: 'Enregistrement...',
    saveSuccess: 'Configuration enregistrée avec succès !',
    saveError: "Erreur lors de l'enregistrement de la configuration !",
    snmp: {
      title: 'Agent SNMP',
      enabled: 'Activer SNMP',
      port: 'Port',
      portHelp: 'Par défaut : 161',
      community: 'Chaîne de communauté',
      communityHelp: 'Par défaut : "public" - Veuillez changer pour la production !',
      location: 'Emplacement',
      locationHelp: 'Optionnel : par ex. "Salle serveur, Bâtiment A"',
      contact: 'Contact',
      contactHelp: 'Optionnel : par ex. "admin@example.com"'
    },
    checkmk: {
      title: 'Agent CheckMK',
      enabled: 'Activer CheckMK',
      port: 'Port',
      portHelp: 'Par défaut : 6556',
      allowedHosts: 'IPs clientes autorisées',
      allowedHostsHelp:
        'Adresses IP séparées par des virgules (par ex. "192.168.1.10,192.168.1.20") ou "*" pour tous'
    },
    mqtt: {
      title: 'Client MQTT',
      enabled: 'Activer MQTT',
      server: 'Serveur',
      serverHelp: "Nom d'hôte ou IP du broker MQTT",
      port: 'Port',
      portHelp: 'Par défaut : 1883',
      user: 'Utilisateur',
      userHelp: "Optionnel : Nom d'utilisateur MQTT",
      password: 'Mot de passe',
      passwordHelp: 'Optionnel : Mot de passe MQTT',
      topicPrefix: 'Préfixe du sujet',
      topicPrefixHelp: 'Par défaut : hb-rf-eth - Les sujets seront comme prefix/status/...',
      haDiscoveryEnabled: 'Découverte Home Assistant',
      haDiscoveryPrefix: 'Préfixe de découverte',
      haDiscoveryPrefixHelp: 'Par défaut : homeassistant'
    },
    enable: 'Activer',
    allowedHosts: 'Hôtes autorisés'
  },

  // About Page
  about: {
    title: 'À propos',
    version: 'Version 2.1.0',
    fork: 'Fork Modernisé',
    forkDescription:
      "Cette version est un fork modernisé par Xerolux (2025), basé sur le firmware original HB-RF-ETH. Mis à jour vers ESP-IDF 5.3, chaînes d'outils modernes (GCC 13.2.0) et technologies WebUI actuelles (Vue 3, Parcel 2, Pinia).",
    original: 'Auteur Original',
    firmwareLicense: 'Le',
    hardwareLicense: 'Le',
    under: 'est publié sous',
    description: 'Passerelle LAN HomeMatic BidCoS/HmIP',
    author: 'Auteur',
    license: 'Licence',
    website: 'Site web',
    documentation: 'Documentation',
    support: 'Support'
  },

  // Third Party
  thirdParty: {
    title: 'Logiciels tiers',
    containsThirdPartySoftware:
      'Ce logiciel contient des produits logiciels tiers gratuits utilisés sous diverses conditions de licence.',
    providedAsIs: 'Le logiciel est fourni "tel quel" SANS AUCUNE GARANTIE.'
  },

  // Change Password
  changePassword: {
    title: 'Changement de mot de passe requis',
    currentPassword: 'Mot de passe actuel',
    newPassword: 'Nouveau mot de passe',
    confirmPassword: 'Confirmer le mot de passe',
    changePassword: 'Changer le mot de passe',
    changeSuccess: 'Mot de passe changé avec succès',
    changeError: 'Erreur lors du changement de mot de passe',
    passwordMismatch: 'Les mots de passe ne correspondent pas',
    passwordTooShort:
      'Le mot de passe doit comporter au moins 6 caractères et contenir des lettres et des chiffres.',
    passwordsDoNotMatch: 'Les mots de passe ne correspondent pas',
    warningMessage:
      'Ceci est votre première connexion ou le mot de passe est toujours défini sur "admin". Pour des raisons de sécurité, vous devez changer le mot de passe.',
    success: 'Mot de passe changé avec succès'
  }
}
