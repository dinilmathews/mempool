CC := gcc
CFLAGS := -g -Wall -Werror -DDEBUG
INC := inc
SRCS := test.c src/mempool.c

mempool: $(SRCS) inc/mempool.h
	$(CC) $(CFLAGS) -I$(INC) $(SRCS) -o $@

run: mempool
	./mempool

clean:
	rm mempool

