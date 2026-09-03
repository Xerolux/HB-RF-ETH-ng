#!/usr/bin/env bash
set -euo pipefail

ESP_IDF_DIR="${ESP_IDF_DIR:-${HOME}/esp-idf}"
ESP_IDF_VERSION="${ESP_IDF_VERSION:-v6.1}"

if [[ ! -d "${ESP_IDF_DIR}" ]]; then
  git clone --depth 1 --branch "${ESP_IDF_VERSION}" https://github.com/espressif/esp-idf.git "${ESP_IDF_DIR}"
else
  if ! git -C "${ESP_IDF_DIR}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "ERROR: ${ESP_IDF_DIR} exists but is not an ESP-IDF Git checkout." >&2
    exit 1
  fi
  installed_version="$(git -C "${ESP_IDF_DIR}" describe --tags --exact-match HEAD 2>/dev/null || true)"
  if [[ "${installed_version}" != "${ESP_IDF_VERSION}" ]]; then
    installed_description="$(git -C "${ESP_IDF_DIR}" describe --tags --always --dirty 2>/dev/null || echo unknown)"
    echo "ERROR: ${ESP_IDF_DIR} is ${installed_description}; this project requires ${ESP_IDF_VERSION}." >&2
    echo "Move that checkout aside or set ESP_IDF_DIR to a verified ${ESP_IDF_VERSION} checkout, then rerun." >&2
    exit 1
  fi
  echo "Using verified ESP-IDF ${installed_version} at ${ESP_IDF_DIR}."
fi

git -C "${ESP_IDF_DIR}" submodule update --init --recursive --depth 1
python3 -m pip install -r "${ESP_IDF_DIR}/requirements.txt"

cat <<MSG
ESP-IDF setup complete.
Next steps:
  . "${ESP_IDF_DIR}/export.sh"
  ./idf.py build
MSG
