# Compiler and flags
CC = clang
CFLAGS = -Wall -g -I${workspaceFolder}/src/tests/unity -fsanitize=address
LDFLAGS = -debug -pthread


# Source files and test files
OBJDIR = build
SRCS = src/http/driver.c
TEST_SRCS = src/tests/testrunner.c
UNITY_SRCS = src/tests/unity/src/unity.c 
OBJS = $(patsubst src/%.c, $(OBJDIR)/%.o, $(SRCS))
TEST_OBJS = $(patsubst src/%.c, $(OBJDIR)/%.o, $(TEST_SRCS))
EXEC = $(OBJDIR)/network
TEST_EXEC = $(OBJDIR)/test

# Default target
all: $(EXEC)

# Build main executable
$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $(EXEC) $(OBJS) $(LDFLAGS)

# Build test executable
$(TEST_EXEC): $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $(TEST_EXEC) $(TEST_OBJS) $(UNITY_SRCS) $(LDFLAGS)

# Compile source files
$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Run tests
test: $(TEST_EXEC)
	 ./$(TEST_EXEC)

$(OBJDIR):
	mkdir -p $(OBJDIR)
	mkdir -p $(OBJDIR)/http
	mkdir -p $(OBJDIR)/tests
run:$(EXEC)
	./$(EXEC)
# Clean up build artifacts
clean:
	rm -rf $(OBJDIR) $(EXEC) $(TEST_EXEC)

.PHONY: all test clean
