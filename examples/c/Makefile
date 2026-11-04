CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wextra -Werror -pedantic -O2 -g
ENET_CFLAGS := $(shell pkg-config --cflags enet 2>/dev/null || pkg-config --cflags libenet 2>/dev/null)
ENET_LIBS   := $(shell pkg-config --libs enet 2>/dev/null || pkg-config --libs libenet 2>/dev/null)
ifeq ($(strip $(ENET_LIBS)),)
ENET_LIBS := -lenet
endif
CURL_CFLAGS := $(shell pkg-config --cflags libcurl 2>/dev/null)
CURL_LIBS   := $(shell pkg-config --libs libcurl 2>/dev/null)

all: test

proto10.o: proto10.c proto10.h
	$(CC) $(CFLAGS) -c $< -o $@

proto075.o: proto075.c proto075.h
	$(CC) $(CFLAGS) -c $< -o $@

test_codec: test_codec.c proto10.c proto10.h
	$(CC) $(CFLAGS) -o $@ test_codec.c proto10.c -lm

test: test_codec
	./test_codec

# needs ENet 1.3
enet_client: enet_client.c proto10.c proto075.c proto10.h proto075.h
	$(CC) $(CFLAGS) $(ENET_CFLAGS) -o $@ enet_client.c proto10.c proto075.c $(ENET_LIBS) -lm

# needs libcurl
master_http: master_http.c
	@if [ -z "$(CURL_LIBS)" ]; then \
		echo "libcurl not found (pkg-config libcurl)."; \
		exit 1; \
	fi
	$(CC) $(CFLAGS) $(CURL_CFLAGS) -o $@ master_http.c $(CURL_LIBS)

clean:
	rm -f *.o test_codec enet_client master_http

# aliases
enet: enet_client
master: master_http

.PHONY: all test clean enet master
