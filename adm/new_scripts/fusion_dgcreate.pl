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

  if (scalar(@ARGV) != 1) {
    PERROR("mauvais nombre d'arguments\n");
    &usage;
    exit 1;
  }

  $conffile = $constantes::DEFAULT_CONFFILE;
  $groupname = $ARGV[0];

  # pour propager les options de force et debug
  # aux sous-commandes
  if ($constantes::DEBUG_LEVEL) {
    $debugopt = "--debug $constantes::DEBUG_LEVEL";
  } else {
    $debugopt = "";
  }

}

sub usage
{
  $date = (stat(__FILE__))[9];
  $tm = localtime($date);
  my $HIGH = `tput bold`;
  my $NORM = `tput sgr0`;

  printf(".$HIGH Usage$NORM : fusion_dgcreate GROUPNAME\n");
  printf("\tCrée un nouveau groupe 'GROUPNAME' tel qu'il est décrit\n");
  printf("\tdans le fichier /etc/driveFUSION.conf\n");

  printf("\n.$HIGH Exemple$NORM :\n");
  printf("$HIGH\t> ./fusion_dgcreate video$NORM\n");

  printf("\n\n** %s \n** v%s (%02d/%02d/%04d)\n",
	__FILE__,$constantes::ADMCMD_VERSION, $tm->mday, $tm->mon+1, $tm->year+1900);
}



sub adddev {
  my $conffile = $_[0];
  my $targethost = $_[1];
  my $devname = $_[2];
  my $sourcehost = $_[3];
  my $groupname = $_[4];

  PDEBUG("targethost=$targethost sourcehost=$sourcehost devname=$devname\n");
  $devpath = &commun::finddevpath($conffile, $clustername, $sourcehost, $targethost, $devname);

  PDEBUG("adding devpath = $devpath on host $targethost\n");
  (my $sizeKB, my $major, my $minor) = &commun::getsizemajmin($host,$devpath);  PSYSTEM("rsh $targethost \"echo gn $groupname $devpath $sizeKB $major $minor > $constantes::VRTPROCCMD\"");
}

sub adddevs_tohost {
  my $conffile = $_[0];
  my $thishost = $_[1];
  my $groupname = $_[2];
  my $i = 0;
  my $j;

  while ($i < scalar(@devslist)) {
    my $nbdevs = $devslist[$i] - 1;
    my $devhost = $devslist[$i+1];
    $i = $i + 2;
    for ($j = $i; $j < $i+$nbdevs; $j++) {
      PDEBUG("adding dev $devslist[$j] (devhost = $devhost) to host $thishost\n");
      adddev($conffile, $thishost, $devslist[$j], $devhost, $groupname);
    }
    $i = $i + $nbdevs;
  }
}


# void createvolumes(char *groupname);
#
# crée les volumes du groupe 'groupname' tels que défini dans le
# fichier de conf. Ce fichier a déjà été parsé auparavant les infos
# sur les volumes sont dans la liste @volslist.
sub createvolumes {
  my $groupname = $_[0];

  $i = 0;
  while ($i < scalar(@volslist)) {
    my $nbappnodes = $volslist[$i] - 2;
    my $volname = $volslist[$i+1];
    my $volsizeMB = $volslist[$i+2];

    $i = $i + 3;
    PSYSTEM("$constantes::VLCREATECMD $groupname:$volname $volsizeMB $debugopt");
    $i = $i + $nbappnodes;
  }
}

sub creategroup {
  foreach $host (@hostslist) {
    PSYSTEM("rsh $host \"echo gc $groupname $layouttype > $constantes::VRTPROCCMD\"");
  }

  foreach $host (@hostslist) {
    &adddevs_tohost($conffile, $host, $groupname);
  }

  PSYSTEM("rsh $masternode \"echo wa $groupname > $constantes::VRTPROCCMD\"");

  foreach $host (@hostslist) {
    PSYSTEM("rsh $host \"echo gs $groupname > $constantes::VRTPROCCMD\"");
  }

  createvolumes($groupname);
}

&init;
PDEBUG("conffile=$conffile, groupname=$groupname\n");
($clustername, $layouttype) = &commun::getdg_infos($conffile, $groupname);
PDEBUG("clustername = $clustername, layouttype = $layouttype\n");
@devslist = &commun::getdg_devslist($conffile, $groupname);
PDEBUG("devslist = @devslist\n");
@volslist = &commun::getdg_volslist($conffile, $groupname);
PDEBUG("volslist = @volslist\n");
@hostslist = &commun::gethostslist($conffile,$clustername);
PDEBUG("hostslist = @hostslist\n");
$masternode = &commun::selectmaster(@hostslist);
PDEBUG("masternode = $masternode\n");
&creategroup;

&commun::byebye($complet);
