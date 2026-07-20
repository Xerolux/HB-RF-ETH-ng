import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import { resolve } from 'path'
import { readFileSync, writeFileSync, existsSync } from 'fs'
import { createHash } from 'crypto'
import { join } from 'path'

/**
 * Cache-busting for the embedded ESP32 WebUI.
 *
 * Why this exists: the firmware serves a fixed set of file names (main.js,
 * main.css, index.html, favicon.ico) — see webui.cpp EMBED_HANDLER. Because the
 * names never change, browsers cache main.js / main.css aggressively, and after
 * a firmware update the user sees the OLD UI until they clear the cache.
 * Renaming the files to main.<hash>.js is NOT possible (the firmware only knows
 * the four fixed names), but the esp_http_server URI matcher ignores the query
 * string — so /main.js?v=a3f9b2c1 still routes to the main.js handler.
 *
 * This plugin appends a content-hash query param to the asset references in
 * index.html after the build. Combined with the no-cache meta tags in
 * index.html (HTML itself is never cached), every build produces fresh URLs and
 * the browser always pulls the new bundle automatically.
 */
function cacheBustingPlugin() {
  return {
    name: 'esp32-cache-busting',
    // writeBundle runs after all assets have been written to outDir, so the
    // final (minified, gzipped-then-un-gzipped? no — raw) file content is on
    // disk and can be hashed.
    writeBundle(opts) {
      const outDir = opts.dir || 'dist'
      const indexPath = join(outDir, 'index.html')
      if (!existsSync(indexPath)) return

      let html = readFileSync(indexPath, 'utf8')

      // Compute a short content hash for each asset and rewrite its reference.
      // Cache-bust every embedded asset. favicon.ico used to be skipped, but
      // a favicon change (new logo) was then stuck behind the browser's
      // aggressive favicon cache. Busting it too guarantees the new icon is
      // fetched after a firmware update. icon-256.png serves as
      // any/maskable/apple-touch (single embedded file).
      for (const file of ['main.js', 'main.css', 'manifest.webmanifest', 'icon-256.png', 'favicon.ico']) {
        const assetPath = join(outDir, file)
        if (!existsSync(assetPath)) continue
        const buf = readFileSync(assetPath)
        const hash = createHash('sha256').update(buf).digest('hex').slice(0, 8)

        // Match the un-bundled reference Vite emitted (e.g. src="/main.js" or
        // href="/main.css"). Avoid double-appending: if a ?v= is already
        // present (re-runs), strip it first.
        const refPattern = new RegExp(`(["'(=\\s]/?${file.replace(/\./g, '\\.')})\\?v=[a-f0-9]{1,16}`, 'g')
        html = html.replace(refPattern, `$1`)
        const plainPattern = new RegExp(`(["'(=\\s]/?${file.replace(/\./g, '\\.')})(["'#?])`, 'g')
        html = html.replace(plainPattern, `$1?v=${hash}$2`)
      }

      writeFileSync(indexPath, html)
      console.log(`[cache-busting] rewrote asset references in index.html`)
    }
  }
}

export default defineConfig({
  plugins: [
    vue(),
    cacheBustingPlugin()
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
