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

  printf(".$HIGH Usage$NORM :  fusion_vlcreate GROUPNAME:VOLNAME SIZEMB\n");
  printf("\tCrée le volume logique VOLNAME  de taille SIZEMB (en Mo)\n");
  printf("\tsur le device group GROUPNAME.\n");

  printf("\n.$HIGH Exemple$NORM :\n");
  printf("$HIGH\t> ./fusion_vlcreate multimedia:divx 50000$NORM\n");
  printf("\t\tCrée le volume 'divx' de taille 50000 Mo sur\n");
  printf("\t\tle groupe 'multimedia'\n");
  printf("\n\n** %s \n** v%s (%02d/%02d/%04d)\n",
	__FILE__,$constantes::ADMCMD_VERSION, $tm->mday, $tm->mon+1, $tm->year+1900);
}





sub createzone {
 # compatible avec des tailles 64 bits
  my $sizeKB = Math::BigInt->new($sizeMB);
  PDEBUG("sizeKB=$sizeKB sizeMB=$sizeMB\n");
  $sizeKB = 1024 * $sizeKB;
  $sizeKB =~ s/\+//;

  foreach $host (@hostslist) {
    PSYSTEM("rsh $host \"echo zc $groupname $zonename $sizeKB > $constantes::VRTPROCCMD\"");
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

&createzone;

&commun::byebye($complet);
