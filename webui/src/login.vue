<template>
  <div class="login-page">
    <div class="login-container">
      <div class="login-card glass-panel">
        <div class="brand-section">
          <span class="brand-logo"><AppIcon name="router" /></span>
          <h1 class="brand-name">HB-RF-ETH-ng</h1>
        </div>

        <div class="form-section">
          <h2 class="welcome-text">{{ t('login.title') }}</h2>
          <p class="subtitle-text">{{ t('login.subtitle') }}</p>

          <BForm @submit.stop.prevent class="login-form">
            <div class="input-group-modern">
              <span class="input-icon"><AppIcon name="lock" /></span>
              <input
                type="password"
                v-model="password"
                :placeholder="t('login.passwordPlaceholder')"
                class="modern-input"
                :class="{ 'has-error': v$.password.$error }"
                @keyup.enter="loginClick"
                autofocus
              />
            </div>
            <div v-if="v$.password.$error" class="error-text">
              {{ t('login.passwordRequired') }}
            </div>

            <BAlert
              :model-value="showError"
              variant="danger"
              class="login-alert"
              dismissible
              @close="showError = false"
            >
              {{ t('login.loginError') }}
            </BAlert>

            <BButton
              variant="primary"
              block
              size="lg"
              @click="loginClick"
              :disabled="!password || loading"
              class="login-btn"
            >
              <span v-if="loading" class="spinner-border spinner-border-sm me-2"></span>
              <span>{{ loading ? t('login.loggingIn') : t('login.login') }}</span>
            </BButton>
          </BForm>
        </div>
      </div>

      <div class="login-footer">
        <small class="version-text">v{{ sysInfoStore.currentVersion || t('common.loading') }}</small>
        <div class="links">
          <span>&copy; Xerolux 2026</span>
          <span class="separator">•</span>
          <a href="https://github.com/Xerolux/HB-RF-ETH-ng" target="_blank" rel="noopener noreferrer">GitHub</a>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue'
import { useRouter, useRoute } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { useVuelidate } from '@vuelidate/core'
import { required } from '@vuelidate/validators'
import { useLoginStore, useSysInfoStore } from './stores.js'

const { t } = useI18n()

const router = useRouter()
const route = useRoute()
const loginStore = useLoginStore()
const sysInfoStore = useSysInfoStore()

const password = ref('')
const showError = ref(false)
const loading = ref(false)

const rules = {
  password: { required }
}

const v$ = useVuelidate(rules, { password })

onMounted(() => {
  sysInfoStore.update().catch((error) => {
    console.warn('Failed to load system info on login page:', error)
  })
})

const loginClick = async () => {
  v$.value.$touch()
  if (v$.value.$error) return

  showError.value = false
  loading.value = true

  const success = await loginStore.tryLogin(password.value)
  loading.value = false

  if (success) {
    router.push(route.query.redirect || '/')
  } else {
    showError.value = true
    password.value = ''
    v$.value.$reset()
  }
}
</script>

<style scoped>
.login-page {
  min-height: 100vh;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: var(--spacing-md);
}

.login-container {
  width: 100%;
  max-width: 400px;
  display: flex;
  flex-direction: column;
  gap: var(--spacing-xl);
}

.login-card {
  border-radius: var(--radius-xl);
  padding: 40px;
}

.brand-section {
  text-align: center;
  margin-bottom: 30px;
}

.brand-logo {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 64px;
  height: 64px;
  margin: 0 auto 10px;
  border-radius: var(--radius-lg);
  color: white;
  background: linear-gradient(135deg, var(--color-primary), var(--color-primary-strong));
  box-shadow: var(--shadow-md);
  font-size: 1.8rem;
}

.brand-name {
  font-size: 1.5rem;
  font-weight: 800;
  color: var(--color-text);
  margin: 0;
  letter-spacing: -0.03em;
}

.welcome-text {
  font-size: 1.25rem;
  font-weight: 700;
  margin-bottom: 8px;
  color: var(--color-text);
}

.subtitle-text {
  font-size: 0.9375rem;
  color: var(--color-text-secondary);
  margin-bottom: 24px;
}

.login-form {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.input-group-modern {
  position: relative;
  display: flex;
  align-items: center;
}

.input-icon {
  position: absolute;
  left: 16px;
  z-index: 2;
  pointer-events: none;
  color: var(--color-text-muted);
  font-size: 1.25rem;
  display: inline-flex;
}

.modern-input {
  width: 100%;
  padding: 14px 16px 14px 48px;
  border: 1px solid transparent;
  background: var(--color-bg);
  border-radius: var(--radius-md);
  font-size: 1rem;
  transition: border-color var(--transition-fast), box-shadow var(--transition-fast), background var(--transition-fast);
  color: var(--color-text);
}

.modern-input:focus {
  background: var(--color-surface);
  border-color: rgba(242, 106, 61, 0.38);
  box-shadow: 0 0 0 4px rgba(242, 106, 61, 0.12);
  outline: none;
}

.modern-input.has-error {
  border-color: var(--color-danger);
  background: var(--color-danger-light);
}

.error-text {
  color: var(--color-danger);
  font-size: 0.8125rem;
  margin-top: -8px;
  margin-left: 12px;
}

.login-alert {
  border-radius: var(--radius-md);
  font-size: 0.9rem;
}

.login-btn {
  padding: 14px;
  border-radius: var(--radius-md);
  font-weight: 600;
  font-size: 1.0625rem;
  margin-top: 8px;
  box-shadow: 0 14px 28px rgba(242, 106, 61, 0.22);
  transition: transform var(--transition-fast), box-shadow var(--transition-fast);
}

.login-btn:hover:not(:disabled) {
  transform: translateY(-1px);
  box-shadow: 0 18px 32px rgba(242, 106, 61, 0.28);
}

.login-btn:active:not(:disabled) {
  transform: scale(0.98);
}

.login-footer {
  text-align: center;
  color: var(--color-text-secondary);
}

.version-text {
  display: block;
  margin-bottom: 8px;
  opacity: 0.7;
}

.links {
  font-size: 0.875rem;
}

.links a {
  color: var(--color-text-secondary);
  text-decoration: none;
  font-weight: 500;
  transition: color var(--transition-fast);
}

.links a:hover {
  color: var(--color-primary);
}

.separator {
  margin: 0 8px;
  opacity: 0.5;
}

@media (max-width: 480px) {
  .login-page {
    padding: var(--spacing-sm);
  }
  .login-card {
    padding: 24px 18px;
    border-radius: var(--radius-lg);
  }
  .brand-section {
    margin-bottom: 18px;
  }
  .brand-logo {
    width: 56px;
    height: 56px;
    font-size: 1.5rem;
  }
  .brand-name {
    font-size: 1.2rem;
  }
  .welcome-text {
    font-size: 1rem;
  }
}
</style>
