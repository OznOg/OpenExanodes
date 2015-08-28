#!/usr/bin/perl
use constantes;
use Time::localtime;
use Math::BigInt;
use commun('PERROR', 'PWARNING', 'PDEBUG', 'PDEBUGV', 'PSYSTEM', 'PTRACECMD');
use Getopt::Long;
require "clcommun.pl";

sub init {
  $complet = 1;

  if (GetOptions('d|debug=i' => \$constantes::DEBUG_LEVEL) == 0) {
    &usage;
    exit 1;
  }

  if (scalar(@ARGV) != 1) {
    PERROR("mauvais nombre d'arguments\n");
    &usage;
    exit 1;
  }

  $clustername = $ARGV[0];
  $conffile = $constantes::DEFAULT_CONFFILE;

  # pour propager les options de force et debug
  # aux sous-commandes
  if ($constantes::DEBUG_LEVEL) {
    $debugopt = "--debug $constantes::DEBUG_LEVEL";
  } else {
    $debugopt = "";
  }

  PDEBUG("conffile = $conffile, cluster = $clustername\n");
}

sub usage
{
  $date = (stat(__FILE__))[9];
  $tm = localtime($date);
  my $HIGH = `tput bold`;
  my $NORM = `tput sgr0`;

  printf(".$HIGH Usage$NORM : fusion_clcreate CLUSTERNAME\n");
  printf("\tDémarre la fusion du stockage du cluster 'CLUSTERNAME'\n");
  printf("\ten analysant le fichier de configuration\n");
  printf("\t/etc/driveFUSION.conf : exportation/importation\n");
  printf("\tdes devices de stockage utilisés par driveFUSION,\n");
  printf("$HIGH\tcréation$NORM des espaces virtuels et des volumes logiques.\n");

  printf("\n.$HIGH Exemples$NORM :\n");
  printf("$HIGH\t> ./fusion_clstart sam$NORM\n");
  printf("\t\tDémarre la fusion du stockage du cluster 'sam'\n");

  printf("\n\n** %s \n** v%s (%02d/%02d/%04d)\n",
	__FILE__,$constantes::ADMCMD_VERSION, $tm->mday, $tm->mon+1, $tm->year+1900);
}

# void creategroups(char *gname[]);
#
# crée les groupes donnés en paramètre
sub creategroups {
  foreach $group (@groupslist) {
    PSYSTEM("$constantes::DGCREATECMD  $debugopt $group");
  }
}

&init;
#@edevslist = commun::getcl_edevs($conffile, $clustername);
# PDEBUG("exported devs list = @edevslist\n");
# &exporte_devs;
# @hostslist = &commun::gethostslist($conffile,$clustername);
# PDEBUG("liste des noeuds : @hostslist\n");
# &importe_devs;
# @groupslist = &commun::getgroupslist($conffile,$clustername);
# PDEBUG("device groups list : @groupslist\n");
# &creategroups;
$config = &commun::read_config($conffile);

#print join(', ', @{$config->{devicegroup}->{importdevice}} ), "\n";

&commun::byebye($complet);
