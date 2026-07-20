import { defineStore } from 'pinia'
import axios from 'axios'
import { safeLocal, safeSession } from './composables/useSafeStorage'

const IDLE_TIMEOUT_MS = 5 * 60 * 1000
const getStoredLastActivity = () => {
  const stored = Number(safeSession.get("hb-rf-eth-ng-last-activity"))
  return Number.isFinite(stored) && stored > 0 ? stored : Date.now()
}

const DEFAULT_PRIMARY_COLOR = '#f26a3d'
const isHexColor = (value) => /^#[0-9a-f]{6}$/i.test(value || '')
const shiftColor = (hex, amount) => {
  const value = parseInt(hex.slice(1), 16)
  const clamp = (part) => Math.max(0, Math.min(255, part + amount))
  const red = clamp((value >> 16) & 0xff)
  const green = clamp((value >> 8) & 0xff)
  const blue = clamp(value & 0xff)
  return `#${[red, green, blue].map((part) => part.toString(16).padStart(2, '0')).join('')}`
}
const rgbaColor = (hex, alpha) => {
  const value = parseInt(hex.slice(1), 16)
  return `rgba(${(value >> 16) & 0xff}, ${(value >> 8) & 0xff}, ${value & 0xff}, ${alpha})`
}

export const useThemeStore = defineStore('theme', {
  state: () => ({
    theme: safeLocal.get('theme') || 'system',
    primaryColor: isHexColor(safeLocal.get('primaryColor'))
      ? safeLocal.get('primaryColor')
      : DEFAULT_PRIMARY_COLOR,
    loadedFromDevice: false
  }),
  actions: {
    apply() {
      const prefersDark = window.matchMedia?.('(prefers-color-scheme: dark)').matches
      const resolved = this.theme === 'system'
        ? (prefersDark ? 'dark' : 'light')
        : this.theme
      document.documentElement.setAttribute('data-bs-theme', resolved)

      const color = isHexColor(this.primaryColor) ? this.primaryColor : DEFAULT_PRIMARY_COLOR
      const root = document.documentElement.style
      root.setProperty('--color-primary', color)
      root.setProperty('--color-primary-hover', shiftColor(color, -20))
      root.setProperty('--color-primary-strong', shiftColor(color, -42))
      root.setProperty('--color-primary-dark', shiftColor(color, -42))
      root.setProperty('--color-primary-soft', rgbaColor(color, resolved === 'dark' ? 0.16 : 0.12))
      root.setProperty('--color-primary-light', rgbaColor(color, resolved === 'dark' ? 0.16 : 0.12))
    },
    persist() {
      axios.post('/api/theme', {
        colorScheme: this.theme,
        primaryColor: this.primaryColor
      }, { timeout: 5000, silent: true }).catch(() => {})
    },
    setTheme(newTheme, persist = true) {
      if (!['system', 'light', 'dark'].includes(newTheme)) return
      this.theme = newTheme
      safeLocal.set('theme', newTheme)
      this.apply()
      if (persist) this.persist()
    },
    setPrimaryColor(newColor, persist = true) {
      if (!isHexColor(newColor)) return
      this.primaryColor = newColor.toLowerCase()
      safeLocal.set('primaryColor', this.primaryColor)
      this.apply()
      if (persist) this.persist()
    },
    toggleTheme() {
      const currentlyDark = document.documentElement.getAttribute('data-bs-theme') === 'dark'
      this.setTheme(currentlyDark ? 'light' : 'dark')
    },
    init() {
      this.apply()
      window.matchMedia?.('(prefers-color-scheme: dark)').addEventListener?.('change', () => {
        if (this.theme === 'system') this.apply()
      })
      axios.get('/api/theme', { timeout: 5000, silent: true })
        .then((response) => {
          const scheme = response.data?.colorScheme
          const color = response.data?.primaryColor
          if (['system', 'light', 'dark'].includes(scheme)) this.theme = scheme
          if (isHexColor(color)) this.primaryColor = color.toLowerCase()
          safeLocal.set('theme', this.theme)
          safeLocal.set('primaryColor', this.primaryColor)
          this.loadedFromDevice = true
          this.apply()
        })
        .catch(() => {})
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
  state: () => ({ testDesignEnabled: true }),
  actions: {
    applyDesignClass() { document.body.classList.add('newdesign-active') },
    setTestDesignEnabled() { this.testDesignEnabled = true; this.applyDesignClass() },
    syncFromServer() { this.testDesignEnabled = true; this.applyDesignClass() },
    init() { this.applyDesignClass() }
  }
})

export const useLoginStore = defineStore('login', {
  state: () => {
    const token = safeSession.get("hb-rf-eth-ng-pw") || ""
    const lastActivity = getStoredLastActivity()
    const sessionExpired = token && Date.now() - lastActivity > IDLE_TIMEOUT_MS

    if (sessionExpired) {
      safeSession.remove("hb-rf-eth-ng-pw")
      safeSession.remove("hb-rf-eth-ng-last-activity")
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
      // Persist FIRST. In iOS Safari private-browsing mode setItem throws
      // QuotaExceededError; if that propagates out of an already-accepted
      // server login the user is bounced back to the login screen despite
      // correct credentials. Set in-memory state regardless of persistence
      // success so the session stays valid for the lifetime of the tab.
      safeSession.set("hb-rf-eth-ng-pw", token)
      this.token = token
      this.isLoggedIn = true
      this.updateActivity()
    },
    logout() {
      this.isLoggedIn = false
      safeSession.remove("hb-rf-eth-ng-pw")
      safeSession.remove("hb-rf-eth-ng-last-activity")
      this.token = ""
    },
    setPasswordChanged(status) {
      this.passwordChanged = status
    },
    updateActivity() {
      if (this.isLoggedIn) {
        this.lastActivity = Date.now()
        safeSession.set("hb-rf-eth-ng-last-activity", this.lastActivity)
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
      let serverAccepted = false
      let token = null
      let passwordChanged = true
      try {
        const response = await axios.post("/login.json", { username, password }, { timeout: 8000 })
        if (response.data?.isAuthenticated && response.data?.token) {
          serverAccepted = true
          token = response.data.token
          passwordChanged = response.data.passwordChanged !== false
        }
      } catch (error) {
        const status = error.response?.status || error.message
        console.error('Login failed:', status)
        // Don't throw - let caller handle the error via response
        return false
      }

      if (serverAccepted) {
        // login() never throws now (storage writes are guarded), but keep the
        // outer guard so any future regression can't turn an accepted login
        // into a reported failure.
        try {
          this.login(token)
          this.setPasswordChanged(passwordChanged)
        } catch (e) {
          console.error('login() state update failed:', e)
        }
        return true
      }
      return false
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
        const response = await axios.get(`/sysinfo.json?t=${Date.now()}`, { timeout: 5000, silent: true })
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
    // NOTE: testDesignEnabled is intentionally NOT in this store. It lives in
    // useExperimentalStore (single source of truth) and is persisted device-
    // wide via that store's setTestDesignEnabled(). Keeping it out of here
    // prevents the "toggle springs back" bug where a settings reload pushed
    // the server value back into the experimental store on every watcher fire.
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
    // Tracks whether the device settings have been fetched at least once. Shell
    // components only need an initialized snapshot; settings.vue may still call
    // load() explicitly when the user opens the settings page.
    hasLoaded: false,
    // Internal: in-flight load() promise to dedupe concurrent callers.
    _loadInFlight: null,
  }),
  actions: {
    async ensureLoaded() {
      if (this.hasLoaded) return
      return this.load()
    },
    // The in-flight promise deduplicates concurrent callers. Once a request has
    // completed, shell components use ensureLoaded() so a delayed header mount
    // cannot overwrite edits already being made on the settings page.
    async load() {
      if (this._loadInFlight) return this._loadInFlight
      this._loadInFlight = (async () => {
        try {
          const response = await axios.get(`/settings.json?t=${Date.now()}`, { timeout: 5000 })
          if (response.data?.settings) {
            const incoming = { ...response.data.settings }
            // The test-design toggle is owned by useExperimentalStore. Pull it
            // out of the incoming settings payload and forward it there as a
            // ONE-TIME server→client sync (never push it back later). Then drop
            // it so it cannot leak into this store's state.
            const serverTestDesign = incoming.testDesignEnabled
            delete incoming.testDesignEnabled
            Object.assign(this.$state, incoming)
            this.hasLoaded = true
            if (serverTestDesign !== undefined) {
              useExperimentalStore().syncFromServer(!!serverTestDesign)
            }
          } else {
            throw new Error('Invalid response format: missing settings')
          }
        } catch (error) {
          console.error('Failed to load settings:', error.response?.status || error.message)
          throw error
        }
      })()
      try {
        return await this._loadInFlight
      } finally {
        this._loadInFlight = null
      }
    },
    async save(settings) {
      try {
        const response = await axios.post("/settings.json", settings, { timeout: 10000 })
        if (response.data?.success !== false) {
          // testDesignEnabled is not part of this store — drop it before
          // assigning so it cannot sneak into state. The experimental store
          // persists the toggle on its own (immediate POST on flip).
          const safe = { ...settings }
          delete safe.testDesignEnabled
          Object.assign(this.$state, safe)
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
    // Reads the cached release info from the device. The device refreshes its
    // release snapshot automatically every 24 h (UpdateCheck esp_timer); there
    // is no longer a manual "check now" trigger.
    async checkForUpdate(currentVersion) {
      if (this.isChecking) return

      this.isChecking = true
      this.checkError = null

      try {
        const response = await axios.get('/api/check_update', {
          params: { t: Date.now() },
        })

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
        this.lastCheck = data.fetchedAt ? new Date(Number(data.fetchedAt)).toISOString() : null

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
