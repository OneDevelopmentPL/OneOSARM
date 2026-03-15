/* OneOS-ARM 1 Kernel
 * Main kernel entry point
 */

#include "uart.h"
#include "mem.h"
#include "keyboard.h"
#include "string.h"
#include "terminal.h"
#include "vfs.h"
#include "gui.h"
#include "virtio_input.h"
#include "virtio_rng.h"
#include "editor.h"

/* Define basic types for bare-metal */
typedef unsigned char uint8_t;

/* Screen dimensions for mouse bounds checking */
#define SCREEN_W 1024
#define SCREEN_H 768

/* Helper: write a string to both the terminal buffer and render it */
static void term_print(const char *s)
{
    terminal_puts(s);
    terminal_render();
}

int main(void)
{
    /* Initialize UART for serial communication */
    uart_init();

    /* Initialize memory management */
    mem_init((void *)0x40200000, 0x100000);  /* 1MB heap at 0x40200000 */

    /* Initialize keyboard (UART fallback) */
    keyboard_init();

    /* Initialize virtio-input (QEMU GUI keyboard + mouse) */
    virtio_input_init();

    /* Initialize virtio-rng (Random Number Generator) */
    virtio_rng_init();

    /* Initialize terminal UI */
    terminal_init();
    terminal_clear();

    /* Initialize filesystem */
    vfs_init();

    terminal_puts("=== OneOS-ARM 1 Booting ===\n");
    terminal_puts("[OK] Memory initialized\n");
    terminal_puts("[OK] Keyboard initialized\n");
    terminal_puts("[OK] Terminal initialized\n");
    terminal_puts("[OK] Filesystem initialized\n");
    terminal_puts("\nWelcome to OneOS-ARM 1!\n");
    terminal_puts("Type 'help' for commands\n\n");
    terminal_puts("> ");
    terminal_render();

    /* Command loop */
    char cmd_buffer[256];
    int cmd_pos = 0;

    while (1) {
        /* Poll for mouse movement */
        int dx = 0, dy = 0, btn = 0;
        if (virtio_input_mouse(&dx, &dy, &btn)) {
            int new_x = terminal.mouse_x + dx;
            int new_y = terminal.mouse_y + dy;
            
            /* Clamp to screen bounds */
            if (new_x < 0) new_x = 0;
            if (new_x >= SCREEN_W) new_x = SCREEN_W - 1;
            if (new_y < 0) new_y = 0;
            if (new_y >= SCREEN_H) new_y = SCREEN_H - 1;
            
            terminal.mouse_x = new_x;
            terminal.mouse_y = new_y;
            terminal_render();
        }
        
        uint8_t key = virtio_input_getchar();
        if (!key) key = keyboard_read_nonblock();  /* UART fallback */

        if (key) {
            if (key == '\r' || key == '\n') {
                cmd_buffer[cmd_pos] = '\0';
                terminal_putchar('\n');

                if (cmd_pos > 0) {
                    if (strcmp(cmd_buffer, "help") == 0) {
                        terminal_puts("Commands:\n");
                        terminal_puts("  help     - Show this help\n");
                        terminal_puts("  mem      - Show memory info\n");
                        terminal_puts("  clear    - Clear screen\n");
                        terminal_puts("  test     - Run memory test\n");
                        terminal_puts("  ls       - List files\n");
                        terminal_puts("  cat <f>  - Show file contents\n");
                        terminal_puts("  touch <f>- Create file\n");
                        terminal_puts("  ste <f>  - Edit text file\n");
                        terminal_puts("  rand     - Show random number\n");
                        terminal_puts("  startgui - Launch GUI\n");
                        terminal_render();
                    } else if (strcmp(cmd_buffer, "rand") == 0) {
                        unsigned int r = virtio_rng_rand();
                        terminal_puts("Random: 0x");
                        terminal_puthex(r);
                        terminal_puts("\n");
                        terminal_render();
                    } else if (strcmp(cmd_buffer, "mem") == 0) {
                        terminal_puts("Memory Info:\n");
                        terminal_puts("  Heap: 1MB\n");
                        terminal_render();
                    } else if (strcmp(cmd_buffer, "clear") == 0) {
                        terminal_clear();
                        terminal_render();
                    } else if (strcmp(cmd_buffer, "test") == 0) {
                        terminal_puts("Memory test...\n");
                        terminal_render();
                        void *p1 = kmalloc(100);
                        void *p2 = kmalloc(200);
                        if (p1 && p2) {
                            term_print("Allocation OK\n");
                            kfree(p1);
                            kfree(p2);
                            term_print("Deallocation OK\n");
                        } else {
                            term_print("Allocation FAILED\n");
                        }
                    } else if (strcmp(cmd_buffer, "ls") == 0) {
                        int indices[VFS_MAX_FILES];
                        int count = vfs_list(vfs_root(), indices, VFS_MAX_FILES);
                        for (int i = 0; i < count; i++) {
                            vfs_node_t *n = vfs_get(indices[i]);
                            if (n) {
                                if (n->type == VFS_TYPE_DIR) {
                                    terminal_puts("  [DIR]  ");
                                } else {
                                    terminal_puts("  [FILE] ");
                                }
                                terminal_puts(n->name);
                                terminal_puts("\n");
                            }
                        }
                        terminal_render();
                    } else if (cmd_buffer[0] == 'c' && cmd_buffer[1] == 'a' && cmd_buffer[2] == 't' && cmd_buffer[3] == ' ') {
                        const char *fname = cmd_buffer + 4;
                        int idx = vfs_find(fname, vfs_root());
                        if (idx >= 0) {
                            char buf[VFS_MAX_CONTENT];
                            vfs_read(idx, buf, VFS_MAX_CONTENT);
                            terminal_puts(buf);
                            terminal_puts("\n");
                        } else {
                            terminal_puts("File not found: ");
                            terminal_puts(fname);
                            terminal_puts("\n");
                        }
                        terminal_render();
                    } else if (cmd_buffer[0] == 's' && cmd_buffer[1] == 't' && cmd_buffer[2] == 'e' && cmd_buffer[3] == ' ') {
                        const char *fname = cmd_buffer + 4;
                        terminal_puts("Opening editor for: ");
                        terminal_puts(fname);
                        terminal_puts("\n");
                        terminal_render();
                        editor_open(fname);
                        terminal_puts("> ");
                        terminal_render();
                    } else if (cmd_buffer[0] == 't' && cmd_buffer[1] == 'o' && cmd_buffer[2] == 'u' &&
                               cmd_buffer[3] == 'c' && cmd_buffer[4] == 'h' && cmd_buffer[5] == ' ') {
                        const char *fname = cmd_buffer + 6;
                        int idx = vfs_create(fname, vfs_root(), VFS_TYPE_FILE);
                        if (idx >= 0) {
                            terminal_puts("Created: ");
                            terminal_puts(fname);
                            terminal_puts("\n");
                        } else {
                            terminal_puts("Error creating file\n");
                        }
                        terminal_render();
                    } else if (strcmp(cmd_buffer, "startgui") == 0) {
                        terminal_puts("Launching GUI...\n");
                        terminal_render();
                        gui_set_time(14, 26);
                        gui_init();
                        gui_run();
                        /* Return to terminal mode */
                        terminal_clear();
                        terminal_puts("Returned to terminal.\n");
                        terminal_puts("Type 'help' for commands\n\n");
                        terminal_render();
                    } else {
                        terminal_puts("Unknown: ");
                        terminal_puts(cmd_buffer);
                        terminal_puts("\n");
                        terminal_render();
                    }
                }

                cmd_pos = 0;
                terminal_puts("> ");
                terminal_render();
            } else if (key == 0x08) {
                if (cmd_pos > 0) {
                    cmd_pos--;
                    terminal_handle_input(key);
                    terminal_render();
                }
            } else if (key >= 32 && key <= 126) {
                cmd_buffer[cmd_pos++] = (char)key;
                terminal_handle_input(key);
                terminal_render();
            }
        }

        /* Small delay */
        for (volatile int i = 0; i < 5000; i++) {}
    }
}