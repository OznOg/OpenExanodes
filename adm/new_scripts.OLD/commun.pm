package commun;
require "constantes.pl";
use Exporter;
@ISA=('Exporter');

@EXPORT_OK=('getclusterinfo','gethostslist','selectmaster');

sub getgroupinfo {
  my $conffile = $_[0];
  my $groupname = $_[1];
  my $ligne;

  if (!open(CONFFILE,$conffile)) {
    PERROR("impossible d'ouvrir le fichier $conffile");
    exit 1;
  }

  my $continue = 1;
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
    if ($ligne =~ /\}/) {
      $continue = 0;
    } else {
      chomp($ligne);
      @champs = split(/,/,$ligne);
      push(@devslist,(scalar(@champs),@champs));
    }
  } while (($ligne = <CONFFILE>) && ($continue));

  close($conffile);
  return ($clustername, $layouttype, @devslist);
}

sub gethostslist {
  my $conffile = $_[0];
  my $clustername = $_[1];
  my $ligne;

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
    if ($ligne =~ /\}/) {
      $continue = 0;
    } else {
      chomp($ligne);
      @champs = split(/,/,$ligne);
      push(@hostslist,$champs[0]);
    }
  } while (($ligne = <CONFFILE>) && ($continue));

  close($conffile);
  return @hostslist;
}

sub selectmaster {
  my @hostslist = @_;
  my $masternode = $hostslist[0];

  return $masternode;
}
