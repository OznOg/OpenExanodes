#!/usr/bin/perl
use constantes;
use Time::localtime;
use Math::BigInt;
use commun('PERROR', 'PWARNING', 'PDEBUG', 'PSYSTEM', 'PTRACECMD');
use Getopt::Long;


sub init {
  $complet = 1;

  if (GetOptions('d|debug=i' => \$constantes::DEBUG_LEVEL) == 0) {
    &usage;
    exit 1;
  }

  if (scalar(@ARGV) != 2) {
    PERROR("mauvais nombre d'arguments\n");
    &usage;
    exit 1;
  }

  $conffile = $constantes::DEFAULT_CONFFILE;
  ($groupname, $zonename) = split(/:/,$ARGV[0]);
  $sizeMB = $ARGV[1];
}

sub usage {
  $date = (stat(__FILE__))[9];
  $tm = localtime($date);
  my $HIGH = `tput bold`;
  my $NORM = `tput sgr0`;

  printf(".$HIGH Usage$NORM :  fusion_vlresize GROUPNAME:VOLNAME SIZEMB\n");
  printf("\tRetaille le volume VOLNAME du groupe GROUPNAME.\n");
  printf("\tLa nouvelle taille est SIZEMB (en Mo)\n");
  printf("\tLe changement de taille est faisable à chaud (ie.\n");
  printf("\tvolume actif et en cours d'utilisation) MAIS en cas\n");
  printf("\tde réduction de taille, il faut avoir fait le nécessaire\n");
  printf("\tavec les outils de resize du file system monté sur le\n");
  printf("\tvolume AVANT de diminuer la taille du volume (sinon\n");
  printf("\tle file system plantera)\n");


  printf("\n.$HIGH Exemple$NORM :\n");
  printf("$HIGH\t> ./fusion_resize multimedia:divx 20000$NORM\n");
  printf("\t\tRetaille le volume 'divx' du groupe 'multimedia'\n");
  printf("\t\tà 20000 Mo\n");
  printf("\n\n** %s \n** v%s (%02d/%02d/%04d)\n",
	__FILE__,$constantes::ADMCMD_VERSION, $tm->mday, $tm->mon+1, $tm->year+1900);
}





sub resizezone {
  # compatible avec des tailles 64 bits
  my $sizeKB = Math::BigInt->new($sizeMB);
  PDEBUG("sizeKB=$sizeKB sizeMB=$sizeMB\n");
  $sizeKB = 1024 * $sizeKB;
  $sizeKB =~ s/\+//;

  foreach $host (@hostslist) {
    PSYSTEM("rsh $host \"echo zr $groupname $zonename $sizeKB > $constantes::VRTPROCCMD\"");
  }

  PSYSTEM("rsh $masternode \"echo wa $groupname > $constantes::VRTPROCCMD\"");
}

&init;
PDEBUG("conffile=$conffile, groupname=$groupname zname=$zonename sizeMB=$sizeMB\n");

($clustername, $layouttype) = &commun::getdg_infos($conffile, $groupname);


@hostslist = &commun::gethostslist($conffile,$clustername);
PDEBUG("hostslist = @hostslist\n");

$masternode = &commun::selectmaster(@hostslist);
PDEBUG("masternode = $masternode\n");

&resizezone;

&commun::byebye($complet);
