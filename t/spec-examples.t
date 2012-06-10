use strict;
use warnings;
use Test::Base;
use Quuz::Filters;

sub eval {
  my $data = shift;
  with_valgrind($data, "./quuz", "-e");
}

filters { input => 'eval' };

__END__

=== Quoting
--- input
(quote a)
(quote #(a b c))
(quote (+ 1 2))
'a
'#(a b c)
'()
'(+ 1 2)
'(quote a)
''a
'"abc"
"abc"
'145932
'#t
#t
--- expected
a
#(a b c)
(+ 1 2)
a
#(a b c)
()
(+ 1 2)
(quote a)
(quote a)
"abc"
"abc"
145932
#t
#t

=== Procedure calls
--- input
(+ 3 4)
((if #f + *) 3 4)
--- expected
7
12

=== If
--- input
(if (> 3 2) 'yes 'no)
(if (> 2 3) 'yes 'no)
(if (> 3 2)
  (- 3 2)
  (+ 3 2))
--- expected
yes
no
1

=== Procedures
--- SKIP
--- input
(lambda (x) (+ x x))
((lambda (x) (+ x x)) 4)
(define reverse-subtract
  (lambda (x y) (- y x)))
(reverse-subtract 7 10)
(define add4
  (let ((x 4))
    (lambda (y) (+ x y))))
(add4 6)
--- expected
<procedure>
8
3
10

=== Assignments
--- SKIP
--- input
(define x 2)
(+ x 1)
(set! x 4)
(+ x 1)
--- expected
3
5

=== Cond
--- input
(cond ((> 3 2) 'greater)
      ((< 3 2) 'less))
(cond ((> 3 3) 'greater)
      ((< 3 3) 'less)
      (else 'equal))
;(cond ((assv 'b '((a b) (b 2))) => cadr)
;      (else #f))
(cond ((+ 1 2) => quote))
--- expected
greater
equal
3

=== Case
--- input
(case (* 2 3)
  ((2 3 5 7) 'prime)
  ((1 4 6 8 9) 'composite))
--- expected
composite

=== And
--- input
(and (= 2 2) (> 2 1))
(and (= 2 2) (< 2 1))
(and 1 2 'c '(f g))
(and)
--- expected
#t
#f
(f g)
#t

=== Or
--- input
(or (= 2 2) (> 2 1))
(or (= 2 2) (< 2 1))
(or #f #f #f)
;(or (memq 'b '(a b c))
;    (/ 3 0))
--- expected
#t
#t
#f

=== When
--- SKIP
--- input
(when (= 1 1.0)
  (display "1")
  (display "2"))
--- output
unspecified

=== Unless
--- SKIP
--- input
(unless (= 1 1.0)
  (display "1")
  (display "2"))
--- output
unspecified

