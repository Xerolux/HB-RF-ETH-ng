<template>
  <BModal
    v-model="showModal"
    :title="t('otaPassword.title') || 'Set OTA Password'"
    :title-class="'modal-title-custom'"
    :header-class="'modal-header-custom'"
    :body-class="'modal-body-custom'"
    :footer-class="'modal-footer-custom'"
    centered
    no-fade
    @hide="handleHide"
  >
    <div class="ota-password-modal">
      <div class="modal-icon">🔒</div>
      <p class="modal-description">
        {{ t('otaPassword.warningMessage') || 'Set a separate password for firmware updates. This is required for OTA updates.' }}
      </p>

      <BForm @submit.stop.prevent="handleSubmit">
        <BFormGroup :label="t('otaPassword.otaPassword') || 'OTA Password'">
          <BFormInput
            type="password"
            v-model="otaPassword"
            :placeholder="t('otaPassword.otaPasswordPlaceholder') || 'Enter OTA password'"
            :state="v$.otaPassword.$error ? false : null"
            size="lg"
          />
          <BFormInvalidFeedback v-if="v$.otaPassword.minLength.$invalid">
            {{ t('otaPassword.passwordTooShort') || 'Password must be at least 8 characters' }}
          </BFormInvalidFeedback>
          <BFormInvalidFeedback v-else-if="v$.otaPassword.password_validator.$invalid">
            {{ t('otaPassword.passwordRequirements') || 'Must contain uppercase, lowercase, and numbers' }}
          </BFormInvalidFeedback>
        </BFormGroup>

        <!-- Password Strength Indicator -->
        <div v-if="otaPassword" class="password-strength">
          <div class="strength-bar">
            <div
              class="strength-fill"
              :class="strengthClass"
              :style="{ width: strengthPercentage + '%' }"
            ></div>
          </div>
          <span class="strength-text" :class="strengthClass">
            {{ strengthText }}
          </span>
        </div>

        <BFormGroup :label="t('otaPassword.confirmPassword') || 'Confirm Password'">
          <BFormInput
            type="password"
            v-model="confirmPassword"
            :placeholder="t('otaPassword.confirmPasswordPlaceholder') || 'Confirm OTA password'"
            :state="v$.confirmPassword.$error ? false : null"
            size="lg"
            @keyup.enter="handleSubmit"
          />
          <BFormInvalidFeedback v-if="v$.confirmPassword.sameAs.$invalid">
            {{ t('otaPassword.passwordsDoNotMatch') || 'Passwords do not match' }}
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
          <span class="alert-icon">⚠️</span>
          {{ error }}
        </BAlert>

        <div class="password-requirements">
          <span class="req-icon">📋</span>
          <div class="req-content">
            <strong>{{ t('otaPassword.requirementsTitle') || 'Password requirements:' }}</strong>
            <ul>
              <li>{{ t('otaPassword.reqMinLength') || 'At least 8 characters' }}</li>
              <li>{{ t('otaPassword.reqMixedCase') || 'Uppercase and lowercase letters' }}</li>
              <li>{{ t('otaPassword.reqNumbers') || 'At least one number' }}</li>
            </ul>
          </div>
        </div>
      </BForm>
    </div>

    <template #footer>
      <div class="modal-footer-content">
        <BButton
          variant="secondary"
          @click="closeModal"
          :disabled="loading"
          class="cancel-btn"
        >
          {{ t('common.cancel') || 'Cancel' }}
        </BButton>
        <BButton
          variant="primary"
          @click="handleSubmit"
          :disabled="v$.$invalid || loading"
          class="save-btn"
        >
          <span v-if="loading" class="spinner-border spinner-border-sm me-2"></span>
          <span>{{ loading ? (t('common.saving') || 'Saving...') : (t('common.save') || 'Save') }}</span>
        </BButton>
      </div>
    </template>
  </BModal>
</template>

<script setup>
import { ref, computed, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useVuelidate } from '@vuelidate/core'
import { required, minLength, sameAs, helpers } from '@vuelidate/validators'
import axios from 'axios'

const { t } = useI18n()

const props = defineProps({
  modelValue: {
    type: Boolean,
    default: false
  }
})

const emit = defineEmits(['update:modelValue', 'success'])

const showModal = ref(props.modelValue)
const otaPassword = ref('')
const confirmPassword = ref('')
const error = ref(null)
const loading = ref(false)

// Updated password requirements: min 8 chars, uppercase, lowercase, digit
const password_validator = helpers.regex(/^(?=.*[a-z])(?=.*[A-Z])(?=.*\d).{8,}$/)

// Password strength calculation (0-100)
const passwordStrength = computed(() => {
  const pwd = otaPassword.value
  if (!pwd) return 0

  let strength = 0

  // Length contribution (up to 40 points)
  strength += Math.min(pwd.length * 5, 40)

  // Character variety (up to 60 points)
  const hasLower = /[a-z]/.test(pwd)
  const hasUpper = /[A-Z]/.test(pwd)
  const hasDigit = /\d/.test(pwd)
  const hasSpecial = /[^a-zA-Z0-9]/.test(pwd)

  if (hasLower) strength += 10
  if (hasUpper) strength += 10
  if (hasDigit) strength += 15
  if (hasSpecial) strength += 25

  return Math.min(strength, 100)
})

// Strength class for styling
const strengthClass = computed(() => {
  if (passwordStrength.value < 30) return 'weak'
  if (passwordStrength.value < 60) return 'medium'
  if (passwordStrength.value < 80) return 'good'
  return 'strong'
})

// Strength percentage for visual bar
const strengthPercentage = computed(() => {
  return passwordStrength.value
})

