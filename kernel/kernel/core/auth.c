#include "auth.h"
#include "string.h"
#include "printk.h"
#include "klog.h"

static user_t users[MAX_USERS];
static int nusers = 0;
static group_t groups[MAX_USERS];
static int ngroups = 0;

static uint32_t current_uid = 0;
static uint32_t current_gid = 0;
static uint32_t current_euid = 0;
static uint32_t current_egid = 0;
static char current_username[USER_NAME_MAX] = "root";
static int logged_in = 0;

static uint32_t hash_password(const char *password) {
    uint32_t hash = 5381;
    int c;
    while ((c = *password++)) {
        hash = ((hash << 5) + hash) + c;
        hash ^= (hash >> 16);
        hash *= 0x9E3779B9;
    }
    return hash;
}

static void hash_to_str(uint32_t h, char *out) {
    const char *hex = "0123456789abcdef";
    for (int i = 0; i < 8; i++) {
        out[i] = hex[(h >> (28 - i * 4)) & 0xF];
    }
    out[8] = 0;
}

static int str_to_hash(const char *s, uint32_t *out) {
    if (!s || strlen(s) < 8) return -1;
    *out = 0;
    for (int i = 0; i < 8; i++) {
        *out <<= 4;
        if (s[i] >= '0' && s[i] <= '9') *out |= (s[i] - '0');
        else if (s[i] >= 'a' && s[i] <= 'f') *out |= (s[i] - 'a' + 10);
        else return -1;
    }
    return 0;
}

void auth_init(void) {
    memset(users, 0, sizeof(users));
    memset(groups, 0, sizeof(groups));
    nusers = 0;
    ngroups = 0;

    user_add("root", "toor", 0, 0);
    user_add("admin", "admin", 1, 1);
    user_add("user", "user", 1000, 1000);

    group_add("root", 0);
    group_add_member("root", "root");
    group_add("admin", 1);
    group_add_member("admin", "admin");
    group_add("users", 1000);
    group_add_member("users", "user");

    current_uid = 0;
    current_gid = 0;
    current_euid = 0;
    current_egid = 0;
    strcpy(current_username, "root");
    logged_in = 1;

    printk("AUTH: System initialized with %d users, %d groups", nusers, ngroups);
    klog_write("AUTH: System initialized with %d users, %d groups\n", nusers, ngroups);
}

int auth_login(const char *username, const char *password) {
    if (!username || !password) return -1;

    if (auth_check_password(username, password) != 0) return -1;

    user_t *u = user_find(username);
    if (!u) return -1;

    current_uid = u->uid;
    current_gid = u->gid;
    current_euid = u->uid;
    current_egid = u->gid;
    strncpy(current_username, u->name, USER_NAME_MAX - 1);
    logged_in = 1;

    klog_write("AUTH: User '%s' (uid=%d) logged in\n", username, u->uid);
    return 0;
}

int auth_logout(void) {
    if (!logged_in) return -1;
    logged_in = 0;
    current_uid = 0;
    current_gid = 0;
    current_euid = 0;
    current_egid = 0;
    strcpy(current_username, "nobody");
    klog_write("AUTH: User logged out\n");
    return 0;
}

int auth_check_password(const char *username, const char *password) {
    user_t *u = user_find(username);
    if (!u) return -1;
    if (u->locked) return -1;

    uint32_t expected_hash;
    if (str_to_hash(u->pass_hash, &expected_hash) < 0) return -1;

    uint32_t input_hash = hash_password(password);
    return (input_hash == expected_hash) ? 0 : -1;
}

uint32_t auth_get_uid(void) { return current_uid; }
uint32_t auth_get_gid(void) { return current_gid; }
const char *auth_get_username(void) { return current_username; }
uint32_t auth_get_euid(void) { return current_euid; }
uint32_t auth_get_egid(void) { return current_egid; }

int auth_set_user(uint32_t uid, uint32_t gid, const char *name) {
    current_uid = uid;
    current_gid = gid;
    current_euid = uid;
    current_egid = gid;
    if (name) strncpy(current_username, name, USER_NAME_MAX - 1);
    return 0;
}

int user_add(const char *name, const char *password, uint32_t uid, uint32_t gid) {
    if (!name || !password || nusers >= MAX_USERS) return -1;
    if (user_find(name)) return -1;

    user_t *u = &users[nusers];
    memset(u, 0, sizeof(user_t));
    strncpy(u->name, name, USER_NAME_MAX - 1);
    u->uid = uid;
    u->gid = gid;
    u->locked = 0;

    uint32_t h = hash_password(password);
    hash_to_str(h, u->pass_hash);

    strcpy(u->home_dir, "/home/");
    strcat(u->home_dir, name);
    strcpy(u->shell, "/bin/sh");

    nusers++;
    klog_write("AUTH: Added user '%s' (uid=%d, gid=%d)\n", name, uid, gid);
    return 0;
}

int user_del(const char *name) {
    if (!name) return -1;
    for (int i = 0; i < nusers; i++) {
        if (strcmp(users[i].name, name) == 0) {
            for (int j = i; j < nusers - 1; j++) users[j] = users[j + 1];
            nusers--;
            klog_write("AUTH: Deleted user '%s'\n", name);
            return 0;
        }
    }
    return -1;
}

int user_set_password(const char *name, const char *new_password) {
    user_t *u = user_find(name);
    if (!u || !new_password) return -1;

    uint32_t h = hash_password(new_password);
    hash_to_str(h, u->pass_hash);
    klog_write("AUTH: Password changed for user '%s'\n", name);
    return 0;
}

user_t *user_find(const char *name) {
    if (!name) return 0;
    for (int i = 0; i < nusers; i++) {
        if (strcmp(users[i].name, name) == 0) return &users[i];
    }
    return 0;
}

user_t *user_find_by_uid(uint32_t uid) {
    for (int i = 0; i < nusers; i++) {
        if (users[i].uid == uid) return &users[i];
    }
    return 0;
}

user_t *user_get(int index) {
    if (index < 0 || index >= nusers) return 0;
    return &users[index];
}

int user_count(void) { return nusers; }

int group_add(const char *name, uint32_t gid) {
    if (!name || ngroups >= MAX_USERS) return -1;
    group_t *g = &groups[ngroups];
    memset(g, 0, sizeof(group_t));
    strncpy(g->name, name, GROUP_NAME_MAX - 1);
    g->gid = gid;
    ngroups++;
    return 0;
}

int group_add_member(const char *group_name, const char *user_name) {
    if (!group_name || !user_name) return -1;
    for (int i = 0; i < ngroups; i++) {
        if (strcmp(groups[i].name, group_name) == 0) {
            group_t *g = &groups[i];
            user_t *u = user_find(user_name);
            if (!u) return -1;
            if (g->nmembers >= MAX_USERS) return -1;
            g->members[g->nmembers++] = u->uid;
            return 0;
        }
    }
    return -1;
}

uint32_t auth_next_uid(void) {
    uint32_t max = 0;
    for (int i = 0; i < nusers; i++) {
        if (users[i].uid > max) max = users[i].uid;
    }
    return max + 1;
}

uint32_t auth_next_gid(void) {
    uint32_t max = 0;
    for (int i = 0; i < ngroups; i++) {
        if (groups[i].gid > max) max = groups[i].gid;
    }
    return max + 1;
}
