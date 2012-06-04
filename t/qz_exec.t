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
Creates a cyclic reference to the environment
--- SKIP
--- input
(define x (lambda () (write "bar")))
(x) (x) (x)
--- expected
"bar""bar""bar"

