CFLAGS=-std=c11 -g -static
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

9cc: $(OBJS)
	$(CC) -o 9cc $(OBJS) $(LDFLAGS)

$(OBJS): 9cc.h utility.h

test: 9cc
	./9cc tests > tmp.s
	echo 'int char_fn() { return 257; }' | cc -xc -c -o tmp2.o -
	cc -static -o tmp tmp.s tmp2.o
	./tmp

clean:
	rm -f 9cc *.o *~ tmp*

.PHONY: test clean
