import { defineConfig } from "vite";
import preact from "@preact/preset-vite";
import tailwindcss from "@tailwindcss/vite";

export default defineConfig({
  plugins: [preact(), tailwindcss()],
  build: {
    outDir: "dist",
    emptyOutDir: true,
    minify: "terser",
    terserOptions: {
      compress: {
        drop_console: true,
        passes: 2,
      },
    },
    cssMinify: "lightningcss",
  },
  server: {
    proxy: {
      "/api": {
        target: "http://4.3.2.1",
        changeOrigin: true,
      },
    },
  },
});
