import { createApp } from 'vue'
import { createPinia } from 'pinia'
import { createRouter, createWebHistory } from 'vue-router'
import axios from 'axios'
import { useLoginStore, useThemeStore } from './stores.js'

import 'bootstrap/dist/css/bootstrap.css'
import 'bootstrap-vue-next/dist/bootstrap-vue-next.css'

// Custom design system
import './styles/main.css'

import App from './app.vue'
import Home from './home.vue'
import Settings from "./settings.vue"
import FirmwareUpdate from "./firmwareupdate.vue"
import Login from './login.vue'
import ChangePassword from './change-password.vue'
import About from './about.vue'
import Monitoring from './monitoring.vue'
import SystemLog from './systemlog.vue'


// Router
const router = createRouter({
  history: createWebHistory(),
  routes: [
    { path: '/', component: Home },
    { path: '/settings', component: Settings, meta: { requiresAuth: true } },
    { path: '/firmware', component: FirmwareUpdate, meta: { requiresAuth: true } },
    { path: '/monitoring', component: Monitoring, meta: { requiresAuth: true } },
    { path: '/systemlog', component: SystemLog, meta: { requiresAuth: true } },
    { path: '/change-password', component: ChangePassword, meta: { requiresAuth: true } },
    { path: '/about', component: About },
    { path: '/login', component: Login },
  ]
})

// Axios interceptors
axios.interceptors.request.use(
  request => {
    const loginStore = useLoginStore()
    if (loginStore.isLoggedIn) {
      request.headers['Authorization'] = 'Token ' + loginStore.token
    }
    return request
  },
  error => Promise.reject(error)
)

axios.interceptors.response.use(
  response => response,
  error => {
    if (error.response && error.response.status === 401) {
      const loginStore = useLoginStore()
      loginStore.logout()
      router.push('/login')
    }
    return Promise.reject(error)
  }
)

// Router guard
router.beforeEach((to, from, next) => {
  const loginStore = useLoginStore()

  // Check activity on navigation
  if (loginStore.isLoggedIn) {
    if (loginStore.checkActivity()) {
      next({
        path: '/login',
        query: { redirect: to.fullPath }
      })
      return
    }
  }

  if (to.matched.some(r => r.meta.requiresAuth)) {
    if (!loginStore.isLoggedIn) {
      next({
        path: '/login',
        query: { redirect: to.fullPath }
      })
      return
    }

    // Force password change if required
    if (!loginStore.passwordChanged && to.path !== '/change-password') {
      next({ path: '/change-password' })
      return
    }
  }
  next()
})

// Create I18n instance
import { createI18n } from 'vue-i18n'
import { messages, getBrowserLocale } from './locales/index.js'

// Get stored locale or use browser locale
const storedLocale = localStorage.getItem('locale') || getBrowserLocale()

const i18n = createI18n({
  legacy: false,
  locale: storedLocale,
  fallbackLocale: 'en',
  messages: messages
})

// Create Bootstrap Vue Next
import { createBootstrap } from 'bootstrap-vue-next'
import * as BootstrapVueNext from 'bootstrap-vue-next'

// Create and mount app
const app = createApp(App)
const pinia = createPinia()

app.use(pinia)
app.use(router)
app.use(i18n)
app.use(createBootstrap({
    components: true,
    directives: true,
}))

// Register all BootstrapVueNext components globally
for (const key in BootstrapVueNext) {
  if (key.startsWith('B')) {
    app.component(key, BootstrapVueNext[key])
  }
}

// Initialize theme
const themeStore = useThemeStore()
themeStore.init()

// Activity tracking for idle timeout
let lastUpdate = 0
const updateActivity = () => {
  const now = Date.now()
  if (now - lastUpdate > 1000) { // Throttle updates to once per second
    const loginStore = useLoginStore()
    if (loginStore.isLoggedIn) {
      loginStore.updateActivity()
    }
    lastUpdate = now
  }
}

window.addEventListener('mousemove', updateActivity)
window.addEventListener('keydown', updateActivity)
window.addEventListener('click', updateActivity)
window.addEventListener('touchstart', updateActivity)

// Check for inactivity every 30 seconds
setInterval(() => {
  const loginStore = useLoginStore()
  if (loginStore.isLoggedIn) {
    if (loginStore.checkActivity()) {
      router.push('/login')
    }
  }
}, 30000)

app.mount('#app')
