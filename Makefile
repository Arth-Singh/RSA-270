# Builds every first-party scanner into build/current/.
# macOS: uses Homebrew gmp and openssl@3 (override GMP_PREFIX / SSL_PREFIX if installed elsewhere).
# Linux: needs libgmp-dev and libssl-dev from the distribution.

CXX      ?= c++
CXXFLAGS ?= -O3 -std=c++20 -Wall -Wno-deprecated-declarations
BIN      := build/current

UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
	GMP_PREFIX ?= /opt/homebrew/opt/gmp
	SSL_PREFIX ?= /opt/homebrew/opt/openssl@3
	CPPFLAGS   += -I$(GMP_PREFIX)/include -I$(SSL_PREFIX)/include
	LDFLAGS    += -L$(GMP_PREFIX)/lib -L$(SSL_PREFIX)/lib
endif

GMP_LIBS := -lgmpxx -lgmp
SSL_LIBS := -lcrypto

TARGETS := md1_cli md1_hamming_scan bsafe1_rng_scan rsaref_md5_state_scan \
	       seed_scan noise_time_scan rng_stats snfs_scan

.PHONY: all smoke clean
all: $(addprefix $(BIN)/,$(TARGETS))

$(BIN):
	mkdir -p $@

$(BIN)/md1_cli: src/md1.cpp src/md1_cli.cpp src/md1.hpp | $(BIN)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) src/md1.cpp src/md1_cli.cpp -o $@

$(BIN)/md1_hamming_scan: src/md1.cpp src/md1_hamming_scan.cpp src/md1.hpp | $(BIN)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) src/md1.cpp src/md1_hamming_scan.cpp $(LDFLAGS) $(GMP_LIBS) -pthread -o $@

$(BIN)/bsafe1_rng_scan: src/bsafe1_rng_scan.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< $(LDFLAGS) $(GMP_LIBS) -pthread -o $@

$(BIN)/rsaref_md5_state_scan: src/rsaref_md5_state_scan.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< $(LDFLAGS) $(GMP_LIBS) $(SSL_LIBS) -pthread -o $@

$(BIN)/seed_scan: src/seed_scan.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< $(LDFLAGS) $(SSL_LIBS) -pthread -o $@

$(BIN)/noise_time_scan: src/noise_time_scan.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< $(LDFLAGS) $(GMP_LIBS) $(SSL_LIBS) -pthread -o $@

$(BIN)/rng_stats: src/rng_stats.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< $(LDFLAGS) $(GMP_LIBS) -o $@

$(BIN)/snfs_scan: src/snfs_scan.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< $(LDFLAGS) $(GMP_LIBS) -o $@

# Known-answer and tiny-run checks. A scanner exit status of 1 means "no modeled state matched", which is the expected outcome.
smoke: $(BIN)/md1_cli $(BIN)/rsaref_md5_state_scan
	test "$$($(BIN)/md1_cli abc)" = 74594a0a73b03503efe62e574dbaf816 && echo "md1 known answer: ok"
	$(BIN)/rsaref_md5_state_scan md5-word 0 1000 2 4; test $$? -le 1 && echo "rsaref_md5_state_scan tiny run: ok"
	python3 src/rsaref_stream_reconstruct.py > /dev/null && echo "rsaref_stream_reconstruct: ok"

clean:
	rm -rf $(BIN)
