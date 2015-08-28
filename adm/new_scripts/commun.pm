package commun;
use constantes;
use Exporter;
use XML::Simple;
use Data::Dumper;

@ISA=('Exporter');

@EXPORT_OK=('PERROR', 'PDEBUG', 'PDEBUGV', 'PSYSTEM','PWARNING', 'PTRACECMD');

sub PERROR {
  print "\n*** ERROR ***\n@_** END ERROR **\n\n";
}

sub PWARNING {
  print "\n*** WARNING ***\n@_** END WARNING **\n\n";
}

sub PTRACECMD {
  if ($constantes::DEBUG_LEVEL >= 1) {
    print "TRACECMD - @_";
  }
}

sub PDEBUG {
  if ($constantes::DEBUG_LEVEL >= 2) {
    printf("DEBUG - @_");
  }
}

sub PDEBUGV {
  if ($constantes::DEBUG_LEVEL >= 3) {
    printf("DEBUGV - @_");
  }
}

sub PSYSTEM {
  PTRACECMD("@_ \n");
  system(@_);
}

# bool goto_and_tokens(char *tokens[])
# parcourt le fichier CONFFILE jusqu'à une ligne qui contient
# TOUS LES tokens passé en paramètres.
# renvoie 1 s'il a trouvé une telle ligne, et 0 sinon
sub goto_and_tokens {
  my @tokens = @_;
  my $ligne;
  my $bonne_ligne;

  while ($bonne_ligne == 0) {
    $ligne = <CONFFILE>;
    chomp($ligne);
    PDEBUGV("GOTO_AND_TOKEN : ligne = $ligne\n");
    $bonne_ligne = 1;
    foreach $token (@tokens) {
      if (($ligne =~ /$token/) == 0) {
	$bonne_ligne = 0;
	last;
      }
    }
  }
  return $bonne_ligne;
}

# char *getline_token(char *token)
#
# renvoie la ligne suivant le curseur fichier qui
# contient le token passé en paramètre. Renvoie une ligne
# vide si la fonction ne trouve rien.
sub getline_token {
  my $token = $_[0];
  my $ligne;
  while ($ligne = <CONFFILE>) {
    $ligne = cleanup_line($ligne);
    if ($ligne =~ /$token/) {
      last;
    }
  }
  return $ligne;
}


# bool goto_or_tokens(char *tokens[])
# parcourt le fichier CONFFILE jusqu'à une ligne qui contient
# L'UN DES tokens passé en paramètres.
# renvoie 1 s'il a trouvé une telle ligne, et 0 sinon
sub goto_or_tokens {
  my @tokens = @_;
  my $ligne;
  my $bonne_ligne;

  while ($bonne_ligne == 0) {
    $ligne = <CONFFILE>;
    chomp($ligne);
    PDEBUGV("GOTO_OR_TOKEN : ligne = $ligne\n");
    $bonne_ligne = 0;
    foreach $token (@tokens) {
      if ($ligne =~ /$token/) {
	$bonne_ligne = 1;
	last;
      }
    }
  }
  return $bonne_ligne;
}

# char* cleanup_line(char *line);
# nettoie la ligne en ne gardant que les caractères autorisés
sub cleanup_line {
  my $ligne = $_[0];
  chomp($ligne);
  $ligne =~ s/[^a-zA-Z0-9\-\{\}\_\[\]\/\*\,]//g;
  return $ligne;
}

#char *nextline(void)
# parcourt le fichier de conf jusqu'à atteindre une ligne non vide (après cleanup) et renvoie cette ligne
sub next_line {
  my $continue = 1;
  my $ligne;

  while ($continue) {
    $ligne = <CONFFILE>;
    if ($ligne) {
      $ligne = cleanup_line($ligne);
      if (length($ligne) != 0) {
	$continue = 0;
      }
    } else {
      $continue = 0
    }
  }

  return $ligne;
}

