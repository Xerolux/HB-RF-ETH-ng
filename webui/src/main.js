import { createApp } from 'vue'
import { createPinia } from 'pinia'
import { createRouter, createWebHistory } from 'vue-router'
import axios from 'axios'
import { useLoginStore, useThemeStore, useUiStore } from './stores.js'

import 'bootstrap/dist/css/bootstrap.css'
import 'bootstrap-vue-next/dist/bootstrap-vue-next.css'

// Current New Design system
import './styles/main.css'

import App from './app.vue'
import Home from './home.vue'
import Settings from './settings.vue'
import FirmwareUpdate from './firmwareupdate.vue'
import WebUIUpdate from './webuiupdate.vue'
import Login from './login.vue'
import ChangePassword from './change-password.vue'
import About from './about.vue'
import Monitoring from './monitoring.vue'
import SystemLog from './systemlog.vue'
import SystemOverview from './systemoverview.vue'
import AppIcon from './components/AppIcon.vue'

// Router
const router = createRouter({
  history: createWebHistory(),
  routes: [
    { path: '/', component: Home },
    { path: '/settings', component: Settings, meta: { requiresAuth: true } },
    { path: '/firmware', component: FirmwareUpdate, meta: { requiresAuth: true } },
    { path: '/webui', component: WebUIUpdate, meta: { requiresAuth: true } },
    { path: '/monitoring', component: Monitoring, meta: { requiresAuth: true } },
    { path: '/systemlog', component: SystemLog, meta: { requiresAuth: true } },
    { path: '/system-overview', component: SystemOverview, meta: { requiresAuth: true } },
    { path: '/change-password', component: ChangePassword, meta: { requiresAuth: true } },
    { path: '/about', component: About },
    // bareLayout: rendered WITHOUT the app shell (no sidebar header, no footer).
    { path: '/login', component: Login, meta: { bareLayout: true } },
  ]
})

// Create I18n instance
import { createI18n } from 'vue-i18n'
import { messages, getBrowserLocale } from './locales/index.js'

const storedLocale = localStorage.getItem('locale') || getBrowserLocale()

const i18n = createI18n({
  legacy: false,
  locale: storedLocale,
  fallbackLocale: 'en',
  messages: messages
})

const translate = (key, params) => i18n.global.t(key, params)

// Axios interceptors
axios.interceptors.request.use(
  request => {
    const loginStore = useLoginStore()
    if (loginStore.isLoggedIn) {
      request.headers['Authorization'] = 'Token ' + loginStore.token
    }
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
    const silent = error.config?.silent === true

    if (error.response) {
      if (error.response.status === 401 && !silent) {
        loginStore.logout()
        router.push('/login')
      }
      else if (error.response.status >= 500 && !silent) {
        uiStore.pushToast({
          type: 'error',
          title: translate('common.network.serverErrorTitle'),
          message: translate('common.network.serverError', { status: error.response.status }),
          duration: 5000
        })
      }
    } else if (error.code === 'ECONNABORTED') {
      if (!silent) {
        uiStore.pushToast({
          type: 'warning',
          title: translate('common.network.timeoutTitle'),
          message: translate('common.network.timeout'),
          duration: 4000
        })
      }
    } else if (!error.response) {
      if (!silent) {
        uiStore.pushToast({
          type: 'error',
          title: translate('common.network.connectionErrorTitle'),
          message: translate('common.network.connectionError'),
          duration: 5000
        })
      }
    }

    return Promise.reject(error)
  }
)

// Router guard
router.beforeEach((to, from, next) => {
  const loginStore = useLoginStore()

  if (loginStore.isLoggedIn && loginStore.checkActivity()) {
    next({
      path: '/login',
      query: { redirect: to.fullPath }
    })
    return
  }

  if (to.matched.some(r => r.meta.requiresAuth)) {
    if (!loginStore.isLoggedIn) {
      next({
        path: '/login',
        query: { redirect: to.fullPath }
      })
      return
    }

    if (!loginStore.passwordChanged && to.path !== '/change-password') {
      next({ path: '/change-password' })
      return
    }
  }
  next()
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

const themeStore = useThemeStore()
themeStore.init()

// The New Design is the only application shell.
document.body.classList.add('newdesign-active')

// Activity tracking for idle timeout
let lastUpdate = 0
let idleCheckInterval = null
const updateActivity = () => {
  const now = Date.now()
  if (now - lastUpdate > 1000) {
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

idleCheckInterval = window.setInterval(() => {
  const loginStore = useLoginStore()
  if (loginStore.isLoggedIn && loginStore.checkActivity()) {
    router.push('/login')
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
