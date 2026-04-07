import urllib.request
url = "https://raw.githubusercontent.com/espressif/esp-eth-drivers/main/include/esp_eth_phy_lan87xx.h"
try:
    response = urllib.request.urlopen(url)
    print(response.read().decode('utf-8'))
except Exception as e:
    print(e)
