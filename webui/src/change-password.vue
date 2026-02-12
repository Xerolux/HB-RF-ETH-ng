<template>
  <div class="change-password-page">
    <BCard class="change-password-card">
      <template #header>
        <div class="card-header-content">
          <span class="lock-icon">🔐</span>
          <div>
            <h2 class="card-title">{{ t('changePassword.title') }}</h2>
            <p class="card-subtitle">{{ t('changePassword.subtitle') }}</p>
          </div>
        </div>
      </template>

      <div class="card-body-content">
        <BAlert show variant="warning" class="warning-alert">
          <span class="alert-icon">⚠️</span>
          <div class="alert-content">
            <strong>{{ t('changePassword.warningTitle') }}</strong>
            {{ t('changePassword.warningMessage') }}
          </div>
        </BAlert>

        <div class="password-requirements">
          <span class="req-icon">📋</span>
          <div class="req-content">
            <strong>{{ t('changePassword.requirementsTitle') }}</strong>
            <ul>
              <li>{{ t('changePassword.reqMinLength') }}</li>
              <li>{{ t('changePassword.reqLettersNumbers') }}</li>
            </ul>
          </div>
        </div>

        <BForm @submit.stop.prevent="handleSubmit">
          <BFormGroup :label="t('changePassword.newPassword')">
            <BFormInput
              type="password"
              v-model="newPassword"
              :placeholder="t('changePassword.newPasswordPlaceholder')"
              :state="v$.newPassword.$error ? false : null"
              size="lg"
            />
            <BFormInvalidFeedback v-if="v$.newPassword.minLength.$invalid">
              {{ t('changePassword.passwordTooShort') }}
            </BFormInvalidFeedback>
            <BFormInvalidFeedback v-else-if="v$.newPassword.password_validator.$invalid">
              {{ t('changePassword.passwordRequirements') }}
            </BFormInvalidFeedback>
          </BFormGroup>

          <BFormGroup :label="t('changePassword.confirmPassword')">
            <BFormInput
              type="password"
              v-model="confirmPassword"
              :placeholder="t('changePassword.confirmPasswordPlaceholder')"
              :state="v$.confirmPassword.$error ? false : null"
              size="lg"
              @keyup.enter="handleSubmit"
            />
            <BFormInvalidFeedback v-if="v$.confirmPassword.sameAs.$invalid">
              {{ t('changePassword.passwordsDoNotMatch') }}
            </BFormInvalidFeedback>
          </BFormGroup>

          <BAlert
            variant="danger"
            :model-value="!!error"
            dismissible
            fade
            @update:model-value="error = null"
            class="mt-3"
          >
            <span class="alert-icon">❌</span>
            {{ error }}
          </BAlert>

          <BButton
            variant="primary"
            block
            size="lg"
            type="submit"
            :disabled="loading"
            class="submit-btn"
          >
            <span v-if="loading" class="spinner-border spinner-border-sm me-2"></span>
            <span>{{ loading ? t('common.changing') : t('changePassword.changePassword') }}</span>
          </BButton>
        </BForm>
      </div>
    </BCard>
  </div>
</template>

<script setup>
import { ref, computed } from 'vue'
import { useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { useVuelidate } from '@vuelidate/core'
import { required, minLength, sameAs, helpers } from '@vuelidate/validators'
import axios from 'axios'
import { useLoginStore } from './stores.js'

const { t } = useI18n()

const router = useRouter()
const loginStore = useLoginStore()

const newPassword = ref('')
const confirmPassword = ref('')
const error = ref(null)
const loading = ref(false)

const password_validator = helpers.regex(/^(?=.*[a-z])(?=.*[A-Z])(?=.*\d).{8,}$/)

const rules = computed(() => ({
  newPassword: { required, minLength: minLength(8), password_validator },
  confirmPassword: { required, sameAs: sameAs(newPassword.value) }
}))

const v$ = useVuelidate(rules, { newPassword, confirmPassword })

const handleSubmit = async () => {
  v$.value.$touch()
  if (v$.value.$invalid || loading.value) return

  error.value = null
  loading.value = true

  try {
    const response = await axios.post('/api/change-password', {
      newPassword: newPassword.value
    })

    if (response.data.success) {
      // Update token in store
      if (response.data.token) {
        loginStore.login(response.data.token)
        loginStore.setPasswordChanged(true)
      }
      // Redirect to home
      router.push('/')
    } else {
      error.value = response.data.error || 'Unknown error'
    }
  } catch (e) {
    error.value = e.response?.data?.message || e.message || 'Failed to change password'
  } finally {
    loading.value = false
  }
}
</script>

<style scoped>
.change-password-page {
  display: flex;
  justify-content: center;
  padding: var(--spacing-md);
}

.change-password-card {
  width: 100%;
  max-width: 500px;
  border: none;
  box-shadow: var(--shadow-xl);
  overflow: hidden;
}

.change-password-card :deep(.card-header) {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  border-bottom: none;
  padding: var(--spacing-xl) var(--spacing-lg);
}

.card-header-content {
  display: flex;
  align-items: center;
  gap: var(--spacing-md);
  color: white;
}

.lock-icon {
  font-size: 3rem;
  filter: drop-shadow(0 2px 8px rgba(0, 0, 0, 0.2));
}

.card-title {
  font-size: 1.5rem;
  font-weight: 700;
  margin: 0;
}

.card-subtitle {
  font-size: 0.875rem;
  opacity: 0.9;
  margin: var(--spacing-xs) 0 0 0;
}

.change-password-card :deep(.card-body) {
  padding: var(--spacing-xl) var(--spacing-lg);
}

.warning-alert {
  border-radius: var(--radius-md);
  padding: var(--spacing-md);
  margin-bottom: var(--spacing-lg);
  display: flex;
  align-items: flex-start;
  gap: var(--spacing-md);
}

.alert-icon {
  font-size: 1.5rem;
  flex-shrink: 0;
}

.alert-content {
  flex: 1;
}

.alert-content strong {
  display: block;
  margin-bottom: var(--spacing-xs);
}

.password-requirements {
  display: flex;
  align-items: flex-start;
  gap: var(--spacing-md);
  padding: var(--spacing-md);
  background: var(--color-bg);
  border-radius: var(--radius-md);
  margin-bottom: var(--spacing-lg);
}

.req-icon {
  font-size: 1.5rem;
}

.req-content {
  flex: 1;
}

.req-content strong {
  display: block;
  margin-bottom: var(--spacing-xs);
  font-size: 0.875rem;
}

.req-content ul {
  margin: 0;
  padding-left: var(--spacing-lg);
  font-size: 0.875rem;
  color: var(--color-text-secondary);
}

.req-content li {
  margin-bottom: var(--spacing-xs);
}

.submit-btn {
  height: var(--touch-target-lg);
  font-weight: 600;
  margin-top: var(--spacing-md);
}

/* Mobile adjustments */
@media (max-width: 480px) {
  .change-password-page {
    padding: var(--spacing-sm);
  }

  .change-password-card :deep(.card-header),
  .change-password-card :deep(.card-body) {
    padding: var(--spacing-lg) var(--spacing-md);
  }
}
</style>
