package Quuz::Filters;
use strict;
use warnings;
use IPC::Open2;

our @ISA = qw(Exporter);
our @EXPORT = qw(external with_valgrind);

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
  external($data, "valgrind", "--quiet", "--leak-check=full",
    "--show-reachable=yes", "--error-exitcode=1",
    "--suppressions=yyparse.supp", @_);
}

