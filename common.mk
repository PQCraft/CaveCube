ifndef OS
define null
	@echo > /dev/null
endef
else
define null
	@echo > NUL
endef
endif

define ccmsg
	@echo Compiling $(NAME)/$<...
endef

define cdmsg
	@echo Compiled $(NAME)/$<
endef

default: all
	$(null)

OUTDIR := $(OBJDIR)/$(NAME)

CFILES := $(wildcard *.c)

OFILES := $(addprefix $(OUTDIR)/,$(CFILES:.c=.o))

TAB := "	"

SNAME := $(NAME):

$(OBJDIR):
	@echo Creating $@...
ifndef OS
	@[ ! -d $@ ] && mkdir -p $@; true
else
	@if not exist $@ mkdir $@
endif
	@echo Created $@

$(OUTDIR): $(OBJDIR)
	@echo Creating $@...
ifndef OS
	@[ ! -d $@ ] && mkdir -p $@; true
else
	@if not exist $@
endif
	@echo Created $@

cleanobjdir: FORCE
	@echo Removing $(OBJDIR)...
ifndef OS
	@rm -rf $(OBJDIR)
else
	@if exist $(OBJDIR) rmdir /S /Q $(OBJDIR)
endif
	@echo Removed $(OBJDIR)...

cleanoutdir: FORCE
	@echo Removing $(OUTDIR)...
ifndef OS
	@rm -rf $(OUTDIR)
else
	@if exist $(OUTDIR) rmdir /S /Q $(OUTDIR)
endif
	@echo Removed $(OUTDIR)...

FORCE:

.PHONY: cleanobjdir cleanoutdir FORCE

