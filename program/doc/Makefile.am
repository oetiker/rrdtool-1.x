## Process this file with automake to produce Makefile.in

SUFFIXES = .pod .1 .man .html .txt .pm .pdf

#AUTOMAKE_OPTIONS        =  foreign

#ACLOCAL_M4 = $(top_srcdir)/config/aclocal.m4

CLEANFILES = *.1 *.html *.txt *-dircache *.pm *.pdf *~ core *itemcache *.rej *.orig

POD = rrdtool.pod rrdlast.pod rrdcreate.pod rrdupdate.pod  rrdtutorial.es.pod \
	cdeftutorial.pod rpntutorial.pod rrdgraph.pod  bin_dec_hex.pod \
	rrdfetch.pod rrdrestore.pod rrddump.pod rrdtune.pod rrdresize.pod \
	rrdcgi.pod rrdtutorial.pod rrdinfo.pod

PMP = RRDs.pm RRDp.pm

MAN = $(POD:.pod=.1) $(PMP:.pm=.1) 
TXT = $(MAN:.1=.txt)
HTML = $(POD:.pod=.html) $(PMP:.pm=.html) 
PDF = $(MAN:.1=.pdf)

# what should go into the distribution
EXTRA_DIST= $(POD) $(HTML) $(TXT)

# some install rules
idocdir = $(prefix)/doc
idoc_DATA = $(POD) $(TXT)
ihtmldir = $(prefix)/html
ihtml_DATA = $(HTML)
imandir = $(prefix)/man/man1
iman_DATA = $(MAN)

all-local: link txt man html

.pod.1 .pm.1 .pl.1:
	pod2man --release=$(VERSION) --center=rrdtool $<  > $@

.1.txt:
	@NROFF@ -man -Tlp $< > $@

.1.pdf:
	@TROFF@ -man $< | ps2pdf - $@

.pm.html .pod.html .pl.html:
	pod2html --infile=$< --outfile=$@ --noindex --htmlroot=. --podpath=. --title=$*

RRDs.pm:
	ln -s ../perl-shared/RRDs.pm .

RRDp.pm:
	ln -s ../perl-piped/RRDp.pm .

link: RRDp.pm RRDs.pm

man: $(MAN)

html: $(HTML)

txt: $(TXT)

pdf: $(PDF)

