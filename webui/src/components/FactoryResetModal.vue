<template>
  <BModal
    v-model="showModal"
    :title="t('firmware.factoryResetTitle')"
    :aria-label="t('firmware.factoryResetTitle')"
    centered
    no-close-on-backdrop
    :ok-title="t('firmware.factoryResetConfirm')"
    ok-variant="danger"
    :cancel-title="t('common.cancel')"
    :ok-disabled="isResetting || !challengeMatches"
    :cancel-disabled="isResetting"
    @ok="onConfirm"
  >
    <p>{{ t('firmware.factoryResetMessage') }}</p>
    <BAlert variant="danger" :model-value="true" fade class="mt-2">
      {{ t('firmware.factoryResetWarning') }}
    </BAlert>

    <div class="factory-reset-challenge">
      <p id="factoryResetChallengeHelp" class="challenge-help">
        {{ t('firmware.factoryResetChallengeHelp') }}
      </p>
      <span class="challenge-label">{{ t('firmware.factoryResetChallengeLabel') }}</span>
      <div
        class="challenge-code"
        data-testid="factory-reset-challenge"
        :aria-label="t('firmware.factoryResetChallengeDisplayLabel', { code: challenge })"
        role="group"
        tabindex="0"
        draggable="false"
        @copy.prevent
        @cut.prevent
        @contextmenu.prevent
        @dragstart.prevent
        @selectstart.prevent
      >
        {{ challenge }}
      </div>

      <label class="challenge-input-label" for="factoryResetConfirmation">
        {{ t('firmware.factoryResetChallengeInputLabel') }}
      </label>
      <input
        id="factoryResetConfirmation"
        v-model="confirmation"
        class="form-control challenge-input"
        :class="{
          'is-invalid': confirmation.length > 0 && !challengeMatches,
          'is-valid': challengeMatches
        }"
        type="text"
        name="factory-reset-confirmation-code"
        maxlength="8"
        autocomplete="off"
        autocapitalize="off"
        autocorrect="off"
        spellcheck="false"
        :placeholder="t('firmware.factoryResetChallengePlaceholder')"
        :aria-describedby="confirmation.length > 0 ? 'factoryResetChallengeFeedback' : 'factoryResetChallengeHelp'"
        :aria-invalid="confirmation.length > 0 && !challengeMatches"
        @paste.prevent
      >
      <p
        v-if="confirmation.length > 0"
        id="factoryResetChallengeFeedback"
        class="challenge-feedback"
        :class="challengeMatches ? 'is-ready' : 'is-mismatch'"
        :role="challengeMatches ? 'status' : 'alert'"
      >
        {{ challengeMatches
          ? t('firmware.factoryResetChallengeReady')
          : t('firmware.factoryResetChallengeMismatch') }}
      </p>
    </div>
  </BModal>
</template>

