# WebUI Design System — AUTHORITATIVE SPECIFICATION

> **READ THIS BEFORE ANY WEBUI STYLING CHANGE.**
> This document is the single source of truth for the HB-RF-ETH-ng WebUI
> design. It is intentionally normative ("MUST / MUST NOT"), not descriptive.
> Every AI assistant and human contributor working on `webui/src/styles/main.css`
> or any `.vue` component's `<style>` block MUST follow these rules.
>
> If a conflict exists between this document and any code comment, **this
> document wins.** Update code to match, not the other way around.

---

## 1. Two Coexisting Themes (CRITICAL)

The WebUI ships **two complete, independent design systems**. Never blend them.

| Theme | Toggle | Scope | Accent | Identity |
|-------|--------|-------|--------|----------|
| **Glass UI** (default) | always on unless `body.newdesign-active` | `:root` tokens | Orange `#f26a3d` | HB-RF-ETH brand |
| **NewDesign** | `body.newdesign-active` (experimental toggle, persisted in `localStorage["hb-rf-eth-ng-test-design"]` + synced to device NVS) | `body.newdesign-active { ... }` override block in `main.css` | Emerald green `#2F8B57` | Industrial dashboard |

**RULE:** NewDesign overrides live *only* inside `body.newdesign-active`-scoped
selectors in `main.css` (the block starting ~line 916) and in
`NewDesignHeader.vue` (which is only mounted when the toggle is on). Never
mutate the `:root` Glass-UI tokens to implement a NewDesign change. Never
hardcode Glass-UI orange (`#f26a3d` / `rgba(242,106,61,…)`).

### 1.1 How the green palette is wired

NewDesign does **not** redefine every component. It overrides the
`--color-primary*` / surface / text tokens *inside* `body.newdesign-active`, so
every rule that reads `var(--color-primary)` flips from orange to green
automatically. NewDesign-specific tokens (`--newdesign-*`) are aliases that also
resolve to green in this scope.

**Implication for contributors:** to restyle a NewDesign element, prefer reading
a token (`--color-primary`, `--newdesign-panel`, `--newdesign-border`, …) over a
literal hex/rgba. Literal green values only belong in the token *definitions* in
the `body.newdesign-active` block — not scattered through component rules.

---

## 2. Color Palette

### 2.1 NewDesign — Light Mode

| Element | Token | Hex |
|---------|-------|-----|
| Main background | `--color-bg` | `#F6F7F8` |
| Sidebar | `--newdesign-sidebar` / `--color-sidebar` | `#FFFFFF` |
| Header (top bar) | `--newdesign-header` | `#FFFFFF` |
| Cards | `--newdesign-panel` | `#FFFFFF` |
| Dividers / soft fills | `--newdesign-panel-soft` | `#ECEDEE` |
| Card border (1px) | `--newdesign-border` | `#E3E5E7` |
| Border strong | `--newdesign-border-strong` | `#D2D4D7` |
| Primary text | `--color-text` | `#222222` |
| Secondary text | `--color-text-secondary` | `#666666` |
| Muted text | `--color-text-muted` | `#999999` |
| **Primary accent** | `--color-primary` | `#2F8B57` |
| Hover accent | `--color-primary-hover` | `#3B9962` |
| Accent strong | `--color-primary-strong` | `#246B43` |
| Success | `--color-success` | `#43A047` |
| Warning | (unchanged) | `#e89a24` |
| Danger | (unchanged) | `#e05247` |

### 2.2 NewDesign — Dark Mode

| Element | Token | Hex |
|---------|-------|-----|
| Main background | `--color-bg` | `#2E3136` |
| Sidebar | `--newdesign-sidebar` | `#24272B` |
| Header (top bar) | `--newdesign-header` | `#25282C` |
| Cards | `--newdesign-panel` | `#2C2F33` |
| Dividers / soft fills | `--newdesign-panel-soft` | `#3A3F45` |
| Card border | `--newdesign-border` | `#474C51` |
| Border strong | `--newdesign-border-strong` | `#565C62` |
| Primary text | `--color-text` | `#F2F2F2` |
| Secondary text | `--color-text-secondary` | `#C5C8CC` |
| Muted text | `--color-text-muted` | `#90959C` |
| **Primary accent** | `--color-primary` | `#2F8B57` |
| Hover accent | `--color-primary-hover` | `#3D9966` |
| Success | `--color-success` | `#55C271` |

Dark mode is NOT pure black. It uses muted charcoal greys for a premium,
low-eye-strain appearance (like modern developer tools).

### 2.3 Brand Logo (DO NOT recolour)

