ENTRY(_start)

PHDRS
{
    text PT_LOAD FILEHDR PHDRS FLAGS(7);
}

SECTIONS
{
    . = 0x400000 + SIZEOF_HEADERS;

    .all : {
        *(.text._start)
        *(.text*)
        *(.rodata*)
        *(.data*)
        *(.bss*)
        *(COMMON)
    } :text

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.interp)
        *(.dynamic)
        *(.hash)
        *(.gnu.hash)
        *(.dynsym)
        *(.dynstr)
        *(.rel.*)
        *(.rela.*)
    }
}