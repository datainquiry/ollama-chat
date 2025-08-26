PROGRAM = ollama-chat
SOURCES = ollama_chat.c ollama_api.c ui.c
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2
DEBUG_CFLAGS = -Wall -Wextra -std=c11 -g -DDEBUG -O0
PKG_CONFIG_PACKAGES = gtk4 libcurl json-c
CFLAGS += $(shell pkg-config --cflags $(PKG_CONFIG_PACKAGES))
LDFLAGS = $(shell pkg-config --libs $(PKG_CONFIG_PACKAGES)) -lpthread
OBJECTS = $(SOURCES:.c=.o)

all: $(PROGRAM)

$(PROGRAM): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(PROGRAM) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

debug: CFLAGS = $(DEBUG_CFLAGS) $(shell pkg-config --cflags $(PKG_CONFIG_PACKAGES))
debug: $(PROGRAM)

clean:
	rm -f $(OBJECTS) $(PROGRAM)

install: $(PROGRAM)
	install -D $(PROGRAM) $(DESTDIR)/usr/local/bin/$(PROGRAM)
	install -D -m644 ollama-chat.desktop $(DESTDIR)/usr/share/applications/ollama-chat.desktop

uninstall:
	rm -f /usr/local/bin/$(PROGRAM)
	rm -f /usr/share/applications/ollama-chat.desktop

# Create desktop entry file
desktop-file:
	@echo "Creating desktop entry file..."
	@echo "[Desktop Entry]" > ollama-chat.desktop
	@echo "Version=1.0" >> ollama-chat.desktop
	@echo "Type=Application" >> ollama-chat.desktop
	@echo "Name=Ollama Chat" >> ollama-chat.desktop
	@echo "Comment=Chat with AI models using Ollama" >> ollama-chat.desktop
	@echo "Exec=/usr/local/bin/ollama-chat" >> ollama-chat.desktop
	@echo "Icon=accessories-text-editor" >> ollama-chat.desktop
	@echo "Terminal=false" >> ollama-chat.desktop
	@echo "Categories=Utility;Office;" >> ollama-chat.desktop

# Check dependencies
check-deps:
	@echo "Checking dependencies..."
	@pkg-config --exists $(PKG_CONFIG_PACKAGES) && echo "All dependencies found" || (echo "Missing dependencies. Please install:"; echo "Ubuntu/Debian: sudo apt install libgtk-4-dev libadwaita-1-dev libcurl4"; exit 1)
