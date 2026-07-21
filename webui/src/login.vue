<template>
  <div class="login-page">
    <div class="login-container">
      <div class="login-card glass-panel">
        <div class="brand-section">
          <span class="brand-logo"><BrandLogo :size="56" /></span>
          <h1 class="brand-name">HB-RF-ETH-ng</h1>
        </div>

        <div class="form-section">
          <h2 class="welcome-text">{{ t('login.title') }}</h2>
          <p class="subtitle-text">{{ t('login.subtitle') }}</p>

          <BForm @submit.stop.prevent class="login-form">
            <div class="input-group-modern">
              <span class="input-icon"><AppIcon name="user" /></span>
              <input
                type="text"
                name="username"
                v-model.trim="username"
                :placeholder="t('login.usernamePlaceholder')"
                class="modern-input"
                :class="{ 'has-error': v$.username.$error }"
                autocomplete="username"
                autocapitalize="none"
                autocorrect="off"
                spellcheck="false"
                autofocus
                @keyup.enter="loginClick"
              />
            </div>
            <div v-if="v$.username.$error" class="error-text">
              {{ t('login.usernameRequired') }}
            </div>

            <div class="input-group-modern">
              <span class="input-icon"><AppIcon name="lock" /></span>
              <input
                type="password"
                name="password"
                v-model="password"
                :placeholder="t('login.passwordPlaceholder')"
                class="modern-input"
                :class="{ 'has-error': v$.password.$error }"
                autocomplete="current-password"
                @keyup.enter="loginClick"
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
              :disabled="!username || !password || loading"
              class="login-btn"
            >
              <span v-if="loading" class="spinner-border spinner-border-sm me-2"></span>
              <span>{{ loading ? t('login.loggingIn') : t('login.login') }}</span>
            </BButton>

            <button type="button" class="forgot-link" @click="startPasswordReset" :disabled="resetLoading">
              {{ t('login.forgotPassword') }}
            </button>
          </BForm>

          <div v-if="resetMode" class="password-reset-panel">
            <div v-if="resetStep === 'waiting'" class="reset-status">
              <strong>{{ t('login.passwordResetTitle') }}</strong>
              <p>{{ t('login.passwordResetPressButton') }}</p>
              <p class="reset-countdown">{{ t('login.passwordResetExpires', { seconds: resetCountdown }) }}</p>
              <BButton variant="outline-secondary" size="sm" @click="cancelPasswordReset">
                {{ t('common.cancel') }}
              </BButton>
            </div>

            <div v-else-if="resetStep === 'confirmed'" class="reset-status">
              <strong>{{ t('login.passwordResetConfirmed') }}</strong>
              <p>{{ t('login.passwordResetEnterNew') }}</p>
              <div class="input-group-modern">
                <span class="input-icon"><AppIcon name="lock" /></span>
                <input
                  type="password"
                  v-model="resetNewPassword"
                  :placeholder="t('login.newPasswordPlaceholder')"
                  class="modern-input"
                  autocomplete="new-password"
                />
              </div>
              <div class="input-group-modern">
                <span class="input-icon"><AppIcon name="lock" /></span>
                <input
                  type="password"
                  v-model="resetConfirmPassword"
                  :placeholder="t('login.confirmPasswordPlaceholder')"
                  class="modern-input"
                  autocomplete="new-password"
                  @keyup.enter="completePasswordReset"
                />
              </div>
              <div v-if="resetPasswordMessage" class="error-text">{{ resetPasswordMessage }}</div>
              <div class="reset-actions">
                <BButton variant="secondary" size="sm" @click="cancelPasswordReset">
                  {{ t('common.cancel') }}
                </BButton>
                <BButton
                  variant="primary"
                  size="sm"
                  @click="completePasswordReset"
                  :disabled="!canCompleteReset || resetCompleteLoading"
                >
                  <span v-if="resetCompleteLoading" class="spinner-border spinner-border-sm me-2"></span>
                  {{ t('login.passwordResetSave') }}
                </BButton>
              </div>
            </div>

            <BAlert
              :model-value="!!resetError"
              variant="danger"
              class="login-alert"
              dismissible
              @close="resetError = ''"
            >
              {{ resetError }}
            </BAlert>
          </div>
        </div>
      </div>

      <div class="login-footer">
        <small class="version-text">v{{ sysInfoStore.currentVersion || t('common.loading') }}</small>
        <div class="links">
          <span>{{ t('login.footerCopyright') }}</span>
          <span class="separator">•</span>
          <a href="https://github.com/Xerolux/HB-RF-ETH-ng" target="_blank" rel="noopener noreferrer">{{ t('login.githubLink') }}</a>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed, ref, onMounted, onUnmounted } from 'vue'
