CC     = gcc
STD    = c2x
ANVL   = ..

CFLAGS = -Wall -Wextra -O2 -std=$(STD) \
         -I$(ANVL)/include \
         -D_POSIX_C_SOURCE=200809L

BIN = bin

LIB_RELEASE = $(ANVL)/lib/release/libanvil.a

.PHONY: all throughput parse_validate lib-release clean

# Links against the built release artifact -- same "prove the shipped thing" discipline as
# test/functional -- these numbers are meant to be quoted publicly, so they must measure
# exactly what a real caller of the shipped library gets, not raw sources.

all: throughput parse_validate

lib-release:
	$(MAKE) -C $(ANVL) lib-release

$(LIB_RELEASE): lib-release

$(BIN)/throughput: throughput.c $(LIB_RELEASE)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) throughput.c $(LIB_RELEASE) -o $@

throughput: $(BIN)/throughput
	$(BIN)/throughput

clean:
	rm -f $(BIN)/*

# Per-commit regression bench (parse floats/ints/mixed, schema validate); see bench/compare.sh
$(BIN)/parse_validate: parse_validate.c $(LIB_RELEASE)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) parse_validate.c $(LIB_RELEASE) -o $@

parse_validate: $(BIN)/parse_validate
	$(BIN)/parse_validate
