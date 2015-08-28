# bool running_daemon(char *daemon_name, char* node_name);
#
# regarde si le démon 'daemon_name' tourne sur le noeud 'node_name'
# renvoie 1 si c'est le cas et 0 sinon
sub running_daemon {
  my $daemon_name=$_[0];
  my $node_name=$_[1];
  my $infos;

  PTRACECMD("ssh $node_name \"$constantes::TESTLOCKCMD $constantes::LOCKPATH/$daemon_name.lock\"\n");
  $infos =`ssh $node_name \"$constantes::TESTLOCKCMD $constantes::LOCKPATH/$daemon_name.lock\"`;
  PDEBUG("return=$infos.\n");
  if ($infos eq "O") {
    return 1;
  }
  else {
    return 0;
  }
}

# bool module_loaded(char *module_name, char* node_name);
#
# regarde si le module 'module_name' a été chargé sur le noeud 'node_name'
# renvoie 1 si c'est le cas et 0 sinon
sub module_loaded {
  my $module_name=$_[0];
  my $node_name=$_[1];
  my $infos;

  PTRACECMD("ssh $node_name \"$constantes::LSMOD | grep $module_name\"\n");
  $infos =`ssh $node_name \"$constantes::LSMOD | grep $module_name\"`;
  PDEBUG("return=$infos.\n");
  if ($infos) {
    return 1;
  }
  else {
    return 0;
  }
}

# note : les démons/modules d'import/export sont lancés sur TOUS
#        les noeuds du cluster. Même si certains noeuds ne sont
#        qu'importateurs ou qu'exportateurs.
sub exporte_devs {
  my $i = 0;
  my $j;
  my $launch;

  while ($i < scalar(@edevslist)) {
    my $nbpathsnames = $edevslist[$i] - 1;
    my $enode = $edevslist[$i+1];
    print "***** EXPORTATION POUR $enode *****\n";
    $launch = 0;
    $i = $i + 2;
    if (!running_daemon($constantes::SERVERDNAME,$enode)) {
      PSYSTEM("ssh $enode \"$constantes::SERVERD\"");
      $launch = 1;
    }
    sleep $SLEEPTIME;
    if (!running_daemon($constantes::EXPORTDNAME,$enode)) {
      PSYSTEM("ssh $enode \"$constantes::EXPORTD \\`$constantes::PIDOF $constantes::SERVERDNAME\\`\"");
      $launch = 1;
    }
    if ($launch) {
      for ($j = $i; $j < $i+$nbpathsnames; $j = $j+2) {
	my $dev_name = $edevslist[$j];
	my $path_name = $edevslist[$j+1];
	PSYSTEM("ssh $enode \"$constantes::EXPORTE -a $path_name $dev_name\"");
      }
    }
    $i = $i + $nbpathsnames;
  }
}

sub importe_devs {
  my $launch;
  foreach $import_host (@hostslist) {
    print "***** IMPORTATION POUR $import_host *****\n";
    $launch = 0;
    if (!module_loaded($constantes::NBDMODNAME,$import_host)) {
      PSYSTEM("ssh $import_host \"$constantes::INSMOD $constantes::NBDMOD\"");
      $launch = 1;
    }

    if (!module_loaded($constantes::VRTMODNAME,$import_host)) {
      PSYSTEM("ssh $import_host \"$constantes::INSMOD $constantes::VRTMOD\"");
      $launch = 1;
    }
	
    if ($launch) {
      foreach $current_host (@hostslist) {
	if ($import_host ne $current_host) {
	  PSYSTEM("ssh $import_host \"$constantes::IMPORTE $current_host\"");
	}
      }
    }
  }
}

1;
