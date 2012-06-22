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
--- input
(define x (lambda () (write "foo")))
(x)
--- expected
"foo"

=== Arguments
--- input
(write "hi") ((lambda (x) (write (+ x 3))) 5)
--- expected
"hi"8

=== Exceptions
--- ONLY
--- input
(with-exception-handler
  (lambda (obj)
    (write "Caught exception ")
    (write obj))
  (lambda ()
    (write "Boo!")
    (raise "Snickerdoodle")
    (write "Bojang!")))
--- expected
"Boo!""Caught exception ""Snickerdoodle"
