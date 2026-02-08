<template>
  <div class="login-page">
    <div class="login-container">
      <div class="login-card glass-panel">
        <div class="brand-section">
          <div class="brand-logo">📡</div>
          <h1 class="brand-name">HB-RF-ETH-ng</h1>
        </div>

        <div class="form-section">
          <h2 class="welcome-text">{{ t('login.title') || 'Welcome Back' }}</h2>
          <p class="subtitle-text">{{ t('login.subtitle') || 'Please enter your password to continue' }}</p>

          <BForm @submit.stop.prevent class="login-form">
            <div class="input-group-modern">
              <div class="input-icon">🔒</div>
              <input
                type="password"
                v-model="password"
                :placeholder="t('login.passwordPlaceholder') || 'Password'"
                class="modern-input"
                :class="{ 'has-error': v$.password.$error }"
                @keyup.enter="loginClick"
                autofocus
              />
            </div>
            <div v-if="v$.password.$error" class="error-text">
              {{ t('login.passwordRequired') || 'Password is required' }}
            </div>

            <BAlert
              v-if="showError"
              variant="danger"
              class="login-alert"
              dismissible
              @dismissed="showError = false"
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
              <span>{{ loading ? (t('login.loggingIn') || 'Logging in...') : t('login.login') }}</span>
            </BButton>
          </BForm>
        </div>
      </div>

      <div class="login-footer">
        <small class="version-text">v{{ sysInfoStore.currentVersion || '...' }}</small>
        <div class="links">
          <span>&copy; Xerolux 2026</span>
          <span class="separator">•</span>
          <a href="https://github.com/Xerolux/HB-RF-ETH-ng" target="_blank">GitHub</a>
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
  sysInfoStore.update().catch(() => {})
})

const loginClick = async () => {
  v$.value.$touch()
  if (v$.value.$error) return

  showError.value = false
  loading.value = true

  // Simulate network delay for smoother UX
  await new Promise(resolve => setTimeout(resolve, 500))

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
  background: linear-gradient(135deg, #f5f7fa 0%, #c3cfe2 100%);
  padding: var(--spacing-md);
}

.login-container {
  width: 100%;
  max-width: 400px;
  display: flex;
  flex-direction: column;
  gap: var(--spacing-xl);
}

.glass-panel {
  background: rgba(255, 255, 255, 0.9);
  backdrop-filter: blur(20px);
  -webkit-backdrop-filter: blur(20px);
  border-radius: 24px;
  box-shadow: 0 20px 40px rgba(0, 0, 0, 0.1);
  padding: 40px;
  border: 1px solid rgba(255, 255, 255, 0.5);
}

.brand-section {
  text-align: center;
  margin-bottom: 30px;
}

.brand-logo {
  font-size: 3rem;
  margin-bottom: 10px;
  filter: drop-shadow(0 4px 6px rgba(0,0,0,0.1));
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
  font-size: 1.25rem;
  z-index: 2;
  pointer-events: none;
}

.modern-input {
  width: 100%;
  padding: 14px 16px 14px 48px;
  border: 2px solid transparent;
  background: var(--color-bg);
  border-radius: 16px;
  font-size: 1rem;
  transition: all 0.2s ease;
  color: var(--color-text);
}

.modern-input:focus {
  background: white;
  border-color: var(--color-primary);
  box-shadow: 0 0 0 4px rgba(255, 107, 53, 0.1);
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
  border-radius: 12px;
  font-size: 0.9rem;
}

.login-btn {
  padding: 14px;
  border-radius: 16px;
  font-weight: 600;
  font-size: 1.0625rem;
  margin-top: 8px;
  box-shadow: 0 4px 12px rgba(255, 107, 53, 0.3);
  transition: all 0.2s;
}

.login-btn:hover {
  transform: translateY(-1px);
  box-shadow: 0 6px 16px rgba(255, 107, 53, 0.4);
}

.login-btn:active {
  transform: translateY(0);
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
  transition: color 0.2s;
}

.links a:hover {
  color: var(--color-primary);
}

.separator {
  margin: 0 8px;
  opacity: 0.5;
}

/* Dark Mode Overrides */
@media (prefers-color-scheme: dark) {
  .login-page {
    background: linear-gradient(135deg, #1a202c 0%, #2d3748 100%);
  }

  .glass-panel {
    background: rgba(30, 30, 30, 0.8);
    border-color: rgba(255, 255, 255, 0.1);
  }

  .modern-input {
    background: rgba(0, 0, 0, 0.3);
    color: white;
  }

  .modern-input:focus {
    background: rgba(0, 0, 0, 0.5);
  }
}
</style>
