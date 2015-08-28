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

  printf(".$HIGH Usage$NORM : fusion_dgstart GROUPNAME\n");
  printf("\tActive le groupe 'GROUPNAME' tel qu'il est décrit \n");
  printf("\tdans le fichier /etc/driveFUSION.conf. Ce groupe a \n");
  printf("\tdéjà été créé par le passé\n");

  printf("\n.$HIGH Exemple$NORM :\n");
  printf("$HIGH\t> ./fusion_dgstart video$NORM\n");
  printf("\n\n** %s \n** v%s (%02d/%02d/%04d)\n",
	__FILE__,$constantes::ADMCMD_VERSION, $tm->mday, $tm->mon+1, $tm->year+1900);
}





sub addolddev {
  my $targethost = $_[0];
  my $sourcehost = $_[2];
  my $devname = $_[1];
  my $groupname = $_[3];

  PDEBUG("targethost=$targethost sourcehost=$sourcehost devname=$devname\n");
  $devpath = &commun::finddevpath($conffile, $clustername, $sourcehost, $targethost, $devname);

  PDEBUG("adding devpath = $devpath on host $targethost\n");
  (my $sizeKB, my $major, my $minor, my @uuid) =
    &commun::getsizemajminuuid($host,$devpath);@devinfos;
  PDEBUG("sizeKB=$sizeKB, major=$major, minor=$minor, uuid=@uuid\n");
  PSYSTEM("rsh $targethost \"echo go $groupname $uuid[3] $uuid[2] $uuid[1] $uuid[0] $devpath $sizeKB $major $minor > $constantes::VRTPROCCMD\"");
}

sub addolddevs_tohost {
  my $thishost = $_[0];
  my $groupname = $_[1];
  $i = 0;
  PDEBUG("addolddevs_tohost: thishost=$thishost groupname=$groupname\n");
  while ($i < scalar(@devslist)) {
    my $nbdevs = $devslist[$i] - 1;
    my $devhost = $devslist[$i+1];
    $i = $i + 2;
    for ($j = $i; $j < $i+$nbdevs; $j++) {
      PDEBUG("adding dev $devslist[$j] (devhost = $devhost) to host $thishost\n");
      addolddev($thishost, $devslist[$j], $devhost, $groupname);
    }
    $i = $i + $nbdevs;
  }
}

# void startvolumes(char *groupname);
#
# démarre les volumes du groupe 'groupname' tels que défini dans le
# fichier de conf. Ce fichier a déjà été parsé auparavant les infos
# sur les volumes sont dans la liste @volslist.
sub startvolumes {
  my $groupname = $_[0];

  $i = 0;
  while ($i < scalar(@volslist)) {
    my $nbappnodes = $volslist[$i] - 2;
    my $volname = $volslist[$i+1];
    my $volsizeMB = $volslist[$i+2];
    my @appnodes;
    my $allopt;

    $i = $i + 3;

    if ($volslist[$i] eq "*") {
      $allopt = "--all";
    }
    else {
      for ($j = $i; $j < $i+$nbappnodes; $j++) {
	push(@appnodes, $volslist[$j]);
      }
    }
    PSYSTEM("$constantes::VLSTARTCMD $groupname:$volname @appnodes $debugopt $allopt");

    $i = $i + $nbappnodes;
  }
}

sub startgroup {
  my @inactive_hosts;
  PDEBUG("hostslist = @hostslist\n");

  foreach $host (@hostslist) {
    if (!&commun::is_groupactive($host, $groupname)) {
      push(@inactive_hosts, $host);
    }
  }
  PDEBUG("inactive_hosts = @inactive_hosts\n");

  foreach $host (@inactive_hosts) {
    $sourcehost = $devslist[1];
    $devname = $devslist[2];
    PDEBUG("sourcehost=$sourcehost targethost=$host devname=$devname\n");
    $devpath = &commun::finddevpath($conffile, $clustername, $sourcehost, $host, $devname);
    (my $sizeKB, my $major, my $minor) = &commun::getsizemajmin($host,$devpath);
    PDEBUG("sizeKB=$sizeKB major=$major minor=$minor\n");
    PSYSTEM("rsh $host \"echo gl $groupname $major $minor $sizeKB > $constantes::VRTPROCCMD\"");
  }

  foreach $host (@inactive_hosts) {
    &addolddevs_tohost($host, $groupname);
  }

  foreach $host (@inactive_hosts) {
    PSYSTEM("rsh $host \"echo gz $groupname > $constantes::VRTPROCCMD\"");
  }

}

&init;
PDEBUG("conffile=$conffile, groupname=$groupname\n");
($clustername, $layouttype) = &commun::getdg_infos($conffile, $groupname);
@devslist = &commun::getdg_devslist($conffile, $groupname);
@volslist = &commun::getdg_volslist($conffile, $groupname);
PDEBUG("clustername = $clustername, layouttype = $layouttype\n");
PDEBUG("devslist = @devslist\n");
PDEBUG("volslist = @volslist\n");
@hostslist = &commun::gethostslist($conffile,$clustername);
PDEBUG("hostslist = @hostslist\n");
$masternode = &commun::selectmaster(@hostslist);
PDEBUG("masternode = $masternode\n");
startgroup;
startvolumes($groupname);

&commun::byebye($complet);
