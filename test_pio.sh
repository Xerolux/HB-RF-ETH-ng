#!/bin/bash
sed -i 's/platformio\/framework-espidf@~3.50503.0/framework-espidf@https:\/\/github.com\/espressif\/esp-idf.git#v6.0-rc1/' platformio.ini
cat platformio.ini
