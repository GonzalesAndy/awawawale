CC = gcc
CFLAGS = -Wall -Wextra -g -I./src
SRCDIR = src
BINDIR = bin
OBJDIR = obj

GAME_SRC = $(SRCDIR)/game.c $(SRCDIR)/persist.c
TEST_SRC = tests/game_test.c
PLAY_SRC = tests/game_playthrough.c
SERVER_SRC = $(SRCDIR)/server.c $(SRCDIR)/server_net.c $(SRCDIR)/server_handlers.c $(SRCDIR)/server_state.c $(SRCDIR)/protocol.c $(SRCDIR)/game.c $(SRCDIR)/persist.c
CLIENT_SRC = $(SRCDIR)/client.c $(SRCDIR)/client_cli.c $(SRCDIR)/client_io.c $(SRCDIR)/protocol.c

GAME_OBJ = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(GAME_SRC))
SERVER_OBJ = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SERVER_SRC))
CLIENT_OBJ = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(CLIENT_SRC))
TEST_OBJ = $(patsubst tests/%.c,$(OBJDIR)/%.o,$(TEST_SRC))
PLAY_OBJ = $(patsubst tests/%.c,$(OBJDIR)/%.o,$(PLAY_SRC))

.PHONY: all clean dirs

all: dirs $(BINDIR)/game_test $(BINDIR)/game_playthrough $(BINDIR)/server $(BINDIR)/client

dirs:
	mkdir -p $(BINDIR) $(OBJDIR)

$(OBJDIR)/game.o: $(SRCDIR)/game.c $(SRCDIR)/game.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/persist.o: $(SRCDIR)/persist.c $(SRCDIR)/persist.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/protocol.o: $(SRCDIR)/protocol.c $(SRCDIR)/protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/server.o: $(SRCDIR)/server.c $(SRCDIR)/server.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/server_net.o: $(SRCDIR)/server_net.c $(SRCDIR)/server_net.h $(SRCDIR)/server.h $(SRCDIR)/protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/server_handlers.o: $(SRCDIR)/server_handlers.c $(SRCDIR)/server_handlers.h $(SRCDIR)/server_net.h $(SRCDIR)/server.h $(SRCDIR)/protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/server_state.o: $(SRCDIR)/server_state.c $(SRCDIR)/server.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/client.o: $(SRCDIR)/client.c $(SRCDIR)/client.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/client_cli.o: $(SRCDIR)/client_cli.c $(SRCDIR)/client_cli.h $(SRCDIR)/client.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/client_io.o: $(SRCDIR)/client_io.c $(SRCDIR)/client_io.h $(SRCDIR)/client.h $(SRCDIR)/protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/game_test.o: tests/game_test.c $(SRCDIR)/game.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/game_playthrough.o: tests/game_playthrough.c $(SRCDIR)/game.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BINDIR)/game_test: $(OBJDIR)/game.o $(OBJDIR)/persist.o $(OBJDIR)/game_test.o
	$(CC) $^ -o $@

$(BINDIR)/game_playthrough: $(OBJDIR)/game.o $(OBJDIR)/persist.o $(OBJDIR)/game_playthrough.o
	$(CC) $^ -o $@

$(BINDIR)/server: $(OBJDIR)/server.o $(OBJDIR)/server_net.o $(OBJDIR)/server_handlers.o $(OBJDIR)/server_state.o $(OBJDIR)/protocol.o $(OBJDIR)/game.o $(OBJDIR)/persist.o
	$(CC) $^ -o $@

$(BINDIR)/client: $(OBJDIR)/client.o $(OBJDIR)/client_cli.o $(OBJDIR)/client_io.o $(OBJDIR)/protocol.o
	$(CC) $^ -o $@

clean:
	rm -rf $(BINDIR) $(OBJDIR)
