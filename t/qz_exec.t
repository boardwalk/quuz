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

=== Define
Not yet implemented
--- SKIP
--- input
(define x 1)
--- expected
