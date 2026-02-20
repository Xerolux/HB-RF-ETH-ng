export default {
  // Common
  common: {
    enabled: 'Activé',
    disabled: 'Désactivé',
    save: 'Enregistrer',
    cancel: 'Annuler',
    close: 'Fermer',
    loading: 'Chargement...',
    changing: 'Modification...',
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
    systemlog: 'Journal système',
    about: 'À propos',
    login: 'Connexion',
    logout: 'Déconnexion',
    toggleTheme: 'Changer de thème',
    language: 'Langue'
  },

  // Login Page
  login: {
    title: 'Veuillez vous connecter',
    subtitle: 'Veuillez entrer votre mot de passe pour continuer',
    username: 'Nom d\'utilisateur',
    password: 'Mot de passe',
    passwordPlaceholder: 'Mot de passe',
    login: 'Connexion',
    loginFailed: 'Échec de la connexion',
    invalidCredentials: 'Identifiants invalides',
    loginError: 'La connexion a échoué.',
    passwordRequired: 'Le mot de passe est requis',
    loggingIn: 'Connexion en cours...'
  },

  // Settings Page
  settings: {
    title: 'Paramètres',
    ccuSettings: 'Connexion CCU',
    ccuIpAddress: 'Adresse IP CCU',
    ccuIpHint: 'Veuillez saisir l\'adresse IP de votre CCU pour éviter le blocage de la connexion. Le système redémarrera après l\'enregistrement.',
    tabGeneral: 'Général',
    tabNetwork: 'Réseau',
    tabTime: 'Heure',
    tabBackup: 'Sauvegarde',
    changePassword: 'Changer le mot de passe',
    repeatPassword: 'Répéter le mot de passe',
    hostname: 'Nom d\'hôte',

    // Security Settings
    security: 'Sécurité',
    changePasswordHint: 'Changez votre mot de passe administrateur pour un accès sécurisé',
    otaPassword: 'Mot de passe OTA',
    otaPasswordHint: 'Mot de passe séparé requis pour les mises à jour du micrologiciel',
    changePasswordBtn: 'Changer le mot de passe',
    changeOtaPassword: 'Changer',
    setOtaPassword: 'Définir le mot de passe',
    clearOtaPassword: 'Effacer',
    clearOtaPasswordConfirm: 'Êtes-vous sûr de vouloir supprimer le mot de passe OTA ? Les mises à jour du micrologiciel ne seront pas possibles tant qu\'un nouveau mot de passe n\'aura pas été défini.',
    clearOtaPasswordSuccess: 'Le mot de passe OTA a été supprimé.',
    clearOtaPasswordError: 'Échec de la suppression du mot de passe OTA',

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
    updateLedBlink: 'LED clignotante lors des mises à jour',
    checkUpdates: 'Vérifier les mises à jour',
    allowPrerelease: 'Autoriser les mises à jour anticipées (Beta/Alpha)',
    language: 'Langue',

    // Messages
    saveSuccess: 'Les paramètres ont été enregistrés avec succès.',
    saveError: 'Une erreur s\'est produite lors de l\'enregistrement des paramètres.',
    restartTitle: 'Redémarrage requis',
    restartMessage: 'Les paramètres ont été enregistrés. Voulez-vous redémarrer l\'appareil maintenant pour appliquer les modifications ?',
    restartNow: 'Redémarrer maintenant',
    restartLater: 'Redémarrer plus tard',

    // Backup & Restore
    backupRestore: 'Sauvegarde et restauration',
    backupInfo: 'Téléchargez une sauvegarde de vos paramètres pour les restaurer plus tard.',
    restoreInfo: 'Téléchargez un fichier de sauvegarde pour restaurer les paramètres. Le système redémarrera ensuite.',
    downloadBackup: 'Télécharger la sauvegarde',
    restore: 'Restaurer',
    restoreBtn: 'Restaurer',
    download: 'Télécharger',
    noFileChosen: 'Aucun fichier choisi',
    browse: 'Parcourir',
    restoreConfirm: 'Êtes-vous sûr ? Les paramètres actuels seront écrasés et le système redémarrera.',
    restoreSuccess: 'Paramètres restaurés avec succès. Redémarrage du système...',
    restoreError: 'Erreur lors de la restauration des paramètres',
    backupError: 'Erreur lors du téléchargement de la sauvegarde'
  },

  // System Info
  sysinfo: {
    title: 'Informations système',
    goodMorning: 'Bonjour',
    goodAfternoon: 'Bon après-midi',
    goodEvening: 'Bonsoir',
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
    voltage: 'Tension d\'alimentation',
    ethernet: 'Ethernet',
    connected: 'Connecté',
    disconnected: 'Déconnecté',
    speed: 'Vitesse',
    duplex: 'Duplex',
    radioModule: 'Module Radio',
    moduleType: 'Type de module',
    firmwareVersion: 'Version du micrologiciel',
    bidcosMAC: 'MAC Radio BidCoS',
    hmipMAC: 'MAC Radio HmIP',
    system: 'Système',
    network: 'Réseau',
    down: 'Bas',
    mbits: 'Mbit/s',
    online: 'En ligne',
    offline: 'Hors ligne',
    dashboardTitle: 'État du système'
  },

  // Update
  update: {
    available: 'Mise à jour disponible',
    updateNow: 'Mettre à jour maintenant'
  },

  // Firmware Update
  firmware: {
    title: 'Micrologiciel',
    subtitle: 'Mettre à jour le micrologiciel de votre appareil',
    restarting: 'Redémarrage...',
    restartingText: 'L\'appareil redémarre. La page se rechargera automatiquement.',
    fileUpload: 'Téléchargement de fichier',
    fileUploadHint: 'Télécharger un fichier de micrologiciel .bin',
    networkUpdate: 'Mise à jour réseau',
    networkUpdateHint: 'Télécharger depuis une URL',
    urlPlaceholder: 'https://exemple.com/firmware.bin',
    downloading: 'Téléchargement...',
    downloadInstall: 'Télécharger et installer',
    factoryReset: 'Réinitialiser',
    factoryResetHint: 'Réinitialiser aux paramètres d\'usine',
    factoryResetConfirm: 'Voulez-vous vraiment réinitialiser aux paramètres d\'usine ? Tous les paramètres seront perdus.',
    otaProgress: 'Téléchargement de la mise à jour du micrologiciel...',
    otaSuccess: 'Téléchargement réussi. Installation...',
    currentVersion: 'Version actuelle',
    installedVersion: 'Version installée',
    versionInfo: 'Fork modernisé v2.1.7 par Xerolux (2025) - Basé sur le travail original d\'Alexander Reinert.',
    updateAvailable: 'Une mise à jour vers la version {latestVersion} est disponible.',
    newVersionAvailable: 'Nouvelle version {version} disponible !',
    viewUpdate: 'Voir',
    onlineUpdate: 'Mise à jour en ligne',
    onlineUpdateConfirm: 'Voulez-vous vraiment télécharger et installer la mise à jour ? Le système redémarrera automatiquement.',
    onlineUpdateStarted: 'Mise à jour commencée. L\'appareil redémarrera automatiquement une fois terminé.',
    showReleaseNotes: 'Afficher les notes de version',
    releaseNotesTitle: 'Notes de version pour v{version}',
    releaseNotesError: 'Échec du chargement des notes de version depuis GitHub.',
    updateFile: 'Fichier du micrologiciel',
    noFileChosen: 'Aucun fichier choisi',
    browse: 'Parcourir',
    selectFile: 'Sélectionner ou déposer un fichier .bin',
    upload: 'Installer le micrologiciel',
    restart: 'Redémarrer',
    restartHint: 'Redémarrer l\'appareil',
    uploading: 'Téléchargement...',
    uploadSuccess: 'Mise à jour du micrologiciel téléchargée avec succès. Le système redémarrera automatiquement dans 3 secondes...',
    uploadError: 'Une erreur est survenue.',
    updateSuccess: 'Micrologiciel mis à jour avec succès',
    updateError: 'Erreur lors de la mise à jour du micrologiciel',
    warning: 'Attention : Ne débranchez pas l\'alimentation pendant la mise à jour !',
    restartConfirm: 'Voulez-vous vraiment redémarrer le système ?'
  },

  // Monitoring
  monitoring: {
    title: 'Surveillance',
    description: 'Configurer la surveillance SNMP et CheckMK pour la passerelle HB-RF-ETH.',
    save: 'Enregistrer',
    saving: 'Enregistrement...',
    saveSuccess: 'Configuration enregistrée avec succès !',
    saveError: 'Erreur lors de l\'enregistrement de la configuration !',
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
      allowedHostsHelp: 'Adresses IP séparées par des virgules (par ex. "192.168.1.10,192.168.1.20") ou "*" pour tous'
    },
    mqtt: {
      title: 'Client MQTT',
      enabled: 'Activer MQTT',
      server: 'Serveur',
      serverHelp: 'Nom d\'hôte ou IP du broker MQTT',
      port: 'Port',
      portHelp: 'Par défaut : 1883',
      user: 'Utilisateur',
      userHelp: 'Optionnel : Nom d\'utilisateur MQTT',
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
    version: 'Version 2.1.7',
    fork: 'Fork Modernisé',
    forkDescription: 'Cette version est un fork modernisé par Xerolux (2025), basé sur le firmware original HB-RF-ETH. Mis à jour vers ESP-IDF 5.3, chaînes d\'outils modernes et technologies WebUI actuelles (Vue 3, Vite, Pinia).',
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
    containsThirdPartySoftware: 'Ce logiciel contient des produits logiciels tiers gratuits utilisés sous diverses conditions de licence.',
    providedAsIs: 'Le logiciel est fourni "tel quel" SANS AUCUNE GARANTIE.'
  },

  // Change Password
  changePassword: {
    title: 'Changement de mot de passe requis',
    subtitle: 'Sécurisez votre compte',
    warningTitle: 'Requis',
    requirementsTitle: 'Exigences du mot de passe :',
    reqMinLength: 'Au moins 6 caractères',
    reqLettersNumbers: 'Doit contenir des lettres et des chiffres',
    currentPassword: 'Mot de passe actuel',
    newPassword: 'Nouveau mot de passe',
    confirmPassword: 'Confirmer le mot de passe',
    changePassword: 'Changer le mot de passe',
    changeSuccess: 'Mot de passe changé avec succès',
    changeError: 'Erreur lors du changement de mot de passe',
    passwordMismatch: 'Les mots de passe ne correspondent pas',
    passwordTooShort: 'Le mot de passe doit comporter au moins 6 caractères et contenir des lettres et des chiffres.',
    passwordRequirements: 'Doit contenir des lettres et des chiffres',
    passwordsDoNotMatch: 'Les mots de passe ne correspondent pas',
    warningMessage: 'Ceci est votre première connexion ou le mot de passe est toujours défini sur "admin". Pour des raisons de sécurité, vous devez changer le mot de passe.',
    success: 'Mot de passe changé avec succès',
    newPasswordPlaceholder: 'Entrer le nouveau mot de passe',
    confirmPasswordPlaceholder: 'Confirmer le nouveau mot de passe'
  },

  // OTA Password Modal
  otaPassword: {
    title: 'Définir le mot de passe OTA',
    warningMessage: 'Définissez un mot de passe distinct pour les mises à jour du micrologiciel. Ceci est requis pour les mises à jour OTA.',
    otaPassword: 'Mot de passe OTA',
    otaPasswordPlaceholder: 'Entrer le mot de passe OTA',
    confirmPassword: 'Confirmer le mot de passe',
    confirmPasswordPlaceholder: 'Confirmer le mot de passe OTA',
    passwordTooShort: 'Le mot de passe doit comporter au moins 8 caractères',
    passwordRequirements: 'Doit contenir des majuscules, des minuscules et des chiffres',
    passwordsDoNotMatch: 'Les mots de passe ne correspondent pas',
    requirementsTitle: 'Exigences du mot de passe :',
    reqMinLength: 'Au moins 8 caractères',
    reqMixedCase: 'Lettres majuscules et minuscules',
    reqNumbers: 'Au moins un chiffre',
    strengthWeak: 'Faible',
    strengthMedium: 'Moyen',
    strengthGood: 'Bon',
    strengthStrong: 'Fort'
  },

  // Sponsor
  sponsor: {
    title: 'Soutenir ce projet',
    description: 'Si vous aimez ce projet et souhaitez soutenir son développement, vous pouvez utiliser l\'une des options ci-dessous.',
    thanks: 'Merci pour votre soutien !'
  },

  // System Log
  systemlog: {
    title: 'Journal système',
    description: 'Vue en direct de la sortie du journal système avec capacité de téléchargement.',
    liveLog: 'Journal en direct',
    autoScroll: 'Défilement automatique',
    enabled: 'Activé',
    disabledMessage: 'La visionneuse de journaux est désactivée. Activez l\'interrupteur pour voir les journaux en direct.',
    refresh: 'Actualiser',
    clear: 'Effacer',
    download: 'Télécharger',
    empty: 'Aucune entrée de journal pour le moment.'
  },

  // Changelog
  changelog: {
    title: 'Journal des modifications',
    loading: 'Chargement...',
    fetching: 'Récupération du journal des modifications depuis GitHub...',
    error: 'Échec du chargement du journal des modifications',
    fetchError: 'Impossible de récupérer le journal des modifications. Veuillez vérifier votre connexion Internet.',
    retry: 'Réessayer',
    close: 'Fermer',
    viewOnGithub: 'Voir sur GitHub'
  }
}
