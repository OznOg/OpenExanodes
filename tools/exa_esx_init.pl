#!/usr/bin/perl
#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

#
# Installation script for Exanodes VM
#

use strict;
use warnings;
use Getopt::Long;
use POSIX qw( :stdlib_h :sys_wait_h );
use Sys::Hostname;
use File::Basename;

# Default number of virtual machines
use constant DEFAULT_NR_VMS => 1;

my $dry_run = 0;
my $nr_vms  = DEFAULT_NR_VMS;
my $host_id = 0;
my $vmx_archive_url;
my $vmx_archive;
my $systemstore;
my $exastore;

#
# Display usage.
#
sub print_usage
{
    print <<EOF
Usage: $0 [OPTIONS]
Init VMware virtual machines with a configuration that should be
suitable for Seanodes' use.

  -n, --nr-vms       Number of virtual machines to create.
  -i, --image        URL of a VMX Zip archive.
  -s, --systemstore  Path of the datastore where the VMX archive will
                     be unzipped.
  -e, --exastore     Path of the datastore where virtual disks will be
                     created in order to provide storage to Exanodes.
  -d, --dry-run      Does nothing, simply print commands that would be
                     executed.
  -h, --help         Display this help and exit.
EOF
}


#
# Parse arguments on the command line.
#
sub parse_args
{
    my %options = ();
    unless (GetOptions(
        "nr-vms|n=i"      => \$options{'nr_vms'},
        "image|i=s"       => \$options{'image'},
        "systemstore|s=s" => \$options{'systemstore'},
        "exastore|e=s"    => \$options{'exastore'},
        "dry-run|d"       => sub { $dry_run = 1 },
        "help|h"          => \$options{'help'}
    )) {
        print STDERR "Check command usage with --help.\n";
        exit(EXIT_FAILURE);
    }

    if (defined($options{'help'})) {
        print_usage();
        exit(EXIT_SUCCESS);
    }

    if (defined($options{'nr_vms'})) {
        $nr_vms = $options{'nr_vms'};
    }

    if (defined($options{'image'})) {
        $vmx_archive_url = $options{'image'};
        $vmx_archive = basename($vmx_archive_url);
    } else {
        print STDERR "You must provide the URL of a VMX archive with --image.\n";
        exit(EXIT_FAILURE);
    }

    if (defined($options{'systemstore'})) {
        $systemstore = $options{'systemstore'};
    } else {
        print STDERR "You must provide a datastore path for the system with --systemstore.\n";
        exit(EXIT_FAILURE);
    }

    if (defined($options{'exastore'})) {
        $exastore = $options{'exastore'};
    } else {
        print STDERR "You must provide a datastore path for Exanodes disks with --exastore.\n";
        exit(EXIT_FAILURE);
    }
}


#
# Run the given command.
# \param $cmd command to run
# \return command's status and output
#
sub run
{
    my $cmd  = shift;

    if ($dry_run) {
        print "CMD: ${cmd}\n";
        return (EXIT_SUCCESS, (""));
    }

    my @output = `$cmd`;
    my $status = EXIT_SUCCESS;

    if (WIFEXITED($?)) {
        $status = WEXITSTATUS($?);
    } else {
        # The command has been interrupted by a signal. This should
        # never occur.
        $status = EXIT_FAILURE;
    }
    return ($status, @output);
}


#
# Get host ID (for instance, host ID of 172.16.129.1 is 129)
#
sub get_host_id
{
    my $hostname = shift;
    my $host = `host $hostname`;

    # Convert IP address to host ID
    # for 172.16.129.1, host ID is 129
    $host =~ /^.*172\.16\.([0-9]*)\..*$/;
    return $1
}


#
# Generate a suitable MAC address
# \param $host_id  Host ID
# \param $vm_id    Virtual machine ID
#
sub get_mac_address
{
    my $host_id = shift;
    my $vm_id   = shift;
    return sprintf("00:50:56:%02d:%02d:%02d", int($host_id / 100), $host_id % 100, $vm_id);
}


#
# Main
#

parse_args();

my $ret = 0;
my @output;

# Compute host's ID
$host_id = get_host_id(hostname());

