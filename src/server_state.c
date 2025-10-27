#include "server.h"
#include <string.h>

int server_init(server_state_t *s)
{
    if (!s)
        return -1;
    s->users_count = 0;
    for (int i = 0; i < SERVER_MAX_USERS; ++i)
    {
        s->users[i].socket_fd = -1;
        s->users[i].username[0] = '\0';
        s->users[i].bio[0] = '\0';
        s->users[i].friends_count = 0;
        s->users[i].active_games_count = 0;
    }
    s->games_count = 0;
    s->challenges_count = 0;
    for (int i = 0; i < SERVER_MAX_PENDING_CHALLENGES; ++i)
        s->challenges[i].active = false;
    s->next_game_id = 1;
    return 0;
}

int server_register_user(server_state_t *s, const char *username, int sockfd)
{
    if (!s || !username)
        return -1;
    for (int i = 0; i < SERVER_MAX_USERS; ++i)
    {
        if (s->users[i].username[0] != '\0' && strcmp(s->users[i].username, username) == 0)
        {
            if (s->users[i].socket_fd != -1 && s->users[i].socket_fd != sockfd)
                return -1;
            s->users[i].socket_fd = sockfd;
            return 0;
        }
    }
    for (int i = 0; i < SERVER_MAX_USERS; ++i)
    {
        if (s->users[i].username[0] == '\0')
        {
            strncpy(s->users[i].username, username, GAME_MAX_USERNAME - 1);
            s->users[i].username[GAME_MAX_USERNAME - 1] = '\0';
            s->users[i].socket_fd = sockfd;
            s->users_count++;
            return 0;
        }
    }
    return -1;
}

int server_unregister_user(server_state_t *s, const char *username)
{
    if (!s || !username)
        return -1;
    for (int i = 0; i < SERVER_MAX_USERS; ++i)
    {
        if (s->users[i].username[0] != '\0' && strcmp(s->users[i].username, username) == 0)
        {
            s->users[i].username[0] = '\0';
            s->users[i].socket_fd = -1;
            if (s->users_count > 0)
                s->users_count--;
            return 0;
        }
    }
    return -1;
}

int server_list_online_users(server_state_t *s, char dest[][GAME_MAX_USERNAME], int max)
{
    if (!s || !dest || max <= 0)
        return 0;
    int n = 0;
    for (int i = 0; i < SERVER_MAX_USERS && n < max; ++i)
    {
        if (s->users[i].username[0] != '\0' && s->users[i].socket_fd != -1)
        {
            strncpy(dest[n], s->users[i].username, GAME_MAX_USERNAME - 1);
            dest[n][GAME_MAX_USERNAME - 1] = '\0';
            n++;
        }
    }
    return n;
}

int server_set_user_bio(server_state_t *s, const char *username, const char *bio_text)
{
    if (!s || !username || !bio_text)
        return -1;
    for (int i = 0; i < SERVER_MAX_USERS; ++i)
    {
        if (s->users[i].username[0] != '\0' && strcmp(s->users[i].username, username) == 0)
        {
            strncpy(s->users[i].bio, bio_text, CLIENT_MAX_BIO - 1);
            s->users[i].bio[CLIENT_MAX_BIO - 1] = '\0';
            return 0;
        }
    }
    return -1;
}

int server_show_user_bio(server_state_t *s, const char *requester, const char *target_username, char *out_bio, size_t n)
{
    (void)requester;
    if (!s || !target_username || !out_bio || n == 0)
        return -1;
    for (int i = 0; i < SERVER_MAX_USERS; ++i)
    {
        if (s->users[i].username[0] != '\0' && strcmp(s->users[i].username, target_username) == 0)
        {
            strncpy(out_bio, s->users[i].bio, n - 1);
            out_bio[n - 1] = '\0';
            return 0;
        }
    }
    return -1;
}

int server_create_challenge(server_state_t *s, const char *from, const char *to)
{
    if (!s || !from || !to)
        return -1;
    for (int i = 0; i < SERVER_MAX_PENDING_CHALLENGES; ++i)
    {
        if (!s->challenges[i].active)
        {
            strncpy(s->challenges[i].from, from, GAME_MAX_USERNAME - 1);
            s->challenges[i].from[GAME_MAX_USERNAME - 1] = '\0';
            strncpy(s->challenges[i].to, to, GAME_MAX_USERNAME - 1);
            s->challenges[i].to[GAME_MAX_USERNAME - 1] = '\0';
            s->challenges[i].active = true;
            s->challenges_count++;
            return 0;
        }
    }
    return -1;
}

int server_cancel_challenge(server_state_t *s, const char *from, const char *to)
{
    if (!s || !from || !to)
        return -1;
    for (int i = 0; i < SERVER_MAX_PENDING_CHALLENGES; ++i)
    {
        if (s->challenges[i].active && strcmp(s->challenges[i].from, from) == 0 && strcmp(s->challenges[i].to, to) == 0)
        {
            s->challenges[i].active = false;
            if (s->challenges_count > 0)
                s->challenges_count--;
            return 0;
        }
    }
    return -1;
}

int server_accept_challenge(server_state_t *s, const char *from, const char *to, uint64_t *out_game_id)
{
    if (!s || !from || !to)
        return -1;
    for (int i = 0; i < SERVER_MAX_PENDING_CHALLENGES; ++i)
    {
        if (s->challenges[i].active && strcmp(s->challenges[i].from, from) == 0 && strcmp(s->challenges[i].to, to) == 0)
        {
            s->challenges[i].active = false;
            if (s->challenges_count > 0)
                s->challenges_count--;
            if (out_game_id)
                *out_game_id = s->next_game_id++;
            return 0;
        }
    }
    return -1;
}

int server_refuse_challenge(server_state_t *s, const char *from, const char *to)
{
    return server_cancel_challenge(s, from, to);
}

// Stubs for future game management
int server_create_game_from_challenge(server_state_t *s, const char *player_a, const char *player_b, uint64_t *out_game_id)
{
    if (!s || !player_a || !player_b) return -1;
    if (s->games_count >= SERVER_MAX_GAMES) return -1;
    game_t *g = &s->games[s->games_count];
    game_init(g, player_a, player_b);
    g->id = s->next_game_id++;
    if (out_game_id) *out_game_id = g->id;
    s->games_count++;
    return 0;
}

int server_get_game(server_state_t *s, uint64_t game_id, game_t **out)
{
    if (!s || !out) return -1;
    for (int i = 0; i < s->games_count; ++i)
    {
        if (s->games[i].id == game_id)
        {
            *out = &s->games[i];
            return 0;
        }
    }
    return -1;
}

int server_remove_game(server_state_t *s, uint64_t game_id)
{
    if (!s) return -1;
    for (int i = 0; i < s->games_count; ++i)
    {
        if (s->games[i].id == game_id)
        {
            // compact by swapping last
            s->games[i] = s->games[s->games_count - 1];
            s->games_count--;
            return 0;
        }
    }
    return -1;
}
