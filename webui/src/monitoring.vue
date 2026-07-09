<template>
  <div class="page-shell monitoring-page">
    <section class="page-hero">
      <div class="hero-copy">
        <span class="hero-eyebrow">
          <AppIcon name="monitoring" />
          {{ t('monitoring.title') }}
        </span>
        <h1 class="hero-title">{{ t('monitoring.title') }}</h1>
        <p class="hero-subtitle">{{ t('monitoring.description') }}</p>
      </div>
      <div class="hero-meta">
        <span
          v-for="service in monitoringChips"
          :key="service.key"
          class="meta-chip"
          :class="{ active: service.active }"
        >
          <AppIcon :name="service.icon" />
          {{ service.label }}
        </span>
      </div>
    </section>

    <div v-if="showResourceWarning" class="monitoring-resource-warning">
      <AppIcon name="alert" />
      <div>
        <strong>{{ t('monitoring.resourceWarningTitle') }}</strong>
        <p>{{ t('monitoring.resourceWarningText', { count: activeMonitoringServiceCount }) }}</p>
      </div>
    </div>

    <section class="diagnostics-panel settings-card card-glass">
      <div class="card-header">
        <div class="header-content">
          <div class="header-icon bg-primary-light text-primary"><AppIcon name="activity" /></div>
          <div>
            <h3>{{ t('monitoring.selfTestTitle') }}</h3>
            <p class="diagnostics-subtitle">{{ t('monitoring.selfTestHint') }}</p>
          </div>
        </div>
      </div>
      <div class="card-body">
        <ul class="diag-list">
          <li v-for="item in diagnosticCards" :key="item.key" class="diag-row" :class="diagStatusClass(item.key)">
            <div class="diag-row-head">
              <span class="diag-name">
                <span class="diag-dot" :class="diagStatusClass(item.key)"></span>
                {{ item.title }}
              </span>
              <button class="diag-test-btn" type="button" :disabled="diagnosticBusy[item.key]" @click="runDiagnostic(item.key)">
                <span v-if="diagnosticBusy[item.key]" class="spinner-border spinner-border-sm"></span>
                <AppIcon v-else name="refresh" />
                {{ t('monitoring.testButton') }}
              </button>
            </div>
            <p v-if="diagMessage(item.key)" class="diag-message">{{ diagMessage(item.key) }}</p>
          </li>
        </ul>
      </div>
    </section>

    <div class="settings-card card-glass">
      <div class="card-header">
        <div class="header-content">
          <div class="header-icon bg-danger-light text-danger"><AppIcon name="search" /></div>
          <h3>{{ t('monitoring.checkmk.title') }}</h3>
        </div>
        <div class="form-check form-switch">
          <input class="form-check-input" type="checkbox" v-model="checkmkConfig.enabled">
        </div>
      </div>

      <Transition name="expand">
        <div v-if="checkmkConfig.enabled" class="card-body">
          <div class="row g-3">
            <div class="col-md-6">
              <label class="form-label">{{ t('monitoring.checkmk.port') }}</label>
              <BFormInput v-model.number="checkmkConfig.port" type="number" min="1" max="65535" />
              <div class="form-text">{{ t('monitoring.checkmk.portHelp') }}</div>
            </div>
            <div class="col-md-6">
              <label class="form-label">{{ t('monitoring.checkmk.allowedHosts') }}</label>
              <BFormInput v-model="checkmkConfig.allowedHosts" />
              <div class="form-text">{{ t('monitoring.checkmk.allowedHostsHelp') }}</div>
            </div>
          </div>
        </div>
      </Transition>
    </div>

    <div class="settings-card card-glass">
      <div class="card-header">
        <div class="header-content">
          <div class="header-icon bg-success-light text-success"><AppIcon name="activity" /></div>
          <h3>{{ t('monitoring.mqtt.title') }}</h3>
        </div>
        <div class="form-check form-switch">
          <input class="form-check-input" type="checkbox" v-model="mqttConfig.enabled">
        </div>
      </div>

      <Transition name="expand">
        <div v-if="mqttConfig.enabled" class="card-body">
          <div class="row g-3">
            <div class="col-md-8">
              <label class="form-label">{{ t('monitoring.mqtt.server') }}</label>
              <BFormInput v-model="mqttConfig.server" required />
              <div class="form-text">{{ t('monitoring.mqtt.serverHelp') }}</div>
            </div>
            <div class="col-md-4">
              <label class="form-label">{{ t('monitoring.mqtt.port') }}</label>
              <BFormInput v-model.number="mqttConfig.port" type="number" min="1" max="65535" />
              <div class="form-text">{{ t('monitoring.mqtt.portHelp') }}</div>
            </div>
            <div class="col-md-6">
              <label class="form-label">{{ t('monitoring.mqtt.user') }}</label>
              <BFormInput v-model="mqttConfig.user" />
              <div class="form-text">{{ t('monitoring.mqtt.userHelp') }}</div>
            </div>
            <div class="col-md-6">
              <label class="form-label">{{ t('monitoring.mqtt.password') }}</label>
              <BFormInput v-model="mqttConfig.password" type="password" />
              <div class="form-text">{{ t('monitoring.mqtt.passwordHelp') }}</div>
            </div>
            <div class="col-12">
              <label class="form-label">{{ t('monitoring.mqtt.topicPrefix') }}</label>
              <BFormInput v-model="mqttConfig.topicPrefix" />
              <div class="form-text">{{ t('monitoring.mqtt.topicPrefixHelp') }}</div>
            </div>

            <div class="col-12 mt-4">
              <div class="tls-section">
                <div class="d-flex justify-content-between align-items-center mb-2">
                  <div>
                    <h4>{{ t('monitoring.mqtt.tls.title') }}</h4>
                    <div class="form-text mb-0">{{ t('monitoring.mqtt.tls.enableHelp') }}</div>
                  </div>
                  <div class="form-check form-switch">
                    <input class="form-check-input" type="checkbox" v-model="mqttConfig.tlsEnable">
                  </div>
                </div>

                <Transition name="expand">
                  <div v-if="mqttConfig.tlsEnable">
                    <div class="d-flex justify-content-between align-items-center mt-3 mb-2">
                      <label class="form-label mb-0">{{ t('monitoring.mqtt.tls.skipVerify') }}</label>
                      <div class="form-check form-switch">
                        <input class="form-check-input" type="checkbox" v-model="mqttConfig.tlsSkipVerify">
                      </div>
                    </div>

                    <div v-if="mqttConfig.tlsSkipVerify" class="alert-warning-soft">
                      <div>{{ t('monitoring.mqtt.tls.skipVerifyAlert') }}</div>
                    </div>

                    <div v-if="!mqttConfig.tlsSkipVerify" class="mt-3">
                      <div class="pem-label-row">
                        <label class="form-label mb-0">{{ t('monitoring.mqtt.tls.caCerts') }}</label>
                        <button type="button" class="btn-load-file" @click="$refs.caFileInput.click()">
                          <AppIcon name="file" /> {{ t('monitoring.mqtt.tls.loadFromFile') }}
                        </button>
                      </div>
                      <input ref="caFileInput" type="file" accept=".pem,.crt,.cer,.txt" hidden
                             @change="loadPemFile($event, 'tlsCaCerts', 'ca')">
                      <textarea class="form-control pem-textarea" v-model="mqttConfig.tlsCaCerts"
                        :placeholder="'-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----'"
                        @blur="validatePemField('tlsCaCerts', 'ca')"></textarea>
                      <div v-if="pemFeedback.tlsCaCerts" class="pem-feedback"
                           :class="pemFeedback.tlsCaCerts.kind === 'ok' ? 'is-ok' : 'is-error'">
                        {{ pemFeedback.tlsCaCerts.msg }}
                      </div>
                      <div class="cert-status-row">
                        <div class="form-text mb-0">{{ t('monitoring.mqtt.tls.caCertsHelp') }}</div>
                        <div v-if="mqttConfig.tlsCaCertsSet && !mqttConfig.tlsCaCerts" class="d-flex gap-3 align-items-center">
                          <span class="cert-present-hint">{{ t('monitoring.mqtt.tls.certPresent') }}</span>
                          <button type="button" class="btn-link-danger"
                                  @click="clearCert('tlsCaCerts', 'tlsCaCertsClear', 'tlsCaCertsSet')">
                            {{ t('monitoring.mqtt.tls.clear') }}
                          </button>
                        </div>
                      </div>
                    </div>

                    <div class="mt-3">
                      <div class="pem-label-row">
                        <label class="form-label mb-0">{{ t('monitoring.mqtt.tls.certfile') }}</label>
                        <button type="button" class="btn-load-file" @click="$refs.certFileInput.click()">
                          <AppIcon name="file" /> {{ t('monitoring.mqtt.tls.loadFromFile') }}
                        </button>
                      </div>
                      <input ref="certFileInput" type="file" accept=".pem,.crt,.cer,.txt" hidden
                             @change="loadPemFile($event, 'tlsCertfile', 'cert')">
                      <textarea class="form-control pem-textarea" v-model="mqttConfig.tlsCertfile"
                        :placeholder="'-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----'"
                        @blur="validatePemField('tlsCertfile', 'cert')"></textarea>
                      <div v-if="pemFeedback.tlsCertfile" class="pem-feedback"
                           :class="pemFeedback.tlsCertfile.kind === 'ok' ? 'is-ok' : 'is-error'">
                        {{ pemFeedback.tlsCertfile.msg }}
                      </div>
                      <div class="cert-status-row">
                        <div class="form-text mb-0">{{ t('monitoring.mqtt.tls.certfileHelp') }}</div>
                        <div v-if="mqttConfig.tlsCertfileSet && !mqttConfig.tlsCertfile" class="d-flex gap-3 align-items-center">
                          <span class="cert-present-hint">{{ t('monitoring.mqtt.tls.certPresent') }}</span>
                          <button type="button" class="btn-link-danger"
                                  @click="clearCert('tlsCertfile', 'tlsCertfileClear', 'tlsCertfileSet')">
                            {{ t('monitoring.mqtt.tls.clear') }}
                          </button>
                        </div>
                      </div>
                    </div>

                    <div class="mt-3">
                      <div class="pem-label-row">
                        <label class="form-label mb-0">{{ t('monitoring.mqtt.tls.keyfile') }}</label>
                        <button type="button" class="btn-load-file" @click="$refs.keyFileInput.click()">
                          <AppIcon name="file" /> {{ t('monitoring.mqtt.tls.loadFromFile') }}
                        </button>
                      </div>
                      <input ref="keyFileInput" type="file" accept=".pem,.key,.txt" hidden
                             @change="loadPemFile($event, 'tlsKeyfile', 'key')">
                      <textarea class="form-control pem-textarea" v-model="mqttConfig.tlsKeyfile"
                        :placeholder="'-----BEGIN PRIVATE KEY-----\n...\n-----END PRIVATE KEY-----'"
                        @blur="validatePemField('tlsKeyfile', 'key')"></textarea>
                      <div v-if="pemFeedback.tlsKeyfile" class="pem-feedback"
                           :class="pemFeedback.tlsKeyfile.kind === 'ok' ? 'is-ok' : 'is-error'">
                        {{ pemFeedback.tlsKeyfile.msg }}
                      </div>
                      <div class="cert-status-row">
                        <div class="form-text mb-0">{{ t('monitoring.mqtt.tls.keyfileHelp') }}</div>
                        <div v-if="mqttConfig.tlsKeyfileSet && !mqttConfig.tlsKeyfile" class="d-flex gap-3 align-items-center">
                          <span class="cert-present-hint">{{ t('monitoring.mqtt.tls.certPresent') }}</span>
                          <button type="button" class="btn-link-danger"
                                  @click="clearCert('tlsKeyfile', 'tlsKeyfileClear', 'tlsKeyfileSet')">
                            {{ t('monitoring.mqtt.tls.clear') }}
                          </button>
                        </div>
                      </div>
                    </div>

                    <div v-if="mtlsInconsistent" class="alert-warning-soft mt-3">
                      <div>{{ t('monitoring.mqtt.tls.mtlsRequiresBoth') }}</div>
                    </div>
                  </div>
                </Transition>
              </div>
            </div>

            <div class="col-12 mt-4">
              <div class="d-flex justify-content-between align-items-center mb-2">
                <label class="form-label mb-0">{{ t('monitoring.mqtt.haDiscoveryEnabled') }}</label>
                <div class="form-check form-switch">
                  <input class="form-check-input" type="checkbox" v-model="mqttConfig.haDiscoveryEnabled">
                </div>
              </div>

              <Transition name="expand">
                <div v-if="mqttConfig.haDiscoveryEnabled" class="mt-2">
                  <label class="form-label">{{ t('monitoring.mqtt.haDiscoveryPrefix') }}</label>
                  <BFormInput v-model="mqttConfig.haDiscoveryPrefix" />
                  <div class="form-text">{{ t('monitoring.mqtt.haDiscoveryPrefixHelp') }}</div>
                </div>
              </Transition>
            </div>

            <div class="col-12 mt-4">
              <div class="command-section">
                <div class="d-flex justify-content-between align-items-center mb-2">
                  <div>
                    <h4>{{ t('monitoring.mqtt.commands.title') }}</h4>
                    <div class="form-text mb-0">{{ t('monitoring.mqtt.commands.enableHelp') }}</div>
                  </div>
                  <div class="form-check form-switch">
                    <input class="form-check-input" type="checkbox" v-model="mqttConfig.commandEnabled">
                  </div>
                </div>

                <Transition name="expand">
                  <div v-if="mqttConfig.commandEnabled">
                    <div v-if="!mqttConfig.tlsEnable" class="alert-warning-soft mt-3">
                      <div>{{ t('monitoring.mqtt.commands.aclHint') }}</div>
                    </div>

                    <div class="mt-3">
                      <label class="form-label">{{ t('monitoring.mqtt.commands.token') }}</label>
                      <div class="d-flex gap-2 align-items-center">
                        <BFormInput v-model="mqttConfig.commandToken" type="text"
                          :placeholder="t('monitoring.mqtt.commands.tokenPlaceholder')" />
                        <button v-if="mqttConfig.commandTokenSet && !mqttConfig.commandToken"
                                type="button" class="btn-link-danger"
                                @click="clearCommandToken">
                          {{ t('monitoring.mqtt.commands.clear') }}
                        </button>
                      </div>
                      <div class="form-text">
                        <span v-if="mqttConfig.commandTokenSet" class="cert-present-hint">
                                          {{ t('monitoring.mqtt.commands.tokenPresent') }}
                                        </span>
                        {{ t('monitoring.mqtt.commands.tokenHelp') }}
                      </div>
                    </div>
                  </div>
                </Transition>
              </div>
            </div>
          </div>
        </div>
      </Transition>
    </div>

    <!-- Prometheus exporter card (Phase A) -->
    <div class="settings-card card-glass">
      <div class="card-header">
        <div class="header-content">
          <div class="header-icon bg-info-light text-info"><AppIcon name="activity" /></div>
          <h3>{{ t('monitoring.prometheus.title') }}</h3>
        </div>
        <div class="form-check form-switch">
          <input class="form-check-input" type="checkbox" v-model="prometheusConfig.enabled">
        </div>
      </div>
      <Transition name="expand">
        <div v-if="prometheusConfig.enabled" class="card-body">
          <div class="row g-3">
            <div class="col-md-6">
              <label class="form-label">{{ t('monitoring.prometheus.port') }}</label>
              <BFormInput v-model.number="prometheusConfig.port" type="number" min="1" max="65535" />
              <div class="form-text">{{ t('monitoring.prometheus.portHelp') }}</div>
            </div>
            <div class="col-md-6">
              <label class="form-label">{{ t('monitoring.prometheus.allowedHosts') }}</label>
              <BFormInput v-model="prometheusConfig.allowedHosts" />
              <div class="form-text">{{ t('monitoring.prometheus.allowedHostsHelp') }}</div>
            </div>
          </div>
        </div>
      </Transition>
    </div>

    <!-- Syslog forwarder card (Phase B) -->
    <div class="settings-card card-glass">
      <div class="card-header">
        <div class="header-content">
          <div class="header-icon bg-info-light text-info"><AppIcon name="logs" /></div>
          <h3>{{ t('monitoring.syslog.title') }}</h3>
        </div>
        <div class="form-check form-switch">
          <input class="form-check-input" type="checkbox" v-model="syslogConfig.enabled">
        </div>
      </div>
      <Transition name="expand">
        <div v-if="syslogConfig.enabled" class="card-body">
          <div class="row g-3">
            <div class="col-md-8">
              <label class="form-label">{{ t('monitoring.syslog.server') }}</label>
              <BFormInput v-model="syslogConfig.server" />
              <div class="form-text">{{ t('monitoring.syslog.serverHelp') }}</div>
            </div>
            <div class="col-md-4">
              <label class="form-label">{{ t('monitoring.syslog.port') }}</label>
              <BFormInput v-model.number="syslogConfig.port" type="number" min="1" max="65535" />
              <div class="form-text">{{ t('monitoring.syslog.portHelp') }}</div>
            </div>
            <div class="col-md-4">
              <label class="form-label">{{ t('monitoring.syslog.transport') }}</label>
              <select class="form-select" v-model.number="syslogConfig.transport">
                <option :value="0">UDP</option>
                <option :value="1">TCP</option>
                <option :value="2">TLS</option>
              </select>
            </div>
            <div class="col-md-4">
              <label class="form-label">{{ t('monitoring.syslog.minSeverity') }}</label>
              <select class="form-select" v-model.number="syslogConfig.minSeverity">
                <option :value="0">EMERG</option>
                <option :value="1">ALERT</option>
                <option :value="2">CRIT</option>
                <option :value="3">ERR</option>
                <option :value="4">WARNING</option>
                <option :value="5">NOTICE</option>
                <option :value="6">INFO</option>
                <option :value="7">DEBUG</option>
              </select>
              <div class="form-text">{{ t('monitoring.syslog.minSeverityHelp') }}</div>
            </div>
            <div class="col-md-4">
              <label class="form-label">{{ t('monitoring.syslog.hostname') }}</label>
              <BFormInput v-model="syslogConfig.hostname" />
              <div class="form-text">{{ t('monitoring.syslog.hostnameHelp') }}</div>
            </div>
          </div>
        </div>
      </Transition>
    </div>

    <!-- Event notifications card (Phase C/D) -->
    <div class="settings-card card-glass">
      <div class="card-header">
        <div class="header-content">
          <div class="header-icon bg-info-light text-info"><AppIcon name="activity" /></div>
          <h3>{{ t('monitoring.notify.title') }}</h3>
        </div>
        <div class="form-check form-switch">
          <input class="form-check-input" type="checkbox" v-model="notifyConfig.enabled">
        </div>
      </div>
      <Transition name="expand">
        <div v-if="notifyConfig.enabled" class="card-body">
          <div class="row g-3">
            <div class="col-md-6">
              <label class="form-label">{{ t('monitoring.notify.channels') }}</label>
              <div class="form-check">
                <input class="form-check-input" type="checkbox" :value="1"
                  :checked="!!(notifyConfig.channels & 1)"
                  @change="notifyConfig.channels = $event.target.checked ? (notifyConfig.channels | 1) : (notifyConfig.channels & ~1)">
                <label class="form-check-label">{{ t('monitoring.notify.channelWebhook') }}</label>
              </div>
              <div class="form-check">
                <input class="form-check-input" type="checkbox" :value="2"
                  :checked="!!(notifyConfig.channels & 2)"
                  @change="notifyConfig.channels = $event.target.checked ? (notifyConfig.channels | 2) : (notifyConfig.channels & ~2)">
                <label class="form-check-label">{{ t('monitoring.notify.channelTelegram') }}</label>
              </div>
              <div class="form-check">
                <input class="form-check-input" type="checkbox" :value="4"
                  :checked="!!(notifyConfig.channels & 4)"
                  @change="notifyConfig.channels = $event.target.checked ? (notifyConfig.channels | 4) : (notifyConfig.channels & ~4)">
                <label class="form-check-label">{{ t('monitoring.notify.channelEmail') }}</label>
              </div>
            </div>
            <div class="col-md-6">
              <label class="form-label">{{ t('monitoring.notify.cooldown') }}</label>
              <BFormInput v-model.number="notifyConfig.cooldownSeconds" type="number" min="0" max="86400" />
              <div class="form-text">{{ t('monitoring.notify.cooldownHelp') }}</div>
            </div>

            <div v-if="notifyConfig.channels & 1" class="col-12 mt-3">
              <h4>{{ t('monitoring.notify.webhookSection') }}</h4>
              <label class="form-label">{{ t('monitoring.notify.webhookUrl') }}</label>
              <BFormInput v-model="notifyConfig.webhookUrl" />
              <label class="form-label mt-2">{{ t('monitoring.notify.webhookSecret') }}</label>
              <div class="d-flex gap-2 align-items-center">
                <BFormInput v-model="notifyConfig.webhookSecret" type="password"
                            :placeholder="notifyConfig.webhookSecretSet ? t('monitoring.notify.secretPresent') : ''" />
                <button v-if="notifyConfig.webhookSecretSet && !notifyConfig.webhookSecret"
                        type="button" class="btn-link-danger"
                        @click="clearNotifySecret('webhookSecret', 'webhookSecretClear', 'webhookSecretSet')">
                  {{ t('common.clear') }}
                </button>
              </div>
            </div>

            <div v-if="notifyConfig.channels & 2" class="col-12 mt-3">
              <h4>{{ t('monitoring.notify.telegramSection') }}</h4>
              <label class="form-label">{{ t('monitoring.notify.telegramToken') }}</label>
              <div class="d-flex gap-2 align-items-center">
                <BFormInput v-model="notifyConfig.telegramToken" type="password"
                            :placeholder="notifyConfig.telegramTokenSet ? t('monitoring.notify.secretPresent') : ''" />
                <button v-if="notifyConfig.telegramTokenSet && !notifyConfig.telegramToken"
                        type="button" class="btn-link-danger"
                        @click="clearNotifySecret('telegramToken', 'telegramTokenClear', 'telegramTokenSet')">
                  {{ t('common.clear') }}
                </button>
              </div>
              <label class="form-label mt-2">{{ t('monitoring.notify.telegramChatId') }}</label>
              <BFormInput v-model="notifyConfig.telegramChatId" />
            </div>

            <div v-if="notifyConfig.channels & 4" class="col-12 mt-3">
              <h4>{{ t('monitoring.notify.smtpSection') }}</h4>
              <div class="row g-3">
                <div class="col-md-8">
                  <label class="form-label">{{ t('monitoring.notify.smtpServer') }}</label>
                  <BFormInput v-model="notifyConfig.smtpServer" />
                </div>
                <div class="col-md-4">
                  <label class="form-label">{{ t('monitoring.notify.smtpPort') }}</label>
                  <BFormInput v-model.number="notifyConfig.smtpPort" type="number" min="1" max="65535" />
                </div>
                <div class="col-md-6">
                  <label class="form-label">{{ t('monitoring.notify.smtpTls') }}</label>
                  <select class="form-select" v-model.number="notifyConfig.smtpTls">
                    <option :value="0">{{ t('monitoring.notify.smtpTlsNone') }}</option>
                    <option :value="1">{{ t('monitoring.notify.smtpTlsStarttls') }}</option>
                    <option :value="2">{{ t('monitoring.notify.smtpTlsImplicit') }}</option>
                  </select>
                </div>
                <div class="col-md-6">
                  <label class="form-label">{{ t('monitoring.notify.smtpUser') }}</label>
                  <BFormInput v-model="notifyConfig.smtpUser" />
                </div>
                <div class="col-md-6">
                  <label class="form-label">{{ t('monitoring.notify.smtpPassword') }}</label>
                  <div class="d-flex gap-2 align-items-center">
                    <BFormInput v-model="notifyConfig.smtpPassword" type="password"
                                :placeholder="notifyConfig.smtpPasswordSet ? t('monitoring.notify.secretPresent') : ''" />
                    <button v-if="notifyConfig.smtpPasswordSet && !notifyConfig.smtpPassword"
                            type="button" class="btn-link-danger"
                            @click="clearNotifySecret('smtpPassword', 'smtpPasswordClear', 'smtpPasswordSet')">
                      {{ t('common.clear') }}
                    </button>
                  </div>
                </div>
                <div class="col-md-6">
                  <label class="form-label">{{ t('monitoring.notify.smtpFrom') }}</label>
                  <BFormInput v-model="notifyConfig.smtpFrom" />
                </div>
                <div class="col-md-6">
                  <label class="form-label">{{ t('monitoring.notify.smtpTo') }}</label>
                  <BFormInput v-model="notifyConfig.smtpTo" />
                </div>
              </div>
            </div>
          </div>
        </div>
      </Transition>
    </div>

    <Transition name="slide-up">
      <div v-if="hasChanges" class="floating-footer">
        <div class="footer-container">
          <BButton
            variant="primary"
            size="lg"
            @click="saveConfig"
            :disabled="saving"
            class="save-btn"
          >
            <span v-if="saving" class="spinner-border spinner-border-sm me-2"></span>
            {{ saving ? t('monitoring.saving') : t('monitoring.save') }}
          </BButton>
        </div>
      </div>
    </Transition>
  </div>
