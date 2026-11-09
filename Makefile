CC      := gcc
CFLAGS  := -std=c11 -D_GNU_SOURCE -Wall -Wextra -O2 -g3 -Iinclude
LDFLAGS := -lraylib -lm

TARGET  := aed_tree_debugger
SRCS    := src/main.c \
           src/ui_workspace.c \
           src/layout_renderer.c \
           src/ipc_gdb.c \
           src/diagnostics.c

OBJS    := $(SRCS:.c=.o)

.PHONY: all clean run install uninstall

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

src/%.o: src/%.c include/visualizer.h include/raygui.h
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

install:
	chmod +x install.sh uninstall.sh
	./install.sh

uninstall:
	chmod +x uninstall.sh
	./uninstall.sh

clean:
	rm -f $(OBJS) $(TARGET) /tmp/aed_sandbox.c /tmp/aed_sandbox_bin /tmp/aed_compile_err.txt
