#!/usr/local/bin/perl -w

require 5.005;
use strict;

# The glyps list can be downloaded from
# http://partners.adobe.com/asn/developer/type/glyphlist.txt
# This URL is from this page:
# http://partners.adobe.com/asn/developer/type/unicodegn.html
# which is refered from
# http://partners.adobe.com/asn/developer/technotes/fonts.html

my $onlyHelvetica = 0;

my %globalName2Unicode;
my %font_code = ();

my $indent0 = "";
my $indent1 = "  ";
my $indent2 = $indent1 x 3;

my $q = 0;
my $qU = 0;

sub read_glyphlist
{
  my $fn ="glyphlist.txt";
  open(FH, $fn)
  || die "Can't read $fn\n";
  my %seen = ();
  while (<FH>) {
    next if /^\s*#/;
    next unless /^([0-9A-F]{4});(\w+);/;
    my $unicode = 0 + hex($1);
    my $name = $2;
    next if ($globalName2Unicode{$name});
    $globalName2Unicode{$name} = $unicode;
  }
  close(FH);
}

sub process_all_fonts
{
  my $dir = ".";
  my $wc = "*.afm";
  $wc = "Helvetica.afm" if $onlyHelvetica;
  $wc = "ZapfDin.afm" if 0;
  $wc = "Helve*.afm" if 0;
  $wc = "Times-BoldItalic.afm" if 0;
  foreach my $fn (glob("$dir/$wc")) {
    process_font($fn);
  }
}

