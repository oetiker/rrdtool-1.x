use strict;
use warnings;
use ExtUtils::Packlist;

my ($root, $file) = @ARGV;
exit unless -f $file;

my $list = ExtUtils::Packlist->new($file);
for my $path (keys %$list) {
    # MakeMaker versions differ on whether packlists include DESTDIR.
    $path =~ s/^\Q$root\E// if length $root;
    my $installed = "$root$path";
    if (-e $installed || -l $installed) {
        unlink $installed or die "unlink $installed: $!";
    }
}
unlink $file or die "unlink $file: $!";
