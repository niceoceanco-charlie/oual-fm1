#!/bin/sh
# Builds out/ci_test.lfp (a version 2 pack of ten synthetic sounds) for the firmware host harness. From the repo root.
set -e
mkdir -p out/raw
cc -O2 -o /tmp/gen_testsounds tools/gen_testsounds.c -lm && /tmp/gen_testsounds out/raw
cc -O2 -o /tmp/enc3 tools/enc3.c -lm
cc -O2 -o /tmp/mkpack3 tools/mkpack3.c -lm
for i in 01 02 03 04 05 06 07 08 09 10; do /tmp/enc3 out/raw/t$i.raw out/t$i.o3 >/dev/null; done
/tmp/mkpack3 out/ci_test.lfp CITEST out/t01.o3:0:0.4 out/t02.o3:0:0.5 out/t03.o3 out/t04.o3:0.1:0.3 out/t05.o3 out/t06.o3:0.2:0.6 out/t07.o3 out/t08.o3:0:0.3 out/t09.o3 out/t10.o3:0.5:0.7
