import { Teleport, defineComponent, h, onBeforeUnmount, watch } from 'vue'
import { lockBodyScroll, unlockBodyScroll } from '../composables/useBodyScrollLock'

const classList = (...values) => values.flat().filter(Boolean)

export const BButton = defineComponent({
  name: 'BButton', inheritAttrs: false,
  props: { variant: { default: 'secondary' }, size: String, block: Boolean, disabled: Boolean, type: { default: 'button' } },
  emits: ['click'],
  setup(props, { attrs, slots, emit }) {
    return () => h('button', {
      ...attrs,
      type: props.type,
      disabled: props.disabled,
      class: classList('btn', `btn-${props.variant}`, props.size ? `btn-${props.size}` : '', props.block ? 'w-100' : '', attrs.class),
      onClick: event => emit('click', event)
    }, slots.default?.())
  }
})

export const BAlert = defineComponent({
  name: 'BAlert', inheritAttrs: false,
  props: { variant: { default: 'info' }, modelValue: { default: undefined }, show: { default: true }, dismissible: Boolean, fade: Boolean },
  emits: ['update:modelValue'],
  setup(props, { attrs, slots, emit }) {
    return () => {
      const visible = props.modelValue === undefined ? props.show !== false : props.modelValue !== false
      if (!visible) return null
      return h('div', { ...attrs, role: 'alert', class: classList('alert', `alert-${props.variant}`, 'show', attrs.class) }, [
        slots.default?.(),
        props.dismissible ? h('button', {
          type: 'button', class: 'btn-close', 'aria-label': 'Schließen',
          onClick: () => emit('update:modelValue', false)
        }) : null
      ])
    }
  }
})

export const BCard = defineComponent({
  name: 'BCard', inheritAttrs: false,
  setup(_props, { attrs, slots }) {
    return () => h('div', { ...attrs, class: classList('card', attrs.class) }, [
      slots.header ? h('div', { class: 'card-header' }, slots.header()) : null,
      h('div', { class: 'card-body' }, slots.default?.())
    ])
  }
})

export const BForm = defineComponent({
  name: 'BForm', inheritAttrs: false, emits: ['submit'],
  setup(_props, { attrs, slots, emit }) {
    return () => h('form', { ...attrs, onSubmit: event => emit('submit', event) }, slots.default?.())
  }
})

export const BFormGroup = defineComponent({
  name: 'BFormGroup', inheritAttrs: false,
  props: { label: String, labelFor: String },
  setup(props, { attrs, slots }) {
    return () => h('div', { ...attrs, class: classList('mb-3', attrs.class) }, [
      props.label ? h('label', { class: 'form-label', for: props.labelFor }, props.label) : null,
      slots.default?.()
    ])
  }
})

const normalizeValue = (value, modifiers = {}) => {
  let next = value
  if (modifiers.trim && typeof next === 'string') next = next.trim()
  if (modifiers.number && next !== '') {
    const number = Number(next)
    if (!Number.isNaN(number)) next = number
  }
  return next
}

export const BFormInput = defineComponent({
  name: 'BFormInput', inheritAttrs: false,
  props: {
    modelValue: [String, Number], modelModifiers: { default: () => ({}) },
    state: { default: null }, size: String, type: { default: 'text' }, disabled: Boolean
  },
  emits: ['update:modelValue', 'input', 'change', 'blur', 'keyup'],
  setup(props, { attrs, emit }) {
    return () => h('input', {
      ...attrs,
      value: props.modelValue ?? '', type: props.type, disabled: props.disabled,
      class: classList('form-control', props.size ? `form-control-${props.size}` : '', props.state === true ? 'is-valid' : '', props.state === false ? 'is-invalid' : '', attrs.class),
      onInput: event => {
        const value = normalizeValue(event.target.value, props.modelModifiers)
        emit('update:modelValue', value); emit('input', event)
      },
      onChange: event => emit('change', event),
      onBlur: event => emit('blur', event),
      onKeyup: event => emit('keyup', event)
    })
  }
})

