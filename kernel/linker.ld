/*
 * Kernel layout. Limine loads the kernel at or above 0xffffffff80000000
 * (the top 2 GiB, which -mcmodel=kernel requires) with paging already on.
 *
 * Each segment has one set of permissions, so code is never writable and
 * data is never executable (write-xor-execute) from the first instruction.
 */
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kmain)

PHDRS
{
    requests PT_LOAD FLAGS(6);  /* read + write: Limine fills in responses */
    text     PT_LOAD FLAGS(5);  /* read + execute */
    rodata   PT_LOAD FLAGS(4);  /* read only */
    data     PT_LOAD FLAGS(6);  /* read + write */
}

SECTIONS
{
    . = 0xffffffff80000000;

    /* Limine only looks for requests between the start and end markers. */
    .limine_requests : {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :requests

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    .text : { *(.text .text.*) } :text

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    .rodata : { *(.rodata .rodata.*) } :rodata

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    .data : { *(.data .data.*) } :data
    .bss : { *(.bss .bss.*) *(COMMON) } :data

    /DISCARD/ : { *(.eh_frame*) *(.note .note.*) *(.comment) }
}
