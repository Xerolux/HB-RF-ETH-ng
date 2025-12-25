import { createApp } from 'vue'
import { createPinia } from 'pinia'
import { createRouter, createWebHistory } from 'vue-router'
import axios from 'axios'
import { useLoginStore, useThemeStore } from './stores.js'

import 'bootstrap/dist/css/bootstrap.css'
import 'bootstrap-vue-next/dist/bootstrap-vue-next.css'

import App from './app.vue'
import Home from './home.vue'
import Settings from "./settings.vue"
import FirmwareUpdate from "./firmwareupdate.vue"
import Login from './login.vue'
import ChangePassword from './change-password.vue'
import About from './about.vue'
import Monitoring from './monitoring.vue'
import Analyzer from './analyzer.vue'
import Log from './log.vue'


// Router
const router = createRouter({
  history: createWebHistory(),
  routes: [
    { path: '/', component: Home },
    { path: '/settings', component: Settings, meta: { requiresAuth: true } },
    { path: '/firmware', component: FirmwareUpdate, meta: { requiresAuth: true } },
    { path: '/monitoring', component: Monitoring, meta: { requiresAuth: true } },
    { path: '/analyzer', component: Analyzer, meta: { requiresAuth: true } },
    { path: '/log', component: Log, meta: { requiresAuth: true } },
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
      router.go()
    }
    return Promise.reject(error)
  }
)

// Router guard
router.beforeEach((to, from, next) => {
  const loginStore = useLoginStore()
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
// Tree-shaking: Import only used components instead of *
import {
  BAlert, BButton, BCard, BCollapse, BDropdownItem, BForm,
  BFormCheckbox, BFormFile, BFormGroup, BFormInput,
  BFormInvalidFeedback, BFormRadio, BFormRadioGroup,
  BFormSelect, BFormSelectOption, BFormText, BInputGroup,
  BListGroup, BListGroupItem, BModal, BNavItem,
  BNavItemDropdown, BNavbar, BNavbarBrand, BNavbarNav,
  BNavbarToggle, BProgress, BProgressBar, BSpinner
} from 'bootstrap-vue-next'

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

// Register used BootstrapVueNext components globally
const components = {
  BAlert, BButton, BCard, BCollapse, BDropdownItem, BForm,
  BFormCheckbox, BFormFile, BFormGroup, BFormInput,
  BFormInvalidFeedback, BFormRadio, BFormRadioGroup,
  BFormSelect, BFormSelectOption, BFormText, BInputGroup,
  BListGroup, BListGroupItem, BModal, BNavItem,
  BNavItemDropdown, BNavbar, BNavbarBrand, BNavbarNav,
  BNavbarToggle, BProgress, BProgressBar, BSpinner
}

for (const [key, component] of Object.entries(components)) {
  app.component(key, component)
}

// Initialize theme
const themeStore = useThemeStore()
themeStore.init()

app.mount('#app')