The `<BrandLogo>` inline SVG uses a **fixed warm gradient `#D96A5A → #EAA08E`**.
This is the brand identity colour and is deliberately **independent** of the
UI accent. It MUST stay the same in both themes and both light/dark modes.
Never bind it to `--color-primary`.

---

## 3. Layout Rules (NewDesign)

- **Desktop:** Fixed left sidebar (~25%), large centered content area.
- **Cards:** Two-column grid where possible, identical widths/heights.
- **Spacing:** 24–32px between cards, 20–30px page padding.
- **Card radius:** `4px` (`--newdesign-radius-card`). NOT the 22–30px glass curves.
- **Shadows:** Minimal. Only a tiny elevation (`--newdesign-shadow`). No glow, no heavy drop shadows, no backdrop blur.
- **Tables/KV-lists:** Rows separated by subtle 1px border lines. Left column = label (secondary text), right column = value (primary text). Generous row height. Perfect alignment.
- **Mobile:** Same palette, single column. Layout positions do not change between light/dark — only colours.

---

## 4. Component Styling Rules

### 4.1 Sidebar nav item — ACTIVE state
- **MUST** be a full-width rounded green rectangle (`var(--color-primary)`).
- **MUST** have white text + white icon.
- Border radius ~5–6px.
- Applies to BOTH desktop `.nav-item.active` and mobile `.mobile-link.router-link-active`.

### 4.2 Buttons
- **Primary:** Emerald green fill, white text, 4px radius.
- **Secondary:** Surface/soft panel background, border.
- **Danger (action buttons in mobile panel):** Dark charcoal fill (`#3A3F45` light / `#1A1C1F` dark) with white text — NOT red. Red is reserved for destructive text/icons only.

### 4.3 Header / top bar
- White (light) / `#25282C` (dark) background.
- Almost-invisible bottom border.
- Single horizontal line: brand+title left, status/clock/settings right.

### 4.4 Chips / badges / pills
- 4px radius, 1px border, soft-panel fill.
- Status badges (online) stay green. Do not recolour semantic status.

### 4.5 Forms
- Inputs: 4px radius, 1px border, soft-panel background.
- Focus: green border + green focus ring (`--newdesign-focus-ring`).

---

## 5. Typography

- **Primary font:** Inter (fallback Roboto).
- Weight hierarchy: Title 700 · Section 600 · Label 400 · Value 500.
- Sizes (NewDesign): Page title 24px · Card title 15px · Table 13px · Footer 11px.

---

## 6. Tokens — Contribution Rules

1. **Prefer tokens over literals.** Any new style rule MUST use a CSS custom property (`var(--…)`) unless it is defining a token.
2. **Literal colours only in token definitions.** A hex/rgba literal inside a component rule is almost always a bug — add/adjust a token instead.
3. **NewDesign overrides stay scoped.** Prefix selectors with `body.newdesign-active` (and usually `.newdesign-shell`). Do not leak NewDesign values into `:root`.
4. **Dark mode is token-driven.** The same component rule must work in both themes without a per-rule dark override — the token cascade handles it.
5. **Teleported content** (Bootstrap modals/dropdowns teleport to `<body>`) is styled with `body.newdesign-active` selectors WITHOUT `.newdesign-shell`, so tokens resolve there too.

---

## 7. Files of Record

| File | Role |
|------|------|
| `webui/src/styles/main.css` | All Glass-UI tokens (`:root`) + all NewDesign overrides (`body.newdesign-active` block ~line 916). |
| `webui/src/components/NewDesignHeader.vue` | Sidebar + top bar + mobile panel — ONLY rendered in NewDesign. |
| `webui/src/components/BrandLogo.vue` | Inline brand SVG. Fixed gradient. |
| `webui/src/components/header.vue` | Legacy Glass-UI header — do not add NewDesign logic here. |
| `webui/public/manifest.webmanifest` | PWA `theme_color` = `#D96A5A` (brand), `background_color` = `#FFFFFF`. |
| `webui/index.html` | `<meta name="theme-color">` = `#D96A5A`. |

---

## 8. Prohibited Patterns (checklist before commit)

- ❌ Hardcoding `#f26a3d` or `rgba(242,106,61,…)` anywhere inside a `body.newdesign-active` rule.
- ❌ Using `#2F8B57` as a literal inside a component `<style>` block (use `var(--color-primary)`).
- ❌ Recolouring `<BrandLogo>` or binding it to a theme token.
- ❌ Mixing Glass-UI radii (16–30px) into NewDesign rules.
- ❌ Adding backdrop-filter / heavy shadows / gradients to NewDesign surfaces.
- ❌ Reference to competitor product names (e.g. "SMLIGHT", "SLZB") in code, comments, changelog, or docs. The design is original — describe it on its own terms.
