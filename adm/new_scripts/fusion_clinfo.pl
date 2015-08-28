#!/usr/bin/perl
use constantes;
use Time::localtime;
use Math::BigInt;
use commun('PERROR', 'PWARNING', 'PDEBUG', 'PDEBUGV', 'PSYSTEM', 'PTRACECMD');
use Getopt::Long;


sub init {
  $complet = 1;

  if (GetOptions('d|debug=i' => \$constantes::DEBUG_LEVEL) == 0) {
    &usage;
    exit 1;
  }

  if (scalar(@ARGV) != 1) {
    PERROR("bad number of arguments\n");
    &usage;
    exit 1;
  }

  $conffile = $constantes::DEFAULT_CONFFILE;
  $clustername = $ARGV[0];

  PDEBUG("conffile = $conffile cluster = $clustername\n");
}

sub usage {
  $date = (stat(__FILE__))[9];
  $tm = localtime($date);
  my $HIGH = `tput bold`;
  my $NORM = `tput sgr0`;

  printf(".$HIGH Usage$NORM : fusion_clinfo [option] CLUSTERNAME\n");
  printf("\tAffiche l'état actuel du cluster 'CLUSTERNAME'\n");

  printf("\n.$HIGH Exemple$NORM :\n");
  printf("$HIGH\t> ./fusion_clinfo sam$NORM\n");
  printf("\n\n** %s \n** v%s (%02d/%02d/%04d)\n",
	__FILE__,$constantes::ADMCMD_VERSION, $tm->mday, $tm->mon+1, $tm->year+1900);
}


sub info_cluster {
  foreach $host_name (@hostslist) {
    print "\n****** STATUS OF NODE $host_name ******\n";
    PSYSTEM("ssh $host_name cat $constantes::VRTPROCCMD");
  }
}

&init;
@hostslist = &commun::gethostslist($conffile,$clustername);
PDEBUG("liste des noeuds : @hostslist\n");

info_cluster;

&commun::byebye($complet);
