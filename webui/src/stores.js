import { defineStore } from 'pinia'
import axios from 'axios'

const IDLE_TIMEOUT_MS = 5 * 60 * 1000
const getStoredLastActivity = () => {
  const stored = Number(sessionStorage.getItem("hb-rf-eth-ng-last-activity"))
  return Number.isFinite(stored) && stored > 0 ? stored : Date.now()
}

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

export const useUiStore = defineStore('ui', {
  state: () => ({
    toasts: [],
    nextToastId: 1
  }),
  actions: {
    pushToast(payload) {
      const toast = {
        id: this.nextToastId++,
        type: payload.type || 'info',
        title: payload.title || '',
        message: payload.message || '',
        duration: payload.duration ?? 3000
      }

      this.toasts.push(toast)

      if (toast.duration > 0) {
        window.setTimeout(() => {
          this.removeToast(toast.id)
        }, toast.duration)
      }

      return toast.id
    },
    removeToast(id) {
      this.toasts = this.toasts.filter((toast) => toast.id !== id)
    }
  }
})

export const useRestartUiStore = defineStore('restartUi', {
  state: () => ({
    visible: false,
    countdown: 30,
    phase: 'restart',
    phaseIndex: 1,
    phaseTotal: 1,
    phaseDuration: 30,
    timer: null
  }),
  actions: {
    setPhase(phase, seconds, index, total) {
      this.phase = phase
      this.countdown = seconds
      this.phaseDuration = seconds
      this.phaseIndex = index
      this.phaseTotal = total
    },
    start(options = {}) {
      const includeFlashPause = !!options.includeFlashPause
      const restartSeconds = options.restartSeconds || 30
      const syncSeconds = options.syncSeconds || 40

      if (this.timer) {
        clearInterval(this.timer)
        this.timer = null
      }

      this.visible = true
      if (includeFlashPause) {
        this.setPhase('sync', syncSeconds, 1, 2)
      } else {
        this.setPhase('restart', restartSeconds, 1, 1)
      }

      this.timer = setInterval(() => {
        this.countdown -= 1
        if (this.countdown <= 0) {
          if (this.phase === 'sync') {
            this.setPhase('restart', restartSeconds, 2, 2)
          } else {
            clearInterval(this.timer)
            this.timer = null
            window.location.reload()
          }
        }
      }, 1000)
    },
    stop() {
      if (this.timer) {
        clearInterval(this.timer)
        this.timer = null
      }
      this.visible = false
    }
  }
})

export const useExperimentalStore = defineStore('experimental', {
  state: () => ({
    testDesignEnabled: localStorage.getItem("hb-rf-eth-ng-test-design") === "1"
  }),
  actions: {
    applyDesignClass() {
      document.documentElement.classList.toggle('newdesign-active', this.testDesignEnabled)
    },
    setTestDesignEnabled(enabled) {
      this.testDesignEnabled = !!enabled
      localStorage.setItem("hb-rf-eth-ng-test-design", this.testDesignEnabled ? "1" : "0")
      this.applyDesignClass()
    },
    init() {
      this.applyDesignClass()
    }
  }
})

