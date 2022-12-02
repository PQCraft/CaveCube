MODULE ?= game

MODULECFLAGS := -DMODULEID_GAME=0 -DMODULEID_SERVER=1 -DMODULEID_TOOLBOX=2
ifeq ($(MODULE),game)
    MODULEID := 0
else ifeq ($(MODULE),server)
    MODULEID := 1
else
    .PHONY: error
    error:
		@echo Invalid module: $(MODULE)
		@exit 1
endif

ifndef OS
    ifndef WINCROSS
        CC ?= gcc
        STRIP ?= strip
        WINDRES ?= true
    else
        ifndef M32
            CC = x86_64-w64-mingw32-gcc
            STRIP = x86_64-w64-mingw32-strip
            WINDRES = x86_64-w64-mingw32-windres
        else
            CC = i686-w64-mingw32-gcc
            STRIP = i686-w64-mingw32-strip
            WINDRES = i686-w64-mingw32-windres
        endif
    endif
else
    CC = gcc
    STRIP = strip
    WINDRES = windres
    ifdef MSYS2
        undefine OS
    endif
endif

ifdef WINCROSS
    OS := Windows_NT
endif

ifdef NATIVE
    CC := $(CC) -march=native -mtune=native
endif
ifdef M32
    CC := $(CC) -m32
endif
ifndef DEBUG
    ifndef NOLTO
        CC := $(CC) -flto=auto
    endif
endif

empty :=
space := ${empty} ${empty}
${space} := ${space}

SRCDIR ?= src
ifndef OS
    ifndef M32
        PLATFORM := $(subst ${ },_,$(subst /,_,$(shell uname -s)_$(shell uname -m)))
    else
        PLATFORM := $(subst ${ },_,$(subst /,_,$(shell i386 uname -s)_$(shell i386 uname -m)))
    endif
else
    ifndef WINCROSS
        ifndef MSYS2
            ifeq ($(shell echo %PROGRAMFILES(x86)%),)
                PLATFORM := Windows_x86
            else
                PLATFORM := Windows_x86_64
            endif
        else
            ifndef M32
                PLATFORM := $(subst ${ },_,$(subst /,_,$(shell uname -s)_$(shell uname -m)))
            else
                PLATFORM := $(subst ${ },_,$(subst /,_,$(shell i386 uname -s)_x86))
            endif
        endif
    else
        ifndef M32
            PLATFORM := $(subst ${ },_,Windows_Cross_$(shell uname -m))
        else
            PLATFORM := $(subst ${ },_,Windows_Cross_$(shell i386 uname -m))
        endif
    endif
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

ifdef MSYS2
    OS := Windows_NT
endif

ifdef OS
    BINEXT := .exe
endif

ifeq ($(MODULE),game)
    BINNAME := cavecube
else
    BINNAME := ccserver
endif

BIN := $(BINNAME)$(BINEXT)

CFLAGS += -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE -D_GNU_SOURCE -pthread
CFLAGS += $(MODULECFLAGS) -DMODULEID=$(MODULEID) -DMODULE=$(MODULE)
ifdef OS
    WRFLAGS += $(MODULECFLAGS) -DMODULE=$(MODULE)
endif
ifdef DEBUG
    CFLAGS += -Og -g -DDEBUG=$(DEBUG)
    ifdef OS
        WRFLAGS += -DDEBUG=$(DEBUG)
    endif
else
    CFLAGS += -O2
endif
ifdef M32
    CFLAGS += -DM32
    ifdef OS
        WRFLAGS += -DM32
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
ifeq ($(MODULE),game)
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

ifdef MSYS2
    undefine OS
endif

ifdef WINCROSS
    undefine OS
endif

MKENV = NAME="$@" MODULE="$(MODULE)" SRCDIR="$(SRCDIR)" OBJDIR="$(OBJDIR)" UTILMK="util.mk" CC="$(CC)" CFLAGS="$(CFLAGS)" BASEDIRS="$(BASEDIRS)"
MKENV2 = NAME="$@" CC="$(CC)" CFLAGS="$(CFLAGS) -I../../$(SRCDIR)" SRCDIR="../../$(SRCDIR)" OBJDIR="../../$(OBJDIR)" UTILMK="../../util.mk"
MKENVSUB = CC="$(CC)" BINFLAGS="$(BINFLAGS)" OBJDIR="$(OBJDIR)"
ifdef DEBUG
    MKENVMOD += DEBUG="$(DEBUG)"
endif
ifdef USESDL2
    MKENVMOD += USESDL2=y
endif
ifdef WINCROSS
    MKENVMOD += WINCROSS=y
endif

GENSENT = $(OBJDIR)/.mkgen

ifdef OS
    WINCROSS=y
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
ifdef WINCROSS
	@$(WINDRES) $(WRFLAGS) -DORIG_NAME="$(BIN)" -DINT_NAME="$(BINNAME)" $(SRCDIR)/main/version.rc -o $(OBJDIR)/version.o
endif
ifndef WINCROSS
	@$(CC) $^ $(BINFLAGS) -o $@
else
	@$(CC) $(OBJDIR)/version.o $^ $(BINFLAGS) -o $@
endif
ifndef DEBUG
	@$(STRIP) --strip-all $@
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

