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

sub usage
{
  $date = (stat(__FILE__))[9];
  $tm = localtime($date);
  my $HIGH = `tput bold`;
  my $NORM = `tput sgr0`;

  printf(".$HIGH Usage$NORM : fusion_delete [options] GROUPNAME\n");
  printf("\tDétruit le groupe 'GROUPNAME' (efface les \n");
  printf("\tsuperblocs)\n");

  printf("\t$HIGH-f, --force$NORM\n");
  printf("\t\tForce la destruction du groupe même si des volumes sont,\n");
  printf("\t\ten cours d'utilisation.\n");

  printf("\n.$HIGH Exemple$NORM :\n");
  printf("$HIGH\t> ./fusion_dgdelete video$NORM\n");

  printf("\n\n** %s \n** v%s (%02d/%02d/%04d)\n",
	__FILE__,$constantes::ADMCMD_VERSION, $tm->mday, $tm->mon+1, $tm->year+1900);
}

sub raz_sbg_devslist {
  my $i = 0;
  my $j;

  while ($i < scalar(@devslist)) {
    my $nbdevs = $devslist[$i] - 1;
    my $devhost = $devslist[$i+1];
    my $devpath;
    $i = $i + 2;
    for ($j = $i; $j < $i+$nbdevs; $j++) {
      print "erasing SBG on dev $devslist[$j] (devhost = $devhost) on $masternode $thishost\n";
      $devpath = &commun::finddevpath($conffile, $clustername, $devhost, $masternode, $devslist[$j]);
      PSYSTEM("ssh $masternode $constantes::RAZSBGCMD $devpath");

    }
    $i = $i + $nbdevs;
  }
}


sub deletegroup {
  # désactivation du group

  my $group_active = 0;
  foreach $host (@hostslist) {
    if (&commun::is_groupactive($host, $groupname)) {
      $group_active = 1;
    }
  }
  print "group_active = $group_active\n";

  if ($group_active) {
    my $return = PSYSTEM("$constantes::DGSTOPCMD $groupname $debugopt $forceopt");
    print "return = $return\n";
    if ($return == 0) {
      $group_active = 0;
    }
  }

  if ($group_active == 0) {
    raz_sbg_devslist();
  }
  else {
    PERROR("can't desactivate group 'groupname'\n");
    exit 1;
  }
}

&init;
PDEBUG("conffile=$conffile, groupname=$groupname\n");
($clustername, $layouttype) = &commun::getdg_infos($conffile, $groupname);
PDEBUG("clustername = $clustername, layouttype = $layouttype\n");
@devslist = &commun::getdg_devslist($conffile, $groupname);
PDEBUG("devslist = @devslist\n");
@hostslist = &commun::gethostslist($conffile,$clustername);
PDEBUG("hostslist = @hostslist\n");
$masternode = &commun::selectmaster(@hostslist);
PDEBUG("masternode = $masternode\n");
&deletegroup;

&commun::byebye($complet);
