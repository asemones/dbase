CC      := clang
CFLAGS  := -Wall -g -m64  -I./src -gdwarf-4 -mavx -fsanitize=address
LDFLAGS := -debug -pthread -luuid -lzstd

OBJDIR := build
TARGET := $(OBJDIR)/test


ALL_SRCS := $(wildcard src/**/*.c)
SRCS := $(filter-out src/tests/% src/util/%, $(ALL_SRCS))
SRCS += $(wildcard src/db/backend/*.c)
SRCS += $(wildcard src/db/backend/dal/*.c)
SRCS += src/tests/testrunner.c
SRCS += src/tests/unity/src/unity.c

OBJS   := $(notdir $(SRCS))
OBJS   := $(OBJS:.c=.o)
OBJS   := $(addprefix $(OBJDIR)/,$(OBJS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

define FIND_SRC
$(shell echo $(SRCS) | tr ' ' '\n' | grep '/$*.c$$')
endef

$(OBJDIR)/%.o:
	@mkdir -p $(OBJDIR)
	@echo "Compiling $* -> $@"
	$(CC) $(CFLAGS) -c $(call FIND_SRC,$*) -o $@

clean:
	rm -rf $(OBJDIR)

test: $(TARGET)
	@echo "Running tests..."
	@./$(TARGET)

.PHONY: all clean test
