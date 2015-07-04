use strict;
use warnings;
use Test::Base;
use Quuz::Filters;

sub parse {
  my $data = shift;
  my ($code, $stdout, $stderr) = with_valgrind($data, "./quuz", "-p");
  die "expected success" if ($code != 0);
  die "expected empty stderr" if ($stderr);
  $stdout;
}

filters { input => 'parse' };

__END__

=== Identifiers
--- input
(%domo arigato-m1suta <roboto>* for_helping$me esc&pe ?just/when I:needed=to~!)
--- expected
(%domo arigato-m1suta <roboto>* for_helping$me esc&pe ?just/when I:needed=to~!)

=== Booleans
--- input
(#t #f #true #false)
--- expected
(#t #f #t #f)

=== Strings
--- input
("" "zoop" "boop" "doopity\x20;" "doop\a\b\t\n\r\"\\\n")
--- expected
("" "zoop" "boop" "doopity " "doop\x07;\x08;\x09;\x0a;\x0d;\"\\\x0a;")

=== Multiline strings
--- input
("zim zim \ 
 zalabim" "erf berf
bloggie berf")
--- expected
("zim zim zalabim" "erf berf\x0a;bloggie berf")

=== Characters
--- input
(#\a #\A #\1 #\alarm #\backspace #\delete #\escape #\newline #\null #\return #\space #\tab #\x20 #\x)
--- expected
(#\a #\A #\1 #\x07 #\x08 #\x7f #\x1b #\x0a #\x00 #\x0d #\x20 #\x09 #\x20 #\x)

=== Nested listed and vectors
--- input
(a #(b c) d (e #(f)) #u8() #() () g)
--- expected
(a #(b c) d (e #(f)) #u8() #() () g)

=== Dotted list
--- input
(a . b)
(a b . (c d e))
--- expected
(a . b)
(a b c d e)

=== Abbreviations
--- input
('a `b ,c ,@d)
--- expected
((quote a) (quasiquote b) (unquote c) (unquote-splicing d))

=== Simple numbers
--- input
(#b1010 #b-1010 #o644 #d123 42 -127 #xFF)
--- expected
(10 -10 420 123 42 -127 255)

=== Bytevectors
--- input
#u8(127 0 0 1)
--- expected
#u8(#x7f #x00 #x00 #x01)

=== Comments
--- input
(a ; Single line comment 
 b d
 e #| nested #| comments rock |# my world |# f)
--- expected
(a b d e f)

=== Datum comment
Hits assert
--- SKIP
--- input
(a #; b c)
--- expected
(a c)

=== Case insensitivity
Not supported yet
--- SKIP
--- input
(#tRuE #fAlSe #X34 #\X10)
--- expected
(#t #f 52 #\x10)

=== Labels
Not supported yet
--- SKIP
--- input
#1=(1 2 #2=(#1# 3 4 5))
--- expected
#1=(1 2 (#1# 3 4 5))

