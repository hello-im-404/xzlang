CC ?= gcc
CFLAGS ?= -O3 -Iinclude
SRC = src/lexer.c src/parser.c src/ast.c src/eval.c src/compiler.c src/main.c
OBJ = $(SRC:.c=.o)
TARGET = xzlang

PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
LIBDIR ?= /usr/xzlib

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	@echo "[+] Installing xzlang binary to $(BINDIR)..."
	install -d $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)/
	@echo "[+] Installing standard libraries to $(LIBDIR)..."
	install -d $(LIBDIR)
	install -m 644 lib/*.xzl $(LIBDIR)/
	@echo "[+] Installation complete!"

clean:
	rm -f $(OBJ) $(TARGET) xz_temp.* app

.PHONY: all install clean