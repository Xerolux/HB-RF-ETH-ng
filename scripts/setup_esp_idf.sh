#!/usr/bin/env bash
set -euo pipefail

ESP_IDF_DIR="${HOME}/esp-idf"
ESP_IDF_VERSION="${ESP_IDF_VERSION:-v6.0}"

if [[ ! -d "${ESP_IDF_DIR}" ]]; then
  git clone --depth 1 --branch "${ESP_IDF_VERSION}" https://github.com/espressif/esp-idf.git "${ESP_IDF_DIR}"
else
  echo "ESP-IDF already exists at ${ESP_IDF_DIR}, skipping clone."
fi

git -C "${ESP_IDF_DIR}" submodule update --init --recursive --depth 1
python3 -m pip install -r "${ESP_IDF_DIR}/requirements.txt"

cat <<MSG
ESP-IDF setup complete.
Next steps:
  . "${ESP_IDF_DIR}/export.sh"
  ./idf.py build
MSG