// Strength text label
const strengthText = computed(() => {
  const score = passwordStrength.value
  if (score < 30) return t('otaPassword.strengthWeak') || 'Weak'
  if (score < 60) return t('otaPassword.strengthMedium') || 'Medium'
  if (score < 80) return t('otaPassword.strengthGood') || 'Good'
  return t('otaPassword.strengthStrong') || 'Strong'
})

const rules = computed(() => ({
  otaPassword: { required, minLength: minLength(8), password_validator },
  confirmPassword: { required, sameAs: sameAs(otaPassword.value) }
}))

const v$ = useVuelidate(rules, { otaPassword, confirmPassword })

// Watch for prop changes
watch(() => props.modelValue, (newVal) => {
  showModal.value = newVal
  if (newVal) {
    // Reset form when opening
    otaPassword.value = ''
    confirmPassword.value = ''
    error.value = null
    v$.value.$reset()
  }
})

watch(showModal, (newVal) => {
  emit('update:modelValue', newVal)
})

const handleHide = () => {
  return true
}

const closeModal = () => {
  showModal.value = false
}

const handleSubmit = async () => {
  if (v$.value.$invalid || loading.value) return

  error.value = null
  loading.value = true

  try {
    const response = await axios.post('/api/set-ota-password', {
      otaPassword: otaPassword.value
    })

    if (response.data.success) {
      emit('success')
      showModal.value = false
    } else {
      error.value = response.data.error || 'Unknown error'
    }
  } catch (e) {
    error.value = e.response?.data?.error || e.message || 'Failed to set OTA password'
  } finally {
    loading.value = false
  }
}
</script>

<style scoped>
:deep(.modal-content) {
  border-radius: var(--radius-xl);
  border: none;
  box-shadow: var(--shadow-xl);
  overflow: hidden;
}

:deep(.modal-header-custom) {
  background: linear-gradient(135deg, var(--color-primary) 0%, var(--color-primary-dark) 100%);
  border-bottom: none;
  padding: var(--spacing-lg);
}

:deep(.modal-title-custom) {
  color: white;
  font-weight: 600;
  font-size: 1.25rem;
}

:deep(.modal-body-custom) {
  padding: var(--spacing-xl) var(--spacing-lg);
}

:deep(.modal-footer-custom) {
  border-top: 1px solid var(--color-border);
  padding: var(--spacing-lg);
  background: var(--color-bg);
}

:deep(.btn-close) {
  filter: brightness(0) invert(1);
}

.ota-password-modal {
  display: flex;
  flex-direction: column;
  align-items: center;
  text-align: center;
}

.modal-icon {
  font-size: 3rem;
  margin-bottom: var(--spacing-md);
  filter: drop-shadow(0 2px 8px rgba(0, 0, 0, 0.15));
}

.modal-description {
  color: var(--color-text-secondary);
  margin-bottom: var(--spacing-lg);
}

.ota-password-modal :deep(.form-label) {
  font-weight: 600;
  color: var(--color-text);
}

.ota-password-modal :deep(.form-control) {
  border: 2px solid var(--color-border);
  border-radius: var(--radius-md);
  padding: 0.75rem 1rem;
}

.ota-password-modal :deep(.form-control:focus) {
  border-color: var(--color-primary);
  box-shadow: 0 0 0 4px rgba(255, 107, 53, 0.1);
}

.alert-icon {
  margin-right: var(--spacing-sm);
}

/* Password Strength Indicator */
.password-strength {
  display: flex;
  align-items: center;
  gap: var(--spacing-sm);
  margin-top: var(--spacing-sm);
}

.strength-bar {
  flex: 1;
  height: 6px;
  background: var(--color-bg);
  border-radius: var(--radius-full);
  overflow: hidden;
}

.strength-fill {
  height: 100%;
  transition: width 0.3s ease, background-color 0.3s ease;
}

.strength-fill.weak {
  background: linear-gradient(90deg, #ef4444 0%, #dc2626 100%);
}

.strength-fill.medium {
  background: linear-gradient(90deg, #f59e0b 0%, #d97706 100%);
}

.strength-fill.good {
  background: linear-gradient(90deg, #10b981 0%, #059669 100%);
}

.strength-fill.strong {
  background: linear-gradient(90deg, #3b82f6 0%, #2563eb 100%);
}

.strength-text {
  font-size: 0.75rem;
  font-weight: 600;
  text-transform: uppercase;
  min-width: 50px;
  text-align: right;
}

.strength-text.weak {
  color: #ef4444;
}

.strength-text.medium {
  color: #f59e0b;
}

.strength-text.good {
  color: #10b981;
}

.strength-text.strong {
  color: #3b82f6;
}

.password-requirements {
  display: flex;
  align-items: flex-start;
  gap: var(--spacing-md);
  padding: var(--spacing-md);
  background: var(--color-bg);
  border-radius: var(--radius-md);
  margin-top: var(--spacing-lg);
  text-align: left;
}

.req-icon {
  font-size: 1.5rem;
  flex-shrink: 0;
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

.modal-footer-content {
  display: flex;
  gap: var(--spacing-md);
  width: 100%;
  justify-content: flex-end;
}

.cancel-btn,
.save-btn {
  min-width: 120px;
}

.save-btn {
  background: linear-gradient(135deg, var(--color-primary) 0%, var(--color-primary-dark) 100%);
  border: none;
}

.save-btn:hover:not(:disabled) {
  transform: translateY(-1px);
  box-shadow: var(--shadow-md);
}

/* Mobile adjustments */
@media (max-width: 576px) {
  :deep(.modal-body-custom) {
    padding: var(--spacing-lg) var(--spacing-md);
  }

  .modal-footer-content {
    flex-direction: column;
  }

  .cancel-btn,
  .save-btn {
    width: 100%;
  }
}
</style>
