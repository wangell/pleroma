all: run-hosted

.PHONY: build-hosted run-hosted build-native

build-native:
	cargo build --features native --config .cargo/config.native.toml

test-native:
	cargo test --features native --config .cargo/config.native.toml

run-native:
	cargo run --features native --config .cargo/config.native.toml

build-hosted:
	cargo build --features hosted --config .cargo/config.hosted.toml

run-hosted:
	cargo run --features hosted --config .cargo/config.hosted.toml
