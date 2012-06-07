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

=== Various
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