</template>
<script setup>
import { ref, onMounted, computed, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useMonitoringStore, useUiStore } from './stores.js'
import { storeToRefs } from 'pinia'

const { t, te } = useI18n()

const monitoringStore = useMonitoringStore()
const uiStore = useUiStore()
const { checkmk: checkmkConfig, mqtt: mqttConfig,
        prometheus: prometheusConfig, syslog: syslogConfig,
        notify: notifyConfig } = storeToRefs(monitoringStore)

const saving = ref(false)
const diagnosticBusy = ref({ checkmk: false, mqtt: false, prometheus: false, syslog: false, notify: false })
const hasChanges = ref(false)
const originalConfig = ref('')

const tlsClearFlags = ref({ tlsCaCertsClear: false, tlsCertfileClear: false, tlsKeyfileClear: false, commandTokenClear: false,
                            webhookSecretClear: false, telegramTokenClear: false, smtpPasswordClear: false })
const pemFeedback = ref({ tlsCaCerts: null, tlsCertfile: null, tlsKeyfile: null })

const MAX_PEM_BYTES = 8 * 1024
const MAX_FILE_BYTES = 16 * 1024
const ALLOWED_LABELS = {
  ca:   ['CERTIFICATE'],
  cert: ['CERTIFICATE'],
  key:  ['PRIVATE KEY', 'RSA PRIVATE KEY', 'EC PRIVATE KEY']
}

