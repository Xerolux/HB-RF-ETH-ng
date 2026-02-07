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
    latestVersion: "",
    rawUartRemoteAddress: "",
    memoryUsage: 0.0,
    cpuUsage: 0.0,
    supplyVoltage: 0.0,
    temperature: 0.0,
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

export const useUpdateStore = defineStore('update', {
  state: () => ({
    latestVersion: null,
    isChecking: false,
    updateAvailable: false,
    lastCheck: null,
    checkError: null
  }),
  getters: {
    shouldShowUpdateBadge: (state) => {
      return state.updateAvailable && state.latestVersion !== null
    }
  },
  actions: {
    async checkForUpdate(currentVersion) {
      if (this.isChecking) return

      this.isChecking = true
      this.checkError = null

      try {
        const response = await fetch('https://raw.githubusercontent.com/Xerolux/HB-RF-ETH-ng/refs/heads/main/version.txt')
        if (!response.ok) throw new Error('Failed to fetch version')

        const latestVersion = await response.text()
        this.latestVersion = latestVersion.trim()
        this.lastCheck = new Date().toISOString()

        // Compare versions
        this.updateAvailable = this.compareVersions(currentVersion, this.latestVersion) < 0
      } catch (error) {
        console.error('Update check failed:', error)
        this.checkError = error.message
      } finally {
        this.isChecking = false
      }
    },

    compareVersions(current, latest) {
      const parseVersion = (v) => {
        const match = v.match(/(\d+)\.(\d+)\.(\d+)/)
        if (!match) return [0, 0, 0]
        return [parseInt(match[1]), parseInt(match[2]), parseInt(match[3])]
      }

      const [major1, minor1, patch1] = parseVersion(current)
      const [major2, minor2, patch2] = parseVersion(latest)

      if (major1 !== major2) return major1 - major2
      if (minor1 !== minor2) return minor1 - minor2
      return patch1 - patch2
    }
  }
})

export const useMonitoringStore = defineStore('monitoring', {
  state: () => ({
    snmp: {
      enabled: false,
      port: 161,
      community: 'public',
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
