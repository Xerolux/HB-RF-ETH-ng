import { defineStore } from 'pinia'
import axios from 'axios'

export const useThemeStore = defineStore('theme', {
  state: () => ({
    theme: localStorage.getItem('theme') || 'light'
  }),
  actions: {
    setTheme(newTheme) {
      this.theme = newTheme
      localStorage.setItem('theme', newTheme)
      document.documentElement.setAttribute('data-bs-theme', newTheme)
    },
    toggleTheme() {
      const newTheme = this.theme === 'light' ? 'dark' : 'light'
      this.setTheme(newTheme)
    },
    init() {
      document.documentElement.setAttribute('data-bs-theme', this.theme)
    }
  }
})

export const useLoginStore = defineStore('login', {
  state: () => ({
    isLoggedIn: sessionStorage.getItem("hb-rf-eth-ng-pw") != null,
    token: sessionStorage.getItem("hb-rf-eth-ng-pw") || "",
    passwordChanged: true // Default to true to avoid blocking if unknown
  }),
  actions: {
    login(token) {
      sessionStorage.setItem("hb-rf-eth-ng-pw", token)
      this.token = token
      this.isLoggedIn = true
    },
    logout() {
      this.isLoggedIn = false
      sessionStorage.removeItem("hb-rf-eth-ng-pw")
      this.token = ""
    },
    setPasswordChanged(status) {
      this.passwordChanged = status
    },
    async tryLogin(password) {
      try {
        const response = await axios.post("/login.json", { password })
        if (response.data.isAuthenticated) {
          this.login(response.data.token)
          this.setPasswordChanged(response.data.passwordChanged !== false)
          return true
        }
        return false
      } catch (error) {
        console.error('Login failed:', error.response?.status || error.message)
        return false
      }
    }
  }
})

export const useSysInfoStore = defineStore('sysInfo', {
  state: () => ({
    serial: "",
    currentVersion: "",
    firmwareVariant: "",
    latestVersion: "",
    releaseNotes: "",
    downloadUrl: "",
    rawUartRemoteAddress: "",
    memoryUsage: 0.0,
    cpuUsage: 0.0,
    uptimeSeconds: 0,
    boardRevision: "",
    resetReason: "",
    ethernetConnected: false,
    ethernetSpeed: 0,
    ethernetDuplex: "",
    radioModuleType: "",
    radioModuleSerial: "",
    radioModuleFirmwareVersion: "",
    radioModuleBidCosRadioMAC: "",
    radioModuleHmIPRadioMAC: "",
    radioModuleSGTIN: ""
  }),
  actions: {
    async update() {
      try {
        const response = await axios.get("/sysinfo.json")
        Object.assign(this.$state, response.data.sysInfo)
      } catch (error) {
        console.error('Failed to load system info:', error.response?.status || error.message)
        throw error
      }
    }
  }
})

export const useSettingsStore = defineStore('settings', {
  state: () => ({
    hostname: "",
    useDHCP: true,
    localIP: "",
    netmask: "",
    gateway: "",
    dns1: "",
    dns2: "",
    // IPv6 settings
    enableIPv6: false,
    ipv6Mode: "auto",
    ipv6Address: "",
    ipv6PrefixLength: 64,
    ipv6Gateway: "",
    ipv6Dns1: "",
    ipv6Dns2: "",
    timesource: 0,
    dcfOffset: 0,
    gpsBaudrate: 9600,
    ntpServer: "",
    ledBrightness: 100,
    checkUpdates: true,
    allowPrerelease: false,
    hmlgwEnabled: false,
    hmlgwPort: 2000,
    hmlgwKeepAlivePort: 2001,
    analyzerEnabled: false,
    // DTLS settings
    dtlsMode: 0,
    dtlsCipherSuite: 1,
    dtlsRequireClientCert: false,
    dtlsSessionResumption: true
  }),
  actions: {
    async load() {
      try {
        const response = await axios.get("/settings.json")
        Object.assign(this.$state, response.data.settings)
      } catch (error) {
        console.error('Failed to load settings:', error.response?.status || error.message)
        throw error
      }
    },
    async save(settings) {
      try {
        const response = await axios.post("/settings.json", settings)
        Object.assign(this.$state, response.data.settings)
      } catch (error) {
        console.error('Failed to save settings:', error.response?.status || error.message)
        throw error
      }
    }
  }
})

export const useFirmwareUpdateStore = defineStore('firmwareUpdate', {
  state: () => ({
    progress: 0,
  }),
  actions: {
    async update(file) {
      try {
        this.progress = 0

        await axios.post("/ota_update", file, {
          headers: {
            'Content-Type': 'application/octet-stream'
          },
          onUploadProgress: event => {
            if (event.lengthComputable) {
              this.progress = Math.ceil((event.loaded || event.position) / event.total * 100)
            }
          }
        })

        this.progress = 0
      } catch (error) {
        console.error('Firmware update failed:', error.response?.status || error.message)
        this.progress = 0
        throw error
      }
    }
  }
})

export const useMonitoringStore = defineStore('monitoring', {
  state: () => ({
    snmp: {
      enabled: false,
      port: 161,
      community: '',  // Empty by default - user must set custom value
      location: '',
      contact: ''
    },
    checkmk: {
      enabled: false,
      port: 6556,
      allowedHosts: '*'
    },
    mqtt: {
      enabled: false,
      server: '',
      port: 1883,
      user: '',
      password: '',
      topicPrefix: 'hb-rf-eth',
      haDiscoveryEnabled: false,
      haDiscoveryPrefix: 'homeassistant'
    }
  }),
  actions: {
    async load() {
      try {
        const response = await axios.get("/api/monitoring")
        Object.assign(this.$state, response.data)
      } catch (error) {
        console.error('Failed to load monitoring config:', error.response?.status || error.message)
        throw error
      }
    },
    async save(config) {
      try {
        await axios.post("/api/monitoring", config)
        Object.assign(this.$state, config)
      } catch (error) {
        console.error('Failed to save monitoring config:', error.response?.status || error.message)
        throw error
      }
    }
  }
})
