<template>
  <div class="container-fluid">
    <BCard class="mb-3 analyzer-card">
      <div class="analyzer-hero">
        <div>
          <div class="eyebrow">{{ t('nav.analyzer') }}</div>
          <h3 class="mb-1 fw-bold">{{ t('analyzer.title') }}</h3>
          <p class="text-muted mb-0">
            {{ t('analyzer.description') || 'Realtime insights for radio frames with live status' }}
          </p>
        </div>
        <div class="status-chip" :class="isConnected ? 'status-chip--on' : 'status-chip--off'">
          <span class="status-dot"></span>
          {{ isConnected ? t('analyzer.connected') : t('analyzer.disconnected') }}
        </div>
      </div>

      <div class="d-flex flex-wrap align-items-center justify-content-between mb-3 gap-2">
        <div class="d-flex flex-wrap gap-2">
          <BButton variant="secondary" @click="clear">
            {{ t('analyzer.clear') }}
          </BButton>
          <BButton variant="outline-primary" size="sm" @click="showNamesModal = true">
            {{ t('analyzer.deviceNames') }}
          </BButton>
        </div>
        <div class="d-flex align-items-center gap-3 flex-wrap">
          <div class="form-switch-label d-flex align-items-center gap-2">
            <BFormCheckbox v-model="autoScroll" switch inline>
              {{ t('analyzer.autoScroll') }}
            </BFormCheckbox>
          </div>
        </div>
      </div>

      <div class="row g-3 mb-3">
        <div class="col-12 col-md-4">
          <div class="stat-tile">
            <div class="label">{{ t('common.total') || 'Total Frames' }}</div>
            <div class="value">{{ totalFrames }}</div>
            <div class="hint">{{ t('common.updated') || 'Live updated' }}</div>
          </div>
        </div>
        <div class="col-12 col-md-4">
          <div class="stat-tile">
            <div class="label">{{ t('analyzer.lastFrame') || 'Letzte Nachricht' }}</div>
            <div class="value">{{ lastTimestamp }}</div>
            <div class="hint">{{ t('common.timestamp') || 'Zeitstempel' }}</div>
          </div>
        </div>
        <div class="col-12 col-md-4">
          <div class="stat-tile">
            <div class="label">{{ t('analyzer.signal') || 'Signal' }}</div>
            <div class="value d-flex align-items-center gap-2">
              <span class="badge" :class="rssiBadgeClass">{{ lastRssiLabel }}</span>
            </div>
            <div class="hint">{{ t('common.latest') || 'Zuletzt empfangen' }}</div>
          </div>
        </div>
      </div>

      <div class="table-responsive">
        <table class="table table-sm table-striped table-hover font-monospace analyzer-table">
          <thead>
            <tr>
              <th>#</th>
              <th>{{ t('analyzer.time') }}</th>
              <th>{{ t('analyzer.rssi') }}</th>
              <th>{{ t('analyzer.len') }}</th>
              <th>{{ t('analyzer.cnt') }}</th>
              <th>{{ t('analyzer.type') }}</th>
              <th>{{ t('analyzer.src') }}</th>
              <th>{{ t('analyzer.dst') }}</th>
              <th>{{ t('analyzer.payload') }}</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="frame in frames" :key="frame.id" :class="frame.rowClass">
              <td>{{ frame.id }}</td>
              <td>{{ frame.formattedTime }}</td>
              <td>
                <span v-if="frame.rssi !== undefined" :class="frame.rssiClass">
                  {{ frame.rssi }} dBm
                </span>
              </td>
              <td>{{ frame.len }}</td>
              <td>{{ frame.cnt }}</td>
              <td>
                <span :title="frame.rawType">
                  {{ frame.type }}
                </span>
              </td>
              <td @click="editName(frame.srcRaw)" style="cursor: pointer" :title="frame.srcRaw">
                {{ frame.src }}
              </td>
              <td @click="editName(frame.dstRaw)" style="cursor: pointer" :title="frame.dstRaw">
                {{ frame.dst }}
              </td>
              <td class="text-break">{{ frame.payload }}</td>
            </tr>
          </tbody>
        </table>
      </div>
    </BCard>

    <!-- Device Names Modal -->
    <BModal v-model="showNamesModal" :title="t('analyzer.deviceNames')" hide-footer>
      <BForm @submit.prevent="saveName">
        <BFormGroup :label="t('analyzer.address')" label-for="name-address">
          <BFormInput id="name-address" v-model="editingAddress" required readonly />
        </BFormGroup>
        <BFormGroup :label="t('analyzer.name')" label-for="name-value">
          <BFormInput id="name-value" v-model="editingName" />
        </BFormGroup>
        <div class="mt-3 text-end">
           <BButton variant="secondary" @click="showNamesModal = false" class="me-2">{{ t('common.close') }}</BButton>
           <BButton type="submit" variant="primary">{{ t('common.save') }}</BButton>
        </div>
      </BForm>
      <hr />
      <h6>{{ t('analyzer.storedNames') }}</h6>
      <div style="max-height: 200px; overflow-y: auto;">
        <table class="table table-sm">
          <tbody>
            <tr v-for="(name, addr) in deviceNames" :key="addr">
              <td>{{ addr }}</td>
              <td>{{ name }}</td>
              <td class="text-end">
                <BButton size="sm" variant="outline-danger" @click="deleteName(addr)">X</BButton>
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </BModal>
  </div>
