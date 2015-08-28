#!/usr/bin/perl
use constantes;
use Time::localtime;
use Math::BigInt;
use commun('PERROR', 'PWARNING', 'PDEBUG', 'PSYSTEM', 'PTRACECMD');
use Getopt::Long;


# fonction d'initialisation de variables globales
# et parsing des arguments de la ligne de commande
sub init {
  $complet = 1;

  if (GetOptions('f|force' => \$forcestop,
		 'd|debug=i' => \$constantes::DEBUG_LEVEL) == 0) {
    &usage;
    exit 1;
  }

  if (scalar(@ARGV) < 1) {
    PERROR("mauvais nombre d'arguments\n");
    &usage;
    exit 1;
  }

  $conffile = $constantes::DEFAULT_CONFFILE;
  $groupname = $ARGV[0];

  # pour propager les options de force et debug
  # aux sous-commandes
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
}

sub usage {
  my $date = (stat(__FILE__))[9];
  my $tm = localtime($date);
  my $HIGH = `tput bold`;
  my $NORM = `tput sgr0`;

  printf(".$HIGH Usage$NORM : fusion_dgstop [options] GROUPNAME\n");
  printf("\tDésactive le groupe GROUPNAME, ainsi que tous\n");
  printf("\tles volumes qu'il contient. Impossible de modifier\n");
  printf("\tl'état du groupe sans le réactiver auparavant\n");

  printf("\n.$HIGH Options$NORM :\n");
  printf("\t$HIGH-f, --force$NORM\n");
  printf("\t\tSi des volumes du groupe sont en cours d'utilisation,\n");
  printf("\t\ton force leur désactivation (risque important de faire\n");
  printf("\t\tplanter les applications qui les utilisent).\n");

  printf("\n.$HIGH Exemples$NORM :\n");
  printf("$HIGH\t> ./fusion_dgstop multimedia$NORM\n");
  printf("\t\tDésactive le groupe 'multimedia'.\n");
  printf("\n\n** %s \n** v%s (%02d/%02d/%04d)\n",
	__FILE__,$constantes::ADMCMD_VERSION, $tm->mday, $tm->mon+1, $tm->year+1900);
}

sub okpourstop {
  my $ok = 1;
  foreach $host (@hostslist) {
    @zones = &commun::getallinusezones($host,$groupname);
    if (@zones) {
      $ok = 0;
      print "volumes (@zones):$groupname on node '$host' are in use.\n";
    }
  }
  return $ok;
}

sub stopgroup {
  foreach $host (@hostslist) {
    #TODO : tester si le group est actif
    #TODO : tester si aucune des zones a désactiver
    #       n'est en cours d'utilisation
    @zones = &commun::getallactivezones($host,$groupname);
    if (@zones) {
      print "desactivating zones (@zones) on node '$host'\n";
    }
    my $one_zone_failed = 0;
    foreach $zone (@zones) {
      $status = PSYSTEM("$constantes::VLSTOPCMD $forceopt $debugopt $groupname:$zone $host");
      if ($status != 0) {
	PERROR("$constantes::VLSTOPCMD for volume $groupname:$zone failed\n");
	$complet = 0;
	$one_zone_failed = 1;
      }
    }

    if ($one_zone_failed == 0) {
      PSYSTEM("ssh $host \"echo gk $groupname > $constantes::VRTPROCCMD\"");
    }
  }
}

&init;
PDEBUG("conffile=$conffile, groupname=$groupname zname=$zonename\n");
PDEBUG("hostsappli init = @hostsappli\n");
($clustername, $layouttype) = &commun::getdg_infos($conffile, $groupname);
@devslist = &commun::getdg_devslist($conffile, $groupname);
PDEBUG("clustername = $clustername, layouttype = $layouttype\n");
PDEBUG("devslist = @devslist\n");

@hostslist = &commun::gethostslist($conffile,$clustername);
PDEBUG("hostslist = @hostslist\n");

if (&okpourstop || $forcestop) {
  &stopgroup;
} else {
  print "can't desactivate group '$groupname' with a volume in use.\nuse --force option if you want to get over.\n";
  $complet = 0;
}

&commun::byebye($complet);
