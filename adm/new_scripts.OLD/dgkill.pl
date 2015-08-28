#!/usr/bin/perl
require "constantes.pl";
use Time::localtime;

# A METTRE DANS COMMUN
sub getclusterinfo {
 if (!open(CONFFILE,$conffile)) {
    PERROR("impossible d'ouvrir le fichier $conffile");
    exit 1;
  }
  $continue = 1;
  while (($ligne = <CONFFILE>) && ($continue)) {
    chomp($ligne);
    PDEBUG("ligne = $ligne\n");
    if ($ligne =~ /$GROUP_TOKEN/
        && $ligne =~ /$groupname/) {
      $continue = 0;
    }
  }

  if ($continue) {
    PERROR("groupe '$groupname' introuvable dans le fichier de config\n");
    exit 1;
  }

 chomp($ligne);
 PDEBUG("ligne = $ligne\n");
 $clustername = $ligne;
 $ligne = <CONFFILE>;

 chomp($ligne);
 PDEBUG("ligne = $ligne\n");
 $layouttype = $ligne;
 $ligne = <CONFFILE>;

 $continue = 1;
 do {
    chomp($ligne);
    PDEBUG("ligne = $ligne\n");
    if ($ligne =~ /\}/) { $continue = 0; }
    else {
      chomp($ligne);
      @champs = split(/,/,$ligne);
      push(@devslist,(scalar(@champs),@champs));
    }
  } while (($ligne = <CONFFILE>) && ($continue));

  close($conffile);
}

sub gethostslist {
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
    PDEBUG("ligne = $ligne\n");
    if ($ligne =~ /\}/) { $continue = 0; }
    else {
      chomp($ligne);
      @champs = split(/,/,$ligne);
      push(@hostslist,$champs[0]);
    }
  } while (($ligne = <CONFFILE>) && ($continue));

  close($conffile);
}

sub selectmaster {
  $masternode = $hostslist[0];
}
#FIN COMMUN

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
  printf(". Usage : dgkill GROUPNAME\n");
  printf(". Désactive le groupe 'GROUPNAME'.\n");
  printf(". Exemple :\n\t");
  printf("> ./dgkill video\n");
  printf("\n\n** %s \n** v%s (%02d/%02d/%04d)\n",
	__FILE__,$ADMCMD_VERSION, $tm->mday, $tm->mon+1, $tm->year+1900);
}

sub killgroup {
  foreach $host (@hostslist) {
    PSYSTEM("rsh $host \"echo gk $groupname > $VRTPROCCMD\"");
  }
}

&init;
&getclusterinfo;
print "clustername = $clustername\n";
print "devslist = @devslist\n";
&gethostslist;
print "hostslist = @hostslist\n";
&selectmaster;
&killgroup;