<script setup>
import { computed, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import axios from 'axios'
import {
  useExperimentalStore,
  useLoginStore,
  useRestartUiStore,
  useThemeStore,
  useUiStore
} from '../stores.js'
import { safeLocal, safeSession } from '../composables/useSafeStorage.js'

const props = defineProps({
  modelValue: {
    type: Boolean,
    default: false
  }
})
const emit = defineEmits(['update:modelValue', 'completed'])

const { t } = useI18n()
const experimentalStore = useExperimentalStore()
const loginStore = useLoginStore()
const restartUiStore = useRestartUiStore()
const themeStore = useThemeStore()
const uiStore = useUiStore()

const isResetting = ref(false)
const challenge = ref('')
const confirmation = ref('')

const CHALLENGE_LENGTH = 8
const CHALLENGE_GROUPS = [
  'ABCDEFGHIJKLMNOPQRSTUVWXYZ',
  'abcdefghijklmnopqrstuvwxyz',
  '0123456789'
]
const CHALLENGE_CHARACTERS = CHALLENGE_GROUPS.join('')

const showModal = computed({
  get: () => props.modelValue,
  set: value => emit('update:modelValue', value)
})

const secureRandomIndex = max => {
  const randomValue = new Uint32Array(1)
  const unbiasedLimit = Math.floor(0x100000000 / max) * max
  do {
    globalThis.crypto.getRandomValues(randomValue)
  } while (randomValue[0] >= unbiasedLimit)
  return randomValue[0] % max
}

const randomCharacter = characters => characters[secureRandomIndex(characters.length)]

const generateChallenge = () => {
  const characters = CHALLENGE_GROUPS.map(randomCharacter)
  while (characters.length < CHALLENGE_LENGTH) {
    characters.push(randomCharacter(CHALLENGE_CHARACTERS))
  }
  for (let index = characters.length - 1; index > 0; index -= 1) {
    const swapIndex = secureRandomIndex(index + 1)
    ;[characters[index], characters[swapIndex]] = [characters[swapIndex], characters[index]]
  }
  return characters.join('')
}

const challengeMatches = computed(() =>
  challenge.value.length === CHALLENGE_LENGTH &&
  confirmation.value === challenge.value
)

watch(() => props.modelValue, isOpen => {
  if (isOpen) {
    challenge.value = generateChallenge()
    confirmation.value = ''
    isResetting.value = false
    return
  }
  challenge.value = ''
  confirmation.value = ''
})

const clearBrowserState = () => {
  themeStore.resetForFactoryReset()
  experimentalStore.resetForFactoryReset()
  // The device-side factory reset erases every user namespace. Mirror that
  // contract in this browser too, including keys introduced by future WebUI
  // versions rather than maintaining an incomplete allow-list.
  safeLocal.clear()
  safeSession.clear()
  loginStore.logout()
}

const onConfirm = event => {
  event.preventDefault()
  if (!challengeMatches.value || isResetting.value) return
  confirmFactoryReset()
}

const confirmFactoryReset = async () => {
  isResetting.value = true
  try {
    await axios.post('/api/factory-reset')
    showModal.value = false
    clearBrowserState()
    restartUiStore.start({
      includeFlashPause: true,
      syncSeconds: 40,
      restartSeconds: 30
    })
    emit('completed')
  } catch {
    uiStore.pushToast({
      type: 'error',
      title: t('common.error'),
      message: t('firmware.factoryResetError')
    })
    isResetting.value = false
  }
}
</script>

<style scoped>
.factory-reset-challenge {
  display: flex;
  flex-direction: column;
  gap: var(--space-2);
  margin-top: var(--space-4);
}

.challenge-help {
  margin: 0;
  color: var(--color-text-secondary);
  font-size: var(--fs-sm);
}

.challenge-label,
.challenge-input-label {
  font-size: var(--fs-xs);
  font-weight: var(--font-weight-bold);
}

.challenge-code {
  align-self: stretch;
  padding: var(--space-3) var(--space-4);
  border: 2px dashed var(--color-danger);
  border-radius: var(--radius-md);
  background: var(--color-danger-soft);
  color: var(--color-danger);
  font-family: var(--font-mono);
  font-size: var(--fs-xl);
  font-weight: var(--font-weight-heavy);
  letter-spacing: .24em;
  text-align: center;
  cursor: default;
  user-select: none;
  -webkit-user-select: none;
  -webkit-touch-callout: none;
}

.challenge-code:focus-visible {
  outline: 3px solid var(--color-primary-soft);
  outline-offset: 2px;
}

.challenge-input {
  font-family: var(--font-mono);
  letter-spacing: .12em;
}

.challenge-feedback {
  margin: 0;
  font-size: var(--fs-xs);
  font-weight: var(--font-weight-semibold);
}

.challenge-feedback.is-mismatch {
  color: var(--color-danger);
}

.challenge-feedback.is-ready {
  color: var(--color-success);
}
</style>
