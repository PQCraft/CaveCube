include $(COMMONMK)

all: $(OFILES)

$(OUTDIR)/%.o: %.c $(wildcard *.h) | $(OUTDIR)
	@$(ccmsg)
	@$(CC) -D_7ZIP_ST $(CFLAGS) $(notdir $(@:.o=.c)) -Wuninitialized -c -o $@
	@$(cdmsg)

clean: cleanoutdir

.PHONY: all clean

