#!/usr/bin/perl
use constantes;
use Time::localtime;
use Math::BigInt;
use commun('PERROR', 'PWARNING', 'PDEBUG', 'PDEBUGV', 'PSYSTEM', 'PTRACECMD');
use Getopt::Long;


sub init {
  $complet = 1;

  if (GetOptions('f|force' => \$forcestop,
		 'd|debug=i' => \$constantes::DEBUG_LEVEL) == 0) {
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

  # pour propager les options de force et debug à vlstop
  if ($forcestop) {
    $forceopt = "--force";
  } else {
    $forceopt = "";
  }
  if ($constantes::DEBUG_LEVEL) {
    $debugopt = "--debug $constantes::DEBUG_LEVEL";
  } else {
    $debugopt = "";
  }

  PDEBUG("conffile = $conffile cluster = $clustername\n");
}

sub usage {
  $date = (stat(__FILE__))[9];
  $tm = localtime($date);
  my $HIGH = `tput bold`;
  my $NORM = `tput sgr0`;

  printf(".$HIGH Usage$NORM : fusion_clstop [option] CLUSTERNAME\n");
  printf("\tStoppe la fusion du stockage du cluster 'CLUSTERNAME en\n");
  printf("\tanalysant le fichier de configuration /etc/driveFUSION.conf\n");

  printf("\n.$HIGH Options$NORM :\n");
  printf("\t$HIGH-f, --force$NORM\n");
  printf("\t\tSi des volumes du cluster sont en cours d'utilisation,\n");
  printf("\t\ton force leur désactivation (risque important de faire\n");
  printf("\t\tplanter les applications qui les utilisent).\n");

  printf("\n.$HIGH Exemple$NORM :\n");
  printf("$HIGH\t> ./fusion_clstop sam$NORM\n");
  printf("\t\tStoppe la fusion du stockage du cluster 'sam'\n");

  printf("\n\n** %s \n** v%s (%02d/%02d/%04d)\n",
	__FILE__,$constantes::ADMCMD_VERSION, $tm->mday, $tm->mon+1, $tm->year+1900);
}


sub stop_cluster {
  foreach $host_name (@hostslist) {
    PSYSTEM("rsh $host_name \"killall -9 $constantes::SERVERDNAME\"");
    PSYSTEM("rsh $host_name \"killall -9 $constantes::EXPORTDNAME\"");
  }

  foreach $import_host (@hostslist) {
    PSYSTEM("rsh $import_host \"$constantes::RMMOD $constantes::NBDMODNAME\"");
    PSYSTEM("rsh $import_host \"$constantes::RMMOD $constantes::VRTMODNAME\"");
  }
}

&init;
@hostslist = &commun::gethostslist($conffile,$clustername);
PDEBUG("liste des noeuds : @hostslist\n");
&stop_cluster;

&commun::byebye($complet);
