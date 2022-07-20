include $(UTILMK)

MKENV = SRCDIR="../../$(SRCDIR)" OBJDIR="../../$(OBJDIR)" UTILMK="../../$(UTILMK)" CC="$(CC)" CFLAGS="$(CFLAGS) $(addprefix -I../../$(SRCDIR)/,$(BASEDIRS))"

ifndef MKRULES
MKOUT = $(OBJDIR)/$@.mk
all: $(BASEDIRS)
else
MKOUT = $(OBJDIR)/$(NAME).mk
all: $(OFILES)
.NOTPARALLEL:
endif

$(BASEDIRS): $(OBJDIR) FORCE
	@echo Writing $@.mk...
	@echo include $$$(esc)(UTILMK$(esc)) > $(MKOUT)
	@$(echoblank) >> $(MKOUT)
	@echo all: $$$(esc)(OUTDIR$(esc)) $(addprefix ../../$(OUTDIR2)/,$(notdir $(CFILES2:.c=.o))) >> $(MKOUT)
	@$(echoblank) >> $(MKOUT)
	@$(MAKE) --silent --no-print-directory -C "$(SRCDIR)/$@" -f ../../gen.mk NAME="$@" ${MKENV} MKRULES=y
	@echo Wrote $@.mk

$(OUTDIR)/%.o: FORCE
#	@echo $@
	@$(CC) $(CFLAGS) -MM "$(notdir $(@:.o=.c))" -MT "$@" >> $(MKOUT)
	@echo $(TAB)$$$(esc)(COMPC$(esc)) >> $(MKOUT)
	@$(echoblank) >> $(MKOUT)

FORCE:

.PHONY: all $(BASEDIRS)