import { useRouter, useRoute } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { useVuelidate } from '@vuelidate/core'
import { required } from '@vuelidate/validators'
import axios from 'axios'
import { useLoginStore, useSysInfoStore } from './stores.js'
import BrandLogo from './components/BrandLogo.vue'

const { t } = useI18n()

const router = useRouter()
const route = useRoute()
const loginStore = useLoginStore()
const sysInfoStore = useSysInfoStore()

const username = ref('')
const password = ref('')
const showError = ref(false)
const loading = ref(false)
const resetMode = ref(false)
const resetStep = ref('idle')
const resetLoading = ref(false)
const resetCompleteLoading = ref(false)
const resetError = ref('')
const resetCountdown = ref(0)
const resetToken = ref('')
const resetPollTimer = ref(null)
const resetNewPassword = ref('')
const resetConfirmPassword = ref('')

const passwordPolicy = /^(?=.*[a-z])(?=.*[A-Z])(?=.*\d).{8,32}$/

const resetPasswordMessage = computed(() => {
  if (!resetNewPassword.value && !resetConfirmPassword.value) return ''
  if (!passwordPolicy.test(resetNewPassword.value)) return t('login.passwordResetPolicy')
  if (resetNewPassword.value !== resetConfirmPassword.value) return t('login.passwordResetMismatch')
  return ''
})

const canCompleteReset = computed(() => {
  return passwordPolicy.test(resetNewPassword.value) && resetNewPassword.value === resetConfirmPassword.value
})

const rules = {
  username: { required },
  password: { required }
}

const v$ = useVuelidate(rules, { username, password })

onMounted(() => {
  sysInfoStore.update().catch((error) => {
    console.warn('Failed to load system info on login page:', error)
  })
})

onUnmounted(() => {
  stopResetPolling()
})

const stopResetPolling = () => {
  if (resetPollTimer.value) {
    clearInterval(resetPollTimer.value)
    resetPollTimer.value = null
  }
}

const pollPasswordReset = async () => {
  if (!resetToken.value) return

  try {
    const response = await axios.post('/api/password-reset/status', { token: resetToken.value }, { silent: true })
    resetCountdown.value = response.data?.expiresIn || 0

    if (response.data?.confirmed) {
      stopResetPolling()
      resetStep.value = 'confirmed'
      return
    }

    if (!response.data?.active) {
      stopResetPolling()
      resetError.value = t('login.passwordResetExpired')
      resetStep.value = 'idle'
    }
  } catch (error) {
    stopResetPolling()
    resetError.value = t('login.passwordResetError')
    resetStep.value = 'idle'
  }
}

const startPasswordReset = async () => {
  resetMode.value = true
  resetStep.value = 'waiting'
  resetLoading.value = true
  resetError.value = ''
  resetNewPassword.value = ''
  resetConfirmPassword.value = ''
  stopResetPolling()

  try {
    const response = await axios.post('/api/password-reset/start', {}, { silent: true })
    resetToken.value = response.data?.token || ''
    resetCountdown.value = response.data?.expiresIn || 90
    resetPollTimer.value = setInterval(pollPasswordReset, 1000)
  } catch (error) {
    resetStep.value = 'idle'
    resetError.value = t('login.passwordResetError')
  } finally {
    resetLoading.value = false
  }
}

const cancelPasswordReset = () => {
  stopResetPolling()
  resetMode.value = false
  resetStep.value = 'idle'
  resetError.value = ''
  resetToken.value = ''
  resetNewPassword.value = ''
  resetConfirmPassword.value = ''
}

