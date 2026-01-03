CC = cc
INCS=/usr/local/include
LIBS=/usr/local/lib
CFLAGS=-Wall -Wextra -Wpedantic
LDFLAGS=-lc -lcurl -lcjson
BIN=coinprice

all: 
	$(CC) -I $(INCS) *.c $(CFLAGS) -c -g
	$(CC) -L $(LIBS) *.o $(LDFLAGS) -o $(BIN)
