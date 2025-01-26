ENTRY(stage2_entry) /* Entry point is the 'start' label in stage2.asm */

SECTIONS
{
    . = 0x0500; /* Start address for the binary */
    
    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
    }

}

