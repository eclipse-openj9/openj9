# Build assembly files which require pre processing on Windows

%$(UMA_DOT_O): %.S
	$(CPP) $(CPPFLAGS) -Fi$*.asm -P $<
	$(AS) $(ASFLAGS) $*.asm
	rm -f $*.asm