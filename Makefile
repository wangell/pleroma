all: shared_src/protoloma.pb.h shared_src/protoloma.pb.cc
	ninja -C build/

shared_src/protoloma.pb.h shared_src/protoloma.pb.cc: shared_src/protoloma.proto
	protoc -I=shared_src/ --cpp_out=shared_src/ shared_src/protoloma.proto

test:
	./run_tests.py
