ifndef OS
CC ?= gcc
else
CC = gcc
endif

SRCDIR ?= src
ifndef OS
OBJDIR ?= obj
else
OBJDIR ?= winobj
endif

SRCDIR := $(patsubst %/,%,$(SRCDIR))
OBJDIR := $(patsubst %/,%,$(OBJDIR))
SRCDIR := $(patsubst %\,%,$(SRCDIR))
OBJDIR := $(patsubst %\,%,$(OBJDIR))

DIRS := $(sort $(dir $(wildcard $(SRCDIR)/*/.)))
DIRS := $(patsubst %/,%,$(DIRS))
DIRS := $(patsubst %\,%,$(DIRS))
BASEDIRS := $(notdir $(DIRS))

INCLUDEDIRS = $(addprefix -I$(SRCDIR)/,$(BASEDIRS))

ifdef OS
BINEXT := .exe
endif

BINNAME := cavecube

BIN := $(BINNAME)$(BINEXT)

CFLAGS += -Wall -Wextra -I. -g -O2

BINFLAGS += -lm -lpthread

ifndef OS
BINFLAGS += -lglfw -lX11 -ldl
else
BINFLAGS += -lglfw3 -lopengl32 -lgdi32 -lws2_32 -Wl,--subsystem,windows
endif

MKENV = NAME="$@" SRCDIR="$(SRCDIR)" OBJDIR="$(OBJDIR)" UTILMK="util.mk" CC="$(CC)" CFLAGS="$(CFLAGS) $(INCLUDEDIRS)" INCLUDEDIRS="$(INCLUDEDIRS)" BASEDIRS="$(BASEDIRS)"
MKENV2 = NAME="$@" CC="$(CC)" CFLAGS="$(CFLAGS) $(addprefix -I../../$(SRCDIR)/,$(BASEDIRS))" SRCDIR="../../$(SRCDIR)" OBJDIR="../../$(OBJDIR)" UTILMK="../../util.mk"
MKENVSUB = CC="$(CC)" BINFLAGS="$(BINFLAGS)" OBJDIR="$(OBJDIR)"

GENSENT = $(OBJDIR)/.mkgen

ifndef OS
define null
@echo > /dev/null
endef
else
define null
@echo. > NUL
endef
endif

ifndef OS
define mkdir
@[ ! -d "$@" ] && echo Creating $@... && mkdir "$@" && echo Created $@; true
endef
else
define mkdir
@if not exist "$@" echo Creating $@... & mkdir "$@" & echo Created $@
endef
endif

build: bin

$(OBJDIR):
	$(mkdir)

ifdef MKSUB
cleanmk: $(wildcard $(OBJDIR)/*.mk)
else
mkfiles: $(OBJDIR) $(GENSENT)
	@$(MAKE) --no-print-directory -f $(lastword $(MAKEFILE_LIST)) MKSUB=y cleanmk
endif

$(GENSENT): $(wildcard $(SRCDIR)/*/*.c $(SRCDIR)/*/*.h) $(SRCDIR)
	@echo Writing makefiles...
ifndef OS
	@rm -f $(OBJDIR)/.mkgen
else
	@if exist $(OBJDIR)\.mkgen del /Q $(OBJDIR)\.mkgen
endif
	@$(MAKE) --no-print-directory -f gen.mk ${MKENV}
ifndef OS
	@touch $@
else
	@type NUL > $@
endif
	@echo Wrote makefiles

ifdef MKSUB
ifndef OS
define MKSRC
$(subst .mk,,$(subst $(OBJDIR)/,$(SRCDIR)/,$@))
endef
else
define MKSRC
$(subst .mk,,$(subst $(OBJDIR)/,$(SRCDIR)\,$@))
endef
endif

$(wildcard $(OBJDIR)/*.mk): FORCE
ifndef OS
	@[ ! -d $(MKSRC) ] && rm -f $@ || exit 0
else
	@if not exist $(MKSRC) del /Q $@
endif
endif

ifndef MKSUB
$(BASEDIRS): FORCE
	@$(MAKE) --no-print-directory -C "$(SRCDIR)/$@" -f "../../$(OBJDIR)/$@.mk" ${MKENV2}

compile: FORCE
	@$(MAKE) --no-print-directory -f $(lastword $(MAKEFILE_LIST)) $(BASEDIRS)
endif

bin: mkfiles compile | $(BIN)
	$(null)

ifndef MKSUB
.PHONY: $(BIN)
$(BIN):
	@$(MAKE) --no-print-directory -f $(lastword $(MAKEFILE_LIST)) ${MKENVSUB} MKSUB=y bin
else
$(BIN): $(wildcard $(OBJDIR)/*/*.o)
	@echo Building $@...
	@$(CC) $^ $(BINFLAGS) -o $@
	@echo Built $@
endif

run: build
ifndef OS
	@./$(BIN)
else
	@.\\$(BIN)
endif

clean:
	@echo Removing $(OBJDIR)...
ifndef OS
	@rm -rf $(OBJDIR)
else
	@if exist $(OBJDIR) rmdir /S /Q $(OBJDIR)
endif
	@echo Removed $(OBJDIR)
	@echo Removing $(BIN)...
ifndef OS
	@rm -f $(BIN)
else
	@if exist $(BIN) del /Q $(BIN)
endif
	@echo Removed $(BIN)

.NOTPARALLEL:

FORCE:

.PHONY: $(OBJDIR) build mkfiles cleanmk $(BASEDIRS) compile bin

