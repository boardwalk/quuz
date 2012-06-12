use strict;
use warnings;
use Test::Base;
use Quuz::Filters;

sub run_ {
  my $data = shift;
  with_valgrind($data, "./quuz", "-r");
  "OK!";
}

filters { input => 'run_', expected => 'chomp' };

__END__

=== Call non-function
--- input
("what the fuck!")
--- expected
OK!

=== Generate an error inside a scheme function
--- input
((lambda () (foo)))
--- expected
OK!

=== Invalid use of => in cond
--- input
(cond (#t =>))
--- expected
OK!

