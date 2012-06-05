use strict;
use warnings;
use Test::Base;
use Quuz::Filters;

sub exec {
  my $data = shift;
  with_valgrind($data, "./quuz", "-e");
}

filters { input => 'exec', expected => 'chomp' };

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
--- input
(define x (lambda () 'y))
(write (x))
--- expected
y

=== Lambda 2
--- input
(define x (lambda () (write 'foo)))
(x)
--- expected
foo

=== Lambda 3
Valgrind errors
--- SKIP
--- input
(define x (lambda () (write "foo")))
(x)
--- expected
"foo"

