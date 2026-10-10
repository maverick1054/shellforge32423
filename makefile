CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude
SRC = src/builtin.c src/expand.c src/history.c src/lexer.c src/main.c src/parser.c src/token.c src/executor.c src/pagesim.c src/cpusched.c src/memalloc.c
TARGET = shellforge

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -lreadline -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: clean
