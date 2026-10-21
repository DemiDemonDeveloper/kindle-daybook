.DEFAULT_GOAL := host
CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Werror
COMMON = src/model.c src/store.c src/vault.c vendor/cJSON.c
SODIUM = .tools/sodium-host/lib/libsodium.a
CPPFLAGS += -I.tools/sodium-host/include
LDLIBS += $(SODIUM)
$(SODIUM): vendor/libsodium-1.0.22-stable.tar.gz scripts/build-sodium.sh
	./scripts/build-sodium.sh host
.PHONY: test host kindle clean
host: $(SODIUM)
	mkdir -p build
	$(CC) $(CFLAGS) $(CPPFLAGS) src/daybook.c $(COMMON) -lm $(LDLIBS) -o build/daybook-host
test: $(SODIUM)
	mkdir -p build
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_model.c $(COMMON) -lm $(LDLIBS) -o build/test-model
	./build/test-model
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_ui.c $(COMMON) -lm $(LDLIBS) -o build/test-ui
	./build/test-ui
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_journal.c $(COMMON) -lm $(LDLIBS) -o build/test-journal
	./build/test-journal
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_cover.c $(COMMON) -lm $(LDLIBS) -o build/test-cover
	./build/test-cover
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_vault.c $(COMMON) -lm $(LDLIBS) -o build/test-vault
	./build/test-vault
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_privacy.c $(COMMON) -lm $(LDLIBS) -o build/test-privacy
	./build/test-privacy
	python3 tests/test_launcher.py
kindle:
	./scripts/build-kindle.sh
clean:
	rm -rf build dist