export const useLoginStore = defineStore('login', {
  state: () => {
    const token = sessionStorage.getItem("hb-rf-eth-ng-pw") || ""
    const lastActivity = getStoredLastActivity()
    const sessionExpired = token && Date.now() - lastActivity > IDLE_TIMEOUT_MS

    if (sessionExpired) {
      sessionStorage.removeItem("hb-rf-eth-ng-pw")
      sessionStorage.removeItem("hb-rf-eth-ng-last-activity")
    }

    return {
      isLoggedIn: !!token && !sessionExpired,
      token: sessionExpired ? "" : token,
      passwordChanged: true, // Default to true to avoid blocking if unknown
      lastActivity: sessionExpired ? Date.now() : lastActivity
    }
  },
  actions: {
    login(token) {
      sessionStorage.setItem("hb-rf-eth-ng-pw", token)
      this.token = token
      this.isLoggedIn = true
      this.updateActivity()
    },
    logout() {
      this.isLoggedIn = false
      sessionStorage.removeItem("hb-rf-eth-ng-pw")
      sessionStorage.removeItem("hb-rf-eth-ng-last-activity")
      this.token = ""
    },
    setPasswordChanged(status) {
      this.passwordChanged = status
    },
    updateActivity() {
      if (this.isLoggedIn) {
        this.lastActivity = Date.now()
        sessionStorage.setItem("hb-rf-eth-ng-last-activity", this.lastActivity)
      }
    },
    checkActivity() {
      if (this.isLoggedIn) {
        const now = Date.now()
        if (now - this.lastActivity > IDLE_TIMEOUT_MS) {
          this.logout()
          return true
        }
      }
      return false
    },
    async tryLogin(username, password) {
      try {
        const response = await axios.post("/login.json", { username, password }, { timeout: 8000 })
        if (response.data?.isAuthenticated && response.data?.token) {
          this.login(response.data.token)
          this.setPasswordChanged(response.data.passwordChanged !== false)
          return true
        }
        return false
      } catch (error) {
        const status = error.response?.status || error.message
        console.error('Login failed:', status)
        // Don't throw - let caller handle the error via response
        return false
      }
    }
  }
})

