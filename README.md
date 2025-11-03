# Programmation Réseaux - Awalé

### Project checklist

Question for teacher:
- Does the bio needs to be written multi-lined or can it be single-lined ?

#### Step 0: Core Game Implementation

- [X] Internal representation of the board (`src/game.h`, `src/game.c`)
- [X] Rules for making a move (`game_is_move_legal`, `game_make_move` in `src/game.c`)
- [X] Counting points (simple capture logic implemented in `src/game.c`)

- [ ] Saving a game (basic append function present: `persist_append_game` in `src/persist.c`, but load/list functions are declared only)

- [X] Printing the board state (`game_print` in `src/game.c`)

#### Step 1: Basic Client/Server Setup

- [X] Server handles multiple clients (simple select-based server in `src/server.c`)
- [X] Clients can register with a username (`REGISTER` handling in `src/server.c` and `client_send_register` in `src/client.c`)
- [X] Clients can request the list of online usernames (`LIST_USERS` handling in `src/server.c` and `proto_build_list_users`)

#### Step 2: Game Matching

- [X] Client can challenge another client (`CHALLENGE` handling + server pending-challenge store in `src/server.c`)
- [X] Client can accept or refuse (`ACCEPT` / `REFUSE` handling in `src/server.c`)

- [X] Match creation on accept: server generates a game id when accepting a challenge (`server_accept_challenge` returns a game id) but full game creation and wiring into `server_state_t.games` is not implemented

- [X] Server decides who starts randomly (game_init in `src/game.c` uses random start)

- [X] Server verifies move legality: game logic exists, but server does not yet apply/verify moves inside a stored game (no `server_create_game_from_challenge`/`server_get_game` implementations found)

#### Step 3: Multiple Games & Observers

- [X] Multiple simultaneous games: server data structures exist (`server_state_t.games[]`), but management functions are only declared in `src/server.h` and not implemented in `src/server.c`

- [ ] Listing ongoing games: not implemented
- [ ] Observer mode (watching games): not implemented (observer fields exist in `game_t` but server-side delivery not implemented)

#### Step 4: Chat System

- [X] Basic chat protocol

#### Step 5: Player Profiles

- [X] Bio storage API declared in `src/persist.h` and `client.h` contains bio field in `user_profile_t`, but server persistence and bio set/display handlers are not implemented

#### Step 6: Privacy & Friends

- [ ] Private mode and allowed spectator lists exist in `game_t` and protocol defines commands for setting private/allow/disallow, but server-side enforcement and UI are not implemented

#### Step 7: Game Persistence

- [ ] Append-only game record function implemented (`persist_append_game` in `src/persist.c`)

- [ ] Full replay storage and retrieval (list/load by id) are declared but not implemented (`persist_list_saved_games`, `persist_load_game_by_id` not implemented)

