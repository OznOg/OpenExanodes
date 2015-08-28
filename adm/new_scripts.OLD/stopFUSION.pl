#!/usr/bin/perl
require "constantes.pl";
use Time::localtime;

sub usage {
  $date = (stat(__FILE__))[9];
  $tm = localtime($date);
  printf(". Usage : stopFUSION CLUSTERNAME\n");
  printf(". Stoppe la fusion du stockage du cluster 'CLUSTERNAME en\n");
  printf("  analysant le fichier de configuration /etc/driveFUSION.conf\n");
  printf(". Exemple :\n");
  printf("\t> ./stopFUSION sam\n");
  printf("\tStoppe la fusion du stockage du cluster 'sam'\n");
  printf("\n\n** %s \n** v%s (%02d/%02d/%04d)\n",
	__FILE__,$ADMCMD_VERSION, $tm->mday, $tm->mon+1, $tm->year+1900);
}

sub init {
  if (scalar(@ARGV) == 0 ||
      scalar(@ARGV) > 1) {
    PERROR("bad number of arguments\n");
    &usage;
    exit 1;
  }

  $conffile = $DEFAULT_CONFFILE;
  $clustername = $ARGV[0];

  PDEBUG("conffile = $conffile cluster = $clustername\n");
}

sub stop_exporte_host {
  my @champs = @_;
  my $host_name = shift(@champs);
  push(@HOSTSLIST,$host_name);

  PSYSTEM("rsh $host_name \"killall -9 $SERVERDNAME\"");
  PSYSTEM("rsh $host_name \"killall -9 $EXPORTDNAME\"");
}

sub stop_exporte
{
  open(CONFFILE,$conffile);

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
      chomp($ligne);
      @champs = split(/,/,$ligne);
      &stop_exporte_host(@champs);
    }
  } while (($ligne = <CONFFILE>) && ($continue));

  close($conffile);
}

sub stop_importe {
  foreach $import_host (@HOSTSLIST) {
    PSYSTEM("rsh $import_host \"$RMMOD $NBDMODNAME\"");
    PSYSTEM("rsh $import_host \"$RMMOD $VRTMODNAME\"");
  }
}

&init;
&stop_exporte;
PDEBUG("@HOSTSLIST\n");
&stop_importe;
