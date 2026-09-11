CC     = gcc
STD    = c2x
ANVL   = ..

CFLAGS = -Wall -Wextra -O2 -std=$(STD) \
         -I$(ANVL)/include \
         -D_POSIX_C_SOURCE=200809L

BIN = bin

LIB_RELEASE = $(ANVL)/lib/release/libanvil.a

.PHONY: all throughput lib-release clean

# Links against the built release artifact -- same "prove the shipped thing" discipline as
# test/functional -- these numbers are meant to be quoted publicly, so they must measure
# exactly what a real caller of the shipped library gets, not raw sources.

all: throughput

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
