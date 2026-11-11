<template>
  <div class="release-notes">
    <template v-for="(line, index) in lines" :key="index">
      <h5 v-if="line.startsWith('#')">{{ line.replace(/^#+\s*/, '') }}</h5>
      <p v-else-if="/^[-*]\s/.test(line)" class="release-item">{{ line.replace(/^[-*]\s/, '• ') }}</p>
      <p v-else>{{ line }}</p>
    </template>
  </div>
</template>
<script setup>
import { computed } from 'vue'
const props = defineProps({ text: { type: String, required: true } })
// Vue escapes all text. No HTML/Markdown dependency or persistent notes cache.
const lines = computed(() => props.text.split('\n').map(line => line.trim()).filter(line => line && !/^---+$/.test(line)))
</script>
<style scoped>
.release-notes { max-height: 40vh; overflow-y: auto; font-family: inherit; overflow-wrap: anywhere; font-size: var(--fs-sm); line-height: var(--line-height-normal); }
.release-notes h5 { color: var(--color-text); font-size: var(--fs-sm); font-weight: var(--font-weight-semibold); margin: var(--space-4) 0 var(--space-2); }
.release-notes p { white-space: pre-wrap; margin: 0 0 var(--space-2); color: var(--color-text-secondary); }
.release-item { padding-left: var(--space-3); }
</style>
