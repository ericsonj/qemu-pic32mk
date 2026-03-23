TARGET          = Release/pic32mk_rtos_demo.elf
TARGET_BIN      = Release/pic32mk_rtos_demo.bin

TARGETS = $(TARGET_BIN)


$(TARGET): $(OBJECTS) $(SLIBS_OBJECTS) 
	$(call logger-compile,"LD",$@)
	$(LD) -m elf32ltsmip -T firmware/src/config/default/link.ld --gc-sections -Map=Release/pic32mk_rtos_demo.map -o $@ $(OBJECTS)

$(TARGET_BIN): $(TARGET)
	$(call logger-compile,"BIN",$@)
	$(OBJCOPY) -O binary --gap-fill 0xFF $(TARGET) Release/pic32mk_rtos_demo.bin


clean_targets:
	rm -rf $(TARGET) $(TARGET_BIN)
