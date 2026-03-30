import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import { resolve } from 'path'

export default defineConfig({
  plugins: [
    vue()
  ],
  resolve: {
    alias: {
      '@': resolve(__dirname, 'src')
    }
  },
  build: {
    outDir: 'dist',
    emptyOutDir: true,
    rolldownOptions: {
      output: {
        entryFileNames: 'main.js',
        assetFileNames: (assetInfo) => {
          const name = (assetInfo.names && assetInfo.names[0]) || assetInfo.name || ''
          if (name.endsWith('.ico')) return 'favicon.ico'
          if (name.endsWith('.css')) return 'main.css'
          return '[name].[ext]'
        },
        // IMPORTANT: Bundle everything into a single JS file. The ESP32 HTTP server
        // only serves specific embedded files (main.js, main.css, index.html, favicon.ico).
        // Any additional chunk files would not be served and break the UI.
        codeSplitting: false
      }
    },
    cssCodeSplit: false,
    minify: 'esbuild',
    target: 'es2020',
    chunkSizeWarningLimit: 1000
  },
  server: {
    port: 1234,
    strictPort: true,
    proxy: {
      '/api': {
        target: 'http://192.168.1.1', // Default target for development
        changeOrigin: true
      }
    }
  }
})