# Print a summary of the configuration that will be set up
print "* Number of VMs      : ${nr_vms}\n";
print "* VMX image          : ${vmx_archive}\n";
print "* System datastore   : ${systemstore}\n";
print "* Exanodes datastore : ${exastore}\n";
print "* Host ID            : ${host_id}\n";
print "\n";

# Retrieve VMX image
print "* Downloading VMX image...\n";
($ret, @output) = run("wget --quiet " . $vmx_archive_url . " --output-document=/tmp/" . $vmx_archive);
die "Failed to download ${vmx_archive_url}" if $ret;


for (my $i = 1; $i <= $nr_vms; $i++) {
    my $vm_name = "ExaVM" . $i;
    my $vm_dir = $systemstore . "/${vm_name}";

    print "\n";
    print "* Initializing virtual machine '${vm_name}'\n";

    # Create directory for the VM files
    mkdir $vm_dir unless $dry_run;

    # Unzip the VMX archive that contains the virtual machine
    print "  - Unzipping VMX archive (please be patient)\n";
    ($ret, @output) = run("unzip -e /tmp/${vmx_archive} -d ${vm_dir}");

    # Get the VMX configuration file
    my @vmx_files;
    my $vmx_file;
    if ($dry_run) {
        $vmx_file = "/fake/vmx/file.vmx";
    } else {
        @vmx_files = glob "${vm_dir}/*.vmx";
        $vmx_file = $vmx_files[0];
    }

    # Add a correct MAC address based on the host ID
    my $mac_addr = get_mac_address($host_id, $i);
    print "  - Setting MAC address to ${mac_addr}\n";

    # Append the MAC address to the configuration file
    run("sed -i -e '/vpx/d' ${vmx_file}");
    run("echo 'ethernet0.addressType = \"static\"' >> ${vmx_file}");
    run("echo 'ethernet0.address = \"${mac_addr}\"' >> ${vmx_file}");

    # Change the display name
    run("sed -i -e '/displayName/d' ${vmx_file}");
    run("echo 'displayName = \"${vm_name}\"' >> ${vmx_file}");

    # Retrieve the UUID based path of the datastore for Exanodes
    # FIXME: workaround for Perl v5.8.0. With Perl v5.8.0
    #        "basename(foo/bar/)" does not return "bar".
    #        As a result, we must remove the trailing "/" if any.
    my $last_char = chop($exastore);
    if (! $last_char eq "/") {
        $exastore .= $last_char;
    }
    my $exastore_name = basename($exastore);
    my $exastore_path_uuid = '';
    ($ret, @output) = run("vmware-vim-cmd hostsvc/summary/fsvolume | grep ${exastore_name}");
    if ($dry_run) {
        $exastore_path_uuid = "/vmfs/volumes/AAAAAAAA-BBBBBBBB-CCCCCCCC-DDDDDDDD";
    } else {
        my @fields = split(/\s+/, $output[0]);
        $exastore_path_uuid = $fields[3];
    }
    print "  - Exanodes datastore is ${exastore_path_uuid}\n";

    # Create a 10 GB virtual disk
    my $exadisk_path = "${exastore_path_uuid}/${vm_name}_disk1.vmdk";
    run("vmkfstools -c 10G -a lsilogic ${exadisk_path}");

    # Add the disk to the configuration file
    run("echo 'scsi0:2.present = \"true\"'                 >> ${vmx_file}");
    run("echo 'scsi0:2.fileName = \"${exadisk_path}\"'     >> ${vmx_file}");
    run("echo 'scsi0:2.mode = \"independent-persistent\"'  >> ${vmx_file}");
    run("echo 'scsi0:2.deviceType = \"scsi-hardDisk\"'     >> ${vmx_file}");

    # Register the virtual machine
    run("vmware-vim-cmd solo/registervm ${vmx_file}");

    # Get the VM ID
    my $vm_id;
    ($ret, @output) = run("vmware-vim-cmd vmsvc/getallvms | grep $ {vm_name}");
    if ($dry_run) {
        # Fake VM ID
        $vm_id = "X";
    } else {
        my @fields = split(/\s+/, $output[0]);
        $vm_id = $fields[0];
    }
    print "  - Virtual machine registered as ${vm_id}\n";

    # Power on the virtual machine
    print "  - Power on the virtual machine\n";
    run("vmware-vim-cmd vmsvc/power.on ${vm_id}");
}
