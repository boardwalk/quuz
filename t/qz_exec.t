use strict;
use warnings;
use Test::Base;
use Quuz::Filters;

sub run_ {
  my $data = shift;
  my ($code, $stdout, $stderr) = with_valgrind($data, "./quuz", "-r");
  die "expected success" if ($code != 0);
  die "expected empty stderr" if ($stderr);
  $stdout;
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

=== Variable arguments
--- input
(define (x . args)
  (write args))
(write "boof")
(x 1 2 3)
--- expected
"boof"(1 2 3)

=== Factorial
6! and above fail!
--- input
(define (factorial n)
  (if (eqv? n 0)
    1
    (* (factorial (- n 1)) n)))
(write (factorial 0)) (newline)
(write (factorial 1)) (newline)
(write (factorial 2)) (newline)
(write (factorial 3)) (newline)
(write (factorial 4)) (newline)
(write (factorial 5))
--- expected
1
1
2
6
24
120
