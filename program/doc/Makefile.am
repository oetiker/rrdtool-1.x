## Process this file with automake to produce Makefile.in

SUFFIXES = .pod .1 .man .html .txt .pm .pdf .src .inc

#AUTOMAKE_OPTIONS        =  foreign

#ACLOCAL_M4 = $(top_srcdir)/config/aclocal.m4

CLEANFILES = *.1 *.html *.txt *-dircache *.pm *.pdf *~ core *itemcache *.rej *.orig rrdgraph_*.pod *.tmp rrdgraph.pod

SRC = rrdgraph.src rrdgraph_examples.src  rrdgraph_rpn.src \
      rrdgraph_data.src rrdgraph_graph.src

POD = rrdtool.pod rrdlast.pod rrdcreate.pod rrdupdate.pod  rrdtutorial.es.pod \
	cdeftutorial.pod rpntutorial.pod rrdgraph-old.pod  bin_dec_hex.pod \
	rrdfetch.pod rrdrestore.pod rrddump.pod rrdtune.pod rrdresize.pod \
	rrdxport.pod rrdcgi.pod rrdtutorial.pod rrdinfo.pod rrdthreads.pod \
	rrd-beginners.pod \
	$(SRC:.src=.pod)

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
imandir = $(mandir)/man1
iman_DATA = $(MAN)

all-local: link txt man html-local

.src.pod:
	perl -n -e 'if (/^=include\s+(\S+)/){open F,"$$1.inc" || die $$?;print <F>; close F} else {print}'  $<  > $@

.pod.1 .pm.1 .pl.1:
	pod2man --release=$(VERSION) --center=rrdtool $<  > $@

.1.txt:
	@NROFF@ -man -Tlp $< > $@

.1.pdf:
	@TROFF@ -man $< | ps2pdf - $@

.pm.html .pod.html .pl.html:
	pod2html --infile=$< --outfile=$@ --noindex --htmlroot=. --podpath=. --title=$*

RRDs.pm:
	ln -s ../bindings/perl-shared/RRDs.pm .

RRDp.pm:
	ln -s ../bindings/perl-piped/RRDp.pm .

index.html:
	rm -f index.html; ln -s rrdtool.html index.html

link: RRDp.pm RRDs.pm index.html

man: $(MAN)

html-local: $(HTML)

txt: $(TXT)

pdf-local: $(PDF)