function normalizePem(raw) {
  let s = String(raw)
  s = s.replace(/^﻿/, '').replace(/\r\n/g, '\n').replace(/\r/g, '\n')
  s = s.split('\n').map(l => l.replace(/[\t ]+$/g, '')).join('\n').trim()
  return s.length > 0 ? s + '\n' : s
}

function validatePem(raw, kind) {
  if (!raw || !raw.trim()) return null
  if (!/-----BEGIN /.test(raw)) {
    return /[\x00-\x08\x0E-\x1F]/.test(raw)
      ? { kind: 'error', msg: t('monitoring.mqtt.tls.binaryFormatError') }
      : { kind: 'error', msg: t('monitoring.mqtt.tls.invalidPemFormat') }
  }
  const re = /-----BEGIN ([A-Z0-9 ]+)-----\s*([\s\S]*?)\s*-----END \1-----/g
  const blocks = []
  let m
  while ((m = re.exec(raw)) !== null) blocks.push({ label: m[1].trim(), body: m[2] })
  if (!blocks.length) return { kind: 'error', msg: t('monitoring.mqtt.tls.invalidPemFormat') }
  if (blocks.some(b => b.label === 'ENCRYPTED PRIVATE KEY'))
    return { kind: 'error', msg: t('monitoring.mqtt.tls.encryptedKeyError') }
  if (kind === 'key' && blocks.length !== 1)
    return { kind: 'error', msg: t('monitoring.mqtt.tls.keySingleBlock') }
  const allowed = ALLOWED_LABELS[kind] || []
  for (const b of blocks) {
    if (!allowed.includes(b.label)) return { kind: 'error', msg: t('monitoring.mqtt.tls.unexpectedPemLabel') }
    if (!/^[A-Za-z0-9+/=\s]+$/.test(b.body)) return { kind: 'error', msg: t('monitoring.mqtt.tls.invalidBase64') }
  }
  if (raw.length > MAX_PEM_BYTES) return { kind: 'error', msg: t('monitoring.mqtt.tls.pemTooLarge') }
  return { kind: 'ok', msg: t('monitoring.mqtt.tls.pemValid', { count: blocks.length }) }
}

function validatePemField(field, kind) {
  const val = mqttConfig.value[field]
  if (!val || !val.trim()) { pemFeedback.value[field] = null; return true }
  const result = validatePem(val, kind)
  pemFeedback.value[field] = result
  return result === null || result.kind !== 'error'
}

function loadPemFile(event, field, kind) {
  const file = event.target.files[0]
  if (!file) return
  if (/\.(p12|pfx)$/i.test(file.name)) {
    pemFeedback.value[field] = { kind: 'error', msg: t('monitoring.mqtt.tls.pkcs12Error') }
    event.target.value = ''; return
  }
  if (file.size > MAX_FILE_BYTES) {
    pemFeedback.value[field] = { kind: 'error', msg: t('monitoring.mqtt.tls.fileTooLarge') }
    event.target.value = ''; return
  }
  const reader = new FileReader()
  reader.onload = (e) => {
    const normalized = normalizePem(e.target.result)
    mqttConfig.value[field] = normalized
    pemFeedback.value[field] = validatePem(normalized, kind)
    // Loading a fresh PEM supersedes any earlier "clear" intent; otherwise the
    // save payload would carry both the new cert body and tlsXxxClear = true.
    tlsClearFlags.value[`${field}Clear`] = false
  }
  reader.onerror = () => { pemFeedback.value[field] = { kind: 'error', msg: t('monitoring.mqtt.tls.fileReadError') } }
  reader.readAsText(file)
  event.target.value = ''
}