#(char *clustername, char* layout_type)
#      getcl_edevs(char *conffile,
#                  char *clustername);
#
# parcourt le fichier de conf et renvoie les liste dans nodes 
# exportateurs + les devnames/devpaths des devs qu'ils exportent 
sub getcl_edevs {
  my $conffile = $_[0];
  my $clustername = $_[1];
  my $ligne;
  my $trouve;
  my @edevslist;
  my @champs;

  if (!open(CONFFILE,$conffile)) {
    PERROR("impossible d'ouvrir le fichier $conffile");
    exit 1;
  }

  $trouve = goto_and_tokens($constantes::CLUSTER_TOKEN, $clustername);
  if ($trouve == 0) {
    PERROR("cluster '$clustername' introuvable dans le fichier de config\n");
    exit 1;
  }

  $ligne = next_line;
  $continue = 1;
  do {
    PDEBUGV("GET CL_EDEVS - ligne = $ligne\n");
    if ($ligne =~ /\}/) {
      $continue = 0;
    } else {
      $ligne =~ s/[\]\[]//g;
      @champs = split(/,/,$ligne);
      if (scalar(@champs) < 2) {
	$continue = 0;
      } else {
	push(@edevslist,(scalar(@champs),@champs));
      }
    }
    $ligne = next_line;
  } while ($continue);

  close($conffile);
  return @edevslist;
}

#(char *clustername, char* layout_type)
#      getdg_infos(char *conffile,
#                  char *groupname);
#
# parcourt le fichier de conf et renvoie le nom du cluster
# qui contient le groupe 'groupname' ainsi que la politique
# de placement utilisée pour ce groupe.
sub getdg_infos {
  my $conffile = $_[0];
  my $groupname = $_[1];
  my $ligne;
  my $trouve;

  if (!open(CONFFILE,$conffile)) {
    PERROR("impossible d'ouvrir le fichier $conffile");
    exit 1;
  }

  $trouve = goto_and_tokens($constantes::GROUP_TOKEN, $groupname);

  if ($trouve == 0) {
    PERROR("groupe '$groupname' introuvable dans le fichier de config\n");
    exit 1;
  }

  $ligne = next_line;
  PDEBUGV("F CNAME - ligne = $ligne\n");
  $clustername = $ligne;

  $ligne = next_line;
  PDEBUGV("F LTYPE - ligne = $ligne\n");
  $layouttype = $ligne;

  close($conffile);
  return ($clustername, $layouttype);
}

#(char *clustername, char* layout_type)
#      getdg_devslist(char *conffile,
#                     char *groupname);
#
# parcourt le fichier de conf et renvoie la liste des devices
# utilisés par le groupe 'groupname'
sub getdg_devslist {

  my $conffile = $_[0];
  my $groupname = $_[1];
  my $ligne;
  my $trouve;
  my @devslist;

  if (!open(CONFFILE,$conffile)) {
    PERROR("impossible d'ouvrir le fichier $conffile");
    exit 1;
  }

  $trouve = goto_and_tokens($constantes::GROUP_TOKEN, $groupname);
  if ($trouve == 0) {
    PERROR("groupe '$groupname' introuvable dans le fichier de config\n");
    exit 1;
  }

  $trouve = goto_and_tokens($constantes::GROUPDEV_TOKEN );
  if ($trouve == 0) {
    PERROR("can't find one of $constantes::GROUPDEV_TOKEN token for group '$groupname' in the config file\n");
    exit 1;
  }

  $ligne = next_line;
  $continue = 1;
  do {
    PDEBUGV("F DEVSLIST - ligne = $ligne\n");
    if ($ligne =~ /\}/) {
      $continue = 0;
    } else {
      chomp($ligne);
      @champs = split(/,/,$ligne);
      if (scalar(@champs) < 2) {
	$continue = 0;
      } else {
	push(@devslist,(scalar(@champs),@champs));
      }
    }
    $ligne = next_line;
  } while ($continue);

  close($conffile);
  return @devslist;
}

