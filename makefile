CFLAGS = -O2
DEPS = dyn_arr.c errors.c operators.c tokenizer.c

all: lreng

debug: CFLAGS = -g
debug: all

clean:
	rm lreng *.o

lreng: ${DEPS} lreng.c
	gcc ${CFLAGS} -o $@ $^;
