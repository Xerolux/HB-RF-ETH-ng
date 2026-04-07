import urllib.request
url = "https://components.espressif.com/api/components/espressif/esp_eth_phy_lan87xx"
try:
    response = urllib.request.urlopen(url)
    print(response.read().decode('utf-8'))
except Exception as e:
    pass
