CFLAGS = -O2
DEPS = dyn_arr.c token.c tree.c errors.c operators.c tokenizer.c tree_parser.c

all: lreng

debug: CFLAGS = -g
debug: all

clean:
	rm lreng *.o

lreng: ${DEPS} lreng.c
	gcc ${CFLAGS} -o $@ $^;
