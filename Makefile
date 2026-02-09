# Modern Makefile with pkg-config and automatic dependency generation

CC := clang
CFLAGS := -std=c23 -Wall -Wextra -Wpedantic -Wshadow -O2 -MMD -MP
LDFLAGS := $(shell pkg-config --libs ncurses 2>/dev/null || echo -lncurses)
CFLAGS += $(shell pkg-config --cflags ncurses 2>/dev/null)

TARGET := fildel
SRCS := fildel.c
OBJS := $(SRCS:.c=.o)
DEPS := $(OBJS:.o=.d)

.PHONY: all clean debug release check

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Include auto-generated dependencies
-include $(DEPS)

debug: CFLAGS := -std=c23 -Wall -Wextra -Wpedantic -g -O0 -fsanitize=address,undefined
debug: clean $(TARGET)

release: CFLAGS := -std=c23 -Wall -Wextra -Wpedantic -O3 -DNDEBUG
release: clean $(TARGET)

check: $(TARGET)
	@echo "Running basic test..."
	@./$(TARGET) --help 2>/dev/null || echo "(no --help, expected)"

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/$(TARGET)

clean:
	rm -f $(OBJS) $(DEPS) $(TARGET)

# Generate compile_commands.json for LSP
compile_commands.json:
	@echo "Generating compile_commands.json..."
	@echo '[' > compile_commands.json
	@echo '  {' >> compile_commands.json
	@echo '    "directory": "'$(PWD)'",' >> compile_commands.json
	@echo '    "command": "$(CC) $(CFLAGS) -c $(SRCS)",' >> compile_commands.json
	@echo '    "file": "$(SRCS)"' >> compile_commands.json
	@echo '  }' >> compile_commands.json
	@echo ']' >> compile_commands.json
