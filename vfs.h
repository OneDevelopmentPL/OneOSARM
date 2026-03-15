/* OneOS-ARM In-Memory Filesystem */

#ifndef VFS_H
#define VFS_H

typedef unsigned int size_t;

#define VFS_MAX_FILES    32
#define VFS_MAX_NAME     32
#define VFS_MAX_CONTENT  4096

#define VFS_TYPE_FILE    0
#define VFS_TYPE_DIR     1

typedef struct {
    char name[VFS_MAX_NAME];
    int type;           /* VFS_TYPE_FILE or VFS_TYPE_DIR */
    int parent;         /* Index of parent dir (-1 for root) */
    char data[VFS_MAX_CONTENT];
    int size;           /* Size of data in bytes */
    int used;           /* 1 if this slot is in use */
} vfs_node_t;

/* Initialize the filesystem with default structure */
void vfs_init(void);

/* Create a file or directory. Returns index or -1 on error */
int vfs_create(const char *name, int parent, int type);

/* Write data to a file */
int vfs_write(int index, const char *data, int len);

/* Read data from a file. Returns bytes read */
int vfs_read(int index, char *buf, int max_len);

/* List files in a directory. Returns count */
int vfs_list(int parent, int *indices, int max);

/* Find a file by name in a directory. Returns index or -1 */
int vfs_find(const char *name, int parent);

/* Delete a file */
int vfs_delete(int index);

/* Rename a file */
int vfs_rename(int index, const char *new_name);

/* Get node info */
vfs_node_t* vfs_get(int index);

/* Get root directory index */
int vfs_root(void);

#endif
