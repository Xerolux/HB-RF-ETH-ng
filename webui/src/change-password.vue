<template>
  <BCard
    :header="t('title')"
    header-tag="h6"
    header-bg-variant="danger"
    header-text-variant="white"
    class="mb-3"
  >
    <BForm @submit.stop.prevent>
      <BAlert show variant="warning" class="mb-3">
        {{ t('warningMessage') }}
      </BAlert>

      <BFormGroup :label="t('newPassword')" label-cols-sm="4">
        <BFormInput
          type="password"
          v-model="newPassword"
          :state="v$.newPassword.$error ? false : null"
        />
        <BFormInvalidFeedback v-if="v$.newPassword.minLength.$invalid">
          {{ t('passwordTooShort') }}
        </BFormInvalidFeedback>
      </BFormGroup>

      <BFormGroup :label="t('confirmPassword')" label-cols-sm="4">
        <BFormInput
          type="password"
          v-model="confirmPassword"
          :state="v$.confirmPassword.$error ? false : null"
        />
        <BFormInvalidFeedback v-if="v$.confirmPassword.sameAs.$invalid">
          {{ t('passwordsDoNotMatch') }}
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
          :disabled="v$.$invalid"
        >{{ t('changePassword') }}</BButton>
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

const password_validator = helpers.regex(/^(?=.*[A-Za-z])(?=.*\d).{6,}$/)

const { t } = useI18n({
  messages: {
    de: {
      title: 'Passwort ändern erforderlich',
      warningMessage: 'Dies ist Ihre erste Anmeldung oder das Passwort ist noch auf "admin" gesetzt. Aus Sicherheitsgründen müssen Sie das Passwort ändern.',
      newPassword: 'Neues Passwort',
      confirmPassword: 'Passwort bestätigen',
      changePassword: 'Passwort ändern',
      passwordTooShort: 'Das Passwort muss mindestens 6 Zeichen lang sein und Buchstaben und Zahlen enthalten.',
      passwordsDoNotMatch: 'Passwörter stimmen nicht überein',
      success: 'Passwort erfolgreich geändert'
    },
    en: {
      title: 'Password change required',
      warningMessage: 'This is your first login or the password is still set to "admin". For security reasons, you must change the password.',
      newPassword: 'New Password',
      confirmPassword: 'Confirm Password',
      changePassword: 'Change Password',
      passwordTooShort: 'Password must be at least 6 characters long and contain letters and numbers.',
      passwordsDoNotMatch: 'Passwords do not match',
      success: 'Password changed successfully'
    }
  }
})

const router = useRouter()
const loginStore = useLoginStore()

const newPassword = ref('')
const confirmPassword = ref('')
const error = ref(null)

const rules = computed(() => ({
  newPassword: { required, minLength: minLength(6), password_validator },
  confirmPassword: { required, sameAs: sameAs(newPassword.value) }
}))

const v$ = useVuelidate(rules, { newPassword, confirmPassword })

const changePassword = async () => {
  if (v$.value.$invalid) return

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
  }
}
</script>
