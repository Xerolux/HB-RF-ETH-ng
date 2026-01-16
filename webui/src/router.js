import { createRouter, createWebHashHistory } from 'vue-router'
import { useLoginStore } from './stores'

const routes = [
  {
    path: '/login',
    name: 'Login',
    component: () => import('./views/Login.vue')
  },
  {
    path: '/',
    component: () => import('./layout/AppLayout.vue'),
    meta: { requiresAuth: true },
    children: [
      {
        path: '',
        name: 'Home',
        component: () => import('./views/Home.vue')
      },
      {
        path: '/settings',
        name: 'Settings',
        component: () => import('./views/Settings.vue')
      },
      {
        path: '/config',
        redirect: '/settings'
      },
      {
        path: '/firmware',
        name: 'Firmware',
        component: () => import('./views/Firmware.vue')
      }
    ]
  }
]

const router = createRouter({
  history: createWebHashHistory(),
  routes
})

router.beforeEach((to, from, next) => {
  const loginStore = useLoginStore()
  if (to.meta.requiresAuth && !loginStore.isLoggedIn) {
    next({ path: '/login', query: { redirect: to.fullPath } })
  } else {
    next()
  }
})

export default router
