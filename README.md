# Programmation Réseaux - Awalé

This project is a networked implementation of the traditional African game Awalé.
To run it locally, start the server and connect multiple clients to play against each other.

## Features

This project implements a networked Awalé game server and a command-line client. The implementation covers the core assignment requirements and several useful extras. Below is a concise summary of what is implemented in this repository.

Core game functionality
- Full Awalé game logic: board representation (12 pits, 4 seeds per pit), move sowing, capture rules, scoring and end-of-game detection.
- Move legality checks and server-side enforcement (server validates turns and legal moves).
- ASCII board rendering for human-readable game state output (printed to clients).

Client / Server features
- TCP server that accepts multiple simultaneous client connections and manages multiple concurrent games.
- Simple text protocol (commands like REGISTER, CHALLENGE, MOVE, CHAT, OBSERVE, etc.).
- Clients register with a username and can request the list of online users.
- Challenge flow: challenge a user, accept or refuse; server creates a game and randomly picks the starting player.
- Focused game support: clients can focus a game so subsequent commands (moves, show_board, set_private) act on that game by default.

Spectating & privacy
- Spectator mode: a client can observe a running game and receives board updates as moves happen.
- Private mode per game: players can set a game private; when private only friends (see below) are allowed to spectate.

Chat & groups
- Chat messages between users and group chats. Group creation, invite and quit commands are implemented server-side.

Profiles & social
- User bio: set and show a short multi-line bio (client supports interactive 10-line input).
- Friends: add/remove friends and list friends; friend lists are used to control spectating of private games.

Persistence & replays
- Finished games are persisted to the `replays/` directory (files named `g-<id>.rec`).
- The server persists the next game id to `replays/next_game_id` so replay ids survive restarts.
- Replay viewer: clients can request a replay and step through moves (next / previous / exit) to review a finished game.

Client
- An interactive terminal client (CLI) providing commands for all supported actions: connect, register, bio, list, challenge, accept/refuse, move, chat, group management, friends, observe, set_private, get_replay, etc.

Extras / notes
- Multiple concurrent games are supported by the server; the server keeps an in-memory game list and compacts it on removal.
- When a game finishes the server notifies players and saves a replay automatically.
- The server handles client disconnects and marks users offline.

Known limitations / TODOs
- No authentication beyond choosing a username; usernames are reused across reconnections.
- Protocol is plain-text and unencrypted (no TLS). Use only on trusted networks or add TLS externally.
- Replay persistence is file-based and simple; there is a `TODO` note in the code about ensuring replays are never overwritten on restart (current implementation uses an incrementing persisted next_game_id to avoid collisions).
- No built-in reconnection/resume for ongoing games beyond re-registering the username.


## How to Run
1. Compile the project using the provided Makefile.
2. Start the server:
    ```bash
    ./server
    ```
3. Start one or more clients in separate terminal windows:
    ```bash
    ./client
    ```
4. Default server address is `localhost` and port `9987`.