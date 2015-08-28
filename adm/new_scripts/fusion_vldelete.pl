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
  ($groupname, $zonename) = split(/:/,$ARGV[0]);

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

  printf(".$HIGH Usage$NORM : fusion_vldelete [options] GROUPNAME:VOLNAME\n");
  printf("\tDétruit le volume de stockage VOLNAME du groupe GROUPNAME.\n");

  printf("\n.$HIGH Option$NORM :\n");
  printf("\t$HIGH-f, --force$NORM\n");
  printf("\t\tSi un volume est en cours d'utilisation par un noeud,\n");
  printf("\t\ton force sa désactivation avant de le détruire (risque \n");
  printf("\t\timportant de faire planter l'application qui l'utilise).\n");


  printf("\n.$HIGH Exemples$NORM :\n");
  printf("$HIGH\t> ./fusion_delete multimedia:divx$NORM\n");
  printf("\t\tDétruit le volume 'divx' du groupe 'multimedia' \n");
  printf("\n\n** %s \n** v%s (%02d/%02d/%04d)\n",
	__FILE__,$constantes::ADMCMD_VERSION, $tm->mday, $tm->mon+1, $tm->year+1900);
}

sub deletezone {
  PSYSTEM("$constantes::VLSTOPCMD --all $groupname:$zonename $debugopt $forceopt");

  foreach $host (@hostslist) {
    my $in_use = &commun::vol_in_use($host,$groupname, $zonename);

    if ($in_use == 0) {
      PSYSTEM("ssh $host \"echo zd $groupname $zonename > $constantes::VRTPROCCMD\"");
    }
    else {
      PERROR("can't desactivate volume '$groupname:$zonename' on node $host\n");
      $complet = 0;
      return;
    }
  }
  PSYSTEM("ssh $masternode \"echo wg $groupname > $constantes::VRTPROCCMD\"");
}


&init;
PDEBUG("conffile=$conffile, groupname=$groupname zname=$zonename\n");

($clustername, $layouttype) = &commun::getdg_infos($conffile, $groupname);
PDEBUG("clustername = $clustername, layouttype = $layouttype\n");

@hostslist = &commun::gethostslist($conffile,$clustername);
PDEBUG("hostslist = @hostslist\n");
$masternode = &commun::selectmaster(@hostslist);
PDEBUG("masternode = $masternode\n");

&deletezone;

&commun::byebye($complet);
