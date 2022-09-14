all: shared_src/protoloma.pb.h shared_src/protoloma.pb.cc
	ninja -C build/

shared_src/protoloma.pb.h shared_src/protoloma.pb.cc: shared_src/protoloma.proto
	protoc -I=shared_src/ --cpp_out=shared_src/ shared_src/protoloma.proto

cmake:
	rm -r build
	mkdir build
	
	CC=/usr/bin/clang CXX=/usr/bin/clang++ cmake -S . -GNinja -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache
	mv build/compile_commands.json .

test:
	./run_tests.py
