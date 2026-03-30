<template>
  <svg
    class="app-icon"
    :class="[`app-icon-${name}`]"
    xmlns="http://www.w3.org/2000/svg"
    :viewBox="icon.viewBox"
    fill="none"
    stroke="currentColor"
    stroke-width="1.8"
    stroke-linecap="round"
    stroke-linejoin="round"
    aria-hidden="true"
  >
    <template v-for="(item, index) in icon.nodes" :key="index">
      <path v-if="item.type === 'path'" :d="item.d" />
      <circle v-else-if="item.type === 'circle'" :cx="item.cx" :cy="item.cy" :r="item.r" />
      <rect
        v-else-if="item.type === 'rect'"
        :x="item.x"
        :y="item.y"
        :width="item.width"
        :height="item.height"
        :rx="item.rx"
      />
      <line
        v-else-if="item.type === 'line'"
        :x1="item.x1"
        :y1="item.y1"
        :x2="item.x2"
        :y2="item.y2"
      />
    </template>
  </svg>
</template>

<script setup>
import { computed } from 'vue'

const props = defineProps({
  name: {
    type: String,
    required: true
  }
})

const icons = {
  activity: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M3 12h4l2-5 4 10 2-5h6' }
    ]
  },
  alert: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M12 3 2.5 19h19L12 3Z' },
      { type: 'line', x1: 12, y1: 9, x2: 12, y2: 13 },
      { type: 'circle', cx: 12, cy: 17, r: 0.8 }
    ]
  },
  backup: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M4 7h16v10a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V7Z' },
      { type: 'path', d: 'M8 7V5a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2' },
      { type: 'path', d: 'M12 11v6' },
      { type: 'path', d: 'm9.5 14.5 2.5 2.5 2.5-2.5' }
    ]
  },
  board: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'rect', x: 6, y: 6, width: 12, height: 12, rx: 2.5 },
      { type: 'line', x1: 9, y1: 3, x2: 9, y2: 6 },
      { type: 'line', x1: 15, y1: 3, x2: 15, y2: 6 },
      { type: 'line', x1: 9, y1: 18, x2: 9, y2: 21 },
      { type: 'line', x1: 15, y1: 18, x2: 15, y2: 21 },
      { type: 'line', x1: 3, y1: 9, x2: 6, y2: 9 },
      { type: 'line', x1: 3, y1: 15, x2: 6, y2: 15 },
      { type: 'line', x1: 18, y1: 9, x2: 21, y2: 9 },
      { type: 'line', x1: 18, y1: 15, x2: 21, y2: 15 }
    ]
  },
  check: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'm5 12 4.2 4.2L19 6.5' }
    ]
  },
  chevronDown: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'm6 9 6 6 6-6' }
    ]
  },
  clock: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'circle', cx: 12, cy: 12, r: 8.5 },
      { type: 'path', d: 'M12 7.5V12l3 2' }
    ]
  },
  close: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M6 6l12 12M18 6 6 18' }
    ]
  },
  copy: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'rect', x: 9, y: 9, width: 10, height: 10, rx: 2.5 },
      { type: 'path', d: 'M15 9V7a2 2 0 0 0-2-2H7a2 2 0 0 0-2 2v6a2 2 0 0 0 2 2h2' }
    ]
  },
  cpu: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'rect', x: 7, y: 7, width: 10, height: 10, rx: 2.5 },
      { type: 'rect', x: 10, y: 10, width: 4, height: 4, rx: 1 },
      { type: 'line', x1: 10, y1: 3, x2: 10, y2: 7 },
      { type: 'line', x1: 14, y1: 3, x2: 14, y2: 7 },
      { type: 'line', x1: 10, y1: 17, x2: 10, y2: 21 },
      { type: 'line', x1: 14, y1: 17, x2: 14, y2: 21 },
      { type: 'line', x1: 3, y1: 10, x2: 7, y2: 10 },
      { type: 'line', x1: 3, y1: 14, x2: 7, y2: 14 },
      { type: 'line', x1: 17, y1: 10, x2: 21, y2: 10 },
      { type: 'line', x1: 17, y1: 14, x2: 21, y2: 14 }
    ]
  },
  dashboard: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'rect', x: 4, y: 4, width: 7, height: 7, rx: 2 },
      { type: 'rect', x: 13, y: 4, width: 7, height: 5, rx: 2 },
      { type: 'rect', x: 13, y: 11, width: 7, height: 9, rx: 2 },
      { type: 'rect', x: 4, y: 13, width: 7, height: 7, rx: 2 }
    ]
  },
  download: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M12 4v10' },
      { type: 'path', d: 'm8.5 10.5 3.5 3.5 3.5-3.5' },
      { type: 'path', d: 'M5 19h14' }
    ]
  },
  ethernet: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M7 4h10v6l-2 2v3h-6v-3L7 10V4Z' },
      { type: 'line', x1: 10, y1: 7, x2: 10, y2: 10 },
      { type: 'line', x1: 14, y1: 7, x2: 14, y2: 10 }
    ]
  },
  file: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M8 3h6l5 5v11a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2Z' },
      { type: 'path', d: 'M14 3v5h5' }
    ]
  },
  firmware: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M12 3v4' },
      { type: 'path', d: 'M9 6h6' },
      { type: 'path', d: 'M6 10h12l-1.5 8.5a2 2 0 0 1-2 1.5h-5a2 2 0 0 1-2-1.5L6 10Z' }
    ]
  },
  globe: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'circle', cx: 12, cy: 12, r: 8.5 },
      { type: 'path', d: 'M3.5 12h17' },
      { type: 'path', d: 'M12 3.5a13 13 0 0 1 0 17' },
      { type: 'path', d: 'M12 3.5a13 13 0 0 0 0 17' }
    ]
  },
  info: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'circle', cx: 12, cy: 12, r: 8.5 },
      { type: 'line', x1: 12, y1: 10, x2: 12, y2: 15 },
      { type: 'circle', cx: 12, cy: 7.5, r: 0.8 }
    ]
  },
  language: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M4 6h8' },
      { type: 'path', d: 'M8 4v2a9 9 0 0 1-3 6' },
      { type: 'path', d: 'M6 10c1 1.2 2.2 2.1 3.5 2.8' },
      { type: 'path', d: 'M14 18h6' },
      { type: 'path', d: 'm15.5 15 2-6 2 6' }
    ]
  },
  logs: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'rect', x: 4, y: 5, width: 16, height: 14, rx: 2.5 },
      { type: 'path', d: 'm8 10-2 2 2 2' },
      { type: 'path', d: 'm12 16 2-8' },
      { type: 'path', d: 'm16 10 2 2-2 2' }
    ]
  },
  memory: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'rect', x: 5, y: 7, width: 14, height: 10, rx: 2.5 },
      { type: 'line', x1: 8, y1: 10, x2: 8, y2: 14 },
      { type: 'line', x1: 12, y1: 10, x2: 12, y2: 14 },
      { type: 'line', x1: 16, y1: 10, x2: 16, y2: 14 }
    ]
  },
  menu: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'line', x1: 4, y1: 7, x2: 20, y2: 7 },
      { type: 'line', x1: 4, y1: 12, x2: 20, y2: 12 },
      { type: 'line', x1: 4, y1: 17, x2: 20, y2: 17 }
    ]
  },
  monitoring: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M4 18h16' },
      { type: 'path', d: 'M7 16V9' },
      { type: 'path', d: 'M12 16V6' },
      { type: 'path', d: 'M17 16v-4' }
    ]
  },
  moon: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M16.5 3.8A7.9 7.9 0 0 0 20 10.2 8.6 8.6 0 1 1 9.8 4a7.8 7.8 0 0 0 6.7-.2Z' }
    ]
  },
  network: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'circle', cx: 6, cy: 17, r: 2 },
      { type: 'circle', cx: 18, cy: 17, r: 2 },
      { type: 'circle', cx: 12, cy: 7, r: 2 },
      { type: 'path', d: 'M8 16l3-7' },
      { type: 'path', d: 'm16 16-3-7' },
      { type: 'path', d: 'M8 17h8' }
    ]
  },
  power: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M12 3v8' },
      { type: 'path', d: 'M7 5.5a7 7 0 1 0 10 0' }
    ]
  },
  refresh: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M20 6v5h-5' },
      { type: 'path', d: 'M4 18v-5h5' },
      { type: 'path', d: 'M7.5 17A7 7 0 0 0 18 11.5V11' },
      { type: 'path', d: 'M16.5 7A7 7 0 0 0 6 12.5V13' }
    ]
  },
  restore: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M3 12a9 9 0 1 0 3-6.7' },
      { type: 'path', d: 'M3 4v5h5' }
    ]
  },
  router: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'rect', x: 4, y: 12, width: 16, height: 6, rx: 2 },
      { type: 'circle', cx: 9, cy: 15, r: 0.7 },
      { type: 'circle', cx: 12, cy: 15, r: 0.7 },
      { type: 'path', d: 'M8 10a4 4 0 0 1 8 0' },
      { type: 'path', d: 'M10 10a2 2 0 0 1 4 0' }
    ]
  },
  search: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'circle', cx: 11, cy: 11, r: 6 },
      { type: 'path', d: 'm16 16 4.5 4.5' }
    ]
  },
  settings: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M12 3.5v3' },
      { type: 'path', d: 'M12 17.5v3' },
      { type: 'path', d: 'M4.6 7.1l2.1 1.2' },
      { type: 'path', d: 'm17.3 14.7 2.1 1.2' },
      { type: 'path', d: 'm4.6 16.9 2.1-1.2' },
      { type: 'path', d: 'm17.3 9.3 2.1-1.2' },
      { type: 'circle', cx: 12, cy: 12, r: 3.5 }
    ]
  },
  shield: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M12 3 5 6v5c0 4.4 2.9 7.8 7 9 4.1-1.2 7-4.6 7-9V6l-7-3Z' },
      { type: 'path', d: 'm9.5 12 1.8 1.8L15 10' }
    ]
  },
  sun: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'circle', cx: 12, cy: 12, r: 4 },
      { type: 'line', x1: 12, y1: 2.5, x2: 12, y2: 5 },
      { type: 'line', x1: 12, y1: 19, x2: 12, y2: 21.5 },
      { type: 'line', x1: 2.5, y1: 12, x2: 5, y2: 12 },
      { type: 'line', x1: 19, y1: 12, x2: 21.5, y2: 12 },
      { type: 'line', x1: 5.4, y1: 5.4, x2: 7.2, y2: 7.2 },
      { type: 'line', x1: 16.8, y1: 16.8, x2: 18.6, y2: 18.6 },
      { type: 'line', x1: 16.8, y1: 7.2, x2: 18.6, y2: 5.4 },
      { type: 'line', x1: 5.4, y1: 18.6, x2: 7.2, y2: 16.8 }
    ]
  },
  support: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M12 19h4' },
      { type: 'path', d: 'M8 14a4 4 0 1 1 8 0c0 1.7-.8 2.5-2 3.5' },
      { type: 'circle', cx: 12, cy: 19, r: 0.8 }
    ]
  },
  thermometer: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M10 6a2 2 0 1 1 4 0v7.5a4 4 0 1 1-4 0V6Z' },
      { type: 'line', x1: 12, y1: 9, x2: 12, y2: 15.5 }
    ]
  },
  time: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'circle', cx: 12, cy: 12, r: 8.5 },
      { type: 'path', d: 'M12 8v4l2.5 2.5' }
    ]
  },
  upload: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M12 20V10' },
      { type: 'path', d: 'm8.5 13.5 3.5-3.5 3.5 3.5' },
      { type: 'path', d: 'M5 5h14' }
    ]
  },
  user: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'circle', cx: 12, cy: 9, r: 3.5 },
      { type: 'path', d: 'M5.5 19a6.5 6.5 0 0 1 13 0' }
    ]
  }
}

const icon = computed(() => icons[props.name] || icons.info)
</script>

<style scoped>
.app-icon {
  width: 1.1em;
  height: 1.1em;
  display: inline-block;
  flex-shrink: 0;
}
</style>
