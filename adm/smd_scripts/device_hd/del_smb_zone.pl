#!/usr/bin/perl

# usage : del_smb_zone <nom de la zone>

# Procedure acceptant en parametre le nom de la zone a retirer du serveur Samba

# Definitions de variables
$StorfactoryPath	= "/mnt/storfactory";

# lecture du fichier de configuration de samba
open (SMBFILE, "/usr/samba/lib/smb.conf") || die "Impossible d'ouvrir smb.conf : $!";

# Ouverture du nouveau fichier de configuration samba
open (SMBNEW, ">/usr/samba/lib/smb.conf.new") || die "Impossible d'ouvrir smb.conf.new : $!";

# Parcours du fichier jusqu'a trouver la definition de zone
$addline=1;
while ($ligne = <SMBFILE>) {
	if ($ligne =~ /$ARGV[0]/)
	{
		$addline=0;
	}
	elsif ($ligne =~ /^\[/)
	{
		$addline=1;
	}
	
	if ($addline==1)
	{
		printf SMBNEW $ligne;
	}
}

# Fermeture des fichiers
close SMBNEW;
close SMBFILE;

# Recopie du nouveau fichier de conf vers le courant
system("/bin/mv -f /usr/samba/lib/smb.conf.new /usr/samba/lib/smb.conf");

# Prise en compte du nouveau fichier par le serveur samba
system '/usr/bin/killall -HUP smbd';
