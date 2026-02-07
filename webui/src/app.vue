<template>
  <div id="app">
    <div class="app-container">
      <Header />
      <main class="main-content">
        <RouterView />
      </main>
      <footer class="app-footer">
        <small class="text-muted">HB-RF-ETH-ng {{ sysInfoStore.currentVersion ? 'v' + sysInfoStore.currentVersion : '' }} &copy; 2025</small>
      </footer>
    </div>
  </div>
</template>

<script setup>
import { onMounted, onUnmounted, watch } from 'vue'
import { useRouter } from 'vue-router'
import { useLoginStore, useSysInfoStore } from './stores.js'
import Header from './header.vue'

const loginStore = useLoginStore()
const sysInfoStore = useSysInfoStore()
const router = useRouter()
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

.main-content {
  flex: 1;
  margin-bottom: var(--spacing-lg);
}

.app-footer {
  text-align: center;
  padding: var(--spacing-lg) 0;
  border-top: 1px solid var(--color-border);
  margin-top: auto;
}
</style>
