#!/usr/bin/env fish
# ESP-IDF requires sourcing export.fish to set up PATH and environment variables.
# This script wraps idf.py so you don't have to source it manually each time.
source /Users/olir/esp/esp-idf/export.fish >/dev/null 2>&1
idf.py $argv