sub process_font
{
  my ($fn) = @_;
  print STDERR "Compiling afm file: $fn\n";
  my %fi = (); # font info
  my $c = "";
  $fi{C} = \$c;
  $fi{ligaturesR} = {};
  $fi{FontSpecificUnicodeNameToChar} = {};
  $fi{filename} = $fn;
  $fi{filename} =~ s/.*\///;
  open(FH, $fn) || die "Can't open $fn\n";
  print STDERR "Reads global font info\n" if $q;
  while (<FH>) {
    chomp;
    next if /^\s*$/ || /^\s*#/;
    last if /^StartCharMetrics/;
    next unless (/^(\S+)\s+(\S(.*\S)?)/);
    my $id = $1;
    my $value = $2;
    $value =~ s/\s+/ /g;
    $fi{"Afm$id"} = $value;
  }
  my $fontName = $fi{AfmFontName};
  $c .= "\n\n/* ". ("-" x 66) . "*/\n";
  $c .= "/* FontName: $fontName */\n";
  $c .= "/* FullName: $fi{AfmFullName} */\n";
  $c .= "/* FamilyName: $fi{AfmFamilyName} */\n";
  $fi{cName} = $fontName;
  $fi{cName} =~ s/\W/_/g;
  my %charMetrics = ();
  my %kerning = ();
  read_charmetrics(\%fi, \%charMetrics);
  while (<FH>) {
    read_kerning(\%fi, \%kerning) if /^StartKernPairs/;
  }
  if (0) {
    my @names = keys %charMetrics;
    print STDERR "Did read ", ($#names + 1), " font metrics\n";
  }
  write_font(\%fi, \%charMetrics, \%kerning);
}

sub read_charmetrics
{
  my ($fiR, $charMetricsR) = @_;
  print STDERR "Reads char metric info\n" if $q;
  my $isZapfDingbats = $$fiR{AfmFontName} eq "ZapfDingbats";
  my $ligaturesR = $$fiR{ligaturesR};
  my %ligatures = ();
  my %seenUnicodes = ();
  while (<FH>) {
    chomp;
    next if /^\s*$/ || /^\s*#/;
    last if /^EndCharMetrics/;
#next unless /N S / || /N comma /;
#next unless /N ([sfil]|fi) /;
#print "$_\n";
    my $line = $_;
# C 102 ; WX 333 ; N f ; B -169 -205 446 698 ; L i fi ; L l fl ;
    my ($width, $unicode, $name, @charLigatures);
    foreach (split/\s*;\s*/, $line) {
      if (/^C\s+(-?\d+)/) {
        $unicode = 0 + $1;
      } elsif (/^N\s+(\w+)/) {
        $name = $1;
      } elsif (/^WX?\s+(-?\d+)/) {
        $width = normalize_width($1, 0);
      } elsif (/^L\s+(\w+)\s+(\w+)/) {
        push(@charLigatures, $1, $2);
      }
    }
    if ($unicode < 0) {
      unless (defined $name) {
        print STDERR "Glyph missing name and code: $_\n";
        next;
      }
      $unicode = name2uni($fiR, $name);
      print STDERR "name2uni: $name -> $unicode\n" if $qU && 0;
    } elsif (defined $name) {
      my $std = $globalName2Unicode{$name};
      if (!defined $std) {
        print STDERR "Adds unicode mapping: ",
          "$name -> $unicode\n" if $qU;
        ${$$fiR{FontSpecificUnicodeNameToChar}}{$name} = $unicode;
      } else {
        $unicode = $std;
      }
    }
    if (!defined($unicode) || $unicode <= 0) {
      next if $isZapfDingbats && $name =~ /^a(\d+)$/;
      next if $$fiR{AfmFontName} eq "Symbol" && $name eq "apple";
      print STDERR "Glyph '$name' has unknown unicode: $_\n";
      next;
    }
    unless (defined $width) {
      print STDERR "Glyph '$name' missing width: $_\n";
      next;
    }
    if ($seenUnicodes{$unicode}) {
      print STDERR "Duplicate character: unicode = $unicode, ",
        "$name and ", $seenUnicodes{$unicode},
        " (might be due to Adobe charset remapping)\n";
      next;
    }
    $seenUnicodes{$unicode} = $name;
    my %c = ();
    $c{name} = $name;
    $c{unicode} = $unicode;
    $c{width} = $width;
    $$charMetricsR{$unicode} = \%c;
    $ligatures{$unicode} = \@charLigatures if $#charLigatures >= 0;
  }
  foreach my $unicode (keys %ligatures) {
    my $aR = $ligatures{$unicode};
    my $unicode2 = name2uni($fiR, $$aR[0]);
    my $unicode3 = name2uni($fiR, $$aR[1]);
    unless ($unicode2) {
      print STDERR "Missing ligature char 1: $$aR[0]\n";
      next;
    }
    unless ($unicode3) {
      print STDERR "Missing ligature char 2: $$aR[1]\n";
      next;
    }
    my $key = sprintf("%04d;%04d", $unicode, $unicode2);
    $$ligaturesR{$key} = $unicode3;
  }
}

sub name2uni
{
  my ($fiR, $name) = @_;
  my $fontMapR = $$fiR{FontSpecificUnicodeNameToChar};
  return $globalName2Unicode{$name} || $$fontMapR{$name};
}

sub read_kerning
{
  my ($fiR, $kerningR) = @_;
  print STDERR "Reads kerning info\n" if $q;
  while (<FH>) {
    chomp;
    next if /^\s*$/ || /^\s*#/;
    last if /^EndKernPairs/;
    unless (/^KPX\s+(\w+)\s+(\w+)\s+(-?\d+)\s*$/) {
      print STDERR "Can't parse kern spec: $_\n";
      next;
    }
    my $name1 = $1;
    my $name2 = $2;
    my $delta = normalize_width($3, 1);
    next unless $delta;
    my $unicode1 = name2uni($fiR, $name1);
    my $unicode2 = name2uni($fiR, $name2);
    unless ($unicode1 && $unicode2) {
      print "Unknown kern pair: $name1 and $name2\n";
      next;
    }
    my $charR = $$kerningR{$unicode1};
    unless (defined $charR) {
      my %c = ();
      $charR = \%c;
      $$kerningR{$unicode1} = $charR;
    }
    $$charR{$unicode2} = $delta;
  }
}

sub write_font
{
  my ($fiR, $charMetricsR, $kerningR) = @_;
  print STDERR "Writes font\n" if $q;
  my $cR = $$fiR{C};
  $$fiR{widthsA} = make_array();
  $$fiR{kerning_indexA} = make_array();
  $$fiR{kerning_dataA} = make_array();
  $$fiR{highchars_indexA} = make_array();
  $$fiR{ligaturesA} = make_array();
  write_font_metrics($fiR, $charMetricsR, $kerningR);
  write_ligatures($fiR);
  my $widths_count = array_size($$fiR{widthsA});
  my $kerning_index_count = array_size($$fiR{kerning_indexA});
  my $kerning_data_count = array_size($$fiR{kerning_dataA});
  my $highchars_count = array_size($$fiR{highchars_indexA});
  my $ligatures_count = array_size($$fiR{ligaturesA}) / 3;
  my $info_code = "";
  my $i2 = $indent2;
  my $packedSize = $widths_count + 2 * $kerning_index_count +
     $kerning_data_count + 2 * $highchars_count +
     3 * 2 * $ligatures_count;
  $info_code .= $indent1 . "{ /* $$fiR{filename}   $packedSize bytes */\n";
    $info_code .= $i2 . "\"$$fiR{AfmFontName}\",";
    $info_code .= " \"$$fiR{AfmFullName}\",\n";
    $info_code .= $i2 . $$fiR{widthsACName} . ",\n";
    $info_code .= $i2 . $$fiR{kerning_indexACName} . ",\n";
    $info_code .= $i2 . $$fiR{kerning_dataACName} . ",\n";
    $info_code .= $i2 . $$fiR{highchars_indexACName} . ", ";
    $info_code .= $highchars_count . ",\n";
    $info_code .= $i2 . $$fiR{ligaturesACName} . ", ";
    $info_code .= $ligatures_count;
    $info_code .= "},\n";
  $font_code{$$fiR{AfmFullName}} = { TABLES => $$cR, INFO => $info_code};
}

sub write_font_metrics
{
  my ($fiR, $charMetricsR, $kerningR) = @_;
  print STDERR "Writes font metrics\n" if $q;
  my $lastUnicode = 31;
  my $cR = $$fiR{C};
  my $widthsA = $$fiR{widthsA};
  my $kerning_indexA = $$fiR{kerning_indexA};
  my $kerning_dataA = $$fiR{kerning_dataA};
  my $highchars_indexA = $$fiR{highchars_indexA};
  my @uniArray = sort { $a <=> $b } keys %$charMetricsR;
  my $highchars_count = 0;
  my $had_kerning = 0;
  while (1) {
    my $fill = 0;
    if ($#uniArray < 0) {
      last if $lastUnicode > 126;
      $fill = 1;
    } elsif ($lastUnicode < 126 && $uniArray[0] > $lastUnicode + 1) {
      $fill = 1;
    }
    if ($fill) {
      $lastUnicode++;
#print STDERR "fill for $lastUnicode, $#uniArray, $uniArray[0]\n";
      append_to_array($widthsA, 0);
      append_to_array($kerning_indexA, 0);
      next;
    }
    my $unicode = shift @uniArray;
    next if $unicode < 32;
    $lastUnicode = $unicode;
    my $metricsR = $$charMetricsR{$unicode};
    if ($unicode > 126) {
      append_to_array($highchars_indexA, $unicode);
      $highchars_count++;
    }
    my $m = $$metricsR{width};
    $m = "/* ".array_size($widthsA)."=$unicode */". $m if 0;
    append_to_array($widthsA, $m);
    my $kerningInfoR = $$kerningR{$unicode};
    my $kerning_index = 0;
    if (defined $kerningInfoR) {
      my @kerns = ();
      my $numKernings = 0;
      foreach my $unicode2 (sort { $a <=> $b } keys %$kerningInfoR) {
        my $delta = $$kerningInfoR{$unicode2};
        $numKernings++;
        append_escaped_16bit_int(\@kerns, $unicode2);
        push(@kerns, $delta);
        $had_kerning = 1;
      }
      $kerning_index = append_8bit_subarray($kerning_dataA, $numKernings, @kerns);
    }
    append_to_array($kerning_indexA, $kerning_index);
  }
  $$fiR{kerning_indexA} = make_array() if !$had_kerning;
  write_array($fiR, "widths", "afm_cuint8");
  write_array($fiR, "kerning_index", "afm_sint16");
  write_array($fiR, "kerning_data", "afm_cuint8");
  write_array($fiR, "highchars_index", "afm_cuint16");
}

sub write_ligatures
{
  my ($fiR) = @_;
  print STDERR "Writes font ligatures\n" if $q;
  my $ligaturesA = $$fiR{ligaturesA};
  my $ligaturesR = $$fiR{ligaturesR};
  foreach (sort keys %$ligaturesR) {
    unless (/^(\w{4});(\w{4})$/) {
      die "Invalid ligature key: $_";
    }
    append_to_array($ligaturesA, $1 + 0, $2 + 0, $$ligaturesR{$_});
  }
  write_array($fiR, "ligatures", "afm_cunicode");
}

sub indent
{
  my ($num) = @_;
  return "  " x $num;
}

sub make_array
{
  my @a = ();
  return \@a;
}

sub append_to_array
{
  my ($aR, @newElements) = @_;
  my $z1 = array_size($aR);
  push(@$aR, @newElements);
  my $z2 = array_size($aR);
  my $zz = $#newElements +1;
}

sub append_8bit_subarray
{
  my ($aR, $numItems, @newElements) = @_;
  push(@$aR, 42) if !array_size($aR); # initial dummy value
  die if $numItems > $#newElements + 1;
  my $idx = $#{$aR} + 1;
#print "append_8bit_subarray ", ($#newElements+1), " = (", join(", ", @newElements), ") -> $idx\n";
  append_escaped_16bit_int($aR, $numItems);
  push(@$aR, @newElements);
  die "Can't handle that big sub array, sorry...\n" if $idx > 50000;
  return $idx;
}

sub append_escaped_16bit_int
{
  my ($aR, $count) = @_;
  die "Invalid count = 0\n" unless $count;
  if ($count >= 510) {
    push(@$aR, 1, int($count / 256), int($count % 256));
    print STDERR "full: $count\n" if 0;
  } elsif ($count >= 254) {
    push(@$aR, 0, $count - 254);
    print STDERR "semi: $count\n" if 0;
  } else {
    push(@$aR, $count + 1);
  }
}

sub array_size
{
  my ($aR) = @_;
  return $#{$aR} + 1;
}

sub write_array
{
  my ($fiR, $name, $type) = @_;
  my $aR = $$fiR{$name."A"};
  my $cName = $$fiR{cName};
  my $num = $#{$aR} + 1;
  my $array_name_key = $name."ACName";
  if ($num == 0) {
    $$fiR{$array_name_key} = "NULL";
    return;
  }
  my $cR = $$fiR{C};
  my $array_name = "afm_" . $cName . "_" . $name;
  $$fiR{$array_name_key} = $array_name;
  $$cR .= "static $type $array_name" . "[] = { /* $num */\n";
  my $line = $indent1;
  for (my $i = 0; $i < $num; $i++) {
    $line .= "," if $i > 0;
    if (length($line) > 65) {
      $line .= "\n";
      $$cR .= $line;
      $line = $indent1;
    }
    $line .= $$aR[$i];
  }
  $line .= "\n";
  $$cR .= $line;
  $$cR .= "};\n";
}

sub normalize_width
{
  my ($w, $signed) = @_;
  my $n = int(($w + 3) / 6);
  if ($signed) {
    $n = -128 if $n < -128;
    $n =  127 if $n >  127;
    $n =  256 + $n if $n < 0; # make unsigned.
  } else {
    $n =    0 if $n <    0;
    $n =  255 if $n >  255;
  }
  return $n;
}

sub main
{
  my $cfn = "../../src/rrd_afm_data.c";
  read_glyphlist();
  process_all_fonts();
  my @fonts = sort keys %font_code;
  unless ($#fonts >= 0) {
    die "You must have at least 1 font.\n";
  }
  open(CFILE, ">$cfn") || die "Can't create $cfn\n";
  print CFILE header($cfn);
  print CFILE ${$font_code{$_}}{TABLES} foreach @fonts;
  print CFILE "const afm_fontinfo afm_fontinfolist[] = {\n";
  print CFILE ${$font_code{$_}}{INFO} foreach @fonts;
  print CFILE $indent1 . "{ 0, 0, 0 }\n";
  print CFILE $indent0 . "};\n";
  print CFILE $indent0 . "const int afm_fontinfo_count = ",
    ($#fonts + 1), ";\n";
  close(CFILE);
  print STDERR "Compiled ", ($#fonts+1), " fonts.\n";
}

sub header
{
  my ($fn) = @_;
  $fn =~ s/.*\///;
  my $h = $fn;
  $h =~ s/\.c$/.h/;
  return <<"END";
/****************************************************************************
 * RRDtool 1.1.x  Copyright Tobias Oetiker, 1997 - 2002
 ****************************************************************************
 * $fn  Encoded afm (Adobe Font Metrics) for selected fonts.
 ****************************************************************************
 *
 * THIS FILE IS AUTOGENERATED BY PERL. DO NOT EDIT.
 *
 ****************************************************************************/

#include "$h"
#include <stdlib.h>

END
}

main();
