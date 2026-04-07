import urllib.request
url = "https://raw.githubusercontent.com/espressif/esp-idf/v6.0-rc1/components/mdns/include/mdns.h"
try:
    response = urllib.request.urlopen(url)
    print(response.read().decode('utf-8'))
except Exception as e:
    pass
