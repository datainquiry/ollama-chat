CC = gcc
PROGRAM = ollama-chat

# Source files
SOURCES = ollama_chat.c ollama_api.c ui.c ui_callbacks.c ui_chat_view.c \
          ui_input.c ui_history.c ui_dialogs.c ui_header.c web_search.c \
          history.c config.c markdown.c

# Object files derived from source files
OBJS = $(SOURCES:.c=.o)

# Compiler and linker flags
PKG_CONFIG_PACKAGES = gtk4 libcurl json-c gtksourceview-5
CFLAGS = -Wall -Wextra -std=c11 -O2 $(shell pkg-config --cflags $(PKG_CONFIG_PACKAGES))
LDFLAGS = $(shell pkg-config --libs $(PKG_CONFIG_PACKAGES)) -lpthread -luuid
DEBUG_CFLAGS = -Wall -Wextra -std=c11 -g -DDEBUG -O0 $(shell pkg-config --cflags $(PKG_CONFIG_PACKAGES))

# Default target
all: $(PROGRAM)

.PHONY: all clean debug install uninstall desktop-file check-deps

# Link the program
$(PROGRAM): $(OBJS)
	$(CC) $(OBJS) -o $(PROGRAM) $(LDFLAGS)

# Compile source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Debug build
debug:
	$(MAKE) "CFLAGS=$(DEBUG_CFLAGS)" all

# Clean the build directory
clean:
	rm -f $(OBJS) $(PROGRAM)

# Install the program and desktop file
install: all desktop-file
	install -D $(PROGRAM) $(DESTDIR)/usr/local/bin/$(PROGRAM)
	install -D -m644 $(PROGRAM).desktop $(DESTDIR)/usr/share/applications/$(PROGRAM).desktop
	install -D -m644 $(PROGRAM).svg $(DESTDIR)/usr/share/icons/hicolor/scalable/apps/$(PROGRAM).svg

# Uninstall the program and desktop file
uninstall:
	rm -f $(DESTDIR)/usr/local/bin/$(PROGRAM)
	rm -f $(DESTDIR)/usr/share/applications/$(PROGRAM).desktop
	rm -f $(DESTDIR)/usr/share/icons/hicolor/scalable/apps/$(PROGRAM).svg

# Create desktop entry file
desktop-file:
	@echo "Creating desktop entry file..."
	@echo "[Desktop Entry]" > $(PROGRAM).desktop
	@echo "Version=1.0" >> $(PROGRAM).desktop
	@echo "Type=Application" >> $(PROGRAM).desktop
	@echo "Name=Ollama Chat" >> $(PROGRAM).desktop
	@echo "Comment=Chat with AI models using Ollama" >> $(PROGRAM).desktop
	@echo "Exec=/usr/local/bin/$(PROGRAM)" >> $(PROGRAM).desktop
	@echo "Icon=$(PROGRAM)" >> $(PROGRAM).desktop
	@echo "Terminal=false" >> $(PROGRAM).desktop
	@echo "Categories=Utility;Office;" >> $(PROGRAM).desktop

# Check dependencies
check-deps:
	@echo "Checking dependencies..."
	@pkg-config --exists $(PKG_CONFIG_PACKAGES) || \
		(echo "Missing dependencies. Please install them for your distribution." && exit 1)
	@echo "All dependencies found."