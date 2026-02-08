<template>
  <BModal
    v-model="showModal"
    centered
    hide-header
    hide-footer
    no-close-on-backdrop
    content-class="sponsor-modal-content"
  >
    <div class="sponsor-modal-body">
      <button class="close-btn" @click="closeModal">✕</button>

      <div class="header-section">
        <div class="heart-icon">❤️</div>
        <h2 class="title">{{ t('sponsor.title') }}</h2>
        <p class="description">
          {{ t('sponsor.description') }}
        </p>
      </div>

      <div class="options-grid">
        <a href="https://paypal.me/Xerolux" target="_blank" class="sponsor-option paypal">
          <span class="icon">☕</span>
          <span class="label">PayPal</span>
        </a>

        <a href="https://www.buymeacoffee.com/xerolux" target="_blank" class="sponsor-option bmc">
          <span class="icon">🥤</span>
          <span class="label">Buy Me a Coffee</span>
        </a>

        <a href="https://ts.la/sebastian564489" target="_blank" class="sponsor-option tesla">
          <span class="icon">🚗</span>
          <span class="label">Tesla Referral</span>
        </a>
      </div>

      <div class="footer-text">
        {{ t('sponsor.thanks') }}
      </div>
    </div>
  </BModal>
</template>

<script setup>
import { ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'

const props = defineProps({
  modelValue: {
    type: Boolean,
    default: false
  }
})

const emit = defineEmits(['update:modelValue'])

const { t } = useI18n()
const showModal = ref(props.modelValue)

watch(() => props.modelValue, (newVal) => {
  showModal.value = newVal
})

watch(showModal, (newVal) => {
  emit('update:modelValue', newVal)
})

const closeModal = () => {
  showModal.value = false
}
</script>

<style scoped>
:deep(.sponsor-modal-content) {
  border-radius: var(--radius-xl);
  border: none;
  overflow: hidden;
  background: var(--color-surface);
  box-shadow: var(--shadow-xl);
}

.sponsor-modal-body {
  padding: 40px;
  text-align: center;
  position: relative;
}

.close-btn {
  position: absolute;
  top: 16px;
  right: 16px;
  background: transparent;
  border: none;
  font-size: 1.5rem;
  color: var(--color-text-secondary);
  cursor: pointer;
  padding: 8px;
  line-height: 1;
  border-radius: 50%;
  transition: background-color 0.2s;
}

.close-btn:hover {
  background-color: var(--color-bg);
  color: var(--color-text);
}

.heart-icon {
  font-size: 4rem;
  margin-bottom: var(--spacing-md);
  animation: pulse 2s infinite;
}

@keyframes pulse {
  0% { transform: scale(1); }
  50% { transform: scale(1.1); }
  100% { transform: scale(1); }
}

.title {
  font-size: 1.75rem;
  font-weight: 800;
  margin-bottom: var(--spacing-sm);
  background: linear-gradient(135deg, #ff6b35 0%, #ff9f43 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
}

.description {
  color: var(--color-text-secondary);
  margin-bottom: var(--spacing-xl);
  font-size: 1rem;
  line-height: 1.6;
}

.options-grid {
  display: grid;
  gap: var(--spacing-md);
  margin-bottom: var(--spacing-xl);
}

.sponsor-option {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: var(--spacing-md);
  padding: 16px;
  border-radius: var(--radius-lg);
  text-decoration: none;
  font-weight: 700;
  font-size: 1.125rem;
  transition: all 0.2s;
}

.sponsor-option.github {
  background-color: #24292e;
  color: white;
}

.sponsor-option.paypal {
  background-color: #0070ba;
  color: white;
}

.sponsor-option.bmc {
  background-color: #FFDD00;
  color: #000000;
}

.sponsor-option.tesla {
  background-color: #cc0000;
  color: white;
}

.sponsor-option:hover {
  transform: translateY(-2px);
  box-shadow: var(--shadow-lg);
}

.sponsor-option .icon {
  font-size: 1.5rem;
}

.footer-text {
  font-size: 0.875rem;
  color: var(--color-text-secondary);
  font-weight: 500;
}
</style>
