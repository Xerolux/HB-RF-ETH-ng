<template>
  <BCard
    :header="t('settings.title')"
    header-tag="h6"
    header-bg-variant="secondary"
    header-text-variant="white"
    class="mb-3"
  >
    <BForm @submit.stop.prevent>
      <BFormGroup :label="t('settings.changePassword')" label-cols-sm="4">
        <BInputGroup>
          <BFormInput
            :type="adminPasswordVisible ? 'text' : 'password'"
            v-model="adminPassword"
            :state="v$.adminPassword.$error ? false : null"
            placeholder="••••••"
          />
          <BInputGroupAppend>
            <BButton
              @click="adminPasswordVisible = !adminPasswordVisible"
              variant="outline-secondary"
              :aria-label="adminPasswordVisible ? 'Hide password' : 'Show password'"
            >
              <svg v-if="adminPasswordVisible" xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="currentColor" class="bi bi-eye-slash" viewBox="0 0 16 16">
                <path d="M13.359 11.238C15.06 9.72 16 8 16 8s-3-5.5-8-5.5a7.028 7.028 0 0 0-2.79.588l.77.771A5.944 5.944 0 0 1 8 3.5c2.12 0 3.879 1.168 5.168 2.457A13.134 13.134 0 0 1 14.828 8c-.058.087-.122.183-.195.288-.335.48-.83 1.12-1.465 1.755-.165.165-.337.328-.517.486z"/>
                <path d="M11.297 5.316 5.066 11.547c-.862-.2-1.635-.726-2.195-1.474-.294-.396-.569-.83-.798-1.258a13.3 13.3 0 0 1-.954-3.14l.872-.486c.205.908.537 1.728.954 2.196.48.538 1.106.945 1.764 1.115l.623-.623A4.5 4.5 0 0 1 4.5 8a4.502 4.502 0 0 1 4.093-6.248l.965.965a5.503 5.503 0 0 0-.66.068c-1.996-.282-3.87 1.053-4.186 3.018l.865.865A3.49 3.49 0 0 0 11 8c0 .245-.045.478-.127.697l2.128 2.128a11.59 11.59 0 0 0 1.25-1.583l-.872-.486c-.238.41-.502.795-.788 1.139l-1.294-1.294z"/>
                <path d="M0 1.354 1.354 0l14 14-1.354 1.354-14-14zM8 5.5a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5z"/>
              </svg>
              <svg v-else xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="currentColor" class="bi bi-eye" viewBox="0 0 16 16">
                <path d="M16 8s-3-5.5-8-5.5S0 8 0 8s3 5.5 8 5.5S16 8 16 8zM1.173 8a13.133 13.133 0 0 1 1.66-2.043C4.12 4.668 5.88 3.5 8 3.5c2.12 0 3.879 1.168 5.168 2.457A13.133 13.133 0 0 1 14.828 8c-.314.435-.8.995-1.465 1.755C11.879 11.332 10.119 12.5 8 12.5c-2.12 0-3.879-1.168-5.168-2.457A13.134 13.134 0 0 1 1.172 8z"/>
                <path d="M8 5.5a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5zM4.5 8a3.5 3.5 0 1 1 7 0 3.5 3.5 0 0 1-7 0z"/>
              </svg>
            </BButton>
          </BInputGroupAppend>
        </BInputGroup>
        <BFormInvalidFeedback v-if="v$.adminPassword.minLength.$invalid || v$.adminPassword.password_validator.$invalid">
          {{ v$.adminPassword.password_validator.$message }}
        </BFormInvalidFeedback>
      </BFormGroup>
      <BFormGroup :label="t('settings.repeatPassword')" label-cols-sm="4">
        <BInputGroup>
          <BFormInput
            :type="adminPasswordRepeatVisible ? 'text' : 'password'"
            v-model="adminPasswordRepeat"
            :state="v$.adminPasswordRepeat.$error ? false : null"
            placeholder="••••••"
          />
          <BInputGroupAppend>
            <BButton
              @click="adminPasswordRepeatVisible = !adminPasswordRepeatVisible"
              variant="outline-secondary"
              :aria-label="adminPasswordRepeatVisible ? 'Hide password' : 'Show password'"
            >
              <svg v-if="adminPasswordRepeatVisible" xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="currentColor" class="bi bi-eye-slash" viewBox="0 0 16 16">
                <path d="M13.359 11.238C15.06 9.72 16 8 16 8s-3-5.5-8-5.5a7.028 7.028 0 0 0-2.79.588l.77.771A5.944 5.944 0 0 1 8 3.5c2.12 0 3.879 1.168 5.168 2.457A13.134 13.134 0 0 1 14.828 8c-.058.087-.122.183-.195.288-.335.48-.83 1.12-1.465 1.755-.165.165-.337.328-.517.486z"/>
                <path d="M11.297 5.316 5.066 11.547c-.862-.2-1.635-.726-2.195-1.474-.294-.396-.569-.83-.798-1.258a13.3 13.3 0 0 1-.954-3.14l.872-.486c.205.908.537 1.728.954 2.196.48.538 1.106.945 1.764 1.115l.623-.623A4.5 4.5 0 0 1 4.5 8a4.502 4.502 0 0 1 4.093-6.248l.965.965a5.503 5.503 0 0 0-.66.068c-1.996-.282-3.87 1.053-4.186 3.018l.865.865A3.49 3.49 0 0 0 11 8c0 .245-.045.478-.127.697l2.128 2.128a11.59 11.59 0 0 0 1.25-1.583l-.872-.486c-.238.41-.502.795-.788 1.139l-1.294-1.294z"/>
                <path d="M0 1.354 1.354 0l14 14-1.354 1.354-14-14zM8 5.5a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5z"/>
              </svg>
              <svg v-else xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="currentColor" class="bi bi-eye" viewBox="0 0 16 16">
                <path d="M16 8s-3-5.5-8-5.5S0 8 0 8s3 5.5 8 5.5S16 8 16 8zM1.173 8a13.133 13.133 0 0 1 1.66-2.043C4.12 4.668 5.88 3.5 8 3.5c2.12 0 3.879 1.168 5.168 2.457A13.133 13.133 0 0 1 14.828 8c-.314.435-.8.995-1.465 1.755C11.879 11.332 10.119 12.5 8 12.5c-2.12 0-3.879-1.168-5.168-2.457A13.134 13.134 0 0 1 1.172 8z"/>
                <path d="M8 5.5a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5zM4.5 8a3.5 3.5 0 1 1 7 0 3.5 3.5 0 0 1-7 0z"/>
              </svg>
            </BButton>
          </BInputGroupAppend>
        </BInputGroup>
        <BFormInvalidFeedback v-if="v$.adminPasswordRepeat.sameAsPassword.$invalid">
          {{ v$.adminPasswordRepeat.sameAsPassword.$message }}
        </BFormInvalidFeedback>
      </BFormGroup>
      <hr />
      <h6 class="text-secondary">{{ t('settings.networkSettings') }}</h6>
      <BFormGroup :label="t('settings.hostname')" label-cols-sm="4">
        <BFormInput
          type="text"
          v-model="hostname"
          trim
          :state="v$.hostname.$error ? false : null"
        />
        <BFormInvalidFeedback v-if="v$.hostname.$error">
            <span v-if="v$.hostname.required.$invalid">{{ v$.hostname.required.$message }}</span>
            <span v-else-if="v$.hostname.hostname_validator.$invalid">{{ v$.hostname.hostname_validator.$message }}</span>
            <span v-else-if="v$.hostname.maxLength.$invalid">{{ v$.hostname.maxLength.$message }}</span>
        </BFormInvalidFeedback>
      </BFormGroup>
      <BFormGroup :label="t('settings.dhcp')" label-cols-sm="4">
        <BFormRadioGroup buttons v-model="useDHCP" required>
          <BFormRadio :value="true">{{ t('common.enabled') }}</BFormRadio>
          <BFormRadio :value="false">{{ t('common.disabled') }}</BFormRadio>
        </BFormRadioGroup>
      </BFormGroup>
      <BFormGroup :label="t('settings.ipAddress')" label-cols-sm="4" v-if="!useDHCP">
        <BFormInput
          type="text"
          v-model="localIP"
          trim
          :state="v$.localIP.$error ? false : null"
        />
        <BFormInvalidFeedback v-if="v$.localIP.$error">
             <span v-if="v$.localIP.required.$invalid">{{ v$.localIP.required.$message }}</span>
             <span v-else>{{ v$.localIP.ipAddress.$message }}</span>
        </BFormInvalidFeedback>
      </BFormGroup>
      <BFormGroup :label="t('settings.netmask')" label-cols-sm="4" v-if="!useDHCP">
        <BFormInput
          type="text"
          v-model="netmask"
          trim
          :state="v$.netmask.$error ? false : null"
        />
        <BFormInvalidFeedback v-if="v$.netmask.$error">
             <span v-if="v$.netmask.required.$invalid">{{ v$.netmask.required.$message }}</span>
             <span v-else>{{ v$.netmask.ipAddress.$message }}</span>
        </BFormInvalidFeedback>
      </BFormGroup>
      <BFormGroup :label="t('settings.gateway')" label-cols-sm="4" v-if="!useDHCP">
        <BFormInput
          type="text"
          v-model="gateway"
          trim
          :state="v$.gateway.$error ? false : null"
        />
        <BFormInvalidFeedback v-if="v$.gateway.$error">
             <span v-if="v$.gateway.required.$invalid">{{ v$.gateway.required.$message }}</span>
             <span v-else>{{ v$.gateway.ipAddress.$message }}</span>
        </BFormInvalidFeedback>
      </BFormGroup>
      <BFormGroup :label="t('settings.dns1')" label-cols-sm="4" v-if="!useDHCP">
        <BFormInput
          type="text"
          v-model="dns1"
          trim
          :state="v$.dns1.$error ? false : null"
        />
        <BFormInvalidFeedback v-if="v$.dns1.$error">
             <span v-if="v$.dns1.required.$invalid">{{ v$.dns1.required.$message }}</span>
             <span v-else>{{ v$.dns1.ipAddress.$message }}</span>
        </BFormInvalidFeedback>
      </BFormGroup>
      <BFormGroup :label="t('settings.dns2')" label-cols-sm="4" v-if="!useDHCP">
        <BFormInput
          type="text"
          v-model="dns2"
          trim
          :state="v$.dns2.$error ? false : null"
        />
        <BFormInvalidFeedback v-if="v$.dns2.$error">
             {{ v$.dns2.ipAddress.$message }}
        </BFormInvalidFeedback>
      </BFormGroup>
      <hr />
      <h6 class="text-secondary">{{ t('settings.ipv6Settings') }}</h6>
      <BFormGroup :label="t('settings.enableIPv6')" label-cols-sm="4">
        <BFormRadioGroup buttons v-model="enableIPv6" required>
            <BFormRadio :value="true">{{ t('common.enabled') }}</BFormRadio>
            <BFormRadio :value="false">{{ t('common.disabled') }}</BFormRadio>
        </BFormRadioGroup>
      </BFormGroup>
      <template v-if="enableIPv6">
        <BFormGroup :label="t('settings.ipv6Mode')" label-cols-sm="4">
          <BFormRadioGroup buttons v-model="ipv6Mode" required>
            <BFormRadio value="auto">{{ t('settings.ipv6Auto') }}</BFormRadio>
            <BFormRadio value="static">{{ t('settings.ipv6Static') }}</BFormRadio>
          </BFormRadioGroup>
        </BFormGroup>
        <template v-if="ipv6Mode === 'static'">
          <BFormGroup :label="t('settings.ipv6Address')" label-cols-sm="4">
            <BFormInput
              type="text"
              v-model="ipv6Address"
              trim
              placeholder="2001:db8::1"
              :state="v$.ipv6Address.$error ? false : null"
            />
            <BFormInvalidFeedback v-if="v$.ipv6Address.$error">
                <span v-if="v$.ipv6Address.required.$invalid">{{ v$.ipv6Address.required.$message }}</span>
                <span v-else>{{ v$.ipv6Address.ipv6_validator.$message }}</span>
            </BFormInvalidFeedback>
          </BFormGroup>
          <BFormGroup :label="t('settings.ipv6PrefixLength')" label-cols-sm="4">
            <BFormInput
              type="number"
              v-model.number="ipv6PrefixLength"
              min="1"
              max="128"
              placeholder="64"
              :state="v$.ipv6PrefixLength.$error ? false : null"
            />
            <BFormInvalidFeedback v-if="v$.ipv6PrefixLength.$error">
                <span v-if="v$.ipv6PrefixLength.required.$invalid">{{ v$.ipv6PrefixLength.required.$message }}</span>
                <span v-else-if="v$.ipv6PrefixLength.numeric.$invalid">{{ v$.ipv6PrefixLength.numeric.$message }}</span>
                <span v-else-if="v$.ipv6PrefixLength.minValue.$invalid">{{ v$.ipv6PrefixLength.minValue.$message }}</span>
                <span v-else-if="v$.ipv6PrefixLength.maxValue.$invalid">{{ v$.ipv6PrefixLength.maxValue.$message }}</span>
            </BFormInvalidFeedback>
          </BFormGroup>
          <BFormGroup :label="t('settings.ipv6Gateway')" label-cols-sm="4">
            <BFormInput
              type="text"
              v-model="ipv6Gateway"
              trim
              placeholder="2001:db8::1"
              :state="v$.ipv6Gateway.$error ? false : null"
            />
            <BFormInvalidFeedback v-if="v$.ipv6Gateway.$error">
                <span v-if="v$.ipv6Gateway.required.$invalid">{{ v$.ipv6Gateway.required.$message }}</span>
                <span v-else>{{ v$.ipv6Gateway.ipv6_validator.$message }}</span>
            </BFormInvalidFeedback>
          </BFormGroup>
          <BFormGroup :label="t('settings.ipv6Dns1')" label-cols-sm="4">
            <BFormInput
              type="text"
              v-model="ipv6Dns1"
              trim
              placeholder="2001:4860:4860::8888"
              :state="v$.ipv6Dns1.$error ? false : null"
            />
            <BFormInvalidFeedback v-if="v$.ipv6Dns1.$error">
                <span v-if="v$.ipv6Dns1.required.$invalid">{{ v$.ipv6Dns1.required.$message }}</span>
                <span v-else>{{ v$.ipv6Dns1.ipv6_validator.$message }}</span>
            </BFormInvalidFeedback>
          </BFormGroup>
          <BFormGroup :label="t('settings.ipv6Dns2')" label-cols-sm="4">
            <BFormInput
              type="text"
              v-model="ipv6Dns2"
              trim
              placeholder="2001:4860:4860::8844"
              :state="v$.ipv6Dns2.$error ? false : null"
            />
            <BFormInvalidFeedback v-if="v$.ipv6Dns2.$error">
                {{ v$.ipv6Dns2.ipv6_validator.$message }}
            </BFormInvalidFeedback>
          </BFormGroup>
        </template>
      </template>
      <hr />
      <h6 class="text-secondary">{{ t('settings.timeSettings') }}</h6>
      <BFormGroup :label="t('settings.timesource')" label-cols-sm="4">
        <BFormRadioGroup buttons v-model="timesource" required>
          <BFormRadio :value="0">{{ t('settings.ntp') }}</BFormRadio>
          <BFormRadio :value="1">{{ t('settings.dcf') }}</BFormRadio>
          <BFormRadio :value="2">{{ t('settings.gps') }}</BFormRadio>
        </BFormRadioGroup>
      </BFormGroup>
      <BFormGroup :label="t('settings.ntpServer')" label-cols-sm="4" v-if="isNtpActivated">
        <BFormInput
          type="text"
          v-model="ntpServer"
          trim
          :state="v$.ntpServer.$error ? false : null"
        />
        <BFormInvalidFeedback v-if="v$.ntpServer.$error">
            <span v-if="v$.ntpServer.required.$invalid">{{ v$.ntpServer.required.$message }}</span>
            <span v-else-if="v$.ntpServer.domainname_validator.$invalid">{{ v$.ntpServer.domainname_validator.$message }}</span>
        </BFormInvalidFeedback>
      </BFormGroup>
      <BFormGroup :label="t('settings.dcfOffset')" label-cols-sm="4" v-if="isDcfActivated">
        <BInputGroup append="µs">
          <BFormInput
            type="number"
            v-model.number="dcfOffset"
            min="0"
            :state="v$.dcfOffset.$error ? false : null"
          />
        </BInputGroup>
        <BFormInvalidFeedback v-if="v$.dcfOffset.$error">
            <span v-if="v$.dcfOffset.required.$invalid">{{ v$.dcfOffset.required.$message }}</span>
            <span v-else-if="v$.dcfOffset.numeric.$invalid">{{ v$.dcfOffset.numeric.$message }}</span>
        </BFormInvalidFeedback>
      </BFormGroup>
      <BFormGroup :label="t('settings.gpsBaudrate')" label-cols-sm="4" v-if="isGpsActivated">
        <BFormSelect v-model.number="gpsBaudrate">
          <BFormSelectOption :value="4800">4800</BFormSelectOption>
          <BFormSelectOption :value="9600">9600</BFormSelectOption>
          <BFormSelectOption :value="19200">19200</BFormSelectOption>
          <BFormSelectOption :value="38400">38400</BFormSelectOption>
          <BFormSelectOption :value="57600">57600</BFormSelectOption>
          <BFormSelectOption :value="115200">115200</BFormSelectOption>
        </BFormSelect>
      </BFormGroup>
      <hr />
      <h6 class="text-secondary">{{ t('settings.systemSettings') }}</h6>
      <BFormGroup :label="t('settings.ledBrightness')" label-cols-sm="4">
        <BInputGroup append="%">
          <BFormSelect v-model.number="ledBrightness">
            <BFormSelectOption :value="0">0</BFormSelectOption>
            <BFormSelectOption :value="5">5</BFormSelectOption>
            <BFormSelectOption :value="10">10</BFormSelectOption>
            <BFormSelectOption :value="25">25</BFormSelectOption>
            <BFormSelectOption :value="50">50</BFormSelectOption>
            <BFormSelectOption :value="75">75</BFormSelectOption>
            <BFormSelectOption :value="100">100</BFormSelectOption>
          </BFormSelect>
        </BInputGroup>
      </BFormGroup>
      <BFormGroup :label="t('settings.checkUpdates')" label-cols-sm="4">
        <BFormRadioGroup buttons v-model="checkUpdates" required>
            <BFormRadio :value="true">{{ t('common.enabled') }}</BFormRadio>
            <BFormRadio :value="false">{{ t('common.disabled') }}</BFormRadio>
        </BFormRadioGroup>
      </BFormGroup>
      <BFormGroup :label="t('settings.allowPrerelease')" label-cols-sm="4">
        <BFormRadioGroup buttons v-model="allowPrerelease" required>
            <BFormRadio :value="true">{{ t('common.enabled') }}</BFormRadio>
            <BFormRadio :value="false">{{ t('common.disabled') }}</BFormRadio>
        </BFormRadioGroup>
      </BFormGroup>
      <hr />
      <h6 class="text-secondary">{{ t('settings.hmlgwSettings') }}</h6>
      <BFormGroup :label="t('settings.enableHmlgw')" label-cols-sm="4">
        <BFormRadioGroup buttons v-model="hmlgwEnabled" required>
            <BFormRadio :value="true">{{ t('common.enabled') }}</BFormRadio>
            <BFormRadio :value="false">{{ t('common.disabled') }}</BFormRadio>
        </BFormRadioGroup>
      </BFormGroup>
      <template v-if="hmlgwEnabled">
        <BFormGroup :label="t('settings.hmlgwPort')" label-cols-sm="4">
          <BFormInput
            type="number"
            v-model.number="hmlgwPort"
            min="1"
            max="65535"
            :state="v$.hmlgwPort.$error ? false : null"
          />
          <BFormInvalidFeedback v-if="v$.hmlgwPort.$error">
                <span v-if="v$.hmlgwPort.required.$invalid">{{ v$.hmlgwPort.required.$message }}</span>
                <span v-else-if="v$.hmlgwPort.numeric.$invalid">{{ v$.hmlgwPort.numeric.$message }}</span>
                <span v-else-if="v$.hmlgwPort.minValue.$invalid">{{ v$.hmlgwPort.minValue.$message }}</span>
                <span v-else-if="v$.hmlgwPort.maxValue.$invalid">{{ v$.hmlgwPort.maxValue.$message }}</span>
          </BFormInvalidFeedback>
        </BFormGroup>
        <BFormGroup :label="t('settings.hmlgwKeepAlivePort')" label-cols-sm="4">
          <BFormInput
            type="number"
            v-model.number="hmlgwKeepAlivePort"
            min="1"
            max="65535"
            :state="v$.hmlgwKeepAlivePort.$error ? false : null"
          />
          <BFormInvalidFeedback v-if="v$.hmlgwKeepAlivePort.$error">
                <span v-if="v$.hmlgwKeepAlivePort.required.$invalid">{{ v$.hmlgwKeepAlivePort.required.$message }}</span>
                <span v-else-if="v$.hmlgwKeepAlivePort.numeric.$invalid">{{ v$.hmlgwKeepAlivePort.numeric.$message }}</span>
                <span v-else-if="v$.hmlgwKeepAlivePort.minValue.$invalid">{{ v$.hmlgwKeepAlivePort.minValue.$message }}</span>
                <span v-else-if="v$.hmlgwKeepAlivePort.maxValue.$invalid">{{ v$.hmlgwKeepAlivePort.maxValue.$message }}</span>
          </BFormInvalidFeedback>
        </BFormGroup>
      </template>
      <hr />
      <h6 class="text-secondary">{{ t('settings.analyzerSettings') }}</h6>
      <BFormGroup :label="t('settings.enableAnalyzer')" label-cols-sm="4">
        <BFormRadioGroup buttons v-model="analyzerEnabled" required>
            <BFormRadio :value="true">{{ t('common.enabled') }}</BFormRadio>
            <BFormRadio :value="false">{{ t('common.disabled') }}</BFormRadio>
        </BFormRadioGroup>
      </BFormGroup>

      <hr />
      <h6 class="text-secondary">{{ t('settings.dtlsSettings') }}</h6>
      <BFormGroup :label="t('settings.dtlsMode')" label-cols-sm="4">
        <BFormRadioGroup buttons v-model="dtlsMode" required>
          <BFormRadio :value="0">{{ t('settings.dtlsModeDisabled') }}</BFormRadio>
          <BFormRadio :value="1">{{ t('settings.dtlsModePsk') }}</BFormRadio>
          <BFormRadio :value="2">{{ t('settings.dtlsModeCertificate') }}</BFormRadio>
        </BFormRadioGroup>
      </BFormGroup>
      <template v-if="dtlsMode > 0">
        <BFormGroup :label="t('settings.dtlsCipherSuite')" label-cols-sm="4">
          <BFormSelect v-model="dtlsCipherSuite">
            <BFormSelectOption :value="0">{{ t('settings.dtlsCipherAes128') }}</BFormSelectOption>
            <BFormSelectOption :value="1">{{ t('settings.dtlsCipherAes256') }}</BFormSelectOption>
            <BFormSelectOption :value="2">{{ t('settings.dtlsCipherChacha') }}</BFormSelectOption>
          </BFormSelect>
        </BFormGroup>
        <BFormGroup :label="t('settings.dtlsSessionResumption')" label-cols-sm="4">
          <BFormRadioGroup buttons v-model="dtlsSessionResumption" required>
            <BFormRadio :value="true">{{ t('common.enabled') }}</BFormRadio>
            <BFormRadio :value="false">{{ t('common.disabled') }}</BFormRadio>
          </BFormRadioGroup>
        </BFormGroup>
        <template v-if="dtlsMode === 2">
          <BFormGroup :label="t('settings.dtlsRequireClientCert')" label-cols-sm="4">
            <BFormRadioGroup buttons v-model="dtlsRequireClientCert" required>
                <BFormRadio :value="true">{{ t('common.enabled') }}</BFormRadio>
                <BFormRadio :value="false">{{ t('common.disabled') }}</BFormRadio>
            </BFormRadioGroup>
          </BFormGroup>
        </template>
        <BAlert variant="info" :model-value="true" class="mt-2">
            {{ t('settings.dtlsInfo') }}
        </BAlert>
      </template>

      <BAlert
        variant="success"
        :model-value="showSuccess"
        dismissible
        fade
        @update:model-value="showSuccess = null"
      >{{ t("settings.saveSuccess") }}</BAlert>
      <BAlert
        variant="danger"
        :model-value="showError"
        dismissible
        fade
        @update:model-value="showError = null"
      >{{ t("settings.saveError") }}</BAlert>

      <BFormGroup label-cols-sm="9">
        <BButton
          variant="primary"
          block
          @click="saveSettingsClick"
          :disabled="v$.$error || loading"
        >
          <BSpinner small v-if="loading" class="me-2" />
          {{ t('common.save') }}
        </BButton>
      </BFormGroup>
    </BForm>
  </BCard>

  <BCard
    :header="t('settings.backupRestore')"
    header-tag="h6"
    header-bg-variant="info"
    header-text-variant="white"
    class="mb-3"
  >
    <BForm @submit.stop.prevent>
        <p>{{ t('settings.backupInfo') }}</p>
        <BButton variant="outline-primary" class="mb-3" @click="downloadBackup">{{ t('settings.downloadBackup') }}</BButton>

        <hr/>
        <p>{{ t('settings.restoreInfo') }}</p>
        <BFormFile
          v-model="restoreFile"
          accept=".json"
          :placeholder="t('settings.noFileChosen')"
          :browse-text="t('settings.browse')"
          class="mb-2"
        />
        <BButton
            variant="warning"
            :disabled="!restoreFile"
            @click="restoreSettings"
        >{{ t('settings.restore') }}</BButton>
    </BForm>
  </BCard>

  <BCard
    :header="t('settings.systemMaintenance')"
    header-tag="h6"
    header-bg-variant="danger"
    header-text-variant="white"
    class="mb-3"
  >
    <BButton variant="warning" block class="me-2" @click="rebootClick">{{ t('settings.reboot') }}</BButton>
    <BButton variant="danger" block @click="factoryResetClick">{{ t('settings.factoryReset') }}</BButton>
  </BCard>
