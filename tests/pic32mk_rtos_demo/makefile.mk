## Local functions
define logger-compile
	@printf "%6s\t%-30s\n" $(1) $(2)
endef

.DEFAULT_GOAL := all

CSRC  =
ASSRC =
INCS  =
COMPILER_FLAGS =
SLIBS_OBJECTS =
SLIBS_NAMES =
SRC_DIRS =

include vars.mk
include srcs.mk

ASSRC_s   = $(filter %.s,$(ASSRC))
ASSRC_S   = $(filter %.S,$(ASSRC))
ASSRC_asm = $(filter %.asm,$(ASSRC))

OBJECTS = $(CSRC:%.c=$(PROJECT_OUT)/%.o) \
          $(ASSRC_s:%.s=$(PROJECT_OUT)/%.o) \
          $(ASSRC_S:%.S=$(PROJECT_OUT)/%.o) \
          $(ASSRC_asm:%.asm=$(PROJECT_OUT)/%.o)

include targets.mk

%.o : CFLAGS = $(COMPILER_FLAGS)

$(PROJECT_OUT)/%.o: %.c
	$(call logger-compile,"CC",$<)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCS) -o $@ -c $<

$(PROJECT_OUT)/%.o: %.S
	$(call logger-compile,"AS",$<)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCS) -o $@ -c $<

$(PROJECT_OUT)/%.o: %.s
	$(call logger-compile,"AS",$<)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCS) -o $@ -c $<

all: $(TARGETS)

clean: clean_targets
	@echo 'CLEAN'
	rm -rf $(PROJECT_OUT)
