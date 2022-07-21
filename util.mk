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
@[ ! -d "$@" ] && echo Creating $@... && mkdir "$@" && echo Created $@; true
endef
else
define mkdir
@if not exist "$@" echo Creating $@... & mkdir "$@" & echo Created $@
endef
endif

default: all
	$(null)

OUTDIR := $(OBJDIR)/$(NAME)
CFILES := $(wildcard $(SRCDIR)/$(NAME)/*.c)
OFILES := $(addprefix $(OUTDIR)/,$(notdir $(CFILES:.c=.o)))

#NAME2 = $@
#OUTDIR2 = $(OBJDIR)/$(NAME2)
#CFILES2 = $(wildcard $(SRCDIR)/$(NAME2)/*.c)
#OFILES2 = $(addprefix $(OUTDIR2)/,$(notdir $(CFILES2:.c=.o)))

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
	@$(cdmsg)
endef

SNAME := $(NAME):

$(OBJDIR):
	$(mkdir)

#$(OUTDIR): $(OBJDIR)
$(OUTDIR):
	$(mkdir)

cleanobjdir: FORCE
	@echo Removing $(OBJDIR)...
ifndef OS
	@rm -rf "$(OBJDIR)"
else
	@if exist "$(OBJDIR)" rmdir /S /Q "$(OBJDIR)"
endif
	@echo Removed $(OBJDIR)...

cleanoutdir: FORCE
	@echo Removing $(OUTDIR)...
ifndef OS
	@rm -rf "$(OUTDIR)"
else
	@if exist "$(OUTDIR)" rmdir /S /Q "$(OUTDIR)"
endif
	@echo Removed $(OUTDIR)...

FORCE:

.PHONY: cleanobjdir cleanoutdir FORCE

