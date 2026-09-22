.PHONY: cli-test tui-build tui-test check clean demo-cli-wizard demo-cli-plain demo-cli-noninteractive demo-tui demo-build

clean:
	rm -rf target tui/build

cli-test:
	cargo test

tui-build:
	cmake -S tui -B tui/build
	cmake --build tui/build

tui-test: tui-build
	ctest --test-dir tui/build --output-on-failure

demo-build:
	cargo build -p configwizard-ui-cli --bin configwizard-demo
	cmake -S tui -B tui/build
	cmake --build tui/build --target configwizard-tui-demo

demo-cli-wizard:
	./examples/run-cli-wizard.sh

demo-cli-plain:
	./examples/run-cli-plain.sh

demo-cli-noninteractive:
	./examples/run-cli-noninteractive.sh

demo-tui:
	./examples/run-tui.sh

demo-tui-licenses:
	./tui/build/examples/configwizard-tui-demo examples/licenses_demo.json

check: cli-test tui-test
