#pragma once
#include <stdint.h>
#include <stddef.h>
#include "orbit.h"

typedef enum {
    FS_DIR,
    FS_FILE
} fs_type_t;

typedef struct fs_node {
    char name[ORBIT_MAX_NAME];
    fs_type_t type;
    uint16_t perms;
    uint16_t owner;
    struct fs_node* parent;
    struct fs_node* children;
    struct fs_node* next;
    char* data;
    uint32_t size;
} fs_node_t;

void fs_init(void);
fs_node_t* fs_root(void);
fs_node_t* fs_child(fs_node_t* dir, const char* name);
fs_node_t* fs_resolve(fs_node_t* cwd, const char* path);
fs_node_t* fs_create(fs_node_t* cwd, const char* path, fs_type_t type, uint16_t owner);
fs_node_t* fs_mkdir(fs_node_t* cwd, const char* path, uint16_t owner);
int fs_remove(fs_node_t* cwd, const char* path, int recursive);
int fs_write(fs_node_t* file, const char* data, uint32_t size);
int fs_append(fs_node_t* file, const char* data, uint32_t size);
int fs_move(fs_node_t* cwd, const char* src, const char* dst);
fs_node_t* fs_copy(fs_node_t* cwd, const char* src, const char* dst, uint16_t owner);
void fs_abspath(fs_node_t* node, char* buf, size_t size);
