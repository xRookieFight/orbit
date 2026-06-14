#include "fs.h"
#include "heap.h"
#include "string.h"

static fs_node_t* root;

static fs_node_t* alloc_node(const char* name, fs_type_t type, uint16_t owner)
{
    fs_node_t* node = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!node)
        return NULL;
    memset(node, 0, sizeof(fs_node_t));
    strlcpy(node->name, name, ORBIT_MAX_NAME);
    node->type = type;
    node->owner = owner;
    node->perms = (type == FS_DIR) ? 0755 : 0644;
    return node;
}

static void detach(fs_node_t* node)
{
    fs_node_t* parent = node->parent;
    if (!parent)
        return;
    if (parent->children == node) {
        parent->children = node->next;
        return;
    }
    fs_node_t* cur = parent->children;
    while (cur && cur->next != node)
        cur = cur->next;
    if (cur)
        cur->next = node->next;
}

static void free_tree(fs_node_t* node)
{
    fs_node_t* child = node->children;
    while (child) {
        fs_node_t* next = child->next;
        free_tree(child);
        child = next;
    }
    if (node->data)
        kfree(node->data);
    kfree(node);
}

static int is_descendant_of(fs_node_t* node, fs_node_t* ancestor)
{
    for (fs_node_t* cur = node; cur; cur = cur->parent)
        if (cur == ancestor)
            return 1;
    return 0;
}

fs_node_t* fs_root(void)
{
    return root;
}

fs_node_t* fs_child(fs_node_t* dir, const char* name)
{
    if (!dir || dir->type != FS_DIR)
        return NULL;
    fs_node_t* cur = dir->children;
    while (cur) {
        if (strcmp(cur->name, name) == 0)
            return cur;
        cur = cur->next;
    }
    return NULL;
}

fs_node_t* fs_resolve(fs_node_t* cwd, const char* path)
{
    fs_node_t* cur = (path[0] == '/') ? root : (cwd ? cwd : root);
    char token[ORBIT_MAX_NAME];

    while (*path) {
        while (*path == '/')
            path++;
        if (!*path)
            break;
        int i = 0;
        while (*path && *path != '/' && i < ORBIT_MAX_NAME - 1)
            token[i++] = *path++;
        token[i] = '\0';

        if (strcmp(token, ".") == 0)
            continue;
        if (strcmp(token, "..") == 0) {
            if (cur->parent)
                cur = cur->parent;
            continue;
        }
        fs_node_t* child = fs_child(cur, token);
        if (!child)
            return NULL;
        cur = child;
    }
    return cur;
}

static fs_node_t* resolve_parent(fs_node_t* cwd, const char* path, char* base_out)
{
    char tmp[ORBIT_MAX_PATH];
    strlcpy(tmp, path, sizeof(tmp));

    size_t len = strlen(tmp);
    while (len > 1 && tmp[len - 1] == '/')
        tmp[--len] = '\0';

    char* slash = strrchr(tmp, '/');
    if (!slash) {
        strlcpy(base_out, tmp, ORBIT_MAX_NAME);
        return cwd ? cwd : root;
    }
    strlcpy(base_out, slash + 1, ORBIT_MAX_NAME);
    if (slash == tmp)
        return root;
    *slash = '\0';
    return fs_resolve(cwd, tmp);
}

fs_node_t* fs_create(fs_node_t* cwd, const char* path, fs_type_t type, uint16_t owner)
{
    char base[ORBIT_MAX_NAME];
    fs_node_t* parent = resolve_parent(cwd, path, base);
    if (!parent || parent->type != FS_DIR || base[0] == '\0')
        return NULL;
    if (fs_child(parent, base))
        return NULL;

    fs_node_t* node = alloc_node(base, type, owner);
    if (!node)
        return NULL;
    node->parent = parent;
    node->next = parent->children;
    parent->children = node;
    return node;
}

fs_node_t* fs_mkdir(fs_node_t* cwd, const char* path, uint16_t owner)
{
    return fs_create(cwd, path, FS_DIR, owner);
}

int fs_remove(fs_node_t* cwd, const char* path, int recursive)
{
    fs_node_t* node = fs_resolve(cwd, path);
    if (!node || node == root)
        return -1;
    if (node->type == FS_DIR && node->children && !recursive)
        return -2;
    detach(node);
    free_tree(node);
    return 0;
}

