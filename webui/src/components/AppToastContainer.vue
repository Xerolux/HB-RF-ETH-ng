<template>
  <div class="app-toast-stack">
    <TransitionGroup name="toast-slide" tag="div" class="toast-group">
      <div
        v-for="toast in uiStore.toasts"
        :key="toast.id"
        class="app-toast"
        :class="`toast-${toast.type || 'info'}`"
      >
        <div class="toast-icon-wrap">
          <AppIcon :name="iconName(toast.type)" />
        </div>
        <div class="toast-copy">
          <strong v-if="toast.title">{{ toast.title }}</strong>
          <span>{{ toast.message }}</span>
        </div>
        <button class="toast-dismiss" @click="uiStore.removeToast(toast.id)">
          <AppIcon name="close" />
        </button>
      </div>
    </TransitionGroup>
  </div>
</template>

<script setup>
import AppIcon from './AppIcon.vue'
import { useUiStore } from '../stores.js'

const uiStore = useUiStore()

const iconName = (type) => {
  switch (type) {
    case 'success':
      return 'check'
    case 'error':
      return 'alert'
    case 'warning':
      return 'alert'
    default:
      return 'info'
  }
}
</script>

<style scoped>
.app-toast-stack {
  position: fixed;
  top: max(18px, env(safe-area-inset-top));
  right: max(18px, env(safe-area-inset-right));
  z-index: 5000;
  pointer-events: none;
}

.toast-group {
  display: flex;
  flex-direction: column;
  gap: 12px;
  max-width: min(420px, calc(100vw - 24px));
}

.app-toast {
  pointer-events: auto;
  display: flex;
  align-items: flex-start;
  gap: 12px;
  padding: 14px 16px;
  border-radius: 20px;
  background: rgba(255, 255, 255, 0.86);
  border: 1px solid rgba(255, 255, 255, 0.72);
  box-shadow: 0 18px 40px rgba(15, 23, 42, 0.14);
  backdrop-filter: blur(18px);
  -webkit-backdrop-filter: blur(18px);
}

:global([data-bs-theme="dark"]) .app-toast {
  background: rgba(28, 28, 30, 0.86);
  border-color: rgba(255, 255, 255, 0.08);
}

.toast-success {
  box-shadow: 0 18px 40px rgba(52, 199, 89, 0.12);
}

.toast-error {
  box-shadow: 0 18px 40px rgba(255, 59, 48, 0.14);
}

.toast-warning {
  box-shadow: 0 18px 40px rgba(255, 149, 0, 0.14);
}

.toast-icon-wrap {
  width: 36px;
  height: 36px;
  border-radius: 12px;
  display: flex;
  align-items: center;
  justify-content: center;
  background: var(--color-info-soft);
  color: var(--color-info);
}

.toast-success .toast-icon-wrap {
  background: var(--color-success-soft);
  color: var(--color-success);
}

.toast-error .toast-icon-wrap {
  background: var(--color-danger-soft);
  color: var(--color-danger);
}

.toast-warning .toast-icon-wrap {
  background: var(--color-warning-soft);
  color: var(--color-warning);
}

.toast-copy {
  display: flex;
  flex-direction: column;
  gap: 3px;
  flex: 1;
  min-width: 0;
}

.toast-copy strong {
  font-size: 0.95rem;
}

.toast-copy span {
  color: var(--color-text-secondary);
  font-size: 0.9rem;
  line-height: 1.35;
}

.toast-dismiss {
  width: 32px;
  height: 32px;
  border: none;
  background: transparent;
  color: var(--color-text-secondary);
  border-radius: 10px;
}

.toast-slide-enter-active,
.toast-slide-leave-active {
  transition: all 0.24s ease;
}

.toast-slide-enter-from,
.toast-slide-leave-to {
  opacity: 0;
  transform: translateY(-8px) translateX(18px);
}

@media (max-width: 640px) {
  .app-toast-stack {
    left: 12px;
    right: 12px;
    top: max(12px, env(safe-area-inset-top));
  }

  .toast-group {
    max-width: none;
  }

  .app-toast {
    border-radius: 18px;
  }
}
</style>
