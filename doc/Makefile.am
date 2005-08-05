
%.1: %.sgml
	$(DOC_TO_MAN) $< > $@

distclean: clean
	rm -f Makefile

clean:
	rm -f ${man_MANS}

EXTRA_DIST = pxdiff.sgml

man_MANS = $(EXTRA_DIST:.sgml=.1)

