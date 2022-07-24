ifndef OS
define null
@echo > /dev/null
endef
else
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

ifndef OS
define mkdir
@[ ! -d "$@" ] && echo Creating $@... && mkdir "$@"; true
endef
else
define mkdir
@if not exist "$@" echo Creating $@... & mkdir "$@"
endef
endif

default: all
	$(null)

OUTDIR := $(OBJDIR)/$(NAME)
CFILES := $(wildcard $(SRCDIR)/$(NAME)/*.c)
OFILES := $(addprefix $(OUTDIR)/,$(notdir $(CFILES:.c=.o)))

ifndef OS
TAB := "	"
else
define TAB
	
endef
endif

ifndef OS
esc = \$()
else
define esc
^
endef
endif

ifndef OS
define echoblank
echo
endef
else
define echoblank
echo.
endef
endif

define COMPC
	@$(ccmsg)
	@$(CC) $(CFLAGS) $< -Wuninitialized -c -o $@
endef

SNAME := $(NAME):

$(OBJDIR):
	$(mkdir)

$(OUTDIR):
	$(mkdir)

cleanobjdir: FORCE
	@echo Removing $(OBJDIR)...
ifndef OS
	@rm -rf "$(OBJDIR)"
else
	@if exist "$(OBJDIR)" rmdir /S /Q "$(OBJDIR)"
endif

cleanoutdir: FORCE
	@echo Removing $(OUTDIR)...
ifndef OS
	@rm -rf "$(OUTDIR)"
else
	@if exist "$(OUTDIR)" rmdir /S /Q "$(OUTDIR)"
endif

FORCE:

.PHONY: cleanobjdir cleanoutdir FORCE

