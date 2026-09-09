import { computed, ref } from 'vue'
import axios from 'axios'
import { safeLocal } from './useSafeStorage'

// Shared state and behaviour of the manual update search.
//
// Both update pages show the same search: the firmware page reports the
// firmware result, the WebUI page the WebUI one, and one device-side search
// answers both. Keeping the state at module scope means a search started on
// one page is already visible when the user switches to the other, and keeping
// the logic in one place means a defect here is fixed once rather than twice.
//
// The device only searches when asked - there is no polling loop here that
// would start one on its own.

const CHANNEL_STORAGE_KEY = 'hb-rf-eth-ng-update-channel'
const POLL_INTERVAL_MS = 1500
const POLL_TIMEOUT_MS = 45000

const emptyStatus = () => ({
  state: 'idle',
  everChecked: false,
  channel: 'stable',
  lastCheck: 0,
  cooldownRemainingSec: 0,
  runningFirmware: '',
  runningWebui: '',
  latestFirmware: '',
  latestWebui: '',
  firmwareUpdateAvailable: false,
  webuiUpdateAvailable: false,
  notesUrl: '',
  lastError: '',
  lastSkipReason: ''
})

// Skip reasons arrive as raw firmware strings. The known ones are mapped to
// localized texts so a German interface does not quote English mid-sentence;
// an unknown reason (newer firmware) is shown verbatim rather than hidden.
const MEMORY_SKIP = /^Too little free memory \(free=(\d+) KB, largest block=(\d+) KB\)$/
const KNOWN_SKIPS = {
  'An update installation is currently running': 'firmware.skipInstallRunning',
  'Network subsystem busy, please try again': 'firmware.skipNetworkBusy',
  'Not enough memory for the manifest buffer': 'firmware.skipManifestBuffer'
}
const localizeSkipReason = (reason, t) => {
  const memory = MEMORY_SKIP.exec(reason)
  if (memory) return t('firmware.skipLowMemory', { free: memory[1], largest: memory[2] })
  return KNOWN_SKIPS[reason] ? t(KNOWN_SKIPS[reason]) : reason
}

const channel = ref(safeLocal.get(CHANNEL_STORAGE_KEY) === 'beta' ? 'beta' : 'stable')
const checking = ref(false)
const updateStatus = ref(emptyStatus())
const triggerOutcome = ref('')
const cooldownRemaining = ref(0)
let pollTimer = null
let cooldownTimer = null

const setCooldown = seconds => {
  const value = Math.max(0, Math.ceil(Number(seconds) || 0))
  cooldownRemaining.value = value
  if (cooldownTimer) {
    clearInterval(cooldownTimer)
    cooldownTimer = null
  }
  if (value <= 0) return
  cooldownTimer = setInterval(() => {
    cooldownRemaining.value -= 1
    if (cooldownRemaining.value <= 0) {
      clearInterval(cooldownTimer)
      cooldownTimer = null
    }
  }, 1000)
}

const stopPolling = () => {
  if (pollTimer) {
    clearTimeout(pollTimer)
    pollTimer = null
  }
}

const loadUpdateStatus = async () => {
  const response = await axios.get(`/api/update/status?t=${Date.now()}`, {
    timeout: 8000,
    silent: true
  })
  if (response.data) {
    updateStatus.value = response.data
    // Entering the page while a cooldown is active (firmware that reports it):
    // adopt the device's remaining seconds instead of guessing.
    if (response.data.cooldownRemainingSec > 0 && cooldownRemaining.value === 0) {
      setCooldown(response.data.cooldownRemainingSec)
    }
  }
  return response.data
}

const checkForUpdates = async () => {
  if (checking.value) return
  checking.value = true
  triggerOutcome.value = ''
  safeLocal.set(CHANNEL_STORAGE_KEY, channel.value)

  try {
    const response = await axios.post(
      '/api/update/check',
      { channel: channel.value },
      { timeout: 8000, silent: true }
    )
    const outcome = response.data?.outcome || 'accepted'
    if (outcome !== 'accepted') {
      // Refused before any network activity: no result is coming, so there is
      // nothing to poll for. The cooldown outcome carries the device's exact
      // remaining seconds; older firmware has no field and gets the full
      // window as the safe upper bound.
      if (outcome === 'cooldown') {
        setCooldown(response.data?.cooldownRemainingSec ?? 60)
      }
      triggerOutcome.value = outcome
      await loadUpdateStatus().catch(() => {})
      checking.value = false
      return
    }
    // An accepted attempt (including one that later skips) starts the window
    // on the device right away.
    setCooldown(60)
  } catch {
    triggerOutcome.value = 'unavailable'
    checking.value = false
    return
  }

  // The device answers immediately and does the work on its own task, so the
  // outcome is polled rather than awaited on the request.
  const deadline = Date.now() + POLL_TIMEOUT_MS
  const poll = async () => {
    let status = null
    try {
      status = await loadUpdateStatus()
    } catch {
      // A transient read failure is not a failed search; keep polling until the
      // deadline and let the device's own state have the last word.
    }
    if ((status && status.state !== 'running') || Date.now() >= deadline) {
      checking.value = false
      return
    }
    pollTimer = setTimeout(poll, POLL_INTERVAL_MS)
  }
  pollTimer = setTimeout(poll, POLL_INTERVAL_MS)
}

const formatTimestamp = seconds => {
  const value = Number(seconds) || 0
  if (value <= 0) return '—'
  return new Date(value * 1000).toLocaleString()
}

export function useUpdateSearch(t) {
  // Why the press did not start a search. Kept apart from the result so a
  // cooldown never hides the finding the user is looking at.
  const triggerMessage = computed(() => {
    switch (triggerOutcome.value) {
      case 'cooldown':
        return cooldownRemaining.value > 0
          ? { variant: 'info', text: t('firmware.checkCooldownRetry', { seconds: cooldownRemaining.value }) }
          : { variant: 'info', text: t('firmware.checkCooldown') }
      case 'busy': return { variant: 'info', text: t('firmware.checkBusy') }
      case 'unavailable': return { variant: 'warning', text: t('firmware.checkUnavailable') }
      default: return { variant: '', text: '' }
    }
  })

  // A skip and a failure are shown as themselves and never collapse into the
  // reassuring "everything is current" case. `describe` supplies the
  // page-specific wording for the two success cases.
  const resultMessage = describe => computed(() => {
    const status = updateStatus.value
    if (status.lastSkipReason) {
      return { variant: 'warning', text: t('firmware.checkSkipped', { reason: localizeSkipReason(status.lastSkipReason, t) }) }
    }
    if (status.lastError) {
      return { variant: 'danger', text: t('firmware.checkFailed', { reason: status.lastError }) }
    }
    if (!status.everChecked) return { variant: '', text: '' }
    return describe(status)
  })

  return {
    channel,
    checking,
    updateStatus,
    triggerOutcome,
    triggerMessage,
    resultMessage,
    cooldownRemaining,
    checkForUpdates,
    loadUpdateStatus,
    stopPolling,
    formatTimestamp
  }
}
