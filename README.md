# Programmation Réseaux - Awalé

This project is a networked implementation of the traditional African game Awalé.
To run it locally, start the server and connect multiple clients to play against each other.

## Features

- Server-client architecture using TCP sockets.
- Multiple clients can connect to a single server.
- Clients can register with unique usernames.
- Player can set a multi-line bio.
- Player can see each other's bios.

- A player can challenge another player to a game of Awalé. He cannot challenge himself or a disconnected player.
- A challenged player can either accept or refuse the challenge. 
- The player can play multiple games in parallel. Using the focus command, he can switch between games.
- The game follows the traditional rules of Awalé.

- Player can chat between each other. Either by private or in a group chat. A player can create a group chat and invite other players to join it.
- Player who got invited to a group chat can leave the group or chat in it.

- Player are also able to add friends, see their friends list and remove friends.
- If in a game, a player can decide to render the game private.
- Player can observe ongoing public games. But also, ongoing private games if they are friends with one of the players.
- Player can also see their own on going games.

- Once a game is finished, players can see the results of the game.
- The server automatically saves the replay of each game in a file with the format "g_<game_id>.rec".
- The server on relaunch automatically retrieve the next game id, so the game ids are unique across server restarts.
- Players can request to see the replay of any finished game by providing the game id.

- The replay in interactive mode allows the user to navigate through the moves of the game using simple commands. "next", "prev", "exit".
- No other commands are available while in replay mode.

## How to Run

To offer a better user experience, ANSI escape codes are used for terminal formatting. Ensure your terminal supports them.

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

## AI Usage

For this project, AI has been used to create the base structure of the app. Helping us dividing the code 
into modules. The AI also helped us debug some parts of the code and suggested improvements.

For this, we mostly used Github Copilot with the following models:
- GPT-5 mini
- Claude sonnet 4