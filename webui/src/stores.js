import { defineStore } from 'pinia'
import axios from 'axios'
import { safeLocal, safeSession } from './composables/useSafeStorage'

const IDLE_TIMEOUT_MS = 5 * 60 * 1000
const getStoredLastActivity = () => {
  const stored = Number(safeSession.get("hb-rf-eth-ng-last-activity"))
  return Number.isFinite(stored) && stored > 0 ? stored : Date.now()
}

export const useThemeStore = defineStore('theme', {
  state: () => ({
    theme: safeLocal.get('theme') || 'light'
  }),
  actions: {
    setTheme(newTheme) {
      this.theme = newTheme
      safeLocal.set('theme', newTheme)
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
    // Boot value comes from localStorage so the very first paint (before any
    // server round-trip) is correct. Once /settings.json is loaded, the value
    // is reconciled with the server (see syncFromServer) and localStorage
    // becomes a mirror of the authoritative device setting.
    testDesignEnabled: safeLocal.get("hb-rf-eth-ng-test-design") === "1",
    // Wall-clock time (ms) of the last user-initiated flip. Used by
    // syncFromServer to ignore stale server values that arrive while the
    // flip's own persist POST is still in flight (see the race note below).
    // 0 means "no recent local flip — trust the server unconditionally".
    _lastFlipAt: 0
  }),
  actions: {
    applyDesignClass() {
      // Single source of truth for the new-design toggle. The class MUST live
      // on <body> because every new-design CSS rule in styles/main.css
      // targets `body.newdesign-active`. (Previously this wrote to <html>,
      // which matched no rule — the toggle only worked by accident via a
      // duplicate watcher in app.vue, and it caused a pre-paint flash of the
      // old UI on cold boot.) index.html also sets this class inline before
      // first paint to prevent FOUC.
      document.body.classList.toggle('newdesign-active', this.testDesignEnabled)
    },
    setTestDesignEnabled(enabled) {
      const next = !!enabled
      if (next === this.testDesignEnabled) return
      this.testDesignEnabled = next
      this._lastFlipAt = Date.now()
      // Mirror to localStorage so a reload / new tab gets the right value
      // before the server round-trip completes (prevents a one-frame flash of
      // the wrong design on navigation).
      safeLocal.set("hb-rf-eth-ng-test-design", next ? "1" : "0")
      this.applyDesignClass()
      // Persist device-wide immediately. The server accepts a partial payload
      // ({testDesignEnabled: bool}) and saves to NVS, so the choice survives
      // reboots and is shared across every browser that opens the WebUI.
      // silent:true prevents the global axios interceptor from surfacing
      // error toasts (and from force-redirecting to /login on a 401) — a
      // failed experimental-toggle persist must NOT disturb the session.
      // Fire-and-forget: a network failure does NOT roll back the local
      // toggle; the device value stays stale until the next successful save.
      axios.post("/settings.json", { testDesignEnabled: next }, { timeout: 5000, silent: true })
        .catch((e) => console.warn("Failed to persist test-design toggle to device:", e?.message || e))
    },
    // Called by useSettingsStore.load() after /settings.json returns, so the
    // device-wide persisted value wins over the localStorage boot guess.
    //
    // RACE GUARD: flipping the toggle swaps the header component (app.vue
    // switches between <Header> and <NewDesignHeader>), and the newly-mounted
    // header calls settingsStore.load() in its onMounted. That load() can
    // resolve BEFORE this flip's persist POST has been written to NVS — in
    // which case the server still reports the OLD value, and naively applying
    // it would flip the toggle straight back (the exact "spring-back" bug).
    // We therefore ignore any server value that arrives within a short grace
    // window after a local flip; once the window passes, the POST has had
    // enough time to land and the server is authoritative again.
    syncFromServer(serverValue) {
      if (typeof serverValue !== 'boolean') return
      if (serverValue === this.testDesignEnabled) {
        // Converged — clear the flip marker so a later genuine change is
        // honoured immediately.
        this._lastFlipAt = 0
        return
      }
      // Within the post-flip grace window, trust the local user choice over
      // the (likely stale) server value. 4 s comfortably covers the persist
      // POST + ESP32 NVS write + the header's checkForUpdate round-trip that
      // precedes its settingsStore.load().
      if (this._lastFlipAt && (Date.now() - this._lastFlipAt) < 4000) {
        return
      }
      // Outside the window: the server is authoritative. Apply and clear.
      this._lastFlipAt = 0
      this.testDesignEnabled = serverValue
      safeLocal.set("hb-rf-eth-ng-test-design", serverValue ? "1" : "0")
      this.applyDesignClass()
    },
    init() {
      this.applyDesignClass()
    }
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
  }),
  actions: {
    async load() {
      try {
        const response = await axios.get("/settings.json", { timeout: 5000 })
        if (response.data?.settings) {
          const incoming = { ...response.data.settings }
          // The test-design toggle is owned by useExperimentalStore. Pull it
          // out of the incoming settings payload and forward it there as a
          // ONE-TIME server→client sync (never push it back later). Then drop
          // it so it cannot leak into this store's state.
          const serverTestDesign = incoming.testDesignEnabled
          delete incoming.testDesignEnabled
          Object.assign(this.$state, incoming)
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
