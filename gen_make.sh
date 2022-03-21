#!/bin/bash

rm -r build
mkdir build
cd build

export CC=/usr/bin/clang
export CXX=/usr/bin/clang++
cmake -GNinja .. -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache
#cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache
mv compile_commands.json ..
