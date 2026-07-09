<template>
  <!--
    HB-RF-ETH-ng brand mark — three organic leaves in a fixed warm gradient
    (#D96A5A → #EAA08E). This is the brand identity colour and is deliberately
    NOT tied to the theme's primary accent (which changes between Glass-UI
    orange and NewDesign emerald). Inlined so it needs no extra file on the
    ESP32 (only fixed asset names are served).
  -->
  <svg
    class="brand-logo-svg"
    xmlns="http://www.w3.org/2000/svg"
    viewBox="0 0 512 512"
    :width="size"
    :height="size"
    aria-hidden="true"
    focusable="false"
  >
    <defs>
      <linearGradient id="hb-rf-eth-brand" x1="0" y1="0" x2="1" y2="1">
        <stop offset="0%" stop-color="#D96A5A" />
        <stop offset="100%" stop-color="#EAA08E" />
      </linearGradient>
    </defs>
    <g fill="url(#hb-rf-eth-brand)">
      <path d="M 256.0 186.0 C 320.6 123.3, 374.3 165.6, 320.7 233.1 C 289.1 197.3, 272.8 175.2, 256.0 186.0 Z" />
      <path d="M 316.6 291.0 C 338.6 378.3, 275.2 403.6, 243.5 323.5 C 290.3 314.0, 317.6 311.0, 316.6 291.0 Z" />
      <path d="M 195.4 291.0 C 108.8 266.4, 118.6 198.8, 203.8 211.4 C 188.6 256.7, 177.6 281.8, 195.4 291.0 Z" />
    </g>
  </svg>
</template>

<script setup>
/**
 * Brand logo mark for HB-RF-ETH-ng.
 * Renders the inline SVG at the requested pixel size.
 * @prop {number|string} size - render size in px (default 48)
 */
defineProps({
  size: {
    type: [Number, String],
    default: 48
  }
})
</script>

<style scoped>
.brand-logo-svg {
  display: block;
  flex: 0 0 auto;
}
</style>
