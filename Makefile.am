AUTOMAKE_OPTIONS = foreign

EXTRA_DIST	= TODO ChangeLog README COPYING AUTHORS ppccfg.example video.x sdl.m4
SUBDIRS		= doc src scripts

dist2: distdir
	-chmod -R a+r $(distdir)
	BZIP2=$(BZIP2_ENV) $(TAR) chojf $(distdir).tar.bz2 $(distdir)
	-rm -rf $(distdir)

dist-all2: dist2
