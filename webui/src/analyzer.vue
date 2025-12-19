<template>
  <div class="container-fluid">
    <BCard :title="t('analyzer.title')" class="mb-3">
      <div class="d-flex justify-content-between align-items-center mb-3">
        <div>
          <BButton :variant="isConnected ? 'success' : 'danger'" disabled class="me-2">
            {{ isConnected ? t('analyzer.connected') : t('analyzer.disconnected') }}
          </BButton>
          <BButton variant="secondary" @click="clear" class="me-2">
            {{ t('analyzer.clear') }}
          </BButton>
        </div>
        <div class="d-flex align-items-center">
           <BFormCheckbox v-model="autoScroll" switch inline class="me-3">
            {{ t('analyzer.autoScroll') }}
          </BFormCheckbox>
          <BButton variant="outline-primary" size="sm" @click="showNamesModal = true">
            {{ t('analyzer.deviceNames') }}
          </BButton>
        </div>
      </div>

      <div class="table-responsive">
        <table class="table table-sm table-striped table-hover font-monospace" style="font-size: 0.9rem;">
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
import { ref, reactive, onMounted, onUnmounted, nextTick, computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { useLoginStore, useSettingsStore } from './stores.js'
import { useRouter } from 'vue-router'

const { t } = useI18n()
const loginStore = useLoginStore()
const settingsStore = useSettingsStore()
const router = useRouter()

const frames = ref([])
const isConnected = ref(false)
const autoScroll = ref(true)
let ws = null
let frameCounter = 0

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

  // Parse CoPro Frame
  // Format: FD <LenH> <LenL> <Dst> <Cnt> <Cmd> <Payload...> <CrcH> <CrcL>
  // Min length: 8 bytes (FD 00 00 Dst Cnt Cmd CrcH CrcL)

  if (!data.startsWith('FD')) {
     // Unknown format or raw radio frame without CoPro wrapper?
     // Fallback to direct parsing
  } else {
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

        let rssi = parseInt(rssiHex, 16)
        if (rssi >= 128) rssi -= 256 // Signed byte

        data = frameHex
        // Add RSSI to frame object
        processBidCosFrame(rawFrame.ts, data, rssi)
     } else {
        // Other events (TX complete, etc)
        // Optionally display them
     }
  }
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
  if (frames.value.length > 200) {
    frames.value.shift()
  }

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

  frames.value.push({
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

  if (autoScroll.value) {
    nextTick(() => {
      window.scrollTo(0, document.body.scrollHeight)
    })
  }
}

const clear = () => {
  frames.value = []
}

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
