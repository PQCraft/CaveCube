include $(UTILMK)

MKENV = SRCDIR="../../$(SRCDIR)" OBJDIR="../../$(OBJDIR)" UTILMK="../../$(UTILMK)" CC="$(CC)" CFLAGS="$(CFLAGS) -I../../$(SRCDIR)"
MKENV2 = SRCDIR="$(SRCDIR)" OBJDIR="$(OBJDIR)" UTILMK="$(UTILMK)" CC="$(CC)" CFLAGS="$(CFLAGS)"

ifndef MKRULES
MKOUT = $(OBJDIR)/$@.mk
all: $(BASEDIRS)
else
MKOUT = $(OBJDIR)/$(NAME).mk
all: $(OFILES)
.NOTPARALLEL:
endif

ifndef MKSUB
$(BASEDIRS):
	@$(MAKE) --silent --no-print-directory -f gen.mk NAME="$@" ${MKENV2} MKSUB=y NAME="$@" $(MKOUT)
else
define MKSRC
$(subst .mk,,$(subst $(OBJDIR)/,$(SRCDIR)/,$@))
endef
define MKND
$(OBJDIR)/$(subst .mk,,$(subst $(OBJDIR)/,,$@))
endef
$(OBJDIR)/%.mk: $(wildcard $(SRCDIR)/$(NAME)/*.c $(SRCDIR)/$(NAME)/*.h)
	@echo Writing $@...
	@echo include $$$(esc)(UTILMK$(esc)) > $@
	@$(echoblank) >> $@
	@echo all: $(addprefix ../../$(OUTDIR)/,$(notdir $(CFILES:.c=.o))) >> $@
	@$(echoblank) >> $@
	@$(MAKE) --silent --no-print-directory -C "$(MKSRC)" -f ../../gen.mk NAME="$(subst .mk,,$(subst $(OBJDIR)/,,$@))" ${MKENV} MKRULES=y
ifndef OS
	@[ ! -d "$(MKND)" ] && echo Creating $(MKND)... && mkdir -p "$(MKND)"; true
else
	@if not exist "$(MKND)" echo Creating $(MKND)... & md "$(subst /,\,$(MKND))"
endif
endif

$(OUTDIR)/%.o: FORCE
	@$(CC) $(CFLAGS) -MM "$(notdir $(@:.o=.c))" -MT "$@" >> $(MKOUT)
	@echo $(TAB)$$$(esc)(COMPC$(esc)) >> $(MKOUT)
	@$(echoblank) >> $(MKOUT)

FORCE:

.PHONY: all $(BASEDIRS)