#(char *clustername, char* layout_type)
#      getdg_volslist(char *conffile,
#                     char *groupname);
#
# parcourt le fichier de conf et renvoie la liste des volumes
# contenus par le groupe 'groupname'
sub getdg_volslist {
  my $conffile = $_[0];
  my $groupname = $_[1];
  my $ligne;
  my $trouve;
  my @volslist;

  if (!open(CONFFILE,$conffile)) {
    PERROR("impossible d'ouvrir le fichier $conffile");
    exit 1;
  }

  $trouve = goto_and_tokens($constantes::GROUP_TOKEN, $groupname);
  if ($trouve == 0) {
    PERROR("groupe '$groupname' introuvable dans le fichier de config\n");
    exit 1;
  }

  $trouve = goto_and_tokens($constantes::GROUPVOL_TOKEN);
  if ($trouve == 0) {
    PERROR("can't find one of $constantes::GROUPVOL_TOKEN token for group '$groupname' in the config file\n");
    exit 1;
  }

  $ligne = next_line;
  $continue = 1;
  do {
    PDEBUGV("F VOLSLIST - ligne = $ligne\n");
    if ($ligne =~ /\}/) {
      $continue = 0;
    } else {
      chomp($ligne);
      @champs = split(/,/,$ligne);
      if (scalar(@champs) < 2) {
	$continue = 0;
      } else {
	push(@volslist,(scalar(@champs),@champs));
      }
    }
    $ligne = next_line;
  } while ($continue);

  close($conffile);
  return @volslist;
}


sub gethostslist {
  my $conffile = $_[0];
  my $clustername = $_[1];
  my $ligne;
  my $trouve;


  if (!open(CONFFILE,$conffile)) {
    PERROR("impossible d'ouvrir le fichier $conffile");
    exit 1;
  }

  $trouve = goto_and_tokens($constantes::CLUSTER_TOKEN, $clustername);
  if ($trouve == 0) {
    PERROR("cluster '$clustername' introuvable dans le fichier de config\n");
    exit 1;
  }

  $ligne = next_line;
  $continue = 1;
  do {
    PDEBUGV("HOSTSLIST - ligne = $ligne\n");
    if ($ligne =~ /\}/) {
      $continue = 0;
    } else {
      @champs = split(/,/,$ligne);
      push(@hostslist,$champs[0]);
    }
    $ligne = next_line;
  } while ($continue);

  close($conffile);
  return @hostslist;
}

# char *selectmaster(char *hostslist[]);
#
# élit un master node (i.e. le node qui effectuera les écritures d'état dans les superblocs)
# et renvoie le nom de ce master node.
sub selectmaster {
  my @hostslist = @_;
  my $masternode = $hostslist[0];

  return $masternode;
}

sub finddevpathlocal {
  my $conffile = $_[0];
  my $clustername =$_[1];
  my $host = $_[2];
  my $devname = $_[3];
  my $devpath;

  if (!open(CONFFILE,$conffile)) {
    PERROR("impossible d'ouvrir le fichier $conffile");
    exit 1;
  }

  $trouve = goto_and_tokens($constantes::CLUSTER_TOKEN, $clustername);
  if ($trouve == 0) {
    PERROR("cluster '$clustername' introuvable dans le fichier de config\n");
    exit 1;
  }

  $continue = 1;
  $ligne = next_line;
  do {
    PDEBUGV("FINDDEVPATH - ligne = $ligne\n");
    if ($ligne =~ /\}/) {
      $continue = 0;
    } else {
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
    $ligne = next_line;
  } while ($continue);

  close($conffile);

  return $devpath;
}

# char *finddevpath (char *conffile, char *clustername, 
#                    char *sourcehost, char * targethost, 
#                    char *devname);
#
# renvoie le path du real device 'devname' tel qu'il est
# visible dans le répertoire /dev du target host, sachant
# qui ce real device est situé sur le source host.
#
# Ex :
# le realdevice 'disqueATA1' situé sur sam4 (source host)
# en tant que /dev/hda6 a un devpath :
# égal à '/dev/scimapdev/sam4/disqueATA1' sur sam3 (target host)
# égal à '/dev/hda6' sur sam4 (target host)

sub finddevpath {
  my $conffile = $_[0];
  my $clustername =$_[1];
  my $sourcehost = $_[2];
  my $targethost = $_[3];
  my $devname = $_[4];
  my $devpath;

  if ($targethost eq $sourcehost) {
    $devpath = finddevpathlocal($conffile, $clustername, $targethost, $devname);
  } else {
    $devpath = "$constantes::NBDDEVPATH/$sourcehost/$devname";
  }
  return $devpath;
}

