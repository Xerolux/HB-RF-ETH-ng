import os
path = "include/ethernet.h"
with open(path, 'r') as f:
    content = f.read()

content = content.replace('#include "esp_eth.h"', '#include "esp_eth.h"\n#include "esp_eth_mac_esp.h"\n#include "esp_eth_phy_lan87xx.h"')

with open(path, 'w') as f:
    f.write(content)
