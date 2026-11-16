#!/bin/sh
# mkpdf.sh TROFF PS2PDF PAGE.1 PAGE.pdf
#
# Format a man page as PDF. With SOURCE_DATE_EPOCH set (release builds)
# the PDF is made reproducible: Ghostscript stamps the current time into
# the document info, the XMP metadata and the trailer /ID, and only
# Debian's patched Ghostscript reads SOURCE_DATE_EPOCH, so they are left
# out (pdfwrite's Omit* options, Ghostscript 9.55 and later; older
# versions ignore them).
set -e
troff=$1 ps2pdf=$2 in=$3 out=$4

if [ -n "$SOURCE_DATE_EPOCH" ]; then
    $troff -man "$in" | $ps2pdf -dOmitID -dOmitXMP -dOmitInfoDate - "$out"
else
    $troff -man "$in" | $ps2pdf - "$out"
fi
