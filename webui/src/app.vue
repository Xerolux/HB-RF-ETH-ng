<template>
  <div id="app" class="container" style="max-width: 800px">
    <Header />
    <RouterView />
  </div>
</template>

<script setup>
import { onMounted, onUnmounted, watch } from 'vue'
import { useRouter } from 'vue-router'
import { useLoginStore } from './stores.js'
import Header from './header.vue'

const loginStore = useLoginStore()
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

watch(
  () => loginStore.isLoggedIn,
  (isLoggedIn) => {
    if (isLoggedIn) {
      setupIdleListeners()
      resetTimer()
    } else {
      removeIdleListeners()
    }
  }
)

onMounted(() => {
  if (loginStore.isLoggedIn) {
    setupIdleListeners()
    resetTimer()
  }
})

onUnmounted(() => {
  removeIdleListeners()
})
</script>
