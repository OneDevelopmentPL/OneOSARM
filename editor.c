/* OneOS-ARM Simple Text Editor Implementation */

#include "editor.h"
#include "terminal.h"
#include "keyboard.h"
#include "vfs.h"
#include "string.h"
#include "virtio_input.h"

#define EDITOR_MAX_SIZE 4096
#define EDITOR_MAX_LINES 128
#define EDITOR_LINE_WIDTH 80

typedef struct {
    char *buffer;
    int size;
    int capacity;
    int cursor_x;
    int cursor_y;
    int scroll_y;
    int modified;
} editor_state_t;

static editor_state_t editor = {0};

static void editor_display(void)
{
    terminal_clear();
    terminal_puts("=== Simple Text Editor ===\n");
    terminal_puts("Ctrl+S: Save | Ctrl+Q: Quit | Arrow keys: Navigate\n");
    terminal_puts("---\n");
    
    /* Display buffer with line numbers */
    int line = 0;
    int pos = 0;
    
    while (pos < editor.size && line < EDITOR_MAX_LINES) {
        terminal_puthex(line + 1);
        terminal_putchar(':');
        terminal_putchar(' ');
        
        /* Find end of line */
        int line_start = pos;
        while (pos < editor.size && editor.buffer[pos] != '\n') {
            pos++;
        }
        
        /* Display line */
        for (int i = line_start; i < pos; i++) {
            terminal_putchar(editor.buffer[i]);
        }
        terminal_putchar('\n');
        
        if (pos < editor.size && editor.buffer[pos] == '\n') {
            pos++;
        }
        line++;
    }
    
    terminal_puts("---\n");
    terminal_puts("Line ");
    terminal_puthex(editor.cursor_y + 1);
    terminal_puts(", Col ");
    terminal_puthex(editor.cursor_x + 1);
    if (editor.modified) {
        terminal_puts(" *");
    }
    terminal_puts("\n");
    terminal_render();
}

static void editor_save(const char *filename)
{
    /* Simple save: write buffer to file */
    int idx = vfs_find(filename, vfs_root());
    if (idx < 0) {
        idx = vfs_create(filename, vfs_root(), VFS_TYPE_FILE);
    }
    
    if (idx >= 0) {
        vfs_write(idx, editor.buffer, editor.size);
        editor.modified = 0;
        terminal_puts("File saved: ");
        terminal_puts(filename);
        terminal_puts("\n");
    } else {
        terminal_puts("Error saving file!\n");
    }
    terminal_render();
}

void editor_open(const char *filename)
{
    /* Initialize editor buffer */
    editor.buffer = (char *)0x40300000;  /* Use fixed memory address */
    editor.capacity = EDITOR_MAX_SIZE;
    editor.size = 0;
    editor.cursor_x = 0;
    editor.cursor_y = 0;
    editor.scroll_y = 0;
    editor.modified = 0;
    
    /* Try to load file from VFS */
    int idx = vfs_find(filename, vfs_root());
    if (idx >= 0) {
        vfs_node_t *node = vfs_get(idx);
        if (node && node->type == VFS_TYPE_FILE) {
            /* Read file data */
            editor.size = node->size;
            for (int i = 0; i < editor.size; i++) {
                editor.buffer[i] = node->data[i];
            }
            editor.buffer[editor.size] = '\0';
            
            /* Recalculate cursor_y based on lines */
            int lines = 0;
            int last_line_start = 0;
            for (int i = 0; i < editor.size; i++) {
                if (editor.buffer[i] == '\n') {
                    lines++;
                    last_line_start = i + 1;
                }
            }
            editor.cursor_y = lines;
            editor.cursor_x = editor.size - last_line_start;
        }
    }
    
    /* Editor loop */
    while (1) {
        editor_display();
        
        /* Poll for input */
        uint8_t key = virtio_input_getchar();
        if (!key) key = keyboard_read_nonblock();
        
        if (key) {
            if (key == 17) {  /* Ctrl+Q */
                if (editor.modified) {
                    terminal_puts("\nUnsaved changes! Ctrl+S to save, Ctrl+Q again to discard.\n");
                    terminal_render();
                    editor.modified = 0;
                    continue;
                }
                break;
            }
            else if (key == 19) {  /* Ctrl+S */
                editor_save(filename);
                continue;
            }
            else if (key == '\b') {  /* Backspace */
                if (editor.size > 0) {
                    editor.size--;
                    editor.buffer[editor.size] = '\0';
                    editor.modified = 1;
                }
            }
            else if (key == '\r' || key == '\n') {  /* Enter */
                if (editor.size < editor.capacity - 1) {
                    editor.buffer[editor.size++] = '\n';
                    editor.buffer[editor.size] = '\0';
                    editor.cursor_y++;
                    editor.cursor_x = 0;
                    editor.modified = 1;
                }
            }
            else if (key >= 32 && key < 127) {  /* Printable characters */
                if (editor.size < editor.capacity - 1) {
                    editor.buffer[editor.size++] = (char)key;
                    editor.buffer[editor.size] = '\0';
                    editor.cursor_x++;
                    if (editor.cursor_x >= EDITOR_LINE_WIDTH) {
                        editor.cursor_x = 0;
                        editor.cursor_y++;
                    }
                    editor.modified = 1;
                }
            }
        }
    }
    
    terminal_puts("Editor closed.\n");
    terminal_render();
}
