CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Werror
COMMON = src/model.c src/store.c vendor/cJSON.c
.PHONY: test host kindle clean
host:
	mkdir -p build
	$(CC) $(CFLAGS) src/daybook.c $(COMMON) -lm -o build/daybook-host
test:
	mkdir -p build
	$(CC) $(CFLAGS) tests/test_model.c $(COMMON) -lm -o build/test-model
	./build/test-model
	$(CC) $(CFLAGS) tests/test_ui.c $(COMMON) -lm -o build/test-ui
	./build/test-ui
	$(CC) $(CFLAGS) tests/test_journal.c $(COMMON) -lm -o build/test-journal
	./build/test-journal
	python3 tests/test_launcher.py
kindle:
	./scripts/build-kindle.sh
clean:
	rm -rf build dist
