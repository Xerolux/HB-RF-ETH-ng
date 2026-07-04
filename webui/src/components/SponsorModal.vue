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
      <button class="close-btn" @click="closeModal" aria-label="Close">
        <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
          <line x1="18" y1="6" x2="6" y2="18"></line>
          <line x1="6" y1="6" x2="18" y2="18"></line>
        </svg>
      </button>

      <div class="header-section">
        <div class="support-icon"><AppIcon name="support" /></div>
        <h2 class="title">{{ t('sponsor.title') }}</h2>
        <p class="description">
          {{ t('sponsor.description') }}
        </p>
      </div>

      <div class="options-grid">
        <a href="https://paypal.me/Xerolux" target="_blank" rel="noopener noreferrer" class="sponsor-option paypal">
          <AppIcon name="support" />
          <span class="label">PayPal</span>
        </a>

        <a href="https://www.buymeacoffee.com/xerolux" target="_blank" rel="noopener noreferrer" class="sponsor-option bmc">
          <AppIcon name="coffee" />
          <span class="label">Buy Me a Coffee</span>
        </a>

        <a href="https://ts.la/sebastian564489" target="_blank" rel="noopener noreferrer" class="sponsor-option tesla">
          <AppIcon name="link" />
          <span class="label">Tesla Referral</span>
        </a>

        <a href="https://x.com/Xerolux" target="_blank" rel="noopener noreferrer" class="sponsor-option social">
          <AppIcon name="xSocial" />
          <span class="label">X</span>
        </a>

        <a href="https://wa.me/" target="_blank" rel="noopener noreferrer" class="sponsor-option social">
          <AppIcon name="whatsapp" />
          <span class="label">WhatsApp</span>
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
  border-radius: var(--radius-lg);
  border: 1px solid var(--color-border);
  overflow: hidden;
  background: var(--color-surface);
  box-shadow: var(--shadow-lg);
}

.sponsor-modal-body {
  padding: 32px;
  text-align: center;
  position: relative;
}

.close-btn {
  position: absolute;
  top: 16px;
  right: 16px;
  background: var(--color-surface);
  border: 1px solid var(--color-border-strong);
  color: var(--color-text-secondary);
  cursor: pointer;
  padding: 8px;
  line-height: 1;
  border-radius: var(--radius-sm);
  transition: background-color 0.2s;
}

.close-btn:hover {
  background-color: var(--color-bg);
  color: var(--color-text);
}

.support-icon {
  width: 46px;
  height: 46px;
  margin: 0 auto var(--spacing-md);
  border: 1px solid var(--color-border);
  border-radius: var(--radius-sm);
  display: inline-flex;
  align-items: center;
  justify-content: center;
  color: var(--color-primary-strong);
  background: var(--color-primary-soft);
}

.support-icon .app-icon {
  width: 24px;
  height: 24px;
}

.title {
  font-size: 1.35rem;
  font-weight: 800;
  margin-bottom: var(--spacing-sm);
  color: var(--color-text);
}

.description {
  color: var(--color-text-secondary);
  margin-bottom: var(--spacing-xl);
  font-size: 1rem;
  line-height: 1.6;
}

.options-grid {
  display: grid;
  gap: 10px;
  margin-bottom: var(--spacing-xl);
}

.sponsor-option {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 10px;
  padding: 12px 14px;
  border-radius: var(--radius-sm);
  text-decoration: none;
  font-weight: 700;
  font-size: 1rem;
  transition: background 0.2s, border-color 0.2s, color 0.2s;
  border: 1px solid var(--color-border-strong);
  background: var(--color-surface);
  color: var(--color-text);
}

.sponsor-option.paypal {
  color: #0070ba;
}

.sponsor-option.bmc {
  color: #9a7400;
}

.sponsor-option.tesla {
  color: #cc0000;
}

.sponsor-option.social {
  color: var(--color-text-secondary);
}

.sponsor-option:hover {
  background: var(--color-bg-alt);
  border-color: var(--color-primary);
  color: var(--color-primary-strong);
}

.sponsor-option .app-icon {
  width: 20px;
  height: 20px;
}

.footer-text {
  font-size: 0.875rem;
  color: var(--color-text-secondary);
  font-weight: 500;
}

@media (max-width: 576px) {
  .sponsor-modal-body {
    padding: 24px 16px;
  }
  .support-icon {
    width: 40px;
    height: 40px;
  }
  .title {
    font-size: 1.375rem;
  }
  .sponsor-option {
    padding: 12px;
    font-size: 1rem;
  }
}
</style>
