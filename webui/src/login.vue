<template>
  <div class="login-page">
    <BCard class="login-card">
      <template #header>
        <div class="login-header">
          <span class="login-icon">🔐</span>
          <h2 class="login-title">{{ t('login.title') }}</h2>
          <p class="login-subtitle">{{ t('login.subtitle') || 'Please enter your password to continue' }}</p>
        </div>
      </template>

      <BForm @submit.stop.prevent class="login-form">
        <BFormGroup class="form-group-custom">
          <template #label>
            <span class="form-label-custom">
              <span class="label-icon">🔑</span>
              {{ t('login.password') }}
            </span>
          </template>
          <BFormInput
            type="password"
            v-model="password"
            :placeholder="t('login.passwordPlaceholder') || 'Enter your password'"
            :state="v$.password.$error ? false : null"
            class="form-input-custom"
            size="lg"
            @keyup.enter="loginClick"
          />
          <BFormInvalidFeedback v-if="v$.password.$error">
            {{ t('login.passwordRequired') || 'Password is required' }}
          </BFormInvalidFeedback>
        </BFormGroup>

        <BAlert
          variant="danger"
          :model-value="showError"
          dismissible
          fade
          @update:model-value="showError = null"
          class="login-alert"
        >
          <span class="alert-icon">⚠️</span>
          {{ t('login.loginError') }}
        </BAlert>

        <BButton
          variant="primary"
          block
          size="lg"
          @click="loginClick"
          :disabled="!password || password === '' || loading"
          class="login-btn"
        >
          <span v-if="loading" class="spinner-border spinner-border-sm me-2"></span>
          <span>{{ loading ? (t('login.loggingIn') || 'Logging in...') : t('login.login') }}</span>
        </BButton>
      </BForm>
    </BCard>

    <div class="login-footer">
      <small class="text-muted">HB-RF-ETH-ng v2.1.1</small>
    </div>
  </div>
</template>

<script setup>
import { ref } from 'vue'
import { useRouter, useRoute } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { useVuelidate } from '@vuelidate/core'
import { required } from '@vuelidate/validators'
import { useLoginStore } from './stores.js'

const { t } = useI18n()

const router = useRouter()
const route = useRoute()
const loginStore = useLoginStore()

const password = ref('')
const showError = ref(null)
const loading = ref(false)

const rules = {
  password: { required }
}

const v$ = useVuelidate(rules, { password })

const loginClick = async () => {
  showError.value = null
  loading.value = true

  const success = await loginStore.tryLogin(password.value)
  loading.value = false

  if (success) {
    router.push(route.query.redirect || '/')
  } else {
    showError.value = 10
    password.value = ''
  }
}
</script>

<style scoped>
.login-page {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  min-height: 60vh;
  padding: var(--spacing-md);
}

.login-card {
  width: 100%;
  max-width: 420px;
  border: none;
  box-shadow: var(--shadow-xl);
  overflow: visible;
}

.login-card :deep(.card-header) {
  background: linear-gradient(135deg, var(--color-primary) 0%, var(--color-primary-dark) 100%);
  border-bottom: none;
  padding: var(--spacing-xl) var(--spacing-lg);
  text-align: center;
}

.login-header {
  color: white;
}

.login-icon {
  font-size: 3rem;
  display: block;
  margin-bottom: var(--spacing-md);
  filter: drop-shadow(0 2px 8px rgba(0, 0, 0, 0.2));
}

.login-title {
  font-size: 1.5rem;
  font-weight: 700;
  margin: 0 0 var(--spacing-sm) 0;
}

.login-subtitle {
  font-size: 0.9375rem;
  opacity: 0.9;
  margin: 0;
}

.login-card :deep(.card-body) {
  padding: var(--spacing-xl) var(--spacing-lg);
}

.login-form {
  display: flex;
  flex-direction: column;
  gap: var(--spacing-md);
}

.form-group-custom {
  margin-bottom: var(--spacing-lg);
}

.form-label-custom {
  display: flex;
  align-items: center;
  gap: var(--spacing-sm);
  font-weight: 600;
  color: var(--color-text);
  margin-bottom: var(--spacing-sm);
}

.label-icon {
  font-size: 1.125rem;
}

.form-input-custom {
  border: 2px solid var(--color-border);
  border-radius: var(--radius-md);
  padding: 0.875rem 1rem;
  font-size: 1rem;
  transition: all var(--transition-fast);
}

.form-input-custom:focus {
  border-color: var(--color-primary);
  box-shadow: 0 0 0 4px rgba(255, 107, 53, 0.1);
}

.login-alert {
  border-radius: var(--radius-md);
  padding: var(--spacing-md) var(--spacing-lg);
  display: flex;
  align-items: center;
  gap: var(--spacing-sm);
}

.alert-icon {
  font-size: 1.25rem;
}

.login-btn {
  height: var(--touch-target-lg);
  font-weight: 600;
  letter-spacing: 0.025em;
  margin-top: var(--spacing-sm);
}

.login-footer {
  margin-top: var(--spacing-xl);
  text-align: center;
}

/* Mobile adjustments */
@media (max-width: 480px) {
  .login-page {
    padding: var(--spacing-sm);
  }

  .login-card :deep(.card-header),
  .login-card :deep(.card-body) {
    padding: var(--spacing-lg) var(--spacing-md);
  }

  .login-icon {
    font-size: 2.5rem;
  }

  .login-title {
    font-size: 1.25rem;
  }
}
</style>
