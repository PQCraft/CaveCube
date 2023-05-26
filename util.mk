ifeq ($(SHCMD),unix)
define null
@echo > /dev/null
endef
else ifeq ($(SHCMD),win32)
define null
@echo. > NUL
endef
endif

define ccmsg
@echo Compiling $(NAME)/$<...
endef

define cdmsg
@echo Compiled $(NAME)/$<
endef

ifeq ($(SHCMD),unix)
define mkdir
@[ ! -d "$@" ] && echo Creating $@... && mkdir -p "$@"; true
endef
else ifeq ($(SHCMD),win32)
define mkdir
@if not exist "$@" echo Creating $@... & md "$(subst /,\,$@)"
endef
endif

default: all
	$(null)

OUTDIR := $(OBJDIR)/$(NAME)
CFILES := $(wildcard $(SRCDIR)/$(NAME)/*.c)
OFILES := $(addprefix $(OUTDIR)/,$(notdir $(CFILES:.c=.o)))

ifeq ($(SHCMD),unix)
TAB := "	"
else ifeq ($(SHCMD),win32)
define TAB
	
endef
endif

ifeq ($(SHCMD),unix)
esc = \$()
else ifeq ($(SHCMD),win32)
define esc
^
endef
endif

ifeq ($(SHCMD),unix)
define echoblank
echo
endef
else ifeq ($(SHCMD),win32)
define echoblank
echo.
endef
endif

define COMPC
	@$(ccmsg)
	@$(CC) $(CFLAGS) $< -Wuninitialized -c -o $@
	@$(cdmsg)
endef

SNAME := $(NAME):

$(OBJDIR):
	$(mkdir)

$(OUTDIR):
	$(mkdir)

cleanobjdir: FORCE
	@echo Removing $(OBJDIR)...
ifeq ($(SHCMD),unix)
	@rm -rf "$(OBJDIR)"
else ifeq ($(SHCMD),win32)
	@if exist "$(subst /,\,$(OBJDIR))" rmdir /S /Q "$(subst /,\,$(OBJDIR))"
endif

cleanoutdir: FORCE
	@echo Removing $(OUTDIR)...
ifeq ($(SHCMD),unix)
	@rm -rf "$(OUTDIR)"
else ifeq ($(SHCMD),win32)
	@if exist "$(subst /,\,$(OUTDIR))" rmdir /S /Q "$(subst /,\,$(OUTDIR))"
endif

FORCE:

.PHONY: cleanobjdir cleanoutdir FORCE

