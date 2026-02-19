SECTIONS
{
    /* 1. Place the Primary Section */
    /* We align to a Page boundary (4KB) or L1 Way size */
    . = ALIGN(0x1000); 
    
    .data_primary : {
        *(.data_primary) 
    }

   
    . = ALIGN(0x1000);
    . = . + 0x800; 

    .data_shadow : {
        *(.data_shadow)
    }
    
    /* Pass through everything else standard (text, bss, etc.) */
}

INSERT AFTER .data;