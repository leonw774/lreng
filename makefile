CFLAGS = -O2
DEPS = *.c

all: lreng

debug: CFLAGS = -g
debug: all

clean:
	rm lreng

lreng: ${DEPS} lreng.c
	gcc ${CFLAGS} -o $@ $^;
