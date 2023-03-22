all: run-hosted

.PHONY: build-hosted run-hosted build-native

build-native:
	cargo build --features native --target x86_64-unknown-none

run-native:
	cargo run --features native --target x86_64-unknown-none

build-hosted:
	cargo build --features hosted --target x86_64-unknown-linux-gnu

run-hosted:
	cargo run --features hosted --target x86_64-unknown-linux-gnu
