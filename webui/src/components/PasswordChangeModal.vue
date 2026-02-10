<template>
  <BModal
    v-model="showModal"
    :title="t('changePassword.title')"
    :title-class="'modal-title-custom'"
    :header-class="'modal-header-custom'"
    :body-class="'modal-body-custom'"
    :footer-class="'modal-footer-custom'"
    centered
    no-fade
    @hide="handleHide"
  >
    <div class="password-change-modal">
      <div class="modal-icon">🔐</div>
      <p class="modal-description">
        {{ t('changePassword.warningMessage') }}
      </p>

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
          <span class="alert-icon">⚠️</span>
          {{ error }}
        </BAlert>
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
          {{ t('common.cancel') }}
        </BButton>
        <BButton
          variant="primary"
          @click="handleSubmit"
          :disabled="v$.$invalid || loading"
          class="save-btn"
        >
          <span v-if="loading" class="spinner-border spinner-border-sm me-2"></span>
          <span>{{ loading ? t('common.changing') : t('changePassword.changePassword') }}</span>
        </BButton>
      </div>
    </template>
  </BModal>
</template>

<script setup>
import { ref, computed, watch } from 'vue'
import { useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { useVuelidate } from '@vuelidate/core'
import { required, minLength, sameAs, helpers } from '@vuelidate/validators'
import axios from 'axios'
import { useLoginStore } from '../stores.js'

const props = defineProps({
  modelValue: {
    type: Boolean,
    default: false
  },
  forceRedirect: {
    type: Boolean,
    default: true
  }
})

const emit = defineEmits(['update:modelValue', 'success'])

const { t } = useI18n()
const router = useRouter()
const loginStore = useLoginStore()

const showModal = ref(props.modelValue)
const newPassword = ref('')
const confirmPassword = ref('')
const error = ref(null)
const loading = ref(false)

const password_validator = helpers.regex(/^(?=.*[A-Za-z])(?=.*\d).{6,}$/)

const rules = computed(() => ({
  newPassword: { required, minLength: minLength(6), password_validator },
  confirmPassword: { required, sameAs: sameAs(newPassword.value) }
}))

const v$ = useVuelidate(rules, { newPassword, confirmPassword }, { $stopPropagation: true })

// Watch for prop changes
watch(() => props.modelValue, (newVal) => {
  showModal.value = newVal
  if (newVal) {
    // Reset form when opening
    newPassword.value = ''
    confirmPassword.value = ''
    error.value = null
    v$.value.$reset()
  }
})

watch(showModal, (newVal) => {
  emit('update:modelValue', newVal)
})

const handleHide = () => {
  // Prevent closing if forced and password not changed
  if (props.forceRedirect && !loginStore.passwordChanged) {
    return false
  }
  return true
}

const closeModal = () => {
  if (!props.forceRedirect || loginStore.passwordChanged) {
    showModal.value = false
  }
}

const handleSubmit = async () => {
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
      emit('success')
      showModal.value = false

      // Redirect to home if this was a forced password change
      if (props.forceRedirect) {
        router.push('/')
      }
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

.password-change-modal {
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

.password-change-modal :deep(.form-label) {
  font-weight: 600;
  color: var(--color-text);
}

.password-change-modal :deep(.form-control) {
  border: 2px solid var(--color-border);
  border-radius: var(--radius-md);
  padding: 0.75rem 1rem;
}

.password-change-modal :deep(.form-control:focus) {
  border-color: var(--color-primary);
  box-shadow: 0 0 0 4px rgba(255, 107, 53, 0.1);
}

.alert-icon {
  margin-right: var(--spacing-sm);
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
