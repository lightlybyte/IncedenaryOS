ENTRY(loader)

SECTIONS
{
    . = 0x100000;

    .text : {
        *(.text)
    }

    .data : {
        *(.data)
        *(.rodata)
    }

    .bss : {
        *(.bss)
        *(COMMON)
    }

    /* Font data */
    .font : {
        _binary_font_psf_start = .;
        *(.font)
        _binary_font_psf_end = .;
    }
}