function clearCert(field, clearFlag, setFlag) {
  mqttConfig.value[field] = ''
  mqttConfig.value[setFlag] = false
  tlsClearFlags.value[clearFlag] = true
  pemFeedback.value[field] = null
}

function clearCommandToken() {
  mqttConfig.value.commandToken = ''
  mqttConfig.value.commandTokenSet = false
  tlsClearFlags.value.commandTokenClear = true
}

function clearNotifySecret(field, clearFlag, setFlag) {
  notifyConfig.value[field] = ''
  notifyConfig.value[setFlag] = false
  tlsClearFlags.value[clearFlag] = true
}

const mtlsInconsistent = computed(() => {
  const hasCert = !!(mqttConfig.value.tlsCertfile || mqttConfig.value.tlsCertfileSet)
  const hasKey  = !!(mqttConfig.value.tlsKeyfile  || mqttConfig.value.tlsKeyfileSet)
  return (hasCert || hasKey) && !(hasCert && hasKey)
})

const activeMonitoringServiceCount = computed(() => [
  checkmkConfig.value.enabled,
  mqttConfig.value.enabled,
  prometheusConfig.value.enabled,
  syslogConfig.value.enabled,
  notifyConfig.value.enabled && Number(notifyConfig.value.channels || 0) !== 0
].filter(Boolean).length)

// Hero status chips — one per monitoring service, highlighted when active.
// Keeps the hero consistent with the five config cards below instead of only
// surfacing MQTT + CheckMK.
const monitoringChips = computed(() => [
  { key: 'mqtt', label: t('monitoring.chipLabelMqtt'), icon: 'activity', active: mqttConfig.value.enabled },
  { key: 'checkmk', label: t('monitoring.chipLabelCheckmk'), icon: 'logs', active: checkmkConfig.value.enabled },
  { key: 'prometheus', label: t('monitoring.chipLabelPrometheus'), icon: 'activity', active: prometheusConfig.value.enabled },
  { key: 'syslog', label: t('monitoring.chipLabelSyslog'), icon: 'logs', active: syslogConfig.value.enabled },
  { key: 'notify', label: t('monitoring.chipLabelNotify'), icon: 'activity', active: notifyConfig.value.enabled && Number(notifyConfig.value.channels || 0) !== 0 }
])

