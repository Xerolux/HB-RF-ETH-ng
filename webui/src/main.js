import { createApp } from 'vue'
import { createPinia } from 'pinia'
import { createI18n } from 'vue-i18n'
import PrimeVue from 'primevue/config'
import Aura from '@primevue/themes/aura'
import ToastService from 'primevue/toastservice'
import ConfirmationService from 'primevue/confirmationservice'

import App from './App.vue'
import router from './router'
import './style.css'

// Import locales (we will migrate these later)
import en from './locales/en.json'
import de from './locales/de.json'

const i18n = createI18n({
    legacy: false,
    locale: localStorage.getItem('locale') || 'en',
    fallbackLocale: 'en',
    messages: { en, de }
})

const app = createApp(App)

app.use(createPinia())
app.use(router)
app.use(i18n)
app.use(PrimeVue, {
    theme: {
        preset: Aura,
        options: {
            darkModeSelector: '.dark',
        }
    }
})
app.use(ToastService)
app.use(ConfirmationService)

app.mount('#app')
