include config.mk

VERSION := 1.0

BIN_DIR := bin
OBJ_DIR := obj
SRC_DIR := src

BIN_NAME := athena
BIN := $(BIN_DIR)/$(BIN_NAME)

SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(addprefix $(OBJ_DIR)/, $(addsuffix .o, $(notdir $(basename $(SRC)))))

all: setup $(BIN)

test: setup $(TST_BIN)

$(BIN): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

setup:
	mkdir -p $(BIN_DIR) $(OBJ_DIR)

clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $(BIN) $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/$(BIN_NAME)
	mkdir -p $(DESTDIR)$(MANPREFIX)/man6
	cp -f athena.6 $(DESTDIR)$(MANPREFIX)/man6
	chmod 644 $(DESTDIR)$(MANPREFIX)/man6/athena.6

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN_NAME)
	rm -f $(DESTDIR)$(MANPREFIX)/man6/athena.6

.PHONY: all setup test clean install uninstall
