CC=gcc
CFLAGS=-Wall -Wextra -pedantic -ansi
LDFLAGS=-lglfw -lGL

gamefuck: gamefuck.c
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $^
