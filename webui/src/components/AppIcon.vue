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
  arrowRight: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M5 12h14' },
      { type: 'path', d: 'm13 6 6 6-6 6' }
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
  coffee: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M5 7h11v7a4 4 0 0 1-4 4H9a4 4 0 0 1-4-4V7Z' },
      { type: 'path', d: 'M16 9h1.5a2.5 2.5 0 0 1 0 5H16' },
      { type: 'path', d: 'M7 3.5v1.2' },
      { type: 'path', d: 'M11 3.5v1.2' },
      { type: 'path', d: 'M15 3.5v1.2' }
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
  externalLink: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M18 13v6a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h6' },
      { type: 'path', d: 'M15 3h6v6' },
      { type: 'path', d: 'M10 14 21 3' }
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
  gitFork: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'circle', cx: 6, cy: 5, r: 2 },
      { type: 'circle', cx: 18, cy: 5, r: 2 },
      { type: 'circle', cx: 12, cy: 19, r: 2 },
      { type: 'path', d: 'M6 7v3a2 2 0 0 0 2 2h8a2 2 0 0 0 2-2V7' },
      { type: 'path', d: 'M12 12v5' }
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
  lock: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'rect', x: 5, y: 11, width: 14, height: 9, rx: 2.5 },
      { type: 'path', d: 'M8 11V8a4 4 0 0 1 8 0v3' },
      { type: 'circle', cx: 12, cy: 15.5, r: 1.2 }
    ]
  },
  link: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M10 13a5 5 0 0 0 7 0l3-3a5 5 0 0 0-7-7l-1 1' },
      { type: 'path', d: 'M14 11a5 5 0 0 0-7 0l-3 3a5 5 0 0 0 7 7l1-1' }
    ]
  },
  // Logout: door frame on the left + arrow pointing out to the right.
  // Visually distinct from the "power" symbol so users no longer confuse
  // logout with reboot (see feedback from the field tester).
  logout: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4' },
      { type: 'path', d: 'M16 17l5-5-5-5' },
      { type: 'line', x1: 21, y1: 12, x2: 9, y2: 12 }
    ]
  },
  list: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M4 6h.01' },
      { type: 'path', d: 'M4 12h.01' },
      { type: 'path', d: 'M4 18h.01' },
      { type: 'path', d: 'M9 6h11' },
      { type: 'path', d: 'M9 12h11' },
      { type: 'path', d: 'M9 18h11' }
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
  package: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M3 7.5 12 3l9 4.5v9L12 21 3 16.5v-9Z' },
      { type: 'path', d: 'M3 7.5 12 12l9-4.5' },
      { type: 'path', d: 'M12 12v9' }
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
  radio: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'rect', x: 4, y: 13, width: 16, height: 7, rx: 2 },
      { type: 'circle', cx: 8, cy: 16.5, r: 1 },
      { type: 'circle', cx: 12, cy: 16.5, r: 1 },
      { type: 'path', d: 'M9.5 13 17 5.5' },
      { type: 'path', d: 'M7.5 13 15 5.5' },
      { type: 'line', x1: 15, y1: 5.5, x2: 18.5, y2: 6.5 }
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
  // Restart / reboot: a near-full clockwise circle (gap at the top-right)
  // with an arrow head. Reads as "rotate / reboot" rather than "power off",
  // which is the ambiguity the old power-icon button had.
  restart: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M21 12a9 9 0 1 1-3-6.7' },
      { type: 'path', d: 'M21 3v5h-5' }
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
  share: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M4 12v8a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2v-8' },
      { type: 'path', d: 'm16 6-4-4-4 4' },
      { type: 'path', d: 'M12 2v13' }
    ]
  },
  satellite: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'm5 5 3 3' },
      { type: 'path', d: 'm9 3 1.5 4.5' },
      { type: 'path', d: 'M3 9l4.5 1.5' },
      { type: 'path', d: 'M7 13c0 4 3 8 8 8' },
      { type: 'rect', x: 11.5, y: 9.5, width: 6, height: 6, rx: 1.5, transform: 'rotate(45 14.5 12.5)' },
      { type: 'path', d: 'M15 15l3 3' }
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
  },
  whatsapp: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M5.4 19.2 6.3 16A7.3 7.3 0 1 1 9 18.7l-3.6.5Z' },
      { type: 'path', d: 'M9.2 8.8c.2-.4.4-.4.7-.4h.5c.2 0 .4.1.5.4l.7 1.6c.1.3.1.5-.1.7l-.4.5c.7 1.2 1.7 2.1 3 2.7l.5-.5c.2-.2.4-.3.7-.2l1.5.7c.3.1.4.3.4.6v.4c0 .4-.2.7-.6.9-.5.3-1.2.4-2.1.2-2.9-.6-5.5-3-6.3-5.8-.2-.8-.1-1.5.4-2Z' }
    ]
  },
  xSocial: {
    viewBox: '0 0 24 24',
    nodes: [
      { type: 'path', d: 'M4 4 20 20' },
      { type: 'path', d: 'M20 4 13.2 11.2' },
      { type: 'path', d: 'M10.8 13 4 20' }
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
