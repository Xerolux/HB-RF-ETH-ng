<template>
  <BModal
    v-model="show"
    :title="t('changelog.title')"
    size="xl"
    scrollable
    centered
    content-class="changelog-modal"
    header-class="changelog-header"
    body-class="changelog-body"
    footer-class="changelog-footer"
  >
    <template #header-close>
      <span class="close-icon">×</span>
    </template>

    <div v-if="loading" class="text-center py-5">
      <div class="spinner-border text-primary" role="status">
        <span class="visually-hidden">{{ t('changelog.loading') }}</span>
      </div>
      <p class="mt-3 text-muted">{{ t('changelog.fetching') }}</p>
    </div>

    <div v-else-if="error" class="text-center py-5">
      <div class="error-icon">⚠️</div>
      <h5 class="mt-3">{{ t('changelog.error') }}</h5>
      <p class="text-danger">{{ error }}</p>
      <BButton variant="outline-secondary" class="btn-retry" @click="fetchChangelog">
        {{ t('changelog.retry') }}
      </BButton>
    </div>

    <div v-else class="changelog-content" v-html="renderedChangelog"></div>

    <template #footer>
      <div class="d-flex justify-content-between align-items-center">
        <a
          href="https://github.com/Xerolux/HB-RF-ETH-ng/blob/main/CHANGELOG.md"
          target="_blank"
          class="text-decoration-none text-primary small fw-bold"
        >
          {{ t('changelog.viewOnGithub') }}
          <span class="ms-1">↗</span>
        </a>
        <BButton variant="secondary" class="btn-close-modal" @click="close">
          {{ t('changelog.close') }}
        </BButton>
      </div>
    </template>
  </BModal>
</template>

<script setup>
import { ref, computed, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { BModal, BButton } from 'bootstrap-vue-next'
import axios from 'axios'
import { marked } from 'marked'

const { t } = useI18n()

const props = defineProps({
  modelValue: Boolean
})

const emit = defineEmits(['update:modelValue'])

const show = computed({
  get: () => props.modelValue,
  set: (value) => emit('update:modelValue', value)
})

const loading = ref(true)
const error = ref(null)
const changelog = ref('')

const renderedChangelog = computed(() => {
  if (!changelog.value) return ''
  try {
    return marked(changelog.value)
  } catch (e) {
    console.error('Error parsing markdown:', e)
    return `<pre>${changelog.value}</pre>`
  }
})

const fetchChangelog = async () => {
  loading.value = true
  error.value = null

  try {
    // Fetch from backend proxy to avoid CORS/network issues
    const response = await axios.get('/api/changelog', {
      timeout: 10000
    })
    changelog.value = response.data
  } catch (err) {
    console.error('Error fetching changelog:', err)
    error.value = `${t('changelog.fetchError')} (${err.message})`
  } finally {
    loading.value = false
  }
}

const close = () => {
  show.value = false
}

watch(() => props.modelValue, (newValue) => {
  if (newValue && !changelog.value) {
    fetchChangelog()
  }
})
</script>

<style scoped>
.changelog-modal {
  border-radius: 16px;
  overflow: hidden;
}

.changelog-header {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  border: none;
  color: white;
  padding: 1.5rem;
}

.changelog-header .close-icon {
  font-size: 2rem;
  line-height: 1;
  cursor: pointer;
  opacity: 0.8;
  transition: opacity 0.2s;
}

.changelog-header .close-icon:hover {
  opacity: 1;
}

.changelog-body {
  padding: 2rem;
  background: #f8f9fa;
  max-height: 60vh;
}

.changelog-content {
  color: #343a40;
  line-height: 1.8;
}

.changelog-content :deep(h1) {
  font-size: 2rem;
  font-weight: 700;
  color: #667eea;
  margin-bottom: 1.5rem;
  padding-bottom: 0.5rem;
  border-bottom: 2px solid #e9ecef;
}

.changelog-content :deep(h2) {
  font-size: 1.5rem;
  font-weight: 600;
  color: #764ba2;
  margin-top: 2rem;
  margin-bottom: 1rem;
}

.changelog-content :deep(h3) {
  font-size: 1.25rem;
  font-weight: 600;
  color: #495057;
  margin-top: 1.5rem;
  margin-bottom: 0.75rem;
}

.changelog-content :deep(h4) {
  font-size: 1.1rem;
  font-weight: 600;
  color: #6c757d;
  margin-top: 1rem;
  margin-bottom: 0.5rem;
}

.changelog-content :deep(ul),
.changelog-content :deep(ol) {
  margin-bottom: 1rem;
  padding-left: 1.5rem;
}

.changelog-content :deep(li) {
  margin-bottom: 0.5rem;
}

.changelog-content :deep(code) {
  background: #e9ecef;
  color: #d63384;
  padding: 0.125rem 0.375rem;
  border-radius: 4px;
  font-size: 0.875em;
}

.changelog-content :deep(pre) {
  background: #212529;
  color: #f8f9fa;
  padding: 1rem;
  border-radius: 8px;
  overflow-x: auto;
  margin: 1rem 0;
}

.changelog-content :deep(pre code) {
  background: transparent;
  color: inherit;
  padding: 0;
}

.changelog-content :deep(a) {
  color: #667eea;
  text-decoration: none;
  transition: color 0.2s;
}

.changelog-content :deep(a:hover) {
  color: #764ba2;
  text-decoration: underline;
}

.changelog-content :deep(blockquote) {
  border-left: 4px solid #667eea;
  padding-left: 1rem;
  margin: 1rem 0;
  color: #6c757d;
  font-style: italic;
}

.changelog-content :deep(hr) {
  border: 0;
  border-top: 1px solid #dee2e6;
  margin: 2rem 0;
}

.error-icon {
  font-size: 4rem;
  margin-bottom: 1rem;
}

.changelog-footer {
  background: white;
  border-top: 1px solid #e9ecef;
  padding: 1rem 1.5rem;
}

/* Dark mode support */
:deep(.dark) .changelog-body {
  background: #1e1e2d;
}

:deep(.dark) .changelog-content {
  color: #e4e6eb;
}

:deep(.dark) .changelog-content h1,
:deep(.dark) .changelog-content h2,
:deep(.dark) .changelog-content h3 {
  color: #a78bfa;
}

:deep(.dark) .changelog-content code {
  background: #2d2d3a;
  color: #f472b6;
}

:deep(.dark) .changelog-content pre {
  background: #0d0d14;
}

:deep(.dark) .changelog-content a {
  color: #a78bfa;
}

:deep(.dark) .changelog-content a:hover {
  color: #c4b5fd;
}

:deep(.dark) .changelog-footer {
  background: #1e1e2d;
  border-top-color: #2d2d3a;
}

.btn-retry {
  color: #343a40 !important;
  border-color: #343a40 !important;
}

.btn-retry:hover {
  background-color: #343a40 !important;
  color: white !important;
}

.btn-close-modal {
  color: #343a40 !important;
}

:deep(.dark) .btn-retry {
  color: #e4e6eb !important;
  border-color: #e4e6eb !important;
}

:deep(.dark) .btn-retry:hover {
  background-color: #e4e6eb !important;
  color: #1e1e2d !important;
}

:deep(.dark) .btn-close-modal {
  color: #e4e6eb !important;
}
</style>
