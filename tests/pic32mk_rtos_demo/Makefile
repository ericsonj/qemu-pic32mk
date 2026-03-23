.DEFAULT_GOAL := all

%: prebuild
	@time -p $(MAKE) -f makefile.mk $@

prebuild:
	@poetry run pymaketool $(or $(MAKECMDGOALS),all)

.INTERMEDIATE: prebuild

.PHONY: VERBOSE
ifndef VERBOSE
MAKEFLAGS += --silent
endif
