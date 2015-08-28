#!/usr/bin/perl
use Time::localtime;
use commun;

require "constantes.pl";

sub init {
  if (scalar(@ARGV) != 1) {
    PERROR("mauvais nombre d'arguments\n");
    &usage;
    exit 1;
  }

  $conffile = $DEFAULT_CONFFILE;
  $groupname = $ARGV[0];
}

sub usage
{
  $date = (stat(__FILE__))[9];
  $tm = localtime($date);
  printf(". Usage : dgcreate GROUPNAME\n");
  printf(". Crée un nouveau groupe 'GROUPNAME' tel qu'il est décrit dans le\n");
  printf("  fichier /etc/driveFUSION.conf\n");
  printf(". Exemple :\n\t");
  printf("> ./dgcreate video\n");
  printf("\n\n** %s \n** v%s (%02d/%02d/%04d)\n",
	__FILE__,$ADMCMD_VERSION, $tm->mday, $tm->mon+1, $tm->year+1900);
}



sub finddevpath {
  my $host = $_[0];
  my $devname = $_[1];
  my $devpath;

    if (!open(CONFFILE,$conffile)) {
    PERROR("impossible d'ouvrir le fichier $conffile");
    exit 1;
  }

  $continue = 1;
  while (($ligne = <CONFFILE>) && ($continue)) {
    chomp($ligne);
    PDEBUG("ligne = $ligne\n");
    if ($ligne =~ /$CLUSTER_TOKEN/
        && $ligne =~ /$clustername/) {
      $continue = 0;
    }
  }

  if ($continue) {
    PERROR("cluster '$clustername' introuvable dans le fichier de config\n");
    exit 1;
  }

  $continue = 1;
  do {
    chomp($ligne);
    PDEBUG("finddevpath - ligne = $ligne\n");
    if ($ligne =~ /\}/) { $continue = 0; }
    else {
      if ($ligne =~ $host) {
	  @champs = split(/,/,$ligne);
	  shift(@champs);
	  $trouve = 0;
	  while ((scalar(@champs !=0)) ||
		 ($trouve == 0)) {
	    if ($champs[0] =~ $devname) {
	      $trouve = 1;
	      $devpath = $champs[1];
	      $devpath =~ s/\]//;
	    }
	    shift(@champs);
	    shift(@champs);
	  }
	}
    }
  } while (($ligne = <CONFFILE>) && ($continue));

  close($conffile);

  return $devpath;
}

sub adddev {
  my $targethost = $_[0];
  my $sourcehost = $_[2];
  my $devname = $_[1];
  my $groupname = $_[3];

  #print("targethost=$targethost sourcehost=$sourcehost devname=$devname\n");
  if ($targethost eq $sourcehost) {
    $devpath = finddevpath($targethost, $devname);
  } else {
    $devpath = "$NBDDEVPATH/$sourcehost/$devname";
  }

  print "adding devpath = $devpath on host $targethost\n";
  @devinfo = `rsh $targethost $DEVINFOCMD $devpath`;
  print "DEVINFO = @devinfo\n";
  chomp(@devinfo);
  (my $sizeKB, my $major, my $minor) = @devinfo;
  PSYSTEM("rsh $targethost \"echo gn $groupname $devpath $sizeKB $major $minor > $VRTPROCCMD\"");
}

sub adddevs_tohost {
  my $thishost = $_[0];
  my $groupname = $_[1];
  $i = 0;
  while ($i < scalar(@devslist)) {
    my $nbdevs = $devslist[$i] - 1;
    my $devhost = $devslist[$i+1];
    $i = $i + 2;
    for ($j = $i; $j < $i+$nbdevs; $j++) {
      print "adding dev $devslist[$j] (devhost = $devhost) to host $thishost\n";
      adddev($thishost, $devslist[$j], $devhost, $groupname);
    }
    $i = $i + $nbdevs;
  }
}

sub creategroup {
  foreach $host (@hostslist) {
    PSYSTEM("rsh $host \"echo gc $groupname $layouttype > $VRTPROCCMD\"");
  }

  foreach $host (@hostslist) {
    &adddevs_tohost($host, $groupname);
  }

  PSYSTEM("rsh $masternode \"echo wa $groupname > $VRTPROCCMD\"");

  foreach $host (@hostslist) {
    PSYSTEM("rsh $host \"echo gs $groupname > $VRTPROCCMD\"");
  }

}

&init;
print "conffile=$conffile, groupname=$groupname\n";
($clustername, $layouttype, @devslist) = &commun::getgroupinfo($conffile, $groupname);
print "clustername = $clustername\n";
print "devslist = @devslist\n";
print "layouttype = $layouttype\n";
@hostslist = &commun::gethostslist($conffile,$clustername);
print "hostslist = @hostslist\n";
$masternode = &commun::selectmaster(@hostslist);
print "masternode = $masternode\n";
&creategroup;
