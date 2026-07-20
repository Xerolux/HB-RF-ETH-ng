import { Teleport, defineComponent, h, onBeforeUnmount, watch } from 'vue'

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
    footerClass: [String, Array, Object], titleClass: [String, Array, Object]
  },
  emits: ['update:modelValue', 'hide', 'shown'],
  setup(props, { attrs, slots, emit }) {
    let locked = false
    const lockBody = value => {
      if (value && !locked) { document.body.classList.add('modal-open'); document.body.style.overflow = 'hidden'; locked = true }
      if (!value && locked) { document.body.classList.remove('modal-open'); document.body.style.overflow = ''; locked = false }
    }
    watch(() => props.modelValue, value => { lockBody(value); if (value) queueMicrotask(() => emit('shown')) }, { immediate: true })
    onBeforeUnmount(() => lockBody(false))
    const requestClose = () => {
      const event = { defaultPrevented: false, preventDefault() { this.defaultPrevented = true } }
      emit('hide', event)
      if (!event.defaultPrevented) emit('update:modelValue', false)
    }
    return () => {
      if (!props.modelValue) return null
      const dialogClass = classList('modal-dialog', props.size ? `modal-${props.size}` : '', props.centered ? 'modal-dialog-centered' : '', props.scrollable ? 'modal-dialog-scrollable' : '')
      const modal = h('div', {
        ...attrs, class: classList('modal', 'fade', 'show', attrs.class), tabindex: '-1', role: 'dialog', style: { display: 'block' },
        onClick: event => { if (event.target === event.currentTarget) requestClose() }
      }, [h('div', { class: dialogClass }, [h('div', { class: classList('modal-content', props.contentClass) }, [
        props.hideHeader ? null : h('div', { class: classList('modal-header', props.headerClass) }, [
          h('h5', { class: classList('modal-title', props.titleClass) }, slots.title?.() || props.title || ''),
          h('button', { type: 'button', class: 'btn-close', 'aria-label': 'Schließen', onClick: requestClose })
        ]),
        h('div', { class: classList('modal-body', props.bodyClass) }, slots.default?.()),
        props.hideFooter ? null : h('div', { class: classList('modal-footer', props.footerClass) }, slots.footer?.() || [])
      ])])])
      return h(Teleport, { to: 'body' }, [modal, h('div', { class: 'modal-backdrop fade show' })])
    }
  }
})
