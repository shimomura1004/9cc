CFLAGS=-std=c11 -g -static
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

9cc: $(OBJS)
	$(CC) -o 9cc $(OBJS) $(LDFLAGS)

$(OBJS): 9cc.h utility.h

test: 9cc
	./9cc tests > tmp.s
	cc -static -o tmp tmp.s
	./tmp

clean:
	rm -f 9cc *.o *~ tmp*

.PHONY: test clean
