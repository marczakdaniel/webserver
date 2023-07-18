CC=g++
CFLAGS=-std=c++17 -Wall -Wextra

all: main.o func.o 
	$(CC) $(CFLAGS) -o http_server main.o func.o

main: main.cpp func.h
	$(CC) $(CFLAGS) -c main.cpp -o main.o

transport: func.cpp func.h
	$(CC) $(CFLAGS) -c func.cpp -o func.o

clean:
	rm -vf *.o

distclean:
	rm -vf *.o
	rm -vf http_server