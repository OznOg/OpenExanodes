#!/usr/bin/perl
use constantes;
use Time::localtime;
use Math::BigInt;
use commun('PERROR', 'PWARNING', 'PDEBUG', 'PSYSTEM', 'PTRACECMD');
use Getopt::Long;

sub init {
  $complet = 1;

  if (GetOptions('a|all' => \$allnodes,
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
  $date = (stat(__FILE__))[9];
  $tm = localtime($date);
  my $HIGH = `tput bold`;
  my $NORM = `tput sgr0`;

  printf(".$HIGH Usage$NORM : fusion_vlstart [options] GROUPNAME:VOLNAME [NODENAME...]\n");
  printf("\tDémarre le volume de stockage de nom VOLNAME du groupe GROUPNAME.\n");
  printf("\tCe volume doit avoir été créé précédemment (au moyen de la\n");
  printf("\tcommande 'vcreate'. NODENAME... sont les noeuds qui auront\n");
  printf("\taccès à ce volume. Par défaut, si aucun noeud n'est\n");
  printf("\tspécifiés : tous les noeuds du cluster auquel appartient\n");
  printf("\tle groupe\n");
  printf("\tGROUPNAME pourront accéder au volume\n");

  printf("\n.$HIGH Options$NORM :\n");
  printf("\t$HIGH-a, --all$NORM\n");
  printf("\t\tDémarre les volumes sur tous les noeuds du cluster.\n");


 printf("\n.$HIGH Exemples$NORM :\n");
  printf("$HIGH\t> ./fusion_vlstart multimedia:divx sam1 sam2 sam3$NORM\n");
  printf("\t\tDémarre le volume 'divx' du groupe 'multimedia'.\n");
  printf("\t\tLes noeuds 'sam1', 'sam2' et 'sam3' pourront accéder\n");
  printf("\t\tà ce volume\n");
  printf("$HIGH\t> ./fusion_vlstart multimedia:mp3$NORM\n");
  printf("\t\tDémarre le volume 'mp3' du groupe 'multimedia'. \n");
  printf("\t\tTous les noeuds du cluster pourront accéder au volume\n");
  printf("\t\t(cluster = celui auquel appartient le groupe GROUPNAME\n");
  printf("\n\n** %s \n** v%s (%02d/%02d/%04d)\n",
	__FILE__,$constantes::ADMCMD_VERSION, $tm->mday, $tm->mon+1, $tm->year+1900);
}

sub startzone {
  foreach $host (@hostsappli) {
    PSYSTEM("rsh $host \"echo zs $groupname $zonename > $constantes::VRTPROCCMD\"");
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

PDEBUG("hostsappli = @hostsappli\n");

&startzone;

&commun::byebye($complet);