const showResourceWarning = computed(() => activeMonitoringServiceCount.value >= 4)

const diagnosticCards = computed(() => [
  { key: 'checkmk', title: t('monitoring.chipLabelCheckmk'), icon: 'logs', tone: 'warning' },
  { key: 'mqtt', title: t('monitoring.chipLabelMqtt'), icon: 'activity', tone: 'success' },
  { key: 'prometheus', title: t('monitoring.chipLabelPrometheus'), icon: 'activity', tone: 'info' },
  { key: 'syslog', title: t('monitoring.chipLabelSyslog'), icon: 'logs', tone: 'info' },
  { key: 'notify', title: t('monitoring.chipLabelNotify'), icon: 'activity', tone: 'info' }
])

const diagStatusClass = (target) => {
  if (diagnosticBusy.value[target]) return 'running'
  const r = monitoringStore.diagnostics[target]
  if (!r) return 'pending'
  return r.ok ? 'ok' : 'failed'
}

const diagMessage = (target) => {
  const r = monitoringStore.diagnostics[target]
  if (!r) return ''
  return formatDiagnosticMessage(r)
}

// Maps a backend diagnostic response to a translated message.
//
// The backend returns a stable machine-readable `code` (e.g.
// "monitoring.diag.mqtt.tcp_failed") plus optional interpolation params
// (host, port, tlsEnabled). We translate the code when the current locale
// has a matching key, otherwise we fall back to the English `message` the
// backend sends alongside the code — this keeps older translations working
// until the new keys are filled in.
const TLS_AWARE_CODES = new Set([
  'monitoring.diag.mqtt.tcp_ok',
  'monitoring.diag.mqtt.tcp_failed'
])
function formatDiagnosticMessage(result) {
  if (!result) return ''
  const code = result.code
  if (code && te(code)) {
    const params = {}
    if (result.host) params.host = result.host
    if (result.port !== undefined && result.port !== null) params.port = result.port
    // The notify diagnostic embeds the channel bitmask in its message;
    // expose it as {mask} for translation.
    if (code === 'monitoring.diag.notify.queued' && result.message) {
      const m = result.message.match(/0x([0-9a-fA-F]+)/)
      if (m) params.mask = m[1]
    }
    let msg = t(code, params)
    if (result.tlsEnabled && TLS_AWARE_CODES.has(code) && te('monitoring.diag.mqtt.tls_note')) {
      msg += t('monitoring.diag.mqtt.tls_note')
    }
    return msg
  }
  // Fallback for older backends / untranslated codes.
  return result.message || ''
}

onMounted(async () => {
  try {
    await monitoringStore.load()
    originalConfig.value = JSON.stringify({
      checkmk: { ...checkmkConfig.value },
      mqtt: { ...mqttConfig.value },
      prometheus: { ...prometheusConfig.value },
      syslog: { ...syslogConfig.value },
      notify: { ...notifyConfig.value }
    })
  } catch (error) {}
})

watch([checkmkConfig, mqttConfig, prometheusConfig, syslogConfig, notifyConfig], () => {
  if (originalConfig.value) {
    hasChanges.value = JSON.stringify({
      checkmk: { ...checkmkConfig.value },
      mqtt: { ...mqttConfig.value },
      prometheus: { ...prometheusConfig.value },
      syslog: { ...syslogConfig.value },
      notify: { ...notifyConfig.value }
    }) !== originalConfig.value
  }
}, { deep: true })

const saveConfig = async () => {
  if (mqttConfig.value.enabled && !mqttConfig.value.server) {
    uiStore.pushToast({ type: 'error', title: t('common.error'), message: t('monitoring.mqtt.serverRequired') })
    return
  }

  if (mqttConfig.value.tlsEnable && mtlsInconsistent.value) {
    uiStore.pushToast({ type: 'error', title: t('common.error'), message: t('monitoring.mqtt.tls.mtlsRequiresBoth') })
    return
  }

  if (mqttConfig.value.tlsEnable) {
    for (const [field, kind] of [['tlsCaCerts', 'ca'], ['tlsCertfile', 'cert'], ['tlsKeyfile', 'key']]) {
      if (!validatePemField(field, kind)) {
        uiStore.pushToast({ type: 'error', title: t('common.error'), message: pemFeedback.value[field]?.msg })
        return
      }
    }
  }

  saving.value = true
  try {
    const { tlsCaCertsSet, tlsCertfileSet, tlsKeyfileSet, commandTokenSet, ...mqttPayload } = mqttConfig.value
    mqttPayload.tlsCaCertsClear  = tlsClearFlags.value.tlsCaCertsClear
    mqttPayload.tlsCertfileClear = tlsClearFlags.value.tlsCertfileClear
    mqttPayload.tlsKeyfileClear  = tlsClearFlags.value.tlsKeyfileClear
    mqttPayload.commandTokenClear = tlsClearFlags.value.commandTokenClear

    // Strip the read-only *Set sentinels from the notify payload — the
    // backend reports them as booleans but does not accept them on write.
    const { webhookSecretSet, telegramTokenSet, smtpPasswordSet, ...notifyPayload } = notifyConfig.value
    notifyPayload.webhookSecretClear  = tlsClearFlags.value.webhookSecretClear
    notifyPayload.telegramTokenClear  = tlsClearFlags.value.telegramTokenClear
    notifyPayload.smtpPasswordClear   = tlsClearFlags.value.smtpPasswordClear

    await monitoringStore.save({
      checkmk: checkmkConfig.value,
      mqtt: mqttPayload,
      prometheus: prometheusConfig.value,
      syslog: syslogConfig.value,
      notify: notifyPayload
    })

    // The backend applies config changes asynchronously (MQTT stop/restart
    // can take several seconds). Calling load() here would read stale data
    // and silently revert the just-saved values — this was the root cause of
    // the "must save twice" bug. Instead, update the *Set sentinel flags and
    // clear the sensitive text fields locally.
    if (mqttPayload.tlsCaCertsClear)   mqttConfig.value.tlsCaCertsSet  = false
    else if (mqttPayload.tlsCaCerts)   mqttConfig.value.tlsCaCertsSet  = true

    if (mqttPayload.tlsCertfileClear)  mqttConfig.value.tlsCertfileSet = false
    else if (mqttPayload.tlsCertfile)  mqttConfig.value.tlsCertfileSet = true

    if (mqttPayload.tlsKeyfileClear)   mqttConfig.value.tlsKeyfileSet  = false
    else if (mqttPayload.tlsKeyfile)   mqttConfig.value.tlsKeyfileSet  = true

    if (mqttPayload.commandTokenClear) mqttConfig.value.commandTokenSet = false
    else if (mqttPayload.commandToken) mqttConfig.value.commandTokenSet = true

    if (notifyPayload.webhookSecretClear) notifyConfig.value.webhookSecretSet = false
    else if (notifyPayload.webhookSecret) notifyConfig.value.webhookSecretSet = true
    if (notifyPayload.telegramTokenClear) notifyConfig.value.telegramTokenSet = false
    else if (notifyPayload.telegramToken) notifyConfig.value.telegramTokenSet = true
    if (notifyPayload.smtpPasswordClear)  notifyConfig.value.smtpPasswordSet = false
    else if (notifyPayload.smtpPassword)  notifyConfig.value.smtpPasswordSet = true

    // Sensitive fields are never echoed back by the backend — clear them
    // from the UI so they do not linger in browser memory.
    mqttConfig.value.tlsCaCerts  = ''
    mqttConfig.value.tlsCertfile = ''
    mqttConfig.value.tlsKeyfile  = ''
    mqttConfig.value.commandToken = ''
    mqttConfig.value.password    = ''
    notifyConfig.value.webhookSecret  = ''
    notifyConfig.value.telegramToken = ''
    notifyConfig.value.smtpPassword   = ''

    tlsClearFlags.value = { tlsCaCertsClear: false, tlsCertfileClear: false, tlsKeyfileClear: false, commandTokenClear: false,
                            webhookSecretClear: false, telegramTokenClear: false, smtpPasswordClear: false }
    pemFeedback.value = { tlsCaCerts: null, tlsCertfile: null, tlsKeyfile: null }

    uiStore.pushToast({ type: 'success', title: t('common.success'), message: t('monitoring.saveSuccess') })
    hasChanges.value = false
    originalConfig.value = JSON.stringify({
      checkmk: { ...checkmkConfig.value },
      mqtt: { ...mqttConfig.value },
      prometheus: { ...prometheusConfig.value },
      syslog: { ...syslogConfig.value },
      notify: { ...notifyConfig.value }
    })
  } catch (error) {
    uiStore.pushToast({ type: 'error', title: t('common.error'), message: error.response?.data?.error || t('monitoring.saveError') })
  } finally {
    saving.value = false
  }
}

