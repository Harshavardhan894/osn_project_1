CC = gcc

CFLAGS = -std=c99 \
-D_POSIX_C_SOURCE=200809L \
-D_XOPEN_SOURCE=700 \
-Wall -Wextra -Werror \
-Wno-unused-parameter \
-fno-asm \
-Iinclude

SRC = src/main.c src/parser.c src/executor.c src/log.c src/hop.c src/reveal.c

OUT = shell.out

all:
	$(CC) $(CFLAGS) $(SRC) -o $(OUT)

clean:
	rm -f $(OUT)