use strict;
use warnings;
use Test::Base;
use Quuz::Filters;

sub run_ {
  my $data = shift;
  with_valgrind($data, "./quuz", "-r");
}

filters { input => 'run_', expected => 'chomp' };

__END__

=== Write
--- input
(write 'x)
--- expected
x

=== Define
--- input
(define x "foo")
(write x)
--- expected
"foo"

=== Lambda
Leaks
--- SKIP
--- input
(define x (lambda () 'y))
(write (x))
--- expected
y

=== Lambda 2
Leaks
--- SKIP
--- input
(define x (lambda () (write 'foo)))
(x)
--- expected
foo

=== Lambda 3
Leaks
--- SKIP
--- input
(define x (lambda () (write "foo")))
(x)
--- expected
"foo"

