/* OneOS-ARM 1 Linker Script
 * Target: QEMU virt machine (AArch64)
 * RAM starts at 0x40000000
 */

ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x40100000, LENGTH = 256M
}

SECTIONS
{
    /* Start at the beginning of RAM as defined above */
    . = ORIGIN(RAM);
    
    .text : {
        *(.text.boot)
        *(.text*)
        *(.rodata*)
    } > RAM
    
    .data : {
        *(.data*)
    } > RAM
    
    . = ALIGN(8);
    _bss_start = .;
    .bss : {
        *(.bss*)
        *(COMMON)
    } > RAM
    . = ALIGN(8);
    _bss_end = .;
    
    /* Set end of kernel for memory management */
    _end = .;
}
