CC=gcc
CFLAGS=-O3 -march=native
OBJ=web.o

all: web

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

web: web.o
	$(CC) $(OBJ) -o web

clean:
	rm web $(OBJ)