# (long long sizeKB, int major, int minor) 
#           getsizemajmin(char *host, 
#                         char*devpath)
# renvoie la taille en KB, le major number, et le minor number 
# du device de path 'devpath' sur le noeud 'host'
sub getsizemajmin {
  my $host = $_[0];
  my $devpath = $_[1];

  PTRACECMD("rsh $host $constantes::DEVINFOCMD $devpath\n");
  my $devinfo = `rsh $host $constantes::DEVINFOCMD $devpath`;
  chomp($devinfo);
  PDEBUG("DEVINFO = $devinfo\n");
  my @devinfos = split(/ /,$devinfo);

  return @devinfos;
}

# (long long sizeKB, int major, int minor, u32 uuid[4]) 
#           getsizemajmin(char *host, 
#                         char*devpath)
# renvoie la taille en KB, le major number, le minor number
# et l'UUID du device de path 'devpath' sur le noeud 'host'
sub getsizemajminuuid {
  my $host = $_[0];
  my $devpath = $_[1];

  PTRACECMD("rsh $host $constantes::DEVINFOCMD -u $devpath\n");
  my $devinfo = `rsh $host $constantes::DEVINFOCMD -u $devpath`;
  chomp($devinfo);
  PDEBUG("DEVINFO = $devinfo\n");
  my @devinfos = split(/ /,$devinfo);

  return @devinfos;
}

# int vol_in_use(char *host, char* groupname, char *zonename);
# fonction qui indique si la volume 'groupe:zonename' est en cours 
# d'utilisation sur le noeud 'host', renvoie TRUE/FALSE
sub vol_in_use {
  my $host = $_[0];
  my $groupname = $_[1];
  my $zonename = $_[2];

  PTRACECMD("ssh $host grep $zonename  $constantes::VRTPROCGROUPSDIR/$groupname/$constantes::VRTPROCUZ\n");
  my @result = `ssh $host grep $zonename  $constantes::VRTPROCGROUPSDIR/$groupname/$constantes::VRTPROCUZ`;
  PDEBUG("result = @result\n");
  return scalar(@result);
}

#termine le programme
sub byebye {
  my $complet = $_[0];
  if ($complet) {
    PTRACECMD("** ALL DONE **\n");
    exit 0;
  } else {
    PERROR("command didn't work successfully\n");
    exit 1;
  }
}
# *zones[] getallactivezones(char *host, char *groupname)
# renvoie toutes les zones actives du groupe 'groupename'
# sur le noeud 'host'
sub getallactivezones {
  my $host = $_[0];
  my $groupname = $_[1];
  my @zones;

  PTRACECMD("ssh $host cat $constantes::VRTPROCGROUPSDIR/$groupname/$constantes::VRTPROCAZ\n");
  my $infos = `ssh $host cat $constantes::VRTPROCGROUPSDIR/$groupname/$constantes::VRTPROCAZ`;
  PDEBUG("getallactivezones - AZ infos = $infos\n");
  chomp($infos);
  @zones = split(/ /,$infos);
  return @zones;
}
# *zones[] getallinusezones(char *host, char *groupname)
# renvoie toutes les zones actives du groupe 'groupename'
# sur le noeud 'host'
sub getallinusezones {
  my $host = $_[0];
  my $groupname = $_[1];
  my @zones;

  PTRACECMD("ssh $host cat $constantes::VRTPROCGROUPSDIR/$groupname/$constantes::VRTPROCUZ\n");
  my $infos = `ssh $host cat $constantes::VRTPROCGROUPSDIR/$groupname/$constantes::VRTPROCUZ`;
  PDEBUG("getallinusezones - UZ infos = $infos\n");
  chomp($infos);
  @zones = split(/ /,$infos);
  return @zones;
}

