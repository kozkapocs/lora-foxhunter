#!/usr/bin/python3
# Post-build script: converts the built .hex to a .uf2 file that can be
# dragged onto the Wio Tracker L1 UF2 bootloader drive.

import os

Import("env")

firmware_hex = "${BUILD_DIR}/${PROGNAME}.hex"
uf2_file     = "${BUILD_DIR}/${PROGNAME}.uf2"

def create_uf2(source, target, env):
    env.Execute(" ".join([
        '"$PYTHONEXE"',
        '"$PROJECT_DIR/../tools/uf2conv/uf2conv.py"',
        "-f", "0xADA52840",
        "-c", firmware_hex,
        "-o", uf2_file,
    ]))

env.AddPostAction("$BUILD_DIR/${PROGNAME}.hex", create_uf2)
