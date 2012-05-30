use strict;
use warnings;
use Test::Base;
use IPC::Open2;

sub external {
  my $data = shift;

  my ($chld_out, $chld_in);
  my $pid = open2($chld_out, $chld_in, @_);

  print $chld_in $data;
  close $chld_in;

  local $/;
  $data = <$chld_out>;
  close $chld_out;

  waitpid($pid, 0);
  die "external failed" if ($?);

  $data;
}

sub parse {
  my $data = shift;
  external($data, "./quuz", "-p");
}

__END__

=== Identifiers
--- input parse
( %domo arigato-m1suta <roboto>* for_helping$me esc&pe ?just/when I:needed=to~! )
--- expected chomp
 ( %domo arigato-m1suta <roboto>* for_helping$me esc&pe ?just/when I:needed=to~! )

=== Booleans
--- input parse
(#t #f #true #false)
--- expected chomp
 ( #t #f #t #f )

=== Strings
--- input parse
("zoop" "boop" "doopity\x20;" "doop\a\b\t\n\r\"\\\n")
--- expected chomp
 ( "zoop" "boop" "doopity " "doop\x07;\x08;\x09;\x0a;\x0d;\"\\\x0a;" )

=== Multiline strings
--- input parse
("zim zim \ 
 zalabim" "erf berf
bloggie berf")
--- expected chomp
 ( "zim zim zalabim" "erf berf\x0a;bloggie berf" )

=== Characters
--- input parse
(#\a #\A #\1 #\alarm #\backspace #\delete #\escape #\newline #\null #\return #\space #\tab #\x20 #\x)
--- expected chomp
 ( #\a #\A #\1 #\x07 #\x08 #\x7f #\x1b #\x0a #\x00 #\x0d #\x20 #\x09 #\x20 #\x )

=== Nested listed and vectors
--- input parse
( a #(b c) d (e #(f)) g)
--- expected chomp
 ( a #( b c ) d ( e #( f ) ) g )

=== Dotted list
--- input parse
(a . b)
--- expected chomp
 ( a . b )

=== Abbreviations
--- input parse
('a `b ,c ,@d)
--- expected chomp
 ( ( quote a ) ( quasiquote b ) ( unquote c ) ( unquote-splicing d ) )

=== Simple numbers
--- input parse
(#b1010 #o644 #d123 42 #xFF)
--- expected chomp
 ( 10 420 123 42 255 )

=== Bytevectors
--- input parse
#u8(127 0 0 1)
--- expected chomp
 #u8( #x7f #x00 #x00 #x01 )

=== Comments
--- input parse
(a ; Single line comment 
 b d
 e #| nested #| comments rock |# my world |# f)
--- expected chomp
 ( a b d e f )

=== Datum comment
Hits assert
--- SKIP
--- input parse
( a #; b c )
--- expected chomp
 ( a b c )

=== Case insensitivity
Not supported yet
--- SKIP
--- input parse
(#tRuE #fAlSe #X34 #\X10)
--- expected chomp
 ( #t #f 52 #\x10 )

=== Labels
Not supported yet
--- SKIP
--- input parse
#1=(1 2 #2=(#1# 3 4 5))
--- expected chomp
 #1= ( ( 1 2 ( #1# 3 4 5 ) ) )

