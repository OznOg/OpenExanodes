#!/usr/bin/perl
use Math::BigInt;

my $two = Math::BigInt->new(2);
my $x = $two**63 +25;
my $y = $two**63 +30;

if ($x == $y) {
  print "integer 64 bits non supportés. x=$x\n";
}
else {
  $x =~ s/\+//;
  print "je peux utiliser les integer 64 bits. x=$x\n";
}
