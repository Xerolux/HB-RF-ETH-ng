<template>
  <div id="app">
    <div class="app-container">
      <Header />
      <main class="main-content">
        <RouterView />
      </main>
      <footer class="app-footer">
        <div class="footer-content">
          <small class="text-muted">HB-RF-ETH-ng {{ sysInfoStore.currentVersion ? 'v' + sysInfoStore.currentVersion : '' }} &copy; 2026 Xerolux</small>
          <button class="sponsor-btn" @click="showSponsorModal = true">
            <span>❤️</span> Sponsor
          </button>
        </div>
      </footer>
    </div>

    <SponsorModal v-model="showSponsorModal" />
  </div>
</template>

<script setup>
import { onMounted, onUnmounted, watch, ref } from 'vue'
import { useRouter } from 'vue-router'
import { useLoginStore, useSysInfoStore } from './stores.js'
import Header from './header.vue'
import SponsorModal from './components/SponsorModal.vue'

const loginStore = useLoginStore()
const sysInfoStore = useSysInfoStore()
const router = useRouter()
const showSponsorModal = ref(false)
let idleTimer = null
const IDLE_TIMEOUT = 5 * 60 * 1000 // 5 minutes

const resetTimer = () => {
  if (idleTimer) clearTimeout(idleTimer)
  if (loginStore.isLoggedIn) {
    idleTimer = setTimeout(logout, IDLE_TIMEOUT)
  }
}

const logout = () => {
  loginStore.logout()
  router.push('/login')
}

const setupIdleListeners = () => {
  window.addEventListener('mousemove', resetTimer)
  window.addEventListener('mousedown', resetTimer)
  window.addEventListener('keypress', resetTimer)
  window.addEventListener('touchmove', resetTimer)
  window.addEventListener('scroll', resetTimer)
}

const removeIdleListeners = () => {
  window.removeEventListener('mousemove', resetTimer)
  window.removeEventListener('mousedown', resetTimer)
  window.removeEventListener('keypress', resetTimer)
  window.removeEventListener('touchmove', resetTimer)
  window.removeEventListener('scroll', resetTimer)
  if (idleTimer) clearTimeout(idleTimer)
}

watch(() => loginStore.isLoggedIn, (isLoggedIn) => {
  if (isLoggedIn) {
    setupIdleListeners()
    resetTimer()
  } else {
    removeIdleListeners()
  }
})

onMounted(() => {
  sysInfoStore.update().catch(() => {})
  if (loginStore.isLoggedIn) {
    setupIdleListeners()
    resetTimer()
  }
})

onUnmounted(() => {
  removeIdleListeners()
})
</script>

<style scoped>
#app {
  min-height: 100vh;
  display: flex;
  flex-direction: column;
}

.app-container {
  max-width: 900px;
  width: 100%;
  margin: 0 auto;
  padding: var(--spacing-md);
  flex: 1;
  display: flex;
  flex-direction: column;
}

@media (min-width: 768px) {
  .app-container {
    padding: var(--spacing-lg);
  }
}

@media (max-width: 480px) {
  .app-container {
    padding: var(--spacing-sm);
  }
}

/* Safe area padding for notch devices */
@supports (padding: env(safe-area-inset-left)) {
  .app-container {
    padding-left: max(var(--spacing-sm), env(safe-area-inset-left));
    padding-right: max(var(--spacing-sm), env(safe-area-inset-right));
  }
}

.main-content {
  flex: 1;
  margin-bottom: var(--spacing-lg);
  /* Prevent content from overflowing horizontally on mobile */
  overflow-x: hidden;
}

@media (max-width: 768px) {
  .main-content {
    margin-bottom: var(--spacing-md);
  }
}

.app-footer {
  padding: var(--spacing-lg) 0;
  border-top: 1px solid var(--color-border);
  margin-top: auto;
}

@media (max-width: 768px) {
  .app-footer {
    padding: var(--spacing-md) 0;
  }
}

.footer-content {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: var(--spacing-sm);
}

.sponsor-btn {
  background: var(--color-surface);
  border: 1px solid var(--color-border);
  padding: 6px 16px;
  border-radius: var(--radius-full);
  font-size: 0.875rem;
  font-weight: 600;
  color: var(--color-text-secondary);
  cursor: pointer;
  transition: all 0.2s;
  display: flex;
  align-items: center;
  gap: 6px;
}

.sponsor-btn:hover {
  background: #ffecec;
  color: #ff3b30;
  border-color: #ff3b30;
  transform: translateY(-1px);
  box-shadow: var(--shadow-sm);
}
</style>
