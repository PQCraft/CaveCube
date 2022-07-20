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
BINFLAGS += -lglfw3 -lopengl32 -lgdi32 -lws2_32
endif

MKENV = NAME="$@" SRCDIR="$(SRCDIR)" OBJDIR="$(OBJDIR)" UTILMK="util.mk" CC="$(CC)" CFLAGS="$(CFLAGS) $(INCLUDEDIRS)" INCLUDEDIRS="$(INCLUDEDIRS)" BASEDIRS="$(BASEDIRS)"
MKENV2 = NAME="$@" CC="$(CC)" CFLAGS="$(CFLAGS) $(addprefix -I../../$(SRCDIR)/,$(BASEDIRS))" SRCDIR="../../$(SRCDIR)" OBJDIR="../../$(OBJDIR)" UTILMK="../../util.mk"
MKENVSUB = CC="$(CC)" BINFLAGS="$(BINFLAGS)" OBJDIR="$(OBJDIR)"

GENSENT = $(OBJDIR)/.mkgen

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

mkfiles: $(OBJDIR) $(GENSENT)

$(GENSENT): $(wildcard $(SRCDIR)/*/*.c $(SRCDIR)/*/*.h) $(SRCDIR) Makefile gen.mk util.mk
	@echo Writing makefiles...
ifndef OS
	@rm -f $(wildcard $(OBJDIR)/*.mk $(OBJDIR)/.mkgen)
else
	@if not "$(wildcard $(OBJDIR)/*.mk $(OBJDIR)/.mkgen)" == "" del /Q $(wildcard $(OBJDIR)/*.mk $(OBJDIR)/.mkgen)
endif
	@$(MAKE) --no-print-directory -f gen.mk ${MKENV}
ifndef OS
	@touch $@
else
	@type NUL > $@
endif
	@echo Wrote makefiles

$(BASEDIRS):
	@$(MAKE) --no-print-directory -C "$(SRCDIR)/$@" -f "../../$(OBJDIR)/$@.mk" ${MKENV2}

compile: $(BASEDIRS)

bin: mkfiles compile | $(BIN)

ifndef MKSUB
.PHONY: $(BIN)
$(BIN):
	@$(MAKE) --no-print-directory -f $(lastword $(MAKEFILE_LIST)) ${MKENVSUB} MKSUB=y bin
else
$(BIN): $(wildcard $(OBJDIR)/*/*.o)
	@echo Compiling $@...
	@$(CC) $^ $(BINFLAGS) -o $@
	@echo Compiled $@
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

.PHONY: build mkfiles $(BASEDIRS) compile bin

