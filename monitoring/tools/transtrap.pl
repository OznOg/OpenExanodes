#/usr/bin/perl

use strict;
use warnings;

#use trans_md_enum;
use POSIX qw( :sys_wait_h );

my $mib_name;
my $translate_exe  = "snmptranslate";
my $translate_opts = "-m";

# ref to hash containing resources status in human readable form
# generated with trans_md_enum.pl from md_types.h
my $status_r = {
                'exaFilesystemStatus' => {'6' => 'OFFLINE',
                                          '4' => 'WILLSTOP',
                                          '1' => 'STARTED',
                                          '3' => 'WILLSTART',
                                          '2' => 'INUSE',
                                          '5' => 'READONLY'
                                         },
                'exaDiskGroupStatus' => {'6' => 'REBUILDING',
                                         '4' => 'DEGRADED',
                                         '1' => 'STOPPED',
                                         '3' => 'USINGSPARE',
                                         '2' => 'OK',
                                         '5' => 'OFFLINE'
                                        },
                'exaDiskStatus' => {'6'  => 'UPDATING',
                                    '3'  => 'DOWN',
                                    '7'  => 'REPLICATING',
                                    '9'  => 'BLANK',
                                    '2'  => 'OK',
                                    '8'  => 'OUTDATED',
                                    '1'  => 'UP',
                                    '4'  => 'BROKEN',
                                    '10' => 'ALIEN',
                                    '5'  => 'MISSING'
                                   },
                'exaNodeStatus' => {'1' => 'DOWN',
                                    '2' => 'UP'
                                   },
                'exaVolumeStatus' => {'6' => 'OFFLINE',
                                      '4' => 'WILLSTOP',
                                      '1' => 'STARTED',
                                      '3' => 'WILLSTART',
                                      '2' => 'INUSE',
                                      '5' => 'READONLY'
                                     }};

# this leaf is the enterprise root of the MIB
my $enterprise_oid = '1.3.6.1.4.1.';

sub run_command
{
    my ($cmd) = @_;

    my @output = `export LC_ALL=C; $cmd 2>&1`;
    my $status;
    if   (WIFEXITED($?)) {$status = WEXITSTATUS($?);}
    else                 {$status = 1;}
    return ($status, \@output);
}

# calls snmptranslate to convert sth like 30730.1.4.3.1 into exaDiskGroupNotification
sub translate_oid
{
    my ($oid) = @_;
    return $oid if (not defined($oid));
    my $cmd = "$translate_exe $translate_opts $mib_name $enterprise_oid" . $oid;

    #    print "BUGY $cmd\n";
    my ($st, $out_r) = run_command($cmd);
    if ($st)
    {
        die "Unable to extract '$oid' from MIB '$mib_name'";
    }

    my ($objtype) = $out_r->[0] =~ m/${mib_name}::(.*)$/ix;
    return $objtype;
}

# big giant grep on log line.
sub extract_trap_data
{
    my ($line) = @_;

    # No use for other messages. This could be useless if only traphandle directive
    # worked properly.
    my ($date, $time, $node) =
      $line =~ m/^.*snmptrapd.*: (\d\d\d\d-\d\d-\d\d) (\d\d:\d\d:\d\d) (\S+) \[/;

    my ($OID) = $line =~ m/SNMPv2-MIB::snmpTrapOID.*enterprises\.(\S+)\t/ix;

    # Then we extract the data. They contain the id of the resource, the name
    # and the status.
    my ($v1, $v2, $v3) = $line =~ m/\t(.*)\t(.*)\t(.*)$/ix;

    if (not(defined($v1)) or not(defined($v2)) or not(defined($v3)))
    {
        die "data extraction failed";
    }

    my $trap_r;
    my $optype = translate_oid($OID);
    if (not defined($optype))
    {
        die "Unable to translate trap OID";
    }
    $trap_r->{'OID'} = $optype;

    $trap_r->{'date'} = $date;
    $trap_r->{'time'} = $time;
    $trap_r->{'node'} = $node;

    my $ext_data = sub {
        my ($l, $key) = @_;

        my ($oid, $value) = $l =~ m/SNMPv2-SMI::enterprises.(\S+) = .*: (.*)/;
        if (not defined($oid) or not defined($value))
        {
            die "unable to grep oid or value";
        }

        my $convert = translate_oid($oid);
        if (not defined($convert))
        {
            die "unable to translate '$oid'";
        }

        $trap_r->{$key}->{'OID'} = $convert;

        if (grep(/resource_status/, $key))
        {
            $value =~ s/\s//gix;
            $trap_r->{$key}->{'value'} = $status_r->{$convert}->{$value};
        }
        else
        {
            $trap_r->{$key}->{'value'} = $value;
        }

    };

    &$ext_data($v1, 'resource_id');
    &$ext_data($v2, 'resource_name');
    &$ext_data($v3, 'resource_status');

    return $trap_r;
}

# Given a hash ref, displays something as pretty as possible.
sub pretty_print
{
    my ($trap_r) = @_;

    my $str = <<EOF;
[$trap_r->{'date'} $trap_r->{'time'}] $trap_r->{'node'}
Event $trap_r->{'OID'}
Resource $trap_r->{'resource_name'}->{'value'} (id = $trap_r->{'resource_id'}->{'value'})
$trap_r->{'resource_status'}->{'OID'} changed to $trap_r->{'resource_status'}->{'value'}

EOF
    print $str;
}

sub usage
{
    print "USAGE: $0 LOGILE MIBNAME\nMIBNAME must be in SNMP MIB path\n";
}

############
#
# Main
#
############

if ($#ARGV != 1)
{
    usage();
    exit 1;
}

(my $logfile, $mib_name) = @ARGV;

# to be cleaner, we should use this instead of the hardcoded
# hashref on top of this file.
# $status_r = trans_md_enum::convert($headerfile);

open(my $F, '<', $logfile) or die("unable to open $logfile");

my $count = 0;
foreach (<$F>)
{
    next if (grep !/snmptrapd/ix, $_);
    next if (grep !/30730/ix,     $_);
    next if (grep !/MIB/ix,       $_);

    my $trap_r = extract_trap_data($_);
    pretty_print($trap_r);
    $count++;
}

close($F);
print "------- $count traps treated successfully\n";
