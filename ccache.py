# ccache.py
import os
Import("env")

CCACHE = "ccache"
TOOLCHAIN_BIN = os.path.expandvars(r"%USERPROFILE%\.platformio\packages\toolchain-riscv32-esp\bin")

# langsung attach compiler ke ccache
env['CC']  = f"{CCACHE} {TOOLCHAIN_BIN}\\riscv32-esp-elf-gcc.exe"
env['CXX'] = f"{CCACHE} {TOOLCHAIN_BIN}\\riscv32-esp-elf-g++.exe"

# set max cache size 5GB
os.system(f"{CCACHE} -M 5G")
