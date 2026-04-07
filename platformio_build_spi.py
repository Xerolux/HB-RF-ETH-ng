import urllib.request
url = "https://raw.githubusercontent.com/espressif/esp-idf/v6.0-rc1/components/esp_eth/include/esp_eth_mac_spi.h"
try:
    response = urllib.request.urlopen(url)
    print(response.read().decode('utf-8'))
except Exception as e:
    print(e)
