import { defineStore } from 'pinia'
import axios from 'axios'

export const useLoginStore = defineStore('login', {
  state: () => ({
    token: sessionStorage.getItem('hb-rf-eth-ng-pw') || null,
    isAuthenticated: !!sessionStorage.getItem('hb-rf-eth-ng-pw'),
    passwordChanged: true // Default assumption, updated on login
  }),
  getters: {
    isLoggedIn: (state) => !!state.token
  },
  actions: {
    setToken(token) {
      this.token = token
      this.isAuthenticated = true
      sessionStorage.setItem('hb-rf-eth-ng-pw', token)
    },
    logout() {
      this.token = null
      this.isAuthenticated = false
      sessionStorage.removeItem('hb-rf-eth-ng-pw')
    }
  }
})

export const useThemeStore = defineStore('theme', {
  state: () => ({
    isDark: localStorage.getItem('theme') === 'dark'
  }),
  actions: {
    toggleTheme() {
      this.isDark = !this.isDark
      localStorage.setItem('theme', this.isDark ? 'dark' : 'light')
      this.applyTheme()
    },
    applyTheme() {
        if (this.isDark) {
            document.documentElement.classList.add('dark')
        } else {
            document.documentElement.classList.remove('dark')
        }
    },
    init() {
        this.applyTheme()
    }
  }
})