const runDiagnostic = async (target) => {
  diagnosticBusy.value[target] = true
  try {
    const result = await monitoringStore.test(target)
    uiStore.pushToast({
      type: result.ok ? 'success' : 'warning',
      title: result.ok ? t('common.success') : t('monitoring.checkFinished'),
      message: formatDiagnosticMessage(result),
      duration: 2800
    })
  } catch (error) {
    uiStore.pushToast({ type: 'error', title: t('common.error'), message: error.response?.data?.error || t('monitoring.diagnosticFailed') })
  } finally {
    diagnosticBusy.value[target] = false
  }
}
</script>

<style scoped>
.monitoring-page {
  padding-bottom: 80px;
  width: min(100%, 1120px);
  margin: 0 auto;
  min-width: 0;
}

.diagnostics-panel {
  margin-bottom: var(--spacing-lg);
}

.monitoring-resource-warning {
  display: flex;
  align-items: flex-start;
  gap: var(--spacing-sm);
  margin-bottom: var(--spacing-lg);
  padding: var(--spacing-md);
  border: 1px solid rgba(245, 158, 11, 0.35);
  border-radius: var(--radius-lg);
  background: rgba(245, 158, 11, 0.12);
  color: var(--warning-dark, #92400e);
}

.monitoring-resource-warning p {
  margin: 0.25rem 0 0;
}

.diagnostics-subtitle {
  margin: 4px 0 0;
  color: var(--color-text-secondary);
  font-size: 0.85rem;
}

.diag-list {
  list-style: none;
  margin: 0;
  padding: 0;
  display: flex;
  flex-direction: column;
}

.diag-row {
  padding: 10px 0;
  border-bottom: 1px solid var(--color-border-light);
}

.diag-row:last-child {
  border-bottom: none;
  padding-bottom: 0;
}

.diag-row:first-child {
  padding-top: 0;
}

.diag-row-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  min-width: 0;
}

.diag-name {
  display: inline-flex;
  align-items: center;
  gap: 10px;
  font-weight: 600;
  font-size: 0.95rem;
  min-width: 0;
  overflow-wrap: anywhere;
}

