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

  if (GetOptions('a|all' => \$allnodes,
		 'f|force' => \$forcestop,
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
  @hostsappli = @ARGV;
  shift(@hostsappli);

  if (($allnodes) && (scalar(@hostsappli) > 0)) {
    PERROR("a option or --all option conflicting with the NODENAME args\n");
    &usage;
    exit 1;
  }
}

sub usage {
  my $date = (stat(__FILE__))[9];
  my $tm = localtime($date);
  my $HIGH = `tput bold`;
  my $NORM = `tput sgr0`;

  printf(".$HIGH Usage$NORM : fusion_vlstop [options] GROUPNAME:VOLNAME [NODENAME...]\n");
  printf("\tDésactive le volume de stockage VOLNAME du groupe GROUPNAME.\n");
  printf("\tNODENAME... est la liste des noeuds qui n'auront plus\n");
  printf("\taccès à ce volume.\n");

  printf("\n.$HIGH Options$NORM :\n");
  printf("\t$HIGH-a, --all$NORM\n");
  printf("\t\tDésactive les volumes sur tous les noeuds du cluster.\n");
  printf("\t$HIGH-f, --force$NORM\n");
  printf("\t\tSi un volume est en cours d'utilisation par un noeud,\n");
  printf("\t\ton force sa désactivation (risque important de faire\n");
  printf("\t\tplanter l'application qui l'utilise).\n");


  printf("\n.$HIGH Exemples$NORM :\n");
  printf("$HIGH\t> ./fusion_vstop multimedia:divx sam1 sam2 sam3$NORM\n");
  printf("\t\tDésactive le volume 'divx' du groupe 'multimedia' \n");
  printf("\t\tsur les noeuds 'sam1', 'sam2' et 'sam3'.\n");
  printf("\t\tIls ne pourront plus accéder à ce volume.\n");
  printf("$HIGH\t> ./fusion_vlstop multimedia:mp3 --all $NORM \n");
  printf("\t\tDésactive le volume 'mp3' du groupe 'multimedia'\n");
  printf("\t\tpour tous les noeuds du cluster auquel le groupe\n");
  printf("\t\t'multimedia' appartient.\n");
  printf("\n\n** %s \n** v%s (%02d/%02d/%04d)\n",
	__FILE__,$constantes::ADMCMD_VERSION, $tm->mday, $tm->mon+1, $tm->year+1900);
}

sub stopzone {
  foreach $host (@hostsappli) {
    # TODO : tester si la zone est active

    # test si la zone est en cours d'utilisation
    my $in_use = &commun::vol_in_use($host,$groupname, $zonename);
    if ( ($in_use == 0) ||
	 ($in_use && $forcestop) ) {
      if ($in_use) {
	PWARNING("volume '$groupname:$zonename' is in use on node '$host'.\nforcing desactivation of '$groupname:$zonename'.\n");
      }
      PSYSTEM("ssh $host \"echo zk $groupname $zonename > $constantes::VRTPROCCMD\"");
    }
    else {
      PERROR("volume '$groupname:$zonename' is in use on node '$host'.\n\tCan't desactivate it!\n");
      $complet = 0;
    }
  }
}

&init;
PDEBUG("conffile=$conffile, groupname=$groupname zname=$zonename\n");
PDEBUG("hostsappli init = @hostsappli\n");

($clustername, $layouttype) = &commun::getdg_infos($conffile, $groupname);
PDEBUG("clustername = $clustername, layouttype = $layouttype\n");

@hostslist = &commun::gethostslist($conffile,$clustername);
PDEBUG("hostslist = @hostslist\n");
if ($allnodes) {
  @hostsappli = @hostslist;
}
print "desactivating volume '$groupname:$zonename' on nodes (@hostsappli).\n";
PDEBUG("hostsappli = @hostsappli\n");

&stopzone;

&commun::byebye($complet);
