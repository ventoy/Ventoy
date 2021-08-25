SECTIONS {

	/* Align sections to keep PE tools happy */
	alignment = 16;

	/* Virtual addresses start at 0x20000 */
	. = 0x20000;
	_start = .;

	/* bzImage prefix */
	_prefix_pos = 0;
	.prefix : AT ( _prefix_pos ) {
		_prefix = .;
		*(.prefix)
		*(.prefix.*)
		. = ALIGN ( alignment );
		_eprefix = .;
	}
	_prefix_len = ABSOLUTE ( _eprefix ) - ABSOLUTE ( _prefix );

	/* Real-mode uninitialised data section */
	.bss16 ( NOLOAD ) : {
		_bss16 = .;
		*(.stack16)
		*(.stack16.*)
		*(.bss16)
		*(.bss16.*)
		. = ALIGN ( alignment );
		_ebss16 = .;
	}
	_bss16_len = ABSOLUTE ( _ebss16 ) - ABSOLUTE ( _bss16 );

	/* Payload section */
	_payload_pos = ( _prefix_pos + _prefix_len );
	.payload : AT ( _payload_pos ) {
		_payload = .;
		/* Portions that must be accessible in 16-bit modes */
		_text16 = .;
		*(.text16)
		*(.text16.*)
		_etext16 = .;
		_data16 = .;
		*(.rodata16)
		*(.rodata16.*)
		*(.data16)
		*(.data16.*)
		_edata16 = .;
		/* Portions that need not be accessible in 16-bit modes */
		_text = .;
		*(.text)
		*(.text.*)
		_etext = .;
		_data = .;
		*(.rodata)
		*(.rodata.*)
		*(.data)
		*(.data.*)
		. = ALIGN ( alignment );
		_edata = .;
		_epayload = .;
	}
	_text16_len = ABSOLUTE ( _etext16 ) - ABSOLUTE ( _text16 );
	_data16_len = ABSOLUTE ( _edata16 ) - ABSOLUTE ( _data16 );
	_text_len = ABSOLUTE ( _etext ) - ABSOLUTE ( _text );
	_data_len = ABSOLUTE ( _edata ) - ABSOLUTE ( _data );
	_payload_len = ABSOLUTE ( _epayload ) - ABSOLUTE ( _payload );

	/* bootmgr.exe hardcodes the address 0x30000 for use as a
	 * buffer accessible by real-mode code.  We can't fit our
	 * .text, .data, and .bss below this region, so explicitly
	 * place the .bss higher in memory.
	 */
	_forbidden_start = 0x30000;
	_forbidden_end = 0x40000;

	/* Uninitialised data section */
	.bss ( NOLOAD ) : {
		_bss = .;
		ASSERT ( ABSOLUTE ( . ) <= ABSOLUTE ( _forbidden_start ),
			 "Binary is too large" );
		. = ABSOLUTE ( _forbidden_end );
		*(.bss)
		*(.bss.*)
		*(COMMON)
		*(.stack)
		*(.stack.*)
		. = ALIGN ( alignment );
		_ebss = .;
	}
	_bss_len = ABSOLUTE ( _ebss ) - ABSOLUTE ( _bss );

	/* Relocations section */
	_reloc_pos = ( _payload_pos + _payload_len );
	_reloc = .;

	_end = .;

	_text_total_len = ( _text_len + _text16_len );
	_data_total_len = ( _data_len + _data16_len );
	_bss_total_len = ( _bss_len + _bss16_len );

	/* Symbols required by i386.x86_64 objects */
	__i386__start = _start;
	__i386__end = _end;

	/DISCARD/ : {
		*(.comment)
		*(.comment.*)
		*(.note)
		*(.note.*)
		*(.eh_frame)
		*(.eh_frame.*)
		*(.rel)
		*(.rel.*)
	}
}
