#ifndef TVN_AUTH_H
#define TVN_AUTH_H

#include <stdint.h>

#define MAX_USERS      16
#define USER_NAME_MAX  32
#define PASS_HASH_MAX  64
#define MAX_GROUPS     8
#define GROUP_NAME_MAX 32

typedef struct {
    uint32_t uid;
    uint32_t gid;
    char name[USER_NAME_MAX];
    char pass_hash[PASS_HASH_MAX];
    char home_dir[64];
    char shell[32];
    uint32_t groups[MAX_GROUPS];
    int ngroups;
    int locked;
} user_t;

typedef struct {
    uint32_t gid;
    char name[GROUP_NAME_MAX];
    uint32_t members[MAX_USERS];
    int nmembers;
} group_t;

void auth_init(void);
int auth_login(const char *username, const char *password);
int auth_logout(void);
int auth_check_password(const char *username, const char *password);
uint32_t auth_get_uid(void);
uint32_t auth_get_gid(void);
const char *auth_get_username(void);
uint32_t auth_get_euid(void);
uint32_t auth_get_egid(void);
int auth_set_user(uint32_t uid, uint32_t gid, const char *name);

int user_add(const char *name, const char *password, uint32_t uid, uint32_t gid);
int user_del(const char *name);
int user_set_password(const char *name, const char *new_password);
user_t *user_find(const char *name);
user_t *user_find_by_uid(uint32_t uid);
user_t *user_get(int index);
int user_count(void);

int group_add(const char *name, uint32_t gid);
int group_add_member(const char *group, const char *user);

uint32_t auth_next_uid(void);
uint32_t auth_next_gid(void);

#endif
