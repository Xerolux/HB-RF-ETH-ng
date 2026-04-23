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
import AppIcon from './components/AppIcon.vue'


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
    // Set default timeout if not already set
    if (!request.timeout) {
      request.timeout = 10000
    }
    return request
  },
  error => Promise.reject(error)
)

axios.interceptors.response.use(
  response => response,
  error => {
    const uiStore = useUiStore()
    const loginStore = useLoginStore()

    if (error.response) {
      // Handle 401 Unauthorized
      if (error.response.status === 401) {
        loginStore.logout()
        router.push('/login')
      }
      // Handle network/server errors
      else if (error.response.status >= 500) {
        uiStore.pushToast({
          type: 'danger',
          title: 'Server Error',
          message: `Server error: ${error.response.status}`,
          duration: 5000
        })
      }
    } else if (error.code === 'ECONNABORTED') {
      uiStore.pushToast({
        type: 'warning',
        title: 'Request Timeout',
        message: 'The request took too long. Please try again.',
        duration: 4000
      })
    } else if (!error.response) {
      uiStore.pushToast({
        type: 'danger',
        title: 'Connection Error',
        message: 'Unable to connect to the device. Check your network.',
        duration: 5000
      })
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
import {
  BAlert,
  BButton,
  BCard,
  BForm,
  BFormGroup,
  BFormInput,
  BFormInvalidFeedback,
  BFormSelect,
  BFormSelectOption,
  BModal
} from 'bootstrap-vue-next'

// Create and mount app
const app = createApp(App)
const pinia = createPinia()

app.use(pinia)
app.use(router)
app.use(i18n)
app.use(createBootstrap({
    components: false,
    directives: true,
}))

app.component('BAlert', BAlert)
app.component('BButton', BButton)
app.component('BCard', BCard)
app.component('BForm', BForm)
app.component('BFormGroup', BFormGroup)
app.component('BFormInput', BFormInput)
app.component('BFormInvalidFeedback', BFormInvalidFeedback)
app.component('BFormSelect', BFormSelect)
app.component('BFormSelectOption', BFormSelectOption)
app.component('BModal', BModal)
app.component('AppIcon', AppIcon)

// Initialize theme
const themeStore = useThemeStore()
themeStore.init()

// Activity tracking for idle timeout
let lastUpdate = 0
let idleCheckInterval = null
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

const activityEvents = ['mousemove', 'keydown', 'click', 'touchstart']
for (const eventName of activityEvents) {
  window.addEventListener(eventName, updateActivity)
}

// Check for inactivity every 30 seconds
idleCheckInterval = window.setInterval(() => {
  const loginStore = useLoginStore()
  if (loginStore.isLoggedIn) {
    if (loginStore.checkActivity()) {
      router.push('/login')
    }
  }
}, 30000)

if (import.meta.hot) {
  import.meta.hot.dispose(() => {
    for (const eventName of activityEvents) {
      window.removeEventListener(eventName, updateActivity)
    }
    if (idleCheckInterval !== null) {
      window.clearInterval(idleCheckInterval)
    }
  })
}

app.mount('#app')
