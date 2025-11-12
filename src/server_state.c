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
    s->groups_count = 0;
    for (int i = 0; i < SERVER_MAX_GROUPS; ++i)
        s->groups[i].active = false;
    s->next_game_id = 1;
    return 0;
}

int server_register_user(server_state_t *s, const char *username, int sockfd)
{
    if (!s || !username)
        return -1;
    int first_empty = -1;
    for (int i = 0; i < SERVER_MAX_USERS; ++i)
    {
        if (s->users[i].username[0] != '\0')
        {
            if (strcmp(s->users[i].username, username) == 0)
            {
                if (s->users[i].socket_fd != -1 && s->users[i].socket_fd != sockfd)
                    return -1;
                s->users[i].socket_fd = sockfd;
                return 0;
            }
        }
        else if (first_empty == -1)
        {
            first_empty = i;
        }
    }
    if (first_empty != -1)
    {
           user_profile_t *u = &s->users[first_empty];
           // Clear the entire struct, then fill required fields
           memset(u, 0, sizeof(*u));
           strncpy(u->username, username, GAME_MAX_USERNAME - 1);
           u->username[GAME_MAX_USERNAME - 1] = '\0';
           u->socket_fd = sockfd;
           u->bio[0] = '\0';
           u->friends_count = 0;
           for (int j = 0; j < CLIENT_MAX_FRIENDS; ++j) u->friends[j][0] = '\0';
           u->active_games_count = 0;
           s->users_count++;
           return 0;
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
                // Reset entire user slot to default empty state
                memset(&s->users[i], 0, sizeof(s->users[i]));
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
            // Unescape any literal "\\n" sequences into actual newlines
            size_t src_i = 0, dst_i = 0;
            while (bio_text[src_i] != '\0' && dst_i + 1 < CLIENT_MAX_BIO) {
                if (bio_text[src_i] == '\\' && bio_text[src_i+1] == 'n') {
                    s->users[i].bio[dst_i++] = '\n';
                    src_i += 2;
                } else {
                    s->users[i].bio[dst_i++] = bio_text[src_i++];
                }
            }
            s->users[i].bio[dst_i] = '\0';
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
                *out_game_id = s->next_game_id;
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
    if (!s || !player_a || !player_b)
        return -1;
    if (s->games_count >= SERVER_MAX_GAMES)
        return -1;
    game_t *g = &s->games[s->games_count];
    game_init(g, player_a, player_b);
    g->id = s->next_game_id++;
    if (out_game_id)
        *out_game_id = g->id;
    s->games_count++;
    return 0;
}

int server_get_game(server_state_t *s, uint64_t game_id, game_t **out)
{
    if (!s || !out)
        return -1;
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
    if (!s)
        return -1;
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

static int find_group(server_state_t *s, const char *name)
{
    if (!s || !name)
        return -1;
    for (int i = 0; i < SERVER_MAX_GROUPS; ++i)
        if (s->groups[i].active && strcmp(s->groups[i].name, name) == 0)
            return i;
    return -1;
}

int server_group_create(server_state_t *s, const char *owner, const char *group_name)
{
    if (!s || !owner || !group_name)
        return -1;
    if (find_group(s, group_name) >= 0)
        return -1;
    for (int i = 0; i < SERVER_MAX_GROUPS; ++i)
    {
        if (!s->groups[i].active)
        {
            s->groups[i].active = true;
            strncpy(s->groups[i].name, group_name, GAME_MAX_USERNAME - 1);
            s->groups[i].name[GAME_MAX_USERNAME - 1] = '\0';
            strncpy(s->groups[i].owner, owner, GAME_MAX_USERNAME - 1);
            s->groups[i].owner[GAME_MAX_USERNAME - 1] = '\0';
            s->groups[i].member_count = 0;
            strncpy(s->groups[i].members[s->groups[i].member_count++], owner, GAME_MAX_USERNAME - 1);
            s->groups_count++;
            return 0;
        }
    }
    return -1;
}

int server_group_is_member(server_state_t *s, const char *group_name, const char *username)
{
    int gi = find_group(s, group_name);
    if (gi < 0)
        return 0;
    for (int i = 0; i < s->groups[gi].member_count; ++i)
        if (strcmp(s->groups[gi].members[i], username) == 0)
            return 1;
    return 0;
}

int server_group_invite(server_state_t *s, const char *owner, const char *group_name, const char *username)
{
    int gi = find_group(s, group_name);
    if (gi < 0)
        return -1;
    if (strcmp(s->groups[gi].owner, owner) != 0)
        return -1;
    if (s->groups[gi].member_count >= SERVER_MAX_GROUP_MEMBERS)
        return -1;
    if (server_group_is_member(s, group_name, username))
        return 0;
    strncpy(s->groups[gi].members[s->groups[gi].member_count++], username, GAME_MAX_USERNAME - 1);
    return 0;
}

int server_group_quit(server_state_t *s, const char *group_name, const char *username)
{
    int gi = find_group(s, group_name);
    if (gi < 0)
        return -1;
    for (int i = 0; i < s->groups[gi].member_count; ++i)
    {
        if (strcmp(s->groups[gi].members[i], username) == 0)
        {
            s->groups[gi].members[i][0] = '\0';
            // compact
            for (int j = i; j < s->groups[gi].member_count - 1; ++j)
                strncpy(s->groups[gi].members[j], s->groups[gi].members[j + 1], GAME_MAX_USERNAME);
            s->groups[gi].member_count--;
            // if owner leaves or last member, deactivate group
            if (s->groups[gi].member_count == 0 || strcmp(username, s->groups[gi].owner) == 0)
            {
                s->groups[gi].active = false;
                if (s->groups_count > 0)
                    s->groups_count--;
            }
            return 0;
        }
    }
    return -1;
}

// ---------------------- Friends management ----------------------
static int find_user_index(server_state_t *s, const char *username)
{
    if (!s || !username) return -1;
    int seen_used = 0;
    for (int i = 0; i < SERVER_MAX_USERS; ++i)
    {
        if (s->users[i].username[0] != '\0')
        {
            ++seen_used;
            if (strcmp(s->users[i].username, username) == 0)
                return i;
            if (seen_used >= s->users_count) break;
        }
    }
    return -1;
}

int server_mark_user_offline(server_state_t *s, const char *username)
{
    if (!s || !username) return -1;
    int idx = find_user_index(s, username);
    if (idx < 0) return -1;
    s->users[idx].socket_fd = -1;
    return 0;
}

int server_friend_add(server_state_t *s, const char *owner, const char *friend_username)
{
    if (!s || !owner || !friend_username) return -1;
    if (strcmp(owner, friend_username) == 0) return -1;
    int oi = find_user_index(s, owner);
    int fi = find_user_index(s, friend_username);
    if (oi < 0 || fi < 0) return -1;
    user_profile_t *up = &s->users[oi];
    if (up->friends_count >= CLIENT_MAX_FRIENDS) return -1;
    for (int i = 0; i < up->friends_count; ++i)
        if (strcmp(up->friends[i], friend_username) == 0)
            return 0; // already friend, idempotent
    strncpy(up->friends[up->friends_count], friend_username, GAME_MAX_USERNAME-1);
    up->friends[up->friends_count][GAME_MAX_USERNAME-1] = '\0';
    up->friends_count++;
    return 0;
}

int server_friend_remove(server_state_t *s, const char *owner, const char *friend_username)
{
    if (!s || !owner || !friend_username) return -1;
    int oi = find_user_index(s, owner);
    if (oi < 0) return -1;
    user_profile_t *up = &s->users[oi];
    for (int i = 0; i < up->friends_count; ++i)
    {
        if (strcmp(up->friends[i], friend_username) == 0)
        {
            if (i < up->friends_count - 1)
                memmove(up->friends + i, up->friends + i + 1, (size_t)(up->friends_count - i - 1) * sizeof(up->friends[0]));
            up->friends_count--;
            if (up->friends_count >= 0)
                up->friends[up->friends_count][0] = '\0';
            return 0;
        }
    }
    return -1;
}

int server_friend_list(server_state_t *s, const char *owner, char dest[][GAME_MAX_USERNAME], int max)
{
    if (!s || !owner || !dest || max <= 0) return 0;
    int oi = find_user_index(s, owner);
    if (oi < 0) return 0;
    user_profile_t *up = &s->users[oi];
    int n = up->friends_count < max ? up->friends_count : max;
    for (int i = 0; i < n; ++i)
    {
        strncpy(dest[i], up->friends[i], GAME_MAX_USERNAME-1);
        dest[i][GAME_MAX_USERNAME-1] = '\0';
    }
    return n;
}