</template>

<script setup>
import { ref, shallowRef, triggerRef, reactive, onMounted, onUnmounted, nextTick, computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { useLoginStore, useSettingsStore } from './stores.js'
import { useRouter } from 'vue-router'

const { t } = useI18n()
const loginStore = useLoginStore()
const settingsStore = useSettingsStore()
const router = useRouter()

// OPTIMIZATION: Use shallowRef for the frames list to prevent deep reactivity overhead.
// The list can contain thousands of complex objects, and we never modify deep properties reactively.
// We only push to the array or clear it. This significantly reduces proxy creation cost and memory usage.
const frames = shallowRef([])
const isConnected = ref(false)
const autoScroll = ref(true)
let ws = null
let frameCounter = 0
const incomingBuffer = []
let flushPending = false

// Device Names
const showNamesModal = ref(false)
const deviceNames = reactive({})
const editingAddress = ref('')
const editingName = ref('')

// Load names
const loadNames = () => {
  try {
    const stored = localStorage.getItem('device_names')
    if (stored) {
      Object.assign(deviceNames, JSON.parse(stored))
    }
  } catch (e) { console.error(e) }
}

const saveName = () => {
  if (editingAddress.value) {
    if (editingName.value.trim()) {
      deviceNames[editingAddress.value] = editingName.value.trim()
    } else {
      delete deviceNames[editingAddress.value]
    }
    localStorage.setItem('device_names', JSON.stringify(deviceNames))
    // Refresh frames view? Vue reactive should handle it if we use a getter or method
    // But frames array contains processed strings. We might need to re-process or just use a method in template.
    // In template: {{ frame.src }} is static. We should use a method or make frames reactive to names.
    // For performance, better to resolve name at render time or update existing frames.
    // Let's update existing frames
    updateFrameNames()
    showNamesModal.value = false
  }
}

const deleteName = (addr) => {
  delete deviceNames[addr]
  localStorage.setItem('device_names', JSON.stringify(deviceNames))
  updateFrameNames()
}

const editName = (addr) => {
  if (!addr) return
  editingAddress.value = addr
  editingName.value = deviceNames[addr] || ''
  showNamesModal.value = true
}

const updateFrameNames = () => {
  frames.value.forEach(f => {
    f.src = deviceNames[f.srcRaw] || f.srcRaw
    f.dst = deviceNames[f.dstRaw] || f.dstRaw
  })
  triggerRef(frames)
}

// Frame Types (BidCoS)
const FRAME_TYPES = {
  '00': 'Device Info',
  '01': 'Config',
  '02': 'Ack',
  '03': 'Aes Ack',
  '04': 'Aes Request',
  '10': 'Info',
  '11': 'Climate Event',
  '12': 'Weather Event',
  '3E': 'Power Event',
  '40': 'Remote Event',
  '41': 'Sensor Event',
  '53': 'Switch Event',
  '58': 'Thermal Event',
  '70': 'Weather Event',
}

const connect = () => {
  const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
  const url = `${protocol}//${window.location.host}/api/analyzer/ws`

  ws = new WebSocket(url)

  ws.onopen = () => {
    isConnected.value = true
    // Send initial message to register client
    ws.send('connected')
  }

  ws.onclose = () => {
    isConnected.value = false
    setTimeout(() => {
        if (ws) connect()
    }, 5000)
  }

  ws.onmessage = (event) => {
    try {
      const msg = JSON.parse(event.data)
      addFrame(msg)
    } catch (e) {
      console.error('Failed to parse frame', e)
    }
  }
}

const disconnect = () => {
  if (ws) {
    ws.onclose = null
    ws.close()
    ws = null
  }
  isConnected.value = false
}

const addFrame = (rawFrame) => {
  let data = rawFrame.data
  let rssi = undefined

  // Parse CoPro Frame
  // Format: FD <LenH> <LenL> <Dst> <Cnt> <Cmd> <Payload...> <CrcH> <CrcL>
  // Min length: 8 bytes (FD 00 00 Dst Cnt Cmd CrcH CrcL)

  if (data.startsWith('FD')) {
     // Check Length
     if (data.length < 16) return // Too short

     // Strip Header (6 bytes = 12 chars)
     // Dst: data[6..8], Cnt: data[8..10], Cmd: data[10..12]
     const cmd = parseInt(data.substring(10, 12), 16)

     // Strip Footer (2 bytes = 4 chars)
     const payloadHex = data.substring(12, data.length - 4)

     if (cmd === 0x04) { // Packet Received
        // Payload: <RSSI> <Frame>
        // RSSI is 1 byte
        const rssiHex = payloadHex.substring(0, 2)
        const frameHex = payloadHex.substring(2)

        rssi = parseInt(rssiHex, 16)
        if (rssi >= 128) rssi -= 256 // Signed byte

        data = frameHex
     } else {
        // Other events (TX complete, etc) - skip
        return
     }
  }

  // Process BidCos frame (either unwrapped from CoPro or raw)
  processBidCosFrame(rawFrame.ts, data, rssi)
}

const processBidCosFrame = (ts, data, rssi) => {
  if (data.length < 2) return

  // Basic BidCos parsing: Len(1), Cnt(1), Type(1), Src(3), Dst(3), Payload...
  let len = parseInt(data.substring(0, 2), 16)
  // Validate length
  if (data.length / 2 < len + 1) return // Incomplete

  let cnt = parseInt(data.substring(2, 4), 16)
  let typeHex = data.substring(4, 6)
  let srcRaw = data.substring(6, 12)
  let dstRaw = data.substring(12, 18)
  let payload = data.substring(18)

  let typeName = FRAME_TYPES[typeHex] || `Unknown (${typeHex})`

  frameCounter++

  // Pre-calculate display values to avoid re-calculation in render loop
  // Format Time
  const sec = Math.floor(ts / 1000)
  const ms = ts % 1000
  const formattedTime = `${sec}.${ms.toString().padStart(3, '0')}`

  // Row Class
  let rowClass = ''
  if (typeHex === '02') rowClass = 'table-success' // Ack
  else if (typeHex === '10') rowClass = 'table-info' // Info

  // RSSI Class
  let rssiClass = 'text-danger'
  if (rssi !== undefined) {
      if (rssi > -50) rssiClass = 'text-success'
      else if (rssi > -70) rssiClass = 'text-primary'
      else if (rssi > -90) rssiClass = 'text-warning'
  }

  // OPTIMIZATION: Buffer frames and flush in batches using requestAnimationFrame
  // This prevents layout thrashing when multiple frames arrive in a short burst
  incomingBuffer.push({
    id: frameCounter,
    ts: ts,
    formattedTime: formattedTime,
    rssi: rssi,
    rssiClass: rssiClass,
    len: len,
    cnt: cnt.toString(16).toUpperCase().padStart(2, '0'),
    type: typeName,
    rawType: typeHex,
    rowClass: rowClass,
    srcRaw: srcRaw,
    dstRaw: dstRaw,
    src: deviceNames[srcRaw] || srcRaw,
    dst: deviceNames[dstRaw] || dstRaw,
    payload: payload
  })

  if (!flushPending) {
    flushPending = true
    requestAnimationFrame(flushQueue)
  }
}

const flushQueue = () => {
  flushPending = false
  if (incomingBuffer.length === 0) return

  // Flush all buffered frames to the reactive array in one operation
  const chunk = incomingBuffer.splice(0)
  frames.value.push(...chunk)

  // Maintain max size efficiently (use splice to avoid replacing array and optimize Vue reactivity)
  const overflow = frames.value.length - 200
  if (overflow > 0) {
    frames.value.splice(0, overflow)
  }

  // Trigger update manually since we are using shallowRef
  triggerRef(frames)

  if (autoScroll.value) {
    nextTick(() => {
      window.scrollTo(0, document.body.scrollHeight)
    })
  }
}

const clear = () => {
  frames.value = []
}

const totalFrames = computed(() => frames.value.length)
const lastFrame = computed(() => frames.value.length ? frames.value[frames.value.length - 1] : null)
const lastTimestamp = computed(() => lastFrame.value ? lastFrame.value.formattedTime : '–')
const lastRssiLabel = computed(() => {
  if (!lastFrame.value || lastFrame.value.rssi === undefined) return '–'
  return `${lastFrame.value.rssi} dBm`
})
const rssiBadgeClass = computed(() => {
  if (!lastFrame.value || lastFrame.value.rssi === undefined) return 'bg-secondary'
  const rssi = lastFrame.value.rssi
  if (rssi > -50) return 'bg-success'
  if (rssi > -70) return 'bg-primary'
  if (rssi > -90) return 'bg-warning text-dark'
  return 'bg-danger'
})

onMounted(async () => {
  await settingsStore.load()
  if (!settingsStore.analyzerEnabled) {
      alert(t('analyzer.disabled'))
      router.push('/')
      return
  }
  loadNames()
  connect()
})

onUnmounted(() => {
  disconnect()
})
</script>

<style scoped>
.analyzer-card {
  border: none;
  box-shadow: 0 8px 30px rgba(0, 0, 0, 0.08);
  overflow: hidden;
}

.analyzer-hero {
  background: linear-gradient(135deg, #0d6efd 0%, #5cb3ff 40%, #e9f4ff 100%);
  border-radius: 0.75rem;
  padding: 1.25rem 1.5rem;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 1rem;
  margin-bottom: 1rem;
  color: #0b2b4c;
}

.eyebrow {
  text-transform: uppercase;
  letter-spacing: 0.08em;
  font-size: 0.75rem;
  font-weight: 700;
  color: rgba(11, 43, 76, 0.7);
}

.status-chip {
  display: inline-flex;
  align-items: center;
  gap: 0.5rem;
  padding: 0.35rem 0.75rem;
  border-radius: 999px;
  font-weight: 600;
  border: 1px solid rgba(11, 43, 76, 0.15);
  background: #fff;
  color: #0b2b4c;
}

.status-chip--on .status-dot {
  background: #2ecc71;
}
.status-chip--off .status-dot {
  background: #dc3545;
}

.status-dot {
  width: 10px;
  height: 10px;
  border-radius: 50%;
  display: inline-block;
  box-shadow: 0 0 0 6px rgba(0, 0, 0, 0.04);
}

.stat-tile {
  background: #f8fbff;
  border: 1px solid #e5eefb;
  border-radius: 0.75rem;
  padding: 1rem 1.25rem;
  height: 100%;
  box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.7);
}

.stat-tile .label {
  text-transform: uppercase;
  font-size: 0.75rem;
  color: #5b708b;
  letter-spacing: 0.05em;
  margin-bottom: 0.35rem;
}

.stat-tile .value {
  font-size: 1.5rem;
  font-weight: 700;
  color: #0b2b4c;
}

.stat-tile .hint {
  color: #7d8fa4;
  font-size: 0.85rem;
  margin-top: 0.1rem;
}

.analyzer-table thead th {
  text-transform: uppercase;
  font-size: 0.75rem;
  letter-spacing: 0.05em;
  color: #5b708b;
  border-bottom: 2px solid #e5eefb;
}

.analyzer-table tbody tr:hover {
  box-shadow: inset 4px 0 0 #0d6efd;
}

.analyzer-table {
  font-size: 0.9rem;
}

.table-hover tbody tr {
  transition: background 0.15s ease, box-shadow 0.15s ease;
}
</style>
