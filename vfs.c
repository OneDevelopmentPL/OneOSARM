/* OneOS-ARM In-Memory Filesystem Implementation */

#include "vfs.h"
#include "string.h"

static vfs_node_t nodes[VFS_MAX_FILES];

void vfs_init(void)
{
    /* Clear all nodes */
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        nodes[i].used = 0;
    }
    
    /* Create root directory at index 0 */
    nodes[0].used = 1;
    nodes[0].type = VFS_TYPE_DIR;
    nodes[0].parent = -1;
    nodes[0].size = 0;
    nodes[0].name[0] = '/';
    nodes[0].name[1] = '\0';
    
    /* Create some default files */
    int docs = vfs_create("Documents", 0, VFS_TYPE_DIR);
    int sys  = vfs_create("System", 0, VFS_TYPE_DIR);
    
    int readme = vfs_create("readme.txt", 0, VFS_TYPE_FILE);
    if (readme >= 0) {
        const char *txt = "Welcome to OneOS-ARM!\nThis is a simple operating system.";
        vfs_write(readme, txt, strlen(txt));
    }
    
    int hello = vfs_create("hello.txt", docs, VFS_TYPE_FILE);
    if (hello >= 0) {
        const char *txt = "Hello from Documents!";
        vfs_write(hello, txt, strlen(txt));
    }
    
    int cfg = vfs_create("config.sys", sys, VFS_TYPE_FILE);
    if (cfg >= 0) {
        const char *txt = "resolution=1024x768\nbpp=32";
        vfs_write(cfg, txt, strlen(txt));
    }
}

int vfs_create(const char *name, int parent, int type)
{
    /* Find a free slot */
    for (int i = 1; i < VFS_MAX_FILES; i++) {
        if (!nodes[i].used) {
            nodes[i].used = 1;
            nodes[i].type = type;
            nodes[i].parent = parent;
            nodes[i].size = 0;
            nodes[i].data[0] = '\0';
            
            /* Copy name */
            int j = 0;
            while (name[j] && j < VFS_MAX_NAME - 1) {
                nodes[i].name[j] = name[j];
                j++;
            }
            nodes[i].name[j] = '\0';
            return i;
        }
    }
    return -1;
}

int vfs_write(int index, const char *data, int len)
{
    if (index < 0 || index >= VFS_MAX_FILES || !nodes[index].used)
        return -1;
    if (nodes[index].type != VFS_TYPE_FILE)
        return -1;
    
    if (len > VFS_MAX_CONTENT - 1)
        len = VFS_MAX_CONTENT - 1;
    
    for (int i = 0; i < len; i++) {
        nodes[index].data[i] = data[i];
    }
    nodes[index].data[len] = '\0';
    nodes[index].size = len;
    return len;
}

int vfs_read(int index, char *buf, int max_len)
{
    if (index < 0 || index >= VFS_MAX_FILES || !nodes[index].used)
        return -1;
    
    int len = nodes[index].size;
    if (len > max_len - 1)
        len = max_len - 1;
    
    for (int i = 0; i < len; i++) {
        buf[i] = nodes[index].data[i];
    }
    buf[len] = '\0';
    return len;
}

int vfs_list(int parent, int *indices, int max)
{
    int count = 0;
    for (int i = 0; i < VFS_MAX_FILES && count < max; i++) {
        if (nodes[i].used && nodes[i].parent == parent) {
            indices[count++] = i;
        }
    }
    return count;
}

int vfs_find(const char *name, int parent)
{
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (nodes[i].used && nodes[i].parent == parent) {
            if (strcmp(nodes[i].name, name) == 0) {
                return i;
            }
        }
    }
    return -1;
}

int vfs_delete(int index)
{
    if (index <= 0 || index >= VFS_MAX_FILES || !nodes[index].used)
        return -1;
    nodes[index].used = 0;
    return 0;
}

int vfs_rename(int index, const char *new_name)
{
    if (index <= 0 || index >= VFS_MAX_FILES || !nodes[index].used)
        return -1;
    
    int j = 0;
    while (new_name[j] && j < VFS_MAX_NAME - 1) {
        nodes[index].name[j] = new_name[j];
        j++;
    }
    nodes[index].name[j] = '\0';
    return 0;
}

vfs_node_t* vfs_get(int index)
{
    if (index < 0 || index >= VFS_MAX_FILES || !nodes[index].used)
        return 0;
    return &nodes[index];
}

int vfs_root(void)
{
    return 0;
}
