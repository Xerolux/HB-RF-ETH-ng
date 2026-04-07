#!/bin/bash
docker run --rm -v $(pwd):/project -w /project espressif/idf:v6.0-rc1 /bin/bash -c "idf.py build"
