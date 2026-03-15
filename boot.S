/* OneOS-ARM 1 Boot Assembly
 * AArch64 entry point
 * Handles transition from firmware to kernel
 */

.section .text.boot

.global _start
_start:
    /* Check CPU ID - only core 0 continues, others spin */
    mrs x19, mpidr_el1
    and x19, x19, #0xFF
    cbz x19, core_zero
    
    /* Spin loop for non-zero cores */
core_spin:
    wfe
    b core_spin

core_zero:
    /* Early diagnostic: write 'B' to UART before anything else */
    mov x9, 0x09000000           /* UART0 base */
    mov w10, 0x42                /* 'B' character */
    str w10, [x9]                /* Write to UART DR */
    
    /* Set up stack pointer for core 0
     * Stack grows downward; place it well above the kernel image.
     */
    ldr x0, =_stack_top
    mov sp, x0
    
    /* Clear BSS section */
    ldr x0, =_bss_start
    ldr x1, =_bss_end
    
clear_bss:
    cmp x0, x1
    beq bss_cleared
    str xzr, [x0], #8
    b clear_bss
    
bss_cleared:
    /* Jump to kernel main() */
    bl main
    
    /* Infinite loop if main() returns */
    b .

.section .data
_stack_top:
    .quad 0x41000000    /* Stack at 16MB into RAM (well above kernel image) */
