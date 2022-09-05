#!/bin/bash

if [[ shared_src/protoloma.proto -nt shared_src/protoloma.pb.cc ]]; then
  protoc -I=shared_src/ --cpp_out=shared_src/ shared_src/protoloma.proto
fi

cd build
ninja
