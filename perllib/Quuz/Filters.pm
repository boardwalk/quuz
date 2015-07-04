package Quuz::Filters;
use strict;
use warnings;
use IPC::Open3;
use File::Which;

our @ISA = qw(Exporter);
our @EXPORT = qw(external with_valgrind);

my $have_valgrind = defined(which('valgrind'));

if(not $have_valgrind) {
    print STDERR "warning: valgrind not found\n";
}

sub external {
  my $data = shift;

  my ($chld_in, $chld_out, $chld_err);
  use Symbol 'gensym'; $chld_err = gensym;
  my $pid = open3($chld_in, $chld_out, $chld_err, @_);

  print $chld_in $data;
  close $chld_in;

  local $/;
  my $stdout = <$chld_out>;
  my $stderr = <$chld_err>;
  close $chld_out;
  close $chld_err;

  waitpid($pid, 0);
  ($?, $stdout, $stderr);
}

sub with_valgrind {
  my $data = shift;
  if($have_valgrind) {
    external($data, "valgrind", "--quiet", "--error-exitcode=1", @_);
  }
  else {
    external($data, @_);
  }
}

1;