# bool is_groupactive(char *nodename, char *groupname);
#
# indique si le groupe 'groupname' est actif sur le noeud
# 'nodename'. renvoie 1 si c'est le cas et 0 sinon
sub is_groupactive {
  my $host = $_[0];
  my $groupname = $_[1];
  PTRACECMD("ssh $host \"ls $constantes::VRTPROCGROUPSDIR | grep $groupname\"\n");
  my $infos = `ssh $host \"ls $constantes::VRTPROCGROUPSDIR | grep $groupname\"`;
  $infos = cleanup_line($infos);
  PDEBUG("is_groupactive : return=$infos\n");

  if (length($infos) > 0) {
    return 1;
  } else {
    return 0;
  }
}

# char *groupsnames[] getgroupslist(char *conffile, char *clustername)
#
# renvoie la liste des device groups du cluster 'clustername'
# en lisant le fichier de configuration passé en paramètre
sub getgroupslist {
  my $conffile = $_[0];
  my $clustername = $_[1];
  my $ligne;
  my $trouve;
  my @groupslist;

  PDEBUG("getgroupslist: conffile=$conffile, clustername=$clustername.\n");
  if (!open(CONFFILE,$conffile)) {
    PERROR("impossible d'ouvrir le fichier $conffile");
    exit 1;
  }
  do {
    $ligne = getline_token($constantes::GROUP_TOKEN);
    PDEBUGV("GROUPSLIST - ligne = $ligne\n");
    if ($ligne) {
      $ligne =~ /($constantes::GROUP_TOKEN)(.*)(\{)/;
      my $groupname = $2;
      $ligne = next_line;
      PDEBUGV("GROUPSLIST - nextline=$ligne\n");
      if ($ligne eq $clustername) {
	PDEBUGV("adding '$groupname' to list\n");
	push(@groupslist, $groupname);
      }
    }
  } while ($ligne);

  close($conffile);

  PDEBUG("getgroupslist: return=@groupslist.\n");
  return @groupslist;
}

# char *groupsnames[] getgroupslist(char *conffile, char *clustername)
#
# renvoie la liste des device groups du cluster 'clustername'
# en lisant le fichier de configuration parsé et passé en paramètre
sub getgroupslist2 {
  my $config = $_[0];
  my $clustername = $_[1];
  my @groupslist;

  for (my $dg = 0; $dg <= $#{$config->{devicegroup}}; $dg++) {
    if($config->{devicegroup}->[$dg]->{cluster} eq $clustername) {
      push(@groupslist, $config->{devicegroup}->[$dg]->{name});
    }
  }

  PDEBUG("getgroupslist: return=@groupslist.\n");
  return @groupslist;
}

# Check 2 items don't have the same name in the same group
# groupname : text describing the test done
# group[]   : an array containing the items to test
# name      : the element to test in the group array
sub has_double {
  my $groupname = $_[0];
  my $group = $_[1];
  my $name = $_[2];
  my $error = 0;

  # Check 2 items don't have the same name in the same group
  PDEBUGV "Checking multiple $name in $groupname\n";

  for ( my $i = 0; $i <= $#{$group}; $i++ ) {
    for ( my $j = $i + 1; $j <= $#{$group}; $j++ ) {
      if ( $group->[$i]->{$name}
	   eq $group->[$j]->{$name} ) {
	print "ERROR: $groupname $group->[$i]->{$name} redefined\n";
	$error = 1;
      }
    }
  }

  # Also check the name is allowed
  $error += has_good_name($groupname, $group,  $name);

  return $error;
}

# Check items name respects the naming convention
# groupname : text describing the test done
# group[]   : an array containing the items to test
# name      : the element to test in the group array
sub has_good_name {
  my $groupname = $_[0];
  my $group = $_[1];
  my $name = $_[2];
  my $error = 0;

  # Check 2 items don't have the same name in the same group
  PDEBUGV "Checking naming for $name in $groupname\n";

  for ( my $i = 0; $i <= $#{$group}; $i++ ) {

    if ( length($group->[$i]->{$name}) > $constantes::CONFFILE_NAMESIZE) {
      print "ERROR: $groupname $group->[$i]->{$name} has more than $constantes::CONFFILE_NAMESIZE digits\n";
      $error++;
    }

    if ($group->[$i]->{$name} =~ /$constantes::CONFFILE_ALLOWEDREGEXP/) {
      print "ERROR: $groupname $group->[$i]->{$name} has character not in \"$constantes::CONFFILE_ALLOWEDNAME\"\n";
      $error++;
    }
  }

  return $error;
}

# Validate config
# Verify the coherency of the config file
#- que deux volumes n'aient pas le même nom dans un même groupe
# - que 2 noeuds n'aient pas le même nom dans le cluster
# - que 2 devices n'aient pas le même nom sur un noeud (par exemple, pas de
# "sam4, disqueATA1, disqueATA2, disqueATA2" dans la description des devices
# utilisés par un groupe)
# - que 2 devices groups n'aient pas le même nom (même s'ils sont dans 2
# clusters différents... parce qu'à terme, l'entité englobante n'est pas le
# cluster mais le device group qui pourra s'étendre sur plusieurs clusters..)
# - que 2 clusters n'aient pas le même nom
# - que les sizes des volumes soient bien des nombres
# - que les noms des volumes, clusters, noeuds, devices group, et devices
# aient une taille <= 16 caractères, et un caractère appartient à A-Z a-z _ 0-9

sub verify_config {
  my $config = $_[0];
  my $error  = 0;

  # Device group verifications
  $error += has_double("Device Group",  $config->{devicegroup},  "name");

  for (my $d = 0; $d <= $#{$config->{devicegroup}}; $d++) {

    if ( defined($config->{devicegroup}->[$d]->{physical}->[1])) {
      print "ERROR: In device group $config->{devicegroup}->[$d]->{name}, physical is redefined\n";
      $error++;
    }

    if ( defined($config->{devicegroup}->[$d]->{logical}->[1])) {
      print "ERROR: In device group $config->{devicegroup}->[$d]->{name}, logical is redefined\n";
      $error++;
    }
	
    $error += has_double("Device Group $config->{devicegroup}->[$d]->{name} / Logical / Device",
			 $config->{devicegroup}->[$d]->{logical}->[0]->{device},  "name");
    $error += has_double("Device Group $config->{devicegroup}->[$d]->{name} / Physical / Device",
			 $config->{devicegroup}->[$d]->{physical}->[0]->{device}, "node");

    # Check devicegroup/logical/device/sizeMB is a valid number
    for (my $dv = 0; $dv <= $#{$config->{devicegroup}->[$d]->{logical}->[0]->{device}}; $dv++) {
      if (!int($config->{devicegroup}->[$d]->{logical}->[0]->{device}->[$dv]->{sizeMB})) {
	$error++;
	print "ERROR: In device group $config->{devicegroup}->[$d]->{name} / logical / device $config->{devicegroup}->[$d]->{logical}->[0]->{device}->[$dv]->{name} sizeMB ($config->{devicegroup}->[$d]->{logical}->[0]->{device}->[$dv]->{sizeMB}) is not a numeric value\n";
      }
    }
  }

  # Cluster verifications
  $error += has_double("Cluster",  $config->{cluster},  "name");
  for (my $c = 0; $c <= $#{$config->{cluster}}; $c++) {
    $error += has_double("Cluster $config->{cluster}->[$c]->{name}/ Node",
			 $config->{cluster}->[$c]->{node},                         "name");

    for (my $n = 0; $n <= $#{$config->{cluster}->[$c]->{node}}; $n++) {
      $error += has_double("Cluster $config->{cluster}->[$c]->{name}/ Node $config->{cluster}->[$c]->{node}->[$n]->{name}/ Device",
			   $config->{cluster}->[$c]->{node}->[$n]->{device},         "name");
    }
  }

  return $error;
}

#
# Dump the configuration file
sub dump_config {
  my $config = $_[0];
  print Dumper($config);
}

# Read the configuration file
# and return it
sub read_config {
  my $conffile = $_[0];

  my $config = XMLin($conffile,
		       forcearray => 1,
		     keyattr => []);

  &commun::dump_config($config);
  print "-------------------\n";

  if (&verify_config($config) != 0) {
    print "ERROR: Please fix the configuration file $conffile\n";
    exit;
  }

  return $config;
}
