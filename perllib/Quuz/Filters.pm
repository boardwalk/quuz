package Quuz::Filters;
use strict;
use warnings;
use IPC::Open2;
use File::Which;

our @ISA = qw(Exporter);
our @EXPORT = qw(external with_valgrind);

my $have_valgrind = defined(which('valgrind'));

if(not $have_valgrind) {
    print STDERR "warning: valgrind not found\n";
}

sub external {
  my $data = shift;

  my ($chld_out, $chld_in);
  my $pid = open2($chld_out, $chld_in, @_);

  print $chld_in $data;
  close $chld_in;

  local $/;
  $data = <$chld_out>;
  close $chld_out;

  waitpid($pid, 0);
  die "external failed" if ($?);

  $data;
}

sub with_valgrind {
  my $data = shift;
  if($have_valgrind) {
    external($data, "valgrind", "--quiet", "--leak-check=full",
      "--show-reachable=yes", "--error-exitcode=1",
      "--suppressions=yyparse.supp", @_);
  }
  else {
    external($data, @_);
  }
}

