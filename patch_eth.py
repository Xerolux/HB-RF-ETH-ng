import os
path = "src/ethernet.cpp"
with open(path, 'r') as f:
    content = f.read()

content = content.replace(
"""    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();

    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    esp32_emac_config.smi_gpio.mdio_num = ETH_MDIO_PIN;
    esp32_emac_config.smi_gpio.mdc_num = ETH_MDC_PIN;""",
"""    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();

    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    esp32_emac_config.smi_gpio.mdio_num = ETH_MDIO_PIN;
    esp32_emac_config.smi_gpio.mdc_num = ETH_MDC_PIN;
    esp32_emac_config.clock_config.rmii.clock_mode = EMAC_CLK_EXT_IN;
    esp32_emac_config.clock_config.rmii.clock_gpio = 0;"""
)

with open(path, 'w') as f:
    f.write(content)