int fs_write(fs_node_t* file, const char* data, uint32_t size)
{
    if (!file || file->type != FS_FILE)
        return -1;
    if (file->data)
        kfree(file->data);
    file->data = (char*)kmalloc(size ? size : 1);
    if (!file->data) {
        file->size = 0;
        return -1;
    }
    memcpy(file->data, data, size);
    file->size = size;
    return 0;
}

int fs_append(fs_node_t* file, const char* data, uint32_t size)
{
    if (!file || file->type != FS_FILE)
        return -1;
    char* buffer = (char*)kmalloc(file->size + size + 1);
    if (!buffer)
        return -1;
    if (file->data)
        memcpy(buffer, file->data, file->size);
    memcpy(buffer + file->size, data, size);
    if (file->data)
        kfree(file->data);
    file->data = buffer;
    file->size += size;
    return 0;
}

int fs_move(fs_node_t* cwd, const char* src, const char* dst)
{
    fs_node_t* source = fs_resolve(cwd, src);
    if (!source || source == root)
        return -1;

    char base[ORBIT_MAX_NAME];
    fs_node_t* target = fs_resolve(cwd, dst);
    fs_node_t* parent;
    if (target && target->type == FS_DIR) {
        parent = target;
        strlcpy(base, source->name, ORBIT_MAX_NAME);
    } else {
        parent = resolve_parent(cwd, dst, base);
    }
    if (!parent || parent->type != FS_DIR || fs_child(parent, base))
        return -1;

    detach(source);
    strlcpy(source->name, base, ORBIT_MAX_NAME);
    source->parent = parent;
    source->next = parent->children;
    parent->children = source;
    return 0;
}

fs_node_t* fs_copy(fs_node_t* cwd, const char* src, const char* dst, uint16_t owner)
{
    fs_node_t* source = fs_resolve(cwd, src);
    if (!source || source->type != FS_FILE)
        return NULL;

    char base[ORBIT_MAX_NAME];
    fs_node_t* target = fs_resolve(cwd, dst);
    fs_node_t* parent;
    if (target && target->type == FS_DIR) {
        parent = target;
        strlcpy(base, source->name, ORBIT_MAX_NAME);
    } else {
        parent = resolve_parent(cwd, dst, base);
    }
    if (!parent || parent->type != FS_DIR || fs_child(parent, base))
        return NULL;

    fs_node_t* node = alloc_node(base, FS_FILE, owner);
    if (!node)
        return NULL;
    node->parent = parent;
    fs_write(node, source->data, source->size);
    node->next = parent->children;
    parent->children = node;
    return node;
}

void fs_abspath(fs_node_t* node, char* buf, size_t size)
{
    if (!node || node == root) {
        strlcpy(buf, "/", size);
        return;
    }

    char stack[32][ORBIT_MAX_NAME];
    int n = 0;
    fs_node_t* cur = node;
    while (cur && cur != root && n < 32) {
        strlcpy(stack[n++], cur->name, ORBIT_MAX_NAME);
        cur = cur->parent;
    }

    size_t pos = 0;
    for (int i = n - 1; i >= 0; i--) {
        if (pos + 1 < size)
            buf[pos++] = '/';
        const char* s = stack[i];
        while (*s && pos + 1 < size)
            buf[pos++] = *s++;
    }
    if (pos == 0 && size > 0)
        buf[pos++] = '/';
    buf[pos < size ? pos : size - 1] = '\0';
}

void fs_init(void)
{
    root = alloc_node("/", FS_DIR, 0);

    fs_mkdir(root, "/bin", 0);
    fs_mkdir(root, "/etc", 0);
    fs_mkdir(root, "/dev", 0);
    fs_mkdir(root, "/tmp", 0);
    fs_mkdir(root, "/home", 0);
    fs_mkdir(root, "/var", 0);
    fs_mkdir(root, "/var/log", 0);
    fs_mkdir(root, "/home/root", 0);

    fs_node_t* release = fs_create(root, "/etc/orbit-release", FS_FILE, 0);
    if (release)
        fs_write(release, "Orbit 1.0\n", 10);

    fs_node_t* motd = fs_create(root, "/etc/motd", FS_FILE, 0);
    if (motd) {
        const char* m = "Welcome to Orbit 1.0 - CLI modular operating system\n";
        fs_write(motd, m, (uint32_t)strlen(m));
    }

    fs_node_t* readme = fs_create(root, "/home/root/readme.txt", FS_FILE, 0);
    if (readme) {
        const char* r = "Orbit 1.0 root home.\nType help to list commands.\n";
        fs_write(readme, r, (uint32_t)strlen(r));
    }
}
