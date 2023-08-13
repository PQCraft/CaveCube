# VARS

ifndef MKSUB

    MODULE ?= game
    CROSS ?= 
    OUTDIR ?= .

    MODULECFLAGS := -DMODULEID_GAME=0 -DMODULEID_SERVER=1 -DMODULEID_TOOLBOX=2
    ifeq ($(MODULE),game)
        MODULEID := 0
        MODULECFLAGS += -DMODULE_GAME
        MODULECFLAGS += -DMODULE_SERVER
    else ifeq ($(MODULE),server)
        MODULEID := 1
        MODULECFLAGS += -DMODULE_SERVER
    else ifeq ($(MODULE),toolbox)
        MODULEID := 2
        MODULECFLAGS += -DMODULE_TOOLBOX
    else
        .PHONY: error
        error:
		    @echo Invalid module: $(MODULE)
		    @exit 1
    endif

    ifndef OS
        ifeq ($(CROSS),)
            CC ?= gcc
            STRIP ?= strip
            WINDRES ?= true
        else ifeq ($(CROSS),win32)
            ifndef M32
                CC = x86_64-w64-mingw32-gcc
                STRIP = x86_64-w64-mingw32-strip
                WINDRES = x86_64-w64-mingw32-windres
            else
                CC = i686-w64-mingw32-gcc
                STRIP = i686-w64-mingw32-strip
                WINDRES = i686-w64-mingw32-windres
            endif
        else ifeq ($(CROSS),emscr)
            CC = emcc
            STRIP ?= true
            USEGLES = y
        else ifeq ($(CROSS),android)
            STRIP ?= true
            USESDL2 = y
            USEGLES = y
        else
            .PHONY: error
            error:
			    @echo Invalid cross-compilation target: $(CROSS)
			    @exit 1
        endif
        SHCMD = unix
    else
        CC = gcc
        STRIP = strip
        WINDRES = windres
        CROSS = win32
        ifdef MSYS2
            SHCMD = unix
        else
            SHCMD = win32
        endif
    endif

    ifdef M32
        CCFLAGS += -m32
    endif
    ifndef DEBUG
        ifndef NOLTO
            CCFLAGS += -flto=auto
        endif
    endif

    CC += $(CCFLAGS)

    empty :=
    space := ${empty} ${empty}
    ${space} := ${space}

    SRCDIR ?= src
    ifeq ($(CROSS),)
        ifndef M32
            PLATFORMDIR := $(subst ${ },_,$(subst /,_,$(shell uname -s)_$(shell uname -m)))
        else
            PLATFORMDIR := $(subst ${ },_,$(subst /,_,$(shell i386 uname -s)_$(shell i386 uname -m)))
        endif
    else ifeq ($(CROSS),win32)
        ifndef MSYS2
            ifdef OS
                ifeq ($(shell echo %PROGRAMFILES(x86)%),)
                    PLATFORMDIR := Windows_x86
                else
                    PLATFORMDIR := Windows_x86_64
                endif
            else
                ifndef M32
                    PLATFORMDIR := $(subst ${ },_,Windows_Cross_$(shell uname -m))
                else
                    PLATFORMDIR := $(subst ${ },_,Windows_Cross_$(shell i386 uname -m))
                endif
            endif
        else
            ifndef M32
                PLATFORMDIR := $(subst ${ },_,$(subst /,_,$(shell uname -s)_$(shell uname -m)))
            else
                PLATFORMDIR := $(subst ${ },_,$(subst /,_,$(shell i386 uname -s)_x86))
            endif
        endif
    else ifeq ($(CROSS),emscr)
        PLATFORMDIR := Emscripten
    else ifeq ($(CROSS),android)
        ifdef CMAKE_ARCH
            PLATFORMDIR := Android_$(CMAKE_ARCH)
        else
            PLATFORMDIR := Android
        endif
    endif
    OBJDIR ?= obj/$(PLATFORMDIR)/$(MODULE)

    SRCDIR := $(patsubst %/,%,$(SRCDIR))
    OBJDIR := $(patsubst %/,%,$(OBJDIR))
    SRCDIR := $(patsubst %\,%,$(SRCDIR))
    OBJDIR := $(patsubst %\,%,$(OBJDIR))

    DIRS := $(sort $(dir $(wildcard $(SRCDIR)/*/)))
    ifeq ($(CROSS),emscr)
        DIRS := $(filter-out $(wildcard $(SRCDIR)/zlib/),$(DIRS))
    endif
    DIRS := $(patsubst %/,%,$(DIRS))
    DIRS := $(patsubst %\,%,$(DIRS))
    DIRS := $(patsubst $(SRCDIR),,$(DIRS))
    BASEDIRS := $(notdir $(DIRS))

    ifeq ($(MODULE),server)
        BINNAME := ccserver
    else ifeq ($(MODULE),toolbox)
        BINNAME := cctoolbx
    else
        BINNAME := cavecube
    endif

    ifeq ($(CROSS),win32)
        BINEXT := .exe
    else ifeq ($(CROSS),emscr)
        BINEXT := .html
    else ifeq ($(CROSS),android)
        BINNAME := lib$(BINNAME)
        BINEXT := .so
    endif

    BIN := $(BINNAME)$(BINEXT)
    ifeq ($(SHCMD),unix)
    BIN := $(OUTDIR)/$(BIN)
    else ifeq ($(SHCMD),win32)
    BIN := $(OUTDIR)\\$(BIN)
    endif

    CFLAGS += -Wall -Wextra -D_DEFAULT_SOURCE -D_GNU_SOURCE -pthread -ffast-math
    ifeq ($(MODULE),game)
        ifdef USEGLES
            CFLAGS += -DUSEGLES
            ifeq ($(CROSS),win32)
                WRFLAGS += -DUSEGLES
            endif
        endif
        ifdef USESDL2
            CFLAGS += -DUSESDL2
            ifeq ($(CROSS),win32)
                WRFLAGS += -DUSESDL2
            endif
        endif
    endif
    ifeq ($(CROSS),win32)
        WRFLAGS += $(MODULECFLAGS) -DMODULE=$(MODULE)
    else ifeq ($(CROSS),emscr)
        CFLAGS += -msimd128 -s USE_ZLIB=1
    else ifeq ($(CROSS),android)
        CFLAGS += -fPIC
    endif
    ifneq ($(CROSS),emscr)
        CFLAGS += -fno-exceptions 
        ifneq ($(CROSS),android)
            CFLAGS += -fno-stack-clash-protection -fcf-protection=none
        endif
    endif
    ifdef DEBUG
        CFLAGS += -Og -g -DDEBUG=$(DEBUG)
        ifeq ($(CROSS),win32)
            WRFLAGS += -DDEBUG=$(DEBUG)
        endif
    else
        CFLAGS += -O2
    endif
    ifdef NATIVE
        CFLAGS += -march=native -mtune=native
    endif
    ifdef M32
        CFLAGS += -DM32
        ifeq ($(CROSS),win32)
            WRFLAGS += -DM32
        endif
    endif
    CFLAGS += $(MODULECFLAGS) -DMODULEID=$(MODULEID) -DMODULE=$(MODULE)

    BINFLAGS += -lm
    ifeq ($(MODULE),game)
        ifeq ($(CROSS),)
            ifdef USESDL2
                BINFLAGS += -lSDL2
            else
                BINFLAGS += -lglfw
            endif
            BINFLAGS += -lX11
        else ifeq ($(CROSS),win32)
            BINFLAGS += -lws2_32 -lwinmm
            ifdef USESDL2
                BINFLAGS += -l:libSDL2.a -lole32 -loleaut32 -limm32 -lsetupapi -lversion
            else
                BINFLAGS += -lglfw3
            endif
            BINFLAGS += -lgdi32
        else ifeq ($(CROSS),emscr)
            BINFLAGS += -s USE_WEBGL2=1
            ifdef USESDL2
                BINFLAGS += -s USE_SDL=2
            else
                BINFLAGS += -s USE_GLFW=3
            endif
            BINFLAGS += --preload-file resources/ --shell-file extras/emscr_shell.html
        endif
    else ifeq ($(MODULE),server)
        ifeq ($(CROSS),win32)
            BINFLAGS += -lwinmm
        endif
    endif
    ifeq ($(CROSS),)
        BINFLAGS += -pthread -lpthread -ldl
    else ifeq ($(CROSS),win32)
        BINFLAGS += -pthread -l:libwinpthread.a
    else ifeq ($(CROSS),emscr)
        BINFLAGS += -O2 -s WASM=1 -s INITIAL_MEMORY=1024MB -s ASYNCIFY=1
        BINFLAGS += -s USE_ZLIB=1 -s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=navigator.hardwareConcurrency
    endif

    GENSENT = $(OBJDIR)/.mkgen

    UTILMK = util.mk

    export

else

    export SHCMD
    export UTILMK := ../../$(UTILMK)    

endif

# DEFINES

ifeq ($(SHCMD),unix)
define null
@echo > /dev/null
endef
else ifeq ($(SHCMD),win32)
define null
@echo. > NUL
endef
endif

ifeq ($(SHCMD),unix)
define mkdir
@[ ! -d "$@" ] && echo Creating $@... && mkdir -p "$@"; true
endef
else ifeq ($(SHCMD),win32)
define mkdir
@if not exist "$@" echo Creating $@... & md "$(subst /,\,$@)"
endef
endif

ifeq ($(SHCMD),unix)
define MKSRC
$(subst .mk,,$(subst $(OBJDIR)/,$(SRCDIR)/,$@))
endef
else ifeq ($(SHCMD),win32)
define MKSRC
$(subst .mk,,$(subst $(OBJDIR)/,$(SRCDIR)\,$@))
endef
endif

# RULES AND TARGETS

ifndef MKSUB

build: mkfiles compile $(BIN)
	$(null)

cleanmk: $(wildcard $(OBJDIR)/*.mk)

$(wildcard $(OBJDIR)/*.mk): .FORCE
ifeq ($(SHCMD),unix)
	@[ ! -d $(MKSRC) ] && rm -f $@ || exit 0
else ifeq ($(SHCMD),win32)
	@if not exist $(MKSRC) del /Q $@
endif

mkfiles: $(OBJDIR) cleanmk $(GENSENT)

$(OBJDIR):
	$(mkdir)

$(GENSENT): $(wildcard $(SRCDIR)/*/*.c $(SRCDIR)/*/*.h) $(SRCDIR)
	@echo Writing makefiles...
ifeq ($(SHCMD),unix)
	@rm -f $(OBJDIR)/.mkgen
else ifeq ($(SHCMD),win32)
	@if exist "$(OBJDIR)\.mkgen" del /Q "$(OBJDIR)\.mkgen"
endif
	@$(MAKE) --no-print-directory -f gen.mk
ifeq ($(SHCMD),unix)
	@touch $@
else ifeq ($(SHCMD),win32)
	@type NUL > $@
endif

compile: .FORCE
	@$(MAKE) --no-print-directory -f $(lastword $(MAKEFILE_LIST)) MKSUB=y compile

$(BIN):
	@$(MAKE) --no-print-directory -f $(lastword $(MAKEFILE_LIST)) MKSUB=y bin

run: build
	@echo Running $(BIN)...
ifeq ($(SHCMD),unix)
	@./$(BIN)
else ifeq ($(SHCMD),win32)
	@.\\$(BIN)
endif

clean:
	@echo Removing $(OBJDIR)...
ifeq ($(SHCMD),unix)
	@rm -rf $(OBJDIR)
else ifeq ($(SHCMD),win32)
	@if exist "$(subst /,\,$(OBJDIR))" rmdir /S /Q "$(subst /,\,$(OBJDIR))"
endif
	@echo Removing $(BIN)...
ifeq ($(SHCMD),unix)
	@rm -f $(BIN)
ifeq ($(CROSS),emscr)
	@rm -f $(BINNAME)*.html
	@rm -f $(BINNAME)*.js
	@rm -f $(BINNAME)*.wasm
	@rm -f $(BINNAME)*.data
endif
else ifeq ($(SHCMD),win32)
	@if exist $(BIN) del /Q $(BIN)
endif

.PHONY: $(BIN) $(OBJDIR) build mkfiles cleanmk compile run clean
.NOTPARALLEL:

else

$(BASEDIRS): .FORCE
	@$(MAKE) --no-print-directory -C "$(SRCDIR)/$@" -f "../../$(OBJDIR)/$@.mk" NAME="$@" CFLAGS="$(CFLAGS) -I../../$(SRCDIR)" SRCDIR="../../$(SRCDIR)" OBJDIR="../../$(OBJDIR)"

compile: $(BASEDIRS) .FORCE

$(BIN): $(wildcard $(OBJDIR)/*/*.o)
	@echo Building $@...
ifeq ($(CROSS),win32)
	@$(WINDRES) $(WRFLAGS) -DORIG_NAME="$(BIN)" -DINT_NAME="$(BINNAME)" $(SRCDIR)/main/version.rc -o $(OBJDIR)/version.o
	@$(CC) $(OBJDIR)/version.o $^ $(BINFLAGS) -o $@
else ifeq ($(CROSS),android)
	@$(CC) -shared $^ $(BINFLAGS) -o $@
else
	@$(CC) $^ $(BINFLAGS) -o $@
endif
ifndef DEBUG
	@$(STRIP) --strip-all $@
endif

bin: $(BIN) .FORCE
	$(null)

.PHONY: bin compile $(BASEDIRS)

endif

.FORCE:
