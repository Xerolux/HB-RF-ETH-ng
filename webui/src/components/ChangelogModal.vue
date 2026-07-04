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
      <div class="error-icon"><AppIcon name="alert" /></div>
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
          rel="noopener noreferrer"
          class="text-decoration-none text-primary small fw-bold"
        >
          {{ t('changelog.viewOnGithub') }}
          <AppIcon name="externalLink" class="external-link-icon" />
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

const escapeHtml = (value) => value
  .replaceAll('&', '&amp;')
  .replaceAll('<', '&lt;')
  .replaceAll('>', '&gt;')
  .replaceAll('"', '&quot;')
  .replaceAll("'", '&#39;')

const renderedChangelog = computed(() => {
  if (!changelog.value) return ''
  try {
    return marked.parse(escapeHtml(changelog.value))
  } catch (e) {
    console.error('Error parsing markdown:', e)
    return `<pre>${escapeHtml(changelog.value)}</pre>`
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
  border-radius: var(--radius-lg);
  overflow: hidden;
  background: var(--color-surface);
  border: 1px solid var(--color-border);
}

.changelog-header {
  background: var(--color-surface);
  border-bottom: 1px solid var(--color-border);
  color: var(--color-text);
  padding: 1rem 1.25rem;
}

.changelog-header .close-icon {
  width: 34px;
  height: 34px;
  border: 1px solid var(--color-border-strong);
  border-radius: var(--radius-sm);
  display: inline-flex;
  align-items: center;
  justify-content: center;
  font-size: 1.4rem;
  line-height: 1;
  cursor: pointer;
  opacity: 1;
  transition: background 0.2s;
}

.changelog-header .close-icon:hover {
  background: var(--color-bg-alt);
}

.changelog-body {
  padding: 1rem 1.25rem;
  background: var(--color-surface);
  max-height: 60vh;
}

.changelog-content {
  color: var(--color-text, #343a40);
  line-height: 1.6;
}

.changelog-content :deep(h1) {
  font-size: 1.15rem;
  font-weight: 800;
  color: var(--color-text);
  margin: 0 0 1rem;
  padding-bottom: 0.75rem;
  border-bottom: 1px solid var(--color-border);
}

.changelog-content :deep(h2) {
  font-size: 1rem;
  font-weight: 800;
  color: var(--color-primary-strong);
  margin: 1rem 0 0.5rem;
  padding: 0.75rem 0;
  border-top: 1px solid var(--color-border);
  border-bottom: 1px solid var(--color-border);
}

.changelog-content :deep(h3) {
  font-size: 0.95rem;
  font-weight: 800;
  color: var(--color-text, #495057);
  margin-top: 1rem;
  margin-bottom: 0.5rem;
}

.changelog-content :deep(h4) {
  font-size: 0.9rem;
  font-weight: 800;
  color: var(--color-text-secondary, #6c757d);
  margin-top: 0.75rem;
  margin-bottom: 0.5rem;
}

.changelog-content :deep(ul),
.changelog-content :deep(ol) {
  margin-bottom: 1rem;
  padding-left: 1.5rem;
}

.changelog-content :deep(li) {
  margin-bottom: 0.35rem;
}

.changelog-content :deep(code) {
  background: var(--color-bg-alt);
  color: var(--color-primary-strong);
  padding: 0.125rem 0.375rem;
  border-radius: var(--radius-sm);
  font-size: 0.875em;
}

.changelog-content :deep(pre) {
  background: #212529;
  color: #f8f9fa;
  padding: 1rem;
  border-radius: var(--radius-sm);
  overflow-x: auto;
  margin: 1rem 0;
}

.changelog-content :deep(pre code) {
  background: transparent;
  color: inherit;
  padding: 0;
}

.changelog-content :deep(a) {
  color: var(--color-primary-strong);
  text-decoration: none;
  transition: color 0.2s;
}

.changelog-content :deep(a:hover) {
  color: var(--color-primary);
  text-decoration: underline;
}

.changelog-content :deep(blockquote) {
  border-left: 3px solid var(--color-primary);
  padding-left: 1rem;
  margin: 1rem 0;
  color: var(--color-text-secondary, #6c757d);
  font-style: italic;
}

.changelog-content :deep(hr) {
  border: 0;
  border-top: 1px solid var(--color-border, #dee2e6);
  margin: 2rem 0;
}

.error-icon {
  font-size: 3.5rem;
  color: var(--color-danger);
  margin-bottom: 1rem;
  display: inline-flex;
}

.external-link-icon {
  font-size: 0.95rem;
  vertical-align: -0.15em;
  margin-left: 0.25rem;
}

.changelog-footer {
  background: var(--color-surface);
  border-top: 1px solid var(--color-border, #e9ecef);
  padding: 0.85rem 1.25rem;
}

/* Dark mode is handled via CSS variables (--color-bg, --color-text, etc.)
   which automatically adapt when [data-bs-theme="dark"] is set on <html> */

.btn-retry {
  color: var(--color-text) !important;
  border-color: var(--color-text) !important;
}

.btn-retry:hover {
  background-color: var(--color-text) !important;
  color: var(--color-surface) !important;
}

.btn-close-modal {
  color: var(--color-text) !important;
}

@media (max-width: 576px) {
  .changelog-body {
    padding: 1rem;
  }
  .changelog-content :deep(h1) {
    font-size: 1.5rem;
  }
  .changelog-content :deep(h2) {
    font-size: 1.25rem;
  }
}
</style>