</template>

<script setup>
import axios from 'axios'
import { ref, computed, onMounted, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useVuelidate } from '@vuelidate/core'
import {
  required,
  minLength,
  maxLength,
  numeric,
  ipAddress,
  sameAs,
  helpers,
  requiredIf,
  requiredUnless
} from '@vuelidate/validators'
import { useSettingsStore } from './stores.js'

const { t } = useI18n()

const hostname_validator = helpers.regex(/^[a-zA-Z0-9_-]{1,63}$/)
const domainname_validator = helpers.regex(/^([a-zA-Z0-9_-]{1,63}\.)*[a-zA-Z0-9_-]{1,63}$/)
const ipv6_validator = helpers.regex(/^(([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,7}:|([0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|[0-9a-fA-F]{1,4}:((:[0-9a-fA-F]{1,4}){1,6})|:((:[0-9a-fA-F]{1,4}){1,7}|:))$/)

const settingsStore = useSettingsStore()

// Local form state
const restoreFile = ref(null)
const adminPassword = ref('')
const adminPasswordRepeat = ref('')
const adminPasswordVisible = ref(false)
const adminPasswordRepeatVisible = ref(false)
const hostname = ref('')
const useDHCP = ref(true)
const localIP = ref('')
const netmask = ref('')
const gateway = ref('')
const dns1 = ref('')
const dns2 = ref('')

// IPv6 settings
const enableIPv6 = ref(false)
const ipv6Mode = ref('auto')
const ipv6Address = ref('')
const ipv6PrefixLength = ref(64)
const ipv6Gateway = ref('')
const ipv6Dns1 = ref('')
const ipv6Dns2 = ref('')

const timesource = ref(0)
const dcfOffset = ref(0)
const gpsBaudrate = ref(9600)
const ntpServer = ref('')
const ledBrightness = ref(100)
const checkUpdates = ref(true)
const allowPrerelease = ref(false)

const hmlgwEnabled = ref(false)
const hmlgwPort = ref(2000)
const hmlgwKeepAlivePort = ref(2001)

const analyzerEnabled = ref(false)

// DTLS
const dtlsMode = ref(0)
const dtlsCipherSuite = ref(1)
const dtlsRequireClientCert = ref(false)
const dtlsSessionResumption = ref(true)

const showSuccess = ref(null)
const showError = ref(null)
const loading = ref(false)

// Computed flags
const isNtpActivated = computed(() => timesource.value === 0)
const isDcfActivated = computed(() => timesource.value === 1)
const isGpsActivated = computed(() => timesource.value === 2)
const isIPv6Static = computed(() => enableIPv6.value && ipv6Mode.value === 'static')

const password_validator = helpers.regex(/^(?=.*[A-Za-z])(?=.*\d).{6,}$/)

// Validation rules
const rules = computed(() => ({
  adminPassword: {
    minLength: minLength(6),
    maxLength: maxLength(32),
    password_validator: helpers.withMessage(t('validation.passwordRequirements'), password_validator)
  },
  adminPasswordRepeat: {
    sameAsPassword: helpers.withMessage(t('validation.sameAsPassword'), sameAs(adminPassword.value))
  },
  hostname: {
    required: helpers.withMessage(t('validation.required'), required),
    hostname_validator: helpers.withMessage(t('validation.invalidHostname'), hostname_validator),
    maxLength: maxLength(32)
  },
  localIP: {
    required: helpers.withMessage(t('validation.required'), requiredUnless(useDHCP.value)),
    ipAddress: helpers.withMessage(t('validation.invalidIP'), ipAddress)
  },
  netmask: {
    required: helpers.withMessage(t('validation.required'), requiredUnless(useDHCP.value)),
    ipAddress: helpers.withMessage(t('validation.invalidIP'), ipAddress)
  },
  gateway: {
    required: helpers.withMessage(t('validation.required'), requiredUnless(useDHCP.value)),
    ipAddress: helpers.withMessage(t('validation.invalidIP'), ipAddress)
  },
  dns1: {
    required: helpers.withMessage(t('validation.required'), requiredUnless(useDHCP.value)),
    ipAddress: helpers.withMessage(t('validation.invalidIP'), ipAddress)
  },
  dns2: {
    ipAddress: helpers.withMessage(t('validation.invalidIP'), ipAddress)
  },
  ipv6Address: {
    required: helpers.withMessage(t('validation.required'), requiredIf(isIPv6Static.value)),
    ipv6_validator: helpers.withMessage(t('validation.invalidIPv6'), ipv6_validator)
  },
  ipv6PrefixLength: {
    required: helpers.withMessage(t('validation.required'), requiredIf(isIPv6Static.value)),
    numeric: helpers.withMessage(t('validation.numeric'), numeric),
    minValue: helpers.withMessage(t('validation.minValue', {min: 1}), val => val >= 1),
    maxValue: helpers.withMessage(t('validation.maxValue', {max: 128}), val => val <= 128)
  },
  ipv6Gateway: {
    required: helpers.withMessage(t('validation.required'), requiredIf(isIPv6Static.value)),
    ipv6_validator: helpers.withMessage(t('validation.invalidIPv6'), ipv6_validator)
  },
  ipv6Dns1: {
    required: helpers.withMessage(t('validation.required'), requiredIf(isIPv6Static.value)),
    ipv6_validator: helpers.withMessage(t('validation.invalidIPv6'), ipv6_validator)
  },
  ipv6Dns2: {
    ipv6_validator: helpers.withMessage(t('validation.invalidIPv6'), ipv6_validator)
  },
  ntpServer: {
    required: helpers.withMessage(t('validation.required'), requiredIf(isNtpActivated.value)),
    domainname_validator: helpers.withMessage(t('validation.invalidHostname'), domainname_validator),
    maxLength: maxLength(64)
  },
  dcfOffset: {
    required: helpers.withMessage(t('validation.required'), requiredIf(isDcfActivated.value)),
    numeric: helpers.withMessage(t('validation.numeric'), numeric)
  },
  hmlgwPort: {
    required: helpers.withMessage(t('validation.required'), requiredIf(hmlgwEnabled.value)),
    numeric: helpers.withMessage(t('validation.numeric'), numeric),
    minValue: helpers.withMessage(t('validation.minValue', {min: 1}), val => val >= 1),
    maxValue: helpers.withMessage(t('validation.maxValue', {max: 65535}), val => val <= 65535)
  },
  hmlgwKeepAlivePort: {
    required: helpers.withMessage(t('validation.required'), requiredIf(hmlgwEnabled.value)),
    numeric: helpers.withMessage(t('validation.numeric'), numeric),
    minValue: helpers.withMessage(t('validation.minValue', {min: 1}), val => val >= 1),
    maxValue: helpers.withMessage(t('validation.maxValue', {max: 65535}), val => val <= 65535)
  }
}))

const v$ = useVuelidate(rules, {
  adminPassword,
  adminPasswordRepeat,
  hostname,
  localIP,
  netmask,
  gateway,
  dns1,
  dns2,
  ipv6Address,
  ipv6PrefixLength,
  ipv6Gateway,
  ipv6Dns1,
  ipv6Dns2,
  ntpServer,
  dcfOffset,
  hmlgwPort,
  hmlgwKeepAlivePort
})

// Load settings from store
const loadSettings = () => {
  hostname.value = settingsStore.hostname
  useDHCP.value = settingsStore.useDHCP
  localIP.value = settingsStore.localIP
  netmask.value = settingsStore.netmask
  gateway.value = settingsStore.gateway
  dns1.value = settingsStore.dns1
  dns2.value = settingsStore.dns2
  timesource.value = settingsStore.timesource
  dcfOffset.value = settingsStore.dcfOffset
  gpsBaudrate.value = settingsStore.gpsBaudrate
  ntpServer.value = settingsStore.ntpServer
  ledBrightness.value = settingsStore.ledBrightness
  checkUpdates.value = settingsStore.checkUpdates
  allowPrerelease.value = settingsStore.allowPrerelease

  if (settingsStore.hmlgwEnabled !== undefined) {
      hmlgwEnabled.value = settingsStore.hmlgwEnabled
      hmlgwPort.value = settingsStore.hmlgwPort || 2000
      hmlgwKeepAlivePort.value = settingsStore.hmlgwKeepAlivePort || 2001
  }

  if (settingsStore.analyzerEnabled !== undefined) {
      analyzerEnabled.value = settingsStore.analyzerEnabled
  }

  // Load DTLS settings
  if (settingsStore.dtlsMode !== undefined) {
    dtlsMode.value = settingsStore.dtlsMode
    dtlsCipherSuite.value = settingsStore.dtlsCipherSuite
    dtlsRequireClientCert.value = settingsStore.dtlsRequireClientCert
    dtlsSessionResumption.value = settingsStore.dtlsSessionResumption
  }

  // Load IPv6 settings if available
  if (settingsStore.enableIPv6 !== undefined) {
    enableIPv6.value = settingsStore.enableIPv6
    ipv6Mode.value = settingsStore.ipv6Mode || 'auto'
    ipv6Address.value = settingsStore.ipv6Address || ''
    ipv6PrefixLength.value = settingsStore.ipv6PrefixLength || 64
    ipv6Gateway.value = settingsStore.ipv6Gateway || ''
    ipv6Dns1.value = settingsStore.ipv6Dns1 || ''
    ipv6Dns2.value = settingsStore.ipv6Dns2 || ''
  }
}

// Watch store changes
watch(() => settingsStore.$state, () => {
  loadSettings()
}, { deep: true })

onMounted(async () => {
  await settingsStore.load()
  loadSettings()
})

const saveSettingsClick = async () => {
  v$.value.$touch()
  if (v$.value.$error) return

  loading.value = true
  showError.value = false
  showSuccess.value = false

  try {
    const settings = {
      adminPassword: adminPassword.value,
      hostname: hostname.value,
      useDHCP: useDHCP.value,
      localIP: localIP.value,
      netmask: netmask.value,
      gateway: gateway.value,
      dns1: dns1.value,
      dns2: dns2.value,
      timesource: timesource.value,
      dcfOffset: dcfOffset.value,
      gpsBaudrate: gpsBaudrate.value,
      ntpServer: ntpServer.value,
      ledBrightness: ledBrightness.value,
      checkUpdates: checkUpdates.value,
      allowPrerelease: allowPrerelease.value,
      // HMLGW
      hmlgwEnabled: hmlgwEnabled.value,
      hmlgwPort: hmlgwPort.value,
      hmlgwKeepAlivePort: hmlgwKeepAlivePort.value,
      analyzerEnabled: analyzerEnabled.value,
      // DTLS
      dtlsMode: dtlsMode.value,
      dtlsCipherSuite: dtlsCipherSuite.value,
      dtlsRequireClientCert: dtlsRequireClientCert.value,
      dtlsSessionResumption: dtlsSessionResumption.value,
      // IPv6 settings
      enableIPv6: enableIPv6.value,
      ipv6Mode: ipv6Mode.value,
      ipv6Address: ipv6Address.value,
      ipv6PrefixLength: ipv6PrefixLength.value,
      ipv6Gateway: ipv6Gateway.value,
      ipv6Dns1: ipv6Dns1.value,
      ipv6Dns2: ipv6Dns2.value
    }

    await settingsStore.save(settings)
    showSuccess.value = true
  } catch (error) {
    showError.value = true
  } finally {
    loading.value = false
  }
}

const downloadBackup = async () => {
  try {
    const response = await axios.get('/api/backup', { responseType: 'blob' })
    const url = window.URL.createObjectURL(new Blob([response.data]))
    const link = document.createElement('a')
    link.href = url
    link.setAttribute('download', 'settings.json')
    document.body.appendChild(link)
    link.click()
    document.body.removeChild(link)
    window.URL.revokeObjectURL(url)
  } catch (error) {
    console.error('Backup download failed:', error)
    alert(t('settings.backupError'))
  }
}

const restoreSettings = async () => {
    if (!restoreFile.value) return

    if (!confirm(t('settings.restoreConfirm'))) return

    try {
        const reader = new FileReader()
        reader.onload = async (e) => {
            try {
                const json = JSON.parse(e.target.result)
                await axios.post('/api/restore', json)
                alert(t('settings.restoreSuccess'))
                window.location.reload()
            } catch (err) {
                alert(t('settings.restoreError') + ': ' + err.message)
            }
        }
        reader.readAsText(restoreFile.value)
    } catch (e) {
        alert(t('settings.restoreError'))
    }
}

const rebootClick = async () => {
    if (confirm(t('common.confirmReboot'))) {
        try {
            await axios.post('/api/restart')
            // Show wait message
            alert(t('common.rebootingWait'))
            setTimeout(() => {
                window.location.reload()
            }, 10000)
        } catch (e) {
            console.error(e)
        }
    }
}

const factoryResetClick = async () => {
    if (confirm(t('common.confirmFactoryReset'))) {
        try {
            await axios.post('/api/factory_reset')
             // Show wait message
            alert(t('common.factoryResettingWait'))
            setTimeout(() => {
                window.location.reload()
            }, 10000)
        } catch (e) {
            console.error(e)
            alert('Factory Reset failed')
        }
    }
}
</script>

<style scoped>
</style>
