CFLAGS = -I include/
DEBUG_FLAGS = -g -D ENABLE_DEBUG
MEMCHECK_FLAGS = -include memcheck/memcheck.h -Wno-implicit-function-declaration

TEST_DIR = tests

SHARED_SRC = src/**/*.c
TEST_SRC = $(wildcard $(TEST_DIR)/*.c) 
MAIN_SRC = src/*.c

TEST_TARGET = $(patsubst %.c, %.out, $(TEST_SRC))
MAIN_TARGET = lreng

.PHONY: all debug memcheck clean

all: CFLAGS += -O2
all: $(MAIN_TARGET)

$(MAIN_TARGET): $(MAIN_SRC) $(SHARED_SRC)
	gcc $(CFLAGS) -o $@ $^

debug: CFLAGS += $(DEBUG_FLAGS)
debug: $(MAIN_TARGET) $(TEST_TARGET)

memcheck: CFLAGS += $(DEBUG_FLAGS) $(MEMCHECK_FLAGS)
memcheck: $(MAIN_TARGET) $(TEST_TARGET)

# The rules of test targets
%.out: %.c $(SHARED_SRC)
	gcc $(CFLAGS) -o $@ $^

clean:
	rm $(MAIN_TARGET) $(TEST_TARGET) || true
