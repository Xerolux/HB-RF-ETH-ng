import urllib.request
url = "https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32/migration-guides/release-6.x/6.0/index.html"
try:
    response = urllib.request.urlopen(url)
    print(response.read().decode('utf-8'))
except Exception as e:
    pass