.diag-dot {
  width: 10px;
  height: 10px;
  border-radius: 999px;
  flex: 0 0 auto;
  background: var(--color-border-strong, #cbd2dd);
  box-shadow: 0 0 0 3px transparent;
  transition: background 0.2s, box-shadow 0.2s;
}

.diag-dot.ok {
  background: var(--color-success);
  box-shadow: 0 0 0 3px var(--color-success-soft);
}

.diag-dot.failed {
  background: var(--color-danger);
  box-shadow: 0 0 0 3px var(--color-danger-soft);
}

.diag-dot.running {
  background: var(--color-warning);
  box-shadow: 0 0 0 3px var(--color-warning-soft);
  animation: diag-pulse 1s ease-in-out infinite;
}

.diag-dot.pending {
  background: var(--color-border-strong, #cbd2dd);
}

@keyframes diag-pulse {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.45; }
}

.diag-test-btn {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  border: 1px solid var(--color-border-light);
  background: rgba(255, 255, 255, 0.68);
  border-radius: var(--radius-sm);
  padding: 6px 12px;
  font-size: 0.82rem;
  font-weight: 600;
  color: var(--color-text);
  white-space: normal;
  flex: 0 0 auto;
  transition: background 0.2s, border-color 0.2s, transform 0.2s;
  max-width: 100%;
}

.diag-test-btn:hover:not(:disabled) {
  background: var(--color-primary-soft);
  border-color: rgba(242, 106, 61, 0.32);
  color: var(--color-primary-strong);
  transform: translateY(-1px);
}

.diag-test-btn .app-icon {
  width: 14px;
  height: 14px;
}

.diag-message {
  margin: 6px 0 0 20px;
  color: var(--color-text-secondary);
  font-size: 0.82rem;
  line-height: 1.4;
  overflow-wrap: anywhere;
}

.diag-row.failed .diag-message {
  color: var(--color-danger);
}

.diag-row.ok .diag-message {
  color: var(--color-success);
}

.settings-card {
  border-radius: var(--radius-xl);
  margin-bottom: var(--spacing-lg);
  overflow: hidden;
  transition: transform 0.2s, box-shadow 0.2s;
  min-width: 0;
}

.card-header {
  padding: var(--spacing-lg);
  background: transparent;
  border: none;
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: var(--spacing-md);
  min-width: 0;
}

.header-content {
  display: flex;
  align-items: center;
  gap: var(--spacing-md);
  min-width: 0;
}

.header-icon {
  width: 40px;
  height: 40px;
  border-radius: 10px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 1.25rem;
}

.card-header h3 {
  margin: 0;
  font-size: 1.125rem;
  font-weight: 600;
  overflow-wrap: anywhere;
}

.card-body {
  padding: 0 var(--spacing-lg) var(--spacing-lg);
  border-top: 1px solid var(--color-border-light);
  margin-top: var(--spacing-xs);
  padding-top: var(--spacing-lg);
  min-width: 0;
}

.floating-footer {
  position: fixed;
  bottom: 0;
  left: 0;
  right: 0;
  padding: var(--spacing-md);
  background: linear-gradient(to top, var(--color-surface) 80%, transparent);
  display: flex;
  justify-content: center;
  z-index: 100;
  pointer-events: none;
}

.footer-container {
  width: 100%;
  max-width: 600px;
  pointer-events: auto;
  min-width: 0;
}

.save-btn {
  width: 100%;
  box-shadow: var(--shadow-lg);
  border-radius: var(--radius-full);
  padding: 1rem;
  font-size: 1.1rem;
}

.expand-enter-active,
.expand-leave-active {
  transition: all 0.3s ease;
  max-height: 1000px;
  opacity: 1;
}

.expand-enter-from,
.expand-leave-to {
  max-height: 0;
  opacity: 0;
  margin: 0;
  padding-top: 0;
  padding-bottom: 0;
  overflow: hidden;
}

.slide-up-enter-active,
.slide-up-leave-active {
  transition: transform 0.3s cubic-bezier(0.16, 1, 0.3, 1);
}

.slide-up-enter-from,
.slide-up-leave-to {
  transform: translateY(100%);
}

.bg-danger-light { background-color: var(--color-danger-light); }
.bg-success-light { background-color: var(--color-success-light); }
.bg-info-light { background-color: var(--color-info-light, var(--color-info-soft, #e7f1ff)); }
.text-danger { color: var(--color-danger); }
.text-success { color: var(--color-success); }
.text-info { color: var(--color-info, #0d6efd); }

.form-text {
  font-size: 0.8125rem;
  color: var(--color-text-secondary);
  margin-top: 4px;
}

.tls-section {
  margin-top: 16px;
}

.command-section {
  margin-top: 16px;
}

.tls-section h4,
.command-section h4 {
  font-size: 1rem;
  margin: 0 0 4px;
  overflow-wrap: anywhere;
}

.alert-warning-soft {
  background: var(--color-warning-light);
  color: var(--color-warning);
  border: 1px solid color-mix(in srgb, var(--color-warning) 30%, transparent);
  border-radius: var(--radius-lg);
  padding: 10px 14px;
  font-size: 0.88rem;
  overflow-wrap: anywhere;
}

.pem-label-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
  margin-bottom: 4px;
  min-width: 0;
}

.btn-load-file {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  padding: 5px 12px;
  border-radius: var(--radius-full);
  border: 1px solid var(--color-border-light);
  background: rgba(255, 255, 255, 0.68);
  font-size: 0.78rem;
  font-weight: 600;
  color: var(--color-text-secondary);
  max-width: 100%;
  white-space: normal;
}

.btn-link-danger {
  color: var(--color-danger);
  font-size: 0.82rem;
  font-weight: 600;
  text-decoration: none;
  padding: 0;
  background: none;
  border: none;
  max-width: 100%;
  overflow-wrap: anywhere;
}

.pem-textarea {
  font-family: "SFMono-Regular", Consolas, "Liberation Mono", monospace;
  font-size: 0.78rem;
  min-height: 110px;
}

.pem-feedback {
  margin-top: 6px;
  font-size: 0.8rem;
  font-weight: 600;
  overflow-wrap: anywhere;
}

.pem-feedback.is-error {
  color: var(--color-danger);
}

.pem-feedback.is-ok {
  color: var(--color-success);
}

.cert-status-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
  margin-top: 6px;
  min-width: 0;
}

.cert-present-hint {
  color: var(--color-success);
  font-size: 0.82rem;
  font-weight: 600;
  min-width: 0;
  overflow-wrap: anywhere;
}

/* Tablet range and the experimental sidebar ("fullscreen") layout.
   In the sidebar layout the available content width is the viewport minus
   the 384px left padding, so 2- and 3-column Bootstrap grids (col-md-*)
   that look fine on a full-width desktop get cramped between ~768px and
   ~1100px. Stack them earlier in that band so the inputs stay readable. */
@media (max-width: 1100px) {
  .monitoring-page .row > [class*="col-md-"] {
    flex: 0 0 100%;
    max-width: 100%;
  }
}

@media (max-width: 768px) {
  .monitoring-page {
    padding-bottom: 100px;
  }

  .settings-card {
    border-radius: var(--radius-lg);
    margin-bottom: var(--spacing-md);
  }

  .card-header,
  .tls-section > .d-flex,
  .command-section > .d-flex {
    flex-direction: column;
    align-items: stretch;
  }

  .card-header .form-check,
  .tls-section .form-check,
  .command-section .form-check {
    align-self: flex-start;
  }

  .card-header {
    padding: var(--spacing-md);
  }

  .header-icon {
    width: 36px;
    height: 36px;
    border-radius: 8px;
    font-size: 1rem;
  }

  .header-content {
    align-items: flex-start;
  }

  .card-header h3 {
    font-size: 1rem;
    overflow-wrap: anywhere;
  }

  .card-body {
    padding: 0 var(--spacing-md) var(--spacing-md);
    padding-top: var(--spacing-md);
  }

  .floating-footer {
    padding: var(--spacing-sm) var(--spacing-md);
  }

  .save-btn {
    font-size: 1rem;
    padding: 0.875rem;
  }

  .cert-status-row,
  .pem-label-row {
    align-items: flex-start;
    flex-direction: column;
  }
}

@media (max-width: 480px) {
  .monitoring-page {
    padding-left: 0;
    padding-right: 0;
  }

  .diag-row-head {
    flex-wrap: wrap;
  }

  .diag-test-btn {
    width: 100%;
    justify-content: center;
  }

  .diag-message,
  .form-text,
  .alert-warning-soft {
    overflow-wrap: anywhere;
  }
}
</style>
