include $(UTILMK)

MKENV = SRCDIR="../../$(SRCDIR)" OBJDIR="../../$(OBJDIR)" UTILMK="../../$(UTILMK)" CC="$(CC)" CFLAGS="$(CFLAGS) $(addprefix -I../../$(SRCDIR)/,$(BASEDIRS))"
MKENV2 = RCDIR="$(SRCDIR)" OBJDIR="$(OBJDIR)" UTILMK="$(UTILMK)" CC="$(CC)" CFLAGS="$(CFLAGS)"

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
ifndef OS
define MKSRC
$(subst .mk,,$(subst $(OBJDIR)/,$(SRCDIR)/,$@))
endef
else
define MKSRC
$(subst .mk,,$(subst $(OBJDIR)/,$(SRCDIR)\,$@))
endef
endif
$(OBJDIR)/%.mk: $(wildcard $(SRCDIR)/$(NAME)/*.c $(SRCDIR)/$(NAME)/*.h)
	@echo Writing $@...
	@echo include $$$(esc)(UTILMK$(esc)) > $@
	@$(echoblank) >> $@
	@echo all: $$$(esc)(OUTDIR$(esc)) $(addprefix ../../$(OUTDIR)/,$(notdir $(CFILES:.c=.o))) >> $@
	@$(echoblank) >> $@
	@$(MAKE) --silent --no-print-directory -C "$(MKSRC)" -f ../../gen.mk NAME="$(subst .mk,,$(subst $(OBJDIR)/,,$@))" ${MKENV} MKRULES=y
endif

$(OUTDIR)/%.o: FORCE
	@$(CC) $(CFLAGS) -MM "$(notdir $(@:.o=.c))" -MT "$@" >> $(MKOUT)
	@echo $(TAB)$$$(esc)(COMPC$(esc)) >> $(MKOUT)
	@$(echoblank) >> $(MKOUT)

FORCE:

.PHONY: all $(BASEDIRS)