const completePasswordReset = async () => {
  if (!canCompleteReset.value || resetCompleteLoading.value) return

  resetCompleteLoading.value = true
  resetError.value = ''

  try {
    const response = await axios.post('/api/password-reset/complete', {
      token: resetToken.value,
      newPassword: resetNewPassword.value
    })

    if (response.data?.success && response.data?.token) {
      loginStore.login(response.data.token)
      loginStore.setPasswordChanged(true)
      cancelPasswordReset()
      router.push(route.query.redirect || '/')
    } else {
      resetError.value = t('login.passwordResetError')
    }
  } catch (error) {
    resetError.value = t('login.passwordResetError')
  } finally {
    resetCompleteLoading.value = false
  }
}

const loginClick = async () => {
  v$.value.$touch()
  if (v$.value.$error) return

  showError.value = false
  loading.value = true

  const success = await loginStore.tryLogin(username.value, password.value)
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
  /* 100dvh accounts for the iOS Safari dynamic toolbar; 100vh is the fallback
     for older browsers that do not understand dvh. */
  min-height: 100vh;
  min-height: 100dvh;
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
}

.brand-name {
  font-size: var(--fs-2xl);
  font-weight: 800;
  color: var(--color-text);
  margin: 0;
  letter-spacing: -0.03em;
}

.welcome-text {
  font-size: var(--fs-xl);
  font-weight: 700;
  margin-bottom: 8px;
  color: var(--color-text);
}

.subtitle-text {
  font-size: var(--fs-sm);
  color: var(--color-text-secondary);
  margin-bottom: 24px;
}

.password-reset-panel {
  margin-top: 20px;
  padding: 18px;
  border-radius: var(--radius-lg);
  background: var(--color-bg);
  border: 1px solid var(--color-border);
}

.reset-status {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.reset-status p {
  margin: 0;
  color: var(--color-text-secondary);
  font-size: var(--fs-sm);
}

.reset-countdown {
  font-weight: 700;
  color: var(--color-primary) !important;
}

.reset-actions {
  display: flex;
  justify-content: flex-end;
  gap: 10px;
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
  font-size: var(--fs-xl);
  display: inline-flex;
}

.modern-input {
  width: 100%;
  padding: 14px 16px 14px 48px;
  border: 1px solid transparent;
  background: var(--color-bg);
  border-radius: var(--radius-md);
  font-size: var(--fs-md);
  transition: border-color var(--transition-fast), box-shadow var(--transition-fast), background var(--transition-fast);
  color: var(--color-text);
}

.modern-input:focus {
  background: var(--color-surface);
  border-color: var(--color-primary);
  box-shadow: 0 0 0 4px var(--color-primary-soft);
  outline: none;
}

.modern-input.has-error {
  border-color: var(--color-danger);
  background: var(--color-danger-light);
}

.error-text {
  color: var(--color-danger);
  font-size: var(--fs-xs);
  margin-top: -8px;
  margin-left: 12px;
}

.login-alert {
  border-radius: var(--radius-md);
  font-size: var(--fs-sm);
}

.login-btn {
  padding: 14px;
  border-radius: var(--radius-md);
  font-weight: 600;
  font-size: var(--fs-lg);
  margin-top: 8px;
  /* Neutral elevation shadow (token-driven) so the glow follows the active
     theme instead of hardcoding the Glass-UI orange. The accent colour comes
     from the button fill, not the shadow. */
  box-shadow: var(--shadow-md);
  transition: transform var(--transition-fast), box-shadow var(--transition-fast);
}

.login-btn:hover:not(:disabled) {
  transform: translateY(-1px);
  box-shadow: var(--shadow-lg);
}

.login-btn:active:not(:disabled) {
  transform: scale(0.98);
}

.forgot-link {
  align-self: center;
  border: 0;
  background: transparent;
  color: var(--color-primary);
  font-weight: 700;
  padding: 4px 8px;
}

.forgot-link:disabled {
  opacity: 0.6;
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
  font-size: var(--fs-xs);
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
    font-size: var(--fs-2xl);
  }
  .brand-name {
    font-size: var(--fs-lg);
  }
  .welcome-text {
    font-size: var(--fs-md);
  }
}
</style>
