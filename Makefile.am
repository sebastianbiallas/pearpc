AUTOMAKE_OPTIONS = foreign

EXTRA_DIST	= TODO ChangeLog README COPYING AUTHORS
SUBDIRS		= src scripts

dist2: distdir
	$(AMTAR) chof - $(distdir) | BZIP2=$(BZIP2_ENV) bzip2 -c >$(distdir).tar.bz2
	$(am__remove_distdir)

dist-all2: dist2
