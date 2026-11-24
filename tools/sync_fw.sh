#!/bin/sh
# Copies the engine into the firmware fork (firmware/app). The engine's .c becomes an .inc: lofi_engine.c includes it, so the
# audio-side code and tables get the RAM attributes (FM1_AUDIO_CODE / FM1_AUDIO_DATA) the firmware needs, and the build keeps one object.
set -e
cd "$(dirname "$0")/.."
D=firmware/app
cp core/oual_tables.h core/oual_codec3.h core/oual_engine.h $D/
cp core/oual_engine.c $D/oual_engine.inc
echo "synced to $D"
