include $(COMMONMK)

all: $(OFILES)

$(OUTDIR)/%.o: %.c $(wildcard *.h) | $(OUTDIR)
	@$(ccmsg)
	@$(CC) $(CFLAGS) $(notdir $(@:.o=.c)) -Wuninitialized -c -o $@
	@$(cdmsg)

clean: cleanoutdir

.PHONY: all clean

