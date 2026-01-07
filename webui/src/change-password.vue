<template>
  <BCard
    :header="t('changePassword.title')"
    header-tag="h6"
    header-bg-variant="danger"
    header-text-variant="white"
    class="mb-3"
  >
    <BForm @submit.stop.prevent>
      <BAlert show variant="warning" class="mb-3">
        {{ t('changePassword.warningMessage') }}
      </BAlert>

      <BFormGroup :label="t('changePassword.newPassword')" label-cols-sm="4">
        <PasswordInput
          v-model="newPassword"
          :state="v$.newPassword.$error ? false : null"
          :placeholder="''"
          autocomplete="new-password"
        />
        <BFormInvalidFeedback v-if="v$.newPassword.minLength.$invalid">
          {{ t('changePassword.passwordTooShort') }}
        </BFormInvalidFeedback>
      </BFormGroup>

      <BFormGroup :label="t('changePassword.confirmPassword')" label-cols-sm="4">
        <PasswordInput
          v-model="confirmPassword"
          :state="v$.confirmPassword.$error ? false : null"
          :placeholder="''"
          autocomplete="new-password"
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
      >{{ error }}</BAlert>

      <BFormGroup label-cols-sm="9">
        <BButton
          variant="primary"
          block
          @click="changePassword"
          :disabled="v$.$invalid || loading"
        >
          <BSpinner small v-if="loading" class="me-2" />
          {{ loading ? t('common.saving') : t('changePassword.changePassword') }}
        </BButton>
      </BFormGroup>
    </BForm>
  </BCard>
</template>

<script setup>
import { ref, computed } from 'vue'
import { useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { useVuelidate } from '@vuelidate/core'
import { required, minLength, sameAs, helpers } from '@vuelidate/validators'
import axios from 'axios'
import { useLoginStore } from './stores.js'
import PasswordInput from './components/PasswordInput.vue'

const password_validator = helpers.regex(/^(?=.*[A-Za-z])(?=.*\d).{6,}$/)

const { t } = useI18n()

const router = useRouter()
const loginStore = useLoginStore()

const newPassword = ref('')
const confirmPassword = ref('')
const error = ref(null)
const loading = ref(false)

const rules = computed(() => ({
  newPassword: { required, minLength: minLength(6), password_validator },
  confirmPassword: { required, sameAs: sameAs(newPassword.value) }
}))

const v$ = useVuelidate(rules, { newPassword, confirmPassword })

const changePassword = async () => {
  if (v$.value.$invalid) return

  loading.value = true
  error.value = null

  try {
    const response = await axios.post('/api/change-password', {
      newPassword: newPassword.value
    })

    if (response.data.success) {
      // Update token in store
      if (response.data.token) {
        loginStore.login(response.data.token)
        // Update passwordChanged status in store
        loginStore.setPasswordChanged(true)
      }
      router.push('/')
    } else {
      error.value = response.data.error || 'Unknown error'
    }
  } catch (e) {
    error.value = e.response?.data?.message || e.message
  } finally {
    loading.value = false
  }
}
</script>
