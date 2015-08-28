#!/usr/bin/perl
require "constantes.pl";
use Time::localtime;

sub init {
  if (scalar(@ARGV) == 0 ||
      scalar(@ARGV) > 1) {
    PERROR("mauvais nombre d'arguments\n");
    &usage;
    exit 1;
  }

  $clustername = $ARGV[0];
  $conffile = $DEFAULT_CONFFILE;

  PDEBUG("conffile = $conffile, cluster = $clustername\n");
}

sub usage
{
  $date = (stat(__FILE__))[9];
  $tm = localtime($date);
  printf(". Usage : startFUSION CLUSTERNAME \n");
  printf(". Démarre la fusion du stockage du cluster 'CLUSTERNAME'\n");
  printf("  en analysant le fichier de configuration\n");
  printf("  /etc/driveFUSION.conf : exportation/importation\n");
  printf("  des devices de stockage utilisés par driveFUSION,\n");
  printf("  création des espaces virtuels et des volumes logiques.\n");
  printf(". Exemple :\n");
  printf("\t> ./startFUSION sam\n");
  printf("\tDémarre la fusion du stockage du cluster 'sam'\n");
  printf("\n\n** %s \n** v%s (%02d/%02d/%04d)\n",
	__FILE__,$ADMCMD_VERSION, $tm->mday, $tm->mon+1, $tm->year+1900);
}

sub exporte_on_host {
  my @champs = @_;
  my $host_name = shift(@champs);
  push(@HOSTSLIST,$host_name);
  print "***** EXPORTATION POUR $host_name *****\n";
  PSYSTEM("rsh $host_name \"$SERVERD\"");
  sleep $SLEEPTIME;
  PSYSTEM("rsh $host_name \"$EXPORTD \\`$PIDOF $SERVERDNAME\\`\"");
  while (@champs) {
    my $dev_name = shift(@champs);
    $dev_name =~ s/\[//;
    my $path_name = shift(@champs);
    $path_name =~ s/\]//;
    chomp($path_name);
    PSYSTEM("rsh $host_name \"$EXPORTE -a $path_name $dev_name\"");
  }
} 

sub exporte_devs {
  if (!open(CONFFILE,$conffile)) {
    PERROR("impossible d'ouvrir le fichier $conffile");
    exit 1;
  }

  # on recherche une ligne contenant "cluster"
  # et le nom du cluster 'clustername'
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
      @champs = split(/,/,$ligne);
      &exporte_on_host(@champs);
    }
  } while (($ligne = <CONFFILE>) && ($continue));

  close($conffile);
}

sub importe_devs {
  foreach $import_host (@HOSTSLIST) {
    print "***** IMPORTATION POUR $import_host *****\n";
    PSYSTEM("rsh $import_host \"$INSMOD $NBDMOD\"");
    PSYSTEM("rsh $import_host \"$INSMOD $VRTMOD\"");
    foreach $current_host (@HOSTSLIST) {
      if ($import_host ne $current_host) {
	PSYSTEM("rsh $import_host \"$IMPORTE $current_host\"");
      }
    }
  }
}

&init;
&exporte_devs;
PDEBUG("liste des noeuds : @HOSTSLIST\n");
print "\n";
&importe_devs;

