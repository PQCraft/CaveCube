ifndef OS
    ifndef WIN32
        CC ?= gcc
        STRIP ?= strip
        OBJCOPY ?= objcopy
        WINDRES ?= true
    else
        CC = x86_64-w64-mingw32-gcc
        STRIP = x86_64-w64-mingw32-strip
        OBJCOPY = x86_64-w64-mingw32-objcopy
        WINDRES = x86_64-w64-mingw32-windres
    endif
else
    CC = gcc
    STRIP = strip
    OBJCOPY = objcopy
    WINDRES = windres
endif

ifdef WIN32
    OS := Windows_NT
endif

ifdef NATIVE
    CC := $(CC) -march=native -mtune=native
endif
ifndef DEBUG
    CC := $(CC) -flto=auto
endif

SRCDIR ?= src
ifndef OS
    PLATFORM := nix
else
    PLATFORM := win
endif
ifndef SERVER
    MODULE := game
else
    MODULE := server
endif
OBJDIR ?= obj/$(MODULE)/$(PLATFORM)

SRCDIR := $(patsubst %/,%,$(SRCDIR))
OBJDIR := $(patsubst %/,%,$(OBJDIR))
SRCDIR := $(patsubst %\,%,$(SRCDIR))
OBJDIR := $(patsubst %\,%,$(OBJDIR))

DIRS := $(sort $(dir $(wildcard $(SRCDIR)/*/)))
DIRS := $(patsubst %/,%,$(DIRS))
DIRS := $(patsubst %\,%,$(DIRS))
DIRS := $(patsubst $(SRCDIR),,$(DIRS))
BASEDIRS := $(notdir $(DIRS))

ifdef OS
    BINEXT := .exe
endif

ifndef SERVER
    BINNAME := cavecube
else
    BINNAME := ccserver
endif

BIN := $(BINNAME)$(BINEXT)

CFLAGS += -Wall -Wextra -O2 -pthread
ifdef DEBUG
    CFLAGS += -g -DDEBUG=$(DEBUG)
    ifdef OS
        WRFLAGS += -DDEBUG=$(DEBUG)
    endif
endif
ifdef SERVER
    CFLAGS += -DSERVER
    ifdef OS
        WRFLAGS += -DSERVER
    endif
endif

BINFLAGS += -pthread -lm
ifndef OS
    BINFLAGS += -lpthread
else
    BINFLAGS += -l:libwinpthread.a -lws2_32
endif
ifdef USEGLES
    CFLAGS += -DUSEGLES
    ifdef OS
        WRFLAGS += -DUSEGLES
    endif
endif
ifndef SERVER
    ifdef USESDL2
        CFLAGS += -DUSESDL2
        ifdef OS
            WRFLAGS += -DUSESDL2
        endif
        BINFLAGS += -lSDL2
    else
        ifndef OS
            BINFLAGS += -lglfw
        else
            BINFLAGS += -lglfw3
        endif
    endif
    ifndef OS
        BINFLAGS += -lX11
    else
        BINFLAGS += -lgdi32
    endif
endif
ifndef OS
    BINFLAGS += -ldl
endif

ifdef WIN32
    undefine OS
endif

MKENV = NAME="$@" SRCDIR="$(SRCDIR)" OBJDIR="$(OBJDIR)" UTILMK="util.mk" CC="$(CC)" CFLAGS="$(CFLAGS)" BASEDIRS="$(BASEDIRS)"
MKENV2 = NAME="$@" CC="$(CC)" CFLAGS="$(CFLAGS) -I../../$(SRCDIR)" SRCDIR="../../$(SRCDIR)" OBJDIR="../../$(OBJDIR)" UTILMK="../../util.mk"
MKENVSUB = CC="$(CC)" BINFLAGS="$(BINFLAGS)" OBJDIR="$(OBJDIR)"
ifdef DEBUG
    MKENVMOD += DEBUG="$(DEBUG)"
endif
ifdef SERVER
    MKENVMOD += SERVER=y
endif
ifdef USESDL2
    MKENVMOD += USESDL2=y
endif
ifdef WIN32
    MKENVMOD += WIN32=y
endif

GENSENT = $(OBJDIR)/.mkgen

ifdef OS
    WIN32=y
endif

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
@[ ! -d "$@" ] && echo Creating $@... && mkdir -p "$@"; true
endef
else
define mkdir
@if not exist "$@" echo Creating $@... & md "$(subst /,\,$@)"
endef
endif

build: mkfiles compile $(BIN)
	$(null)

$(OBJDIR):
	$(mkdir)

ifdef MKSUB
cleanmk: $(wildcard $(OBJDIR)/*.mk)
else
mkfiles: $(OBJDIR) $(GENSENT)
	@$(MAKE) --no-print-directory -f $(lastword $(MAKEFILE_LIST)) ${MKENVMOD} MKSUB=y cleanmk
endif

$(GENSENT): $(wildcard $(SRCDIR)/*/*.c $(SRCDIR)/*/*.h) $(SRCDIR)
	@echo Writing makefiles...
ifndef OS
	@rm -f $(OBJDIR)/.mkgen
else
	@if exist "$(OBJDIR)\.mkgen" del /Q "$(OBJDIR)\.mkgen"
endif
	@$(MAKE) --no-print-directory -f gen.mk ${MKENV}
ifndef OS
	@touch $@
else
	@type NUL > $@
endif

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
	@$(MAKE) --no-print-directory -f $(lastword $(MAKEFILE_LIST)) ${MKENVMOD} $(BASEDIRS)
endif

bin: $(BIN)
	$(null)

ifndef MKSUB
.PHONY: $(BIN)
$(BIN):
	@$(MAKE) --no-print-directory -f $(lastword $(MAKEFILE_LIST)) ${MKENVMOD} ${MKENVSUB} MKSUB=y bin
else
$(BIN): $(wildcard $(OBJDIR)/*/*.o)
	@echo Building $@...
ifdef WIN32
	@$(WINDRES) $(WRFLAGS) -DORIG_NAME="$(BIN)" -DINT_NAME="$(BINNAME)" $(SRCDIR)/cavecube.rc -o $(OBJDIR)/rc.o
	@$(CC) $^ $(OBJDIR)/rc.o $(BINFLAGS) -o $@
else
	@$(CC) $^ $(BINFLAGS) -o $@
endif
ifndef DEBUG
	@$(STRIP) --strip-all $@
	@$(OBJCOPY) -w --remove-section ".note*" $@
endif
endif

run: build
	@echo Running $(BIN)...
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
	@if exist "$(subst /,\,$(OBJDIR))" rmdir /S /Q "$(subst /,\,$(OBJDIR))"
endif
	@echo Removing $(BIN)...
ifndef OS
	@rm -f $(BIN)
else
	@if exist $(BIN) del /Q $(BIN)
endif

.NOTPARALLEL:

FORCE:

.PHONY: $(OBJDIR) build mkfiles cleanmk $(BASEDIRS) compile bin run clean

