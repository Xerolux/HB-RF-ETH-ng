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
        <div>
           <BFormCheckbox v-model="autoScroll" switch inline>
            {{ t('analyzer.autoScroll') }}
          </BFormCheckbox>
        </div>
      </div>

      <div class="table-responsive">
        <table class="table table-sm table-striped table-hover font-monospace" style="font-size: 0.9rem;">
          <thead>
            <tr>
              <th>#</th>
              <th>{{ t('analyzer.time') }}</th>
              <th>{{ t('analyzer.len') }}</th>
              <th>{{ t('analyzer.cnt') }}</th>
              <th>{{ t('analyzer.type') }}</th>
              <th>{{ t('analyzer.src') }}</th>
              <th>{{ t('analyzer.dst') }}</th>
              <th>{{ t('analyzer.payload') }}</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="frame in frames" :key="frame.id">
              <td>{{ frame.id }}</td>
              <td>{{ formatTime(frame.ts) }}</td>
              <td>{{ frame.len }}</td>
              <td>{{ frame.cnt }}</td>
              <td>
                <span :title="frame.rawType">
                  {{ frame.type }}
                </span>
              </td>
              <td>{{ frame.src }}</td>
              <td>{{ frame.dst }}</td>
              <td class="text-break">{{ frame.payload }}</td>
            </tr>
          </tbody>
        </table>
      </div>
    </BCard>
  </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted, nextTick } from 'vue'
import { useI18n } from 'vue-i18n'
import { useLoginStore } from './stores.js'

const { t } = useI18n()
const loginStore = useLoginStore()

const frames = ref([])
const isConnected = ref(false)
const autoScroll = ref(true)
let ws = null
let frameCounter = 0

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
    // Try reconnect after 5s if component is still mounted
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
    ws.onclose = null // Prevent reconnect
    ws.close()
    ws = null
  }
  isConnected.value = false
}

const addFrame = (rawFrame) => {
  frameCounter++

  const data = rawFrame.data
  // Basic parsing: Len(1), Cnt(1), Type(1), Src(3), Dst(3), Payload...
  let len = parseInt(data.substring(0, 2), 16)
  let cnt = parseInt(data.substring(2, 4), 16)
  let typeHex = data.substring(4, 6)
  let src = data.substring(6, 12)
  let dst = data.substring(12, 18)
  let payload = data.substring(18)

  // Resolve Type Name
  let typeName = FRAME_TYPES[typeHex] || `Unknown (${typeHex})`

  if (frames.value.length > 200) {
    frames.value.shift()
  }

  frames.value.push({
    id: frameCounter,
    ts: rawFrame.ts,
    len: len,
    cnt: cnt.toString(16).toUpperCase().padStart(2, '0'),
    type: typeName,
    rawType: typeHex,
    src: src,
    dst: dst,
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

const formatTime = (ts) => {
    const sec = Math.floor(ts / 1000)
    const ms = ts % 1000
    return `${sec}.${ms.toString().padStart(3, '0')}`
}

onMounted(() => {
  connect()
})

onUnmounted(() => {
  disconnect()
})
</script>