export const useSysInfoStore = defineStore('sysInfo', {
  state: () => ({
    serial: "",
    hostname: "",
    currentVersion: "",
    latestVersion: "",
    rawUartRemoteAddress: "",
    memoryUsage: 0.0,
    cpuUsage: 0.0,
    supplyVoltage: null,
    temperature: null,
    uptimeSeconds: 0,
    boardRevision: "",
    boardSenseVoltage: 0,
    resetReason: "",
    ethernetConnected: false,
    ethernetSpeed: 0,
    ethernetDuplex: "",
    localIP: "",
    netmask: "",
    gateway: "",
    dns1: "",
    dns2: "",
    ipv6Addresses: [],
    radioModuleType: "",
    radioModuleSerial: "",
    radioModuleFirmwareVersion: "",
    radioModuleBidCosRadioMAC: "",
    radioModuleHmIPRadioMAC: "",
    radioModuleSGTIN: "",
    // Supporter badge — computed by the firmware from the stored key
    // (see main/supporter_key.cpp). Polled with the rest of /sysinfo.json.
    supporterActive: false,
    supporterValid: false,
    supporterExpired: false,
    supporterRevoked: false,
    supporterExpiresAt: ""
  }),
  actions: {
    async update() {
      try {
        // silent: polled periodically - failures must not flood the UI with
        // connection-error toasts while the device reboots or is offline.
        const response = await axios.get("/sysinfo.json", { timeout: 5000, silent: true })
        if (response.data?.sysInfo) {
          const { supporter, ...rest } = response.data.sysInfo
          Object.assign(this.$state, rest)
          // Flatten the nested supporter object into reactive scalars so
          // components can bind supporterActive / supporterExpiresAt directly.
          if (supporter) {
            this.supporterActive = !!supporter.active
            this.supporterValid = !!supporter.valid
            this.supporterExpired = !!supporter.expired
            this.supporterRevoked = !!supporter.revoked
            this.supporterExpiresAt = supporter.expiresAt || ""
          } else {
            this.supporterActive = false
            this.supporterValid = false
            this.supporterExpired = false
            this.supporterRevoked = false
            this.supporterExpiresAt = ""
          }
        } else {
          throw new Error('Invalid response format: missing sysInfo')
        }
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
    adminUsername: "admin",
    systemLogEnabled: false,
    flashPause: false,
    testDesignEnabled: false,
    supporterKey: "",
    useDHCP: true,
    localIP: "",
    netmask: "",
    gateway: "",
    dns1: "",
    dns2: "",
    ccuIP: "",
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
    ledPrograms: {
      idle: 1,
      ccu_disconnected: 5,
      ccu_connected: 6,
      update_available: 4,
      error: 10,
      booting: 4,
      update_in_progress: 5
    },
  }),
  actions: {
    async load() {
      try {
        const response = await axios.get("/settings.json", { timeout: 5000 })
        if (response.data?.settings) {
          Object.assign(this.$state, response.data.settings)
          if (response.data.settings.testDesignEnabled !== undefined) {
            useExperimentalStore().setTestDesignEnabled(!!response.data.settings.testDesignEnabled)
          }
        } else {
          throw new Error('Invalid response format: missing settings')
        }
      } catch (error) {
        console.error('Failed to load settings:', error.response?.status || error.message)
        throw error
      }
    },
    async save(settings) {
      try {
        const response = await axios.post("/settings.json", settings, { timeout: 10000 })
        if (response.data?.success !== false) {
          Object.assign(this.$state, settings)
          if (settings.testDesignEnabled !== undefined) {
            useExperimentalStore().setTestDesignEnabled(!!settings.testDesignEnabled)
          }
        } else {
          throw new Error('Server rejected settings save')
        }
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
    async update(file, options = {}) {
      try {
        this.progress = 0

        const headers = {
          'Content-Type': 'application/octet-stream'
        }

        const externalProgressCb = options.onUploadProgress

        await axios.post("/ota_update", file, {
          headers,
          // Flash-write throttling makes a ~1.5 MB upload take well over the
          // global 10 s default timeout - allow up to 10 minutes. (Must be a
          // non-zero value: the request interceptor replaces falsy timeouts
          // with the 10 s default.)
          timeout: 600000,
          onUploadProgress: event => {
            if (event.lengthComputable) {
              this.progress = Math.ceil((event.loaded || event.position) / event.total * 100)
            }
            if (externalProgressCb) {
              externalProgressCb(event)
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
    isPrerelease: false,
    // Markdown release-notes excerpt straight from the GitHub release body.
    releaseNotes: '',
    // html_url of the release (link to "view on GitHub").
    releaseUrl: '',
    // browser_download_url of the firmware_*.bin asset. Empty when no asset
    // is attached (should not happen for normal releases).
    downloadUrl: '',
    sha256: '',
    publishedAt: '',
    // Whether the device is currently configured to consider pre-releases.
    betaChannel: false,
    // True while a refresh is in flight on the device.
    fetchInProgress: false,
    lastCheck: null,
    checkError: null
  }),
  getters: {
    shouldShowUpdateBadge: (state) => {
      return state.updateAvailable && state.latestVersion !== null
    }
  },
  actions: {
    // Refreshes the cached release info from the device. By default a POST
    // triggers an immediate static-manifest fetch on the device. Pass
    // { cached: true } to only read the cached state.
    async checkForUpdate(currentVersion, options = {}) {
      if (this.isChecking) return
      const cached = options && options.cached === true

      this.isChecking = true
      this.checkError = null

      try {
        // Worst case on the device: up to 15 s waiting for g_net_fetch_mutex
        // (another TLS fetch already running) plus up to 10 s for the manifest
        // request itself - give the client enough headroom to not time out
        // while the device is still legitimately working.
        const config = cached
          ? { params: { t: Date.now() } }
          : { timeout: 30000 }
        const response = cached
          ? await axios.get('/api/check_update', config)
          : await axios.post('/api/check_update', {}, config)

        const data = response.data || {}
        this.latestVersion = (data.latestVersion || 'n/a').toString()
        this.updateAvailable = !!data.updateAvailable
        this.isPrerelease = !!data.isPrerelease
        this.releaseNotes = data.releaseNotes || ''
        this.releaseUrl = data.releaseUrl || ''
        this.downloadUrl = data.downloadUrl || ''
        this.sha256 = data.sha256 || ''
        this.publishedAt = data.publishedAt || ''
        this.betaChannel = !!data.betaChannel
        this.fetchInProgress = !!data.fetchInProgress
        this.lastCheck = new Date().toISOString()

        if (data.error) {
          this.checkError = data.error
        }

        // Sanity check: trust the device's comparison, but fall back to a
        // local comparison if it omitted the flag.
        if (!data.updateAvailable && currentVersion && this.latestVersion !== 'n/a') {
          this.updateAvailable = this.compareVersions(currentVersion, this.latestVersion) < 0
        }
      } catch (error) {
        console.error('Update check failed:', error)
        this.checkError = error.response?.data?.error || error.message || 'Unknown error'
      } finally {
        this.isChecking = false
      }
    },

    compareVersions(current, latest) {
      const parseVersion = (v) => {
        const match = v.match(/(\d+)\.(\d+)\.(\d+)(?:-(.+))?/)
        if (!match) return { parts: [0, 0, 0], pre: null }
        const pre = match[4] || null
        return { parts: [parseInt(match[1]), parseInt(match[2]), parseInt(match[3])], pre }
      }

      const cur = parseVersion(current)
      const lat = parseVersion(latest)

      for (let i = 0; i < 3; i++) {
        if (cur.parts[i] !== lat.parts[i]) return cur.parts[i] - lat.parts[i]
      }

      if (cur.pre === null && lat.pre === null) return 0
      if (cur.pre === null) return 1
      if (lat.pre === null) return -1

      // Compare pre-release identifiers per semver: split on '.', compare each
      // segment numerically when both are numeric, otherwise lexically. This
      // fixes multi-digit suffixes (e.g. "Beta.3" vs "Beta.10"), which plain
      // localeCompare gets wrong ('3' > '1' character-wise).
      const ap = cur.pre.split('.')
      const bp = lat.pre.split('.')
      const n = Math.max(ap.length, bp.length)
      for (let i = 0; i < n; i++) {
        if (ap[i] === undefined) return -1
        if (bp[i] === undefined) return 1
        if (ap[i] === bp[i]) continue
        const an = /^\d+$/.test(ap[i]) ? parseInt(ap[i], 10) : NaN
        const bn = /^\d+$/.test(bp[i]) ? parseInt(bp[i], 10) : NaN
        if (!isNaN(an) && !isNaN(bn)) return an - bn
        if (!isNaN(an)) return -1 // numeric identifiers have lower precedence
        if (!isNaN(bn)) return 1
        return ap[i] < bp[i] ? -1 : 1
      }
      return 0
    }
  }
})

export const useMonitoringStore = defineStore('monitoring', {
  state: () => ({
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
      topicPrefix: 'hb-rf-eth-ng',
      haDiscoveryEnabled: false,
      haDiscoveryPrefix: 'homeassistant',
      tlsEnable: false,
      tlsSkipVerify: false,
      tlsCaCerts: '',
      tlsCertfile: '',
      tlsKeyfile: '',
      tlsCaCertsSet: false,
      tlsCertfileSet: false,
      tlsKeyfileSet: false,
      // Phase A: command-topic security
      commandEnabled: true,
      commandToken: '',
      commandTokenSet: false
    },
    prometheus: {
      enabled: false,
      port: 9100,
      allowedHosts: '*'
    },
    syslog: {
      enabled: false,
      server: '',
      port: 514,
      transport: 0,
      minSeverity: 6,
      hostname: ''
    },
    notify: {
      enabled: false,
      channels: 0,
      webhookUrl: '',
      webhookSecret: '',
      webhookSecretSet: false,
      telegramToken: '',
      telegramTokenSet: false,
      telegramChatId: '',
      smtpServer: '',
      smtpPort: 587,
      smtpTls: 1,
      smtpUser: '',
      smtpPassword: '',
      smtpPasswordSet: false,
      smtpFrom: '',
      smtpTo: '',
      cooldownSeconds: 300
    },
    diagnostics: {
      checkmk: null,
      mqtt: null,
      prometheus: null,
      syslog: null,
      notify: null
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
        // Do NOT Object.assign the payload into $state here:
        // 1) The mqtt payload strips the *Set sentinel flags and adds *Clear
        //    flags — replacing the whole mqtt object would lose them.
        // 2) The backend applies the config asynchronously, so an immediate
        //    GET would return stale data. The caller is responsible for
        //    updating *Set flags / clearing sensitive text fields after save.
      } catch (error) {
        console.error('Failed to save monitoring config:', error.response?.status || error.message)
        throw error
      }
    },
    async test(target) {
      try {
        const response = await axios.get('/api/monitoring/test', { params: { target } })
        this.diagnostics[target] = response.data
        return response.data
      } catch (error) {
        console.error('Failed to test monitoring target:', error.response?.status || error.message)
        throw error
      }
    }
  }
})
