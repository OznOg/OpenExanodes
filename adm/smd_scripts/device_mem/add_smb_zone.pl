#!/usr/bin/perl

# usage : add_smb_zone <nom de la zone>

# Procedure acceptant en parametre le nom de la zone a ajouter au serveur Samba

# Definitions de variables
$StorfactoryPath	= "/mnt/storfactory";

# lecture du fichier de configuration de samba
open (SMBFILE, '>>/usr/samba/lib/smb.conf') || die "Impossible d'ouvrir smb.conf : $!";

print SMBFILE	"[$ARGV[0]]\n";
print SMBFILE	"\tcomment = StorFactory $ARGV[0]\n";
print SMBFILE	"\tpath = $StorfactoryPath/$ARGV[0]\n";
print SMBFILE	"\tpublic = yes\n";
print SMBFILE	"\twritable = yes\n";
print SMBFILE	"\tprintable = no\n";
print SMBFILE	"\n";

close SMBFILE;

# Prise en compte du nouveau fichier par le serveur samba
system '/usr/bin/killall -HUP smbd'