export const BFormInvalidFeedback = defineComponent({
  name: 'BFormInvalidFeedback', inheritAttrs: false,
  setup(_props, { attrs, slots }) {
    return () => h('div', { ...attrs, class: classList('invalid-feedback', 'd-block', attrs.class) }, slots.default?.())
  }
})

export const BFormSelect = defineComponent({
  name: 'BFormSelect', inheritAttrs: false,
  props: { modelValue: [String, Number, Boolean], modelModifiers: { default: () => ({}) }, options: { type: Array, default: () => [] }, size: String, disabled: Boolean },
  emits: ['update:modelValue', 'change'],
  setup(props, { attrs, slots, emit }) {
    const optionNode = option => {
      if (option && typeof option === 'object') {
        const value = option.value ?? option.id ?? option.text
        return h('option', { value, disabled: !!option.disabled }, option.text ?? option.label ?? String(value ?? ''))
      }
      return h('option', { value: option }, String(option ?? ''))
    }
    return () => h('select', {
      ...attrs, value: props.modelValue, disabled: props.disabled,
      class: classList('form-select', props.size ? `form-select-${props.size}` : '', attrs.class),
      onChange: event => {
        const value = normalizeValue(event.target.value, props.modelModifiers)
        emit('update:modelValue', value); emit('change', event)
      }
    }, [...props.options.map(optionNode), ...(slots.default?.() || [])])
  }
})

export const BFormSelectOption = defineComponent({
  name: 'BFormSelectOption', inheritAttrs: false,
  props: { value: [String, Number, Boolean], disabled: Boolean },
  setup(props, { attrs, slots }) {
    return () => h('option', { ...attrs, value: props.value, disabled: props.disabled }, slots.default?.())
  }
})

