use strict;
use warnings;
use Test::Base;
use Quuz::Filters;

sub run_ {
  my $data = shift;
  my ($code, $stdout, $stderr) = with_valgrind($data, "./quuz", "-r");
  die "expected failure" if ($code == 0);
  chomp $stderr;
  $stderr;
}

filters { input => 'run_', expected => 'chomp' };

__END__

=== Call non-function
--- input
("what the fuck!")
--- expected
An error occurred: [error "uncallable value" ("what the fuck!")]

=== Generate an error inside a scheme function
--- input
((lambda () (foo)))
--- expected
An error occurred: [error "unbound variable" (foo)]

=== Invalid use of => in cond
--- input
(cond (#t =>))
--- expected
An error occurred: [error "expected list" (())]

=== Unbound variable in list
--- input
(list 1 2 a)
--- expected
An error occurred: [error "unbound variable" (a)]

