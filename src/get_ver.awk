# fetch rrdtool version number from input file and write them to STDOUT
BEGIN {
  while ((getline < ARGV[1]) > 0) {
    if (match ($0, /^PACKAGE_VERSION=/)) {
      split($1, t, "=");
      my_ver_str = substr(t[2],2,length(t[2])-2);
      split(my_ver_str, v, ".");
      gsub("[^0-9].*$", "", v[3]);
      my_ver = v[1] "," v[2] "," v[3];
    }
    if (match ($0, /^NUMVERS=/)) {
      split($1, t, "=");
      my_num_ver = t[2];
    }
  }
  print "RRD_VERSION = " my_ver "";
  print "RRD_VERSION_STR = " my_ver_str "";
  print "RRD_NUMVERS = " my_num_ver "";
}