export const BModal = defineComponent({
  name: 'BModal', inheritAttrs: false,
  props: {
    modelValue: Boolean, title: String, size: String, centered: Boolean, scrollable: Boolean,
    hideHeader: Boolean, hideFooter: Boolean, contentClass: [String, Array, Object],
    headerClass: [String, Array, Object], bodyClass: [String, Array, Object],
    footerClass: [String, Array, Object], titleClass: [String, Array, Object],
    // Footer-button props (bootstrap-compatible). When hideFooter is false and
    // no default footer slot is supplied, BModal renders a default OK / Cancel
    // pair so every dismissible modal has a visible close affordance — fixes
    // the "Supporter-Key-Dialog blocks the page" bug where modals with only
    // ok-title/cancel-title props previously rendered NO buttons at all.
    okTitle: { type: String, default: 'OK' },
    cancelTitle: { type: String, default: 'Abbrechen' },
    okVariant: { type: String, default: 'primary' },
    cancelVariant: { type: String, default: 'secondary' },
    okOnly: Boolean,
    // Dismiss flags. backdrop = click on the modal-mask outside the dialog;
    // esc = the Escape key. When false, that dismiss path is disabled.
    noCloseOnBackdrop: Boolean,
    noCloseOnEsc: Boolean,
    hideHeaderClose: Boolean,
    // Disable the default OK / Cancel footer buttons (bootstrap-compatible).
    okDisabled: Boolean,
    cancelDisabled: Boolean,
    // Skip the CSS `fade` transition class so the dialog opens/closes
    // instantly. Several callers (restart confirmations) pass `no-fade`.
    noFade: Boolean
  },
  emits: ['update:modelValue', 'hide', 'shown', 'ok', 'cancel'],
  setup(props, { attrs, slots, emit }) {
    // Drive ALL body locking through the shared useBodyScrollLock composable
    // (single depth counter) so a BModal opened over the mobile menu — or vice
    // versa — releases the lock cleanly when the inner one closes. The
    // `modal-open` class stays for Bootstrap CSS expectations; the actual
    // scroll/position lock is owned by the composable.
    let locked = false
    const lockBody = value => {
      if (value && !locked) {
        document.body.classList.add('modal-open')
        lockBodyScroll()
        locked = true
      } else if (!value && locked) {
        document.body.classList.remove('modal-open')
        unlockBodyScroll()
        locked = false
      }
    }

    const requestClose = (trigger = null) => {
      const event = {
        trigger,
        defaultPrevented: false,
        preventDefault() { this.defaultPrevented = true }
      }
      emit('hide', event)
      if (!event.defaultPrevented) emit('update:modelValue', false)
    }

    // Programmatic OK / Cancel: fire the typed event, then close unless the
    // caller preventDefault()s the hide event (e.g. to keep the modal open
    // while a follow-up modal opens). Mirrors bootstrap-vue-next semantics.
    const triggerOk = () => {
      const event = { trigger: 'ok', defaultPrevented: false, preventDefault() { this.defaultPrevented = true } }
      emit('ok', event)
      if (!event.defaultPrevented) requestClose('ok')
    }
    const triggerCancel = () => {
      const event = { trigger: 'cancel', defaultPrevented: false, preventDefault() { this.defaultPrevented = true } }
      emit('cancel', event)
      if (!event.defaultPrevented) requestClose('cancel')
    }

    const onKeydown = (event) => {
      if (!props.modelValue) return
      if (event.key === 'Escape' && !props.noCloseOnEsc) {
        event.preventDefault()
        triggerCancel()
      }
    }

    watch(() => props.modelValue, value => {
      lockBody(value)
      if (value) {
        queueMicrotask(() => emit('shown'))
        // Attach Escape handling while open. Use a window-level listener so it
        // fires regardless of focus (matches the modal-mask click behaviour).
        window.addEventListener('keydown', onKeydown, true)
      } else {
        window.removeEventListener('keydown', onKeydown, true)
      }
    }, { immediate: true })

    onBeforeUnmount(() => {
      window.removeEventListener('keydown', onKeydown, true)
      lockBody(false)
    })

    return () => {
      if (!props.modelValue) return null
      const dialogClass = classList('modal-dialog', props.size ? `modal-${props.size}` : '', props.centered ? 'modal-dialog-centered' : '', props.scrollable ? 'modal-dialog-scrollable' : '')
      const onMaskClick = (event) => {
        if (props.noCloseOnBackdrop) return
        if (event.target === event.currentTarget) triggerCancel()
      }

      // Default footer: explicit OK / Cancel buttons unless the caller supplied
      // a footer slot or set hideFooter. okOnly hides the Cancel button.
      const defaultFooter = () => [
        props.okOnly ? null : h('button', {
          type: 'button',
          class: classList('btn', `btn-${props.cancelVariant}`),
          disabled: props.cancelDisabled,
          onClick: triggerCancel
        }, props.cancelTitle),
        h('button', {
          type: 'button',
          class: classList('btn', `btn-${props.okVariant}`),
          disabled: props.okDisabled,
          onClick: triggerOk
        }, props.okTitle)
      ]

      const modal = h('div', {
        ...attrs, class: classList('modal', props.noFade ? null : 'fade', 'show', attrs.class), tabindex: '-1', role: 'dialog', 'aria-modal': 'true', style: { display: 'block' },
        onClick: onMaskClick
      }, [h('div', { class: dialogClass }, [h('div', { class: classList('modal-content', props.contentClass) }, [
        props.hideHeader ? null : h('div', { class: classList('modal-header', props.headerClass) }, [
          h('h5', { class: classList('modal-title', props.titleClass) }, slots.title?.() || props.title || ''),
          props.hideHeaderClose ? null : h('button', { type: 'button', class: 'btn-close', 'aria-label': 'Schließen', onClick: triggerCancel })
        ]),
        h('div', { class: classList('modal-body', props.bodyClass) }, slots.default?.()),
        props.hideFooter ? null : h('div', { class: classList('modal-footer', props.footerClass) }, slots.footer?.() || defaultFooter())
      ])])])
      return h(Teleport, { to: 'body' }, [modal, h('div', { class: classList('modal-backdrop', props.noFade ? null : 'fade', 'show') })])
    }
  }
})
