#!/usr/bin/perl

#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

#
# TODO:
#  * Check for node ownership using nodereserve
#

use strict;
use warnings;

use Getopt::Long;
use File::Basename;
use Net::Ping;
use POSIX ":sys_wait_h";
use Win32::Console::ANSI;
use List::MoreUtils qw/part/;

my $PROGNAME = basename($0);

my $DEBUG = defined($ENV{PUSHEXAWIN_DEBUG}) && $ENV{PUSHEXAWIN_DEBUG} != "0";

# Platform this script is running on
my $PLATFORM = $^O;
chomp($PLATFORM);

# Wix source used to build the installer
my $WIX_SOURCE = "exanodes.wxs";

# MSI installer to push
my $INSTALLER = undef;

# Product id (Windows GUID)
my $PRODUCT_ID = undef;

# Nodes to push on
my @NODENAMES = ();

# Target nodes login
my $USER     = "Administrator";
my $PASSWORD = "toto11";

# Directory where the installer is copied
my $REMOTE_DIR = "C:\\Users\\${USER}\\Desktop";

# List of components selected for installation
my @SELECTED_FEATURES = ();

# List of the features supported by the installer
my @INSTALLER_FEATURES = ('Nodes', 'CLI', 'Tools');

# Maximum number of workers that can push in parallel
my $NBMAX_WORKERS = 6;

#
# Print script usage
#
sub usage() {
    print "Push an MSI-packaged product.

Usage: ${PROGNAME} [OPTION] -s INSTALLER NODENAME...

where INSTALLER   is the MSI installer of the product
      NODENAME... is a list of regexes for the nodes on which to install
                  the product.

The script is assumed to be run on Windows, from the toplevel of an
Exanodes build directory.

Push options:

  -s, --source=FILE        Specify the installer package file
  -u, --user=USER          Push as user USER (default=${USER})
  -p, --password=PASSWORD  Define password (default=${PASSWORD})
  -h, --help               Display this help and exit
\n";
}


#
# Filter installer options out of the list and add them in the list of
# selected components.
#
sub filter_installer_options {
    my @args = @_;
    my @not_passed;

    while (my $arg = shift(@args)) {
        my ($prefix, $opt) = $arg =~ /^--(with)-(.*)/;
        if (defined($prefix) && defined($opt)) {
            # Push options are lower case while the MSI features are
            # capitalized or upper case.
            if ($opt eq "nodes") {
                $opt = "Nodes";
            } elsif ($opt eq "cli") {
                $opt = "CLI";
            } elsif ($opt eq "tools") {
                $opt = "Tools";
            }
            if (grep(/${opt}/, @INSTALLER_FEATURES)) {
                push(@SELECTED_FEATURES, $opt);
            } else {
                &print_error("Unknown installer option: '${arg}'.");
                exit 1;
            }
        } else {
            push(@not_passed, $arg)
        }
    }

    if (grep(/debug/, @SELECTED_FEATURES) &&
        !grep(/node|cli/, @SELECTED_FEATURES)) {
        &print_error("Option --with-debug requires --with-nodes or --with-cli.");
        exit 1;
    }

    return @not_passed;
}


#
# Initialize options
#
sub init() {
    my %options;

    Getopt::Long::Configure("pass_through");
    Getopt::Long::Configure("no_auto_abbrev");
    if (!GetOptions(\%options, "source|s=s",
                    "user|u=s", "password|p=s",
                    "help|h")) {
        print STDERR "Try `$0 --help' for more information.\n";
        exit 1;
    }

    if ($options{"help"}) {
        &usage();
        exit 0;
    }

    @ARGV = &filter_installer_options(@ARGV);

    $INSTALLER = $options{"source"} if defined($options{"source"});
    $USER      = $options{"user"} if defined($options{"user"});
    $PASSWORD  = $options{"password"} if defined($options{"password"});
    # TODO: Check for duplicate nodes like in pushexa.pl
    @NODENAMES = name_expand(@ARGV) if scalar(@ARGV);
}


#          /^\
# FIXME:  / ! \  Ugly copy paste from pushexa.pl!
#        /__!__\

#
# Colors.
#
my $COLOR_SUCCESS = "\e[1;32m";
my $COLOR_FAILURE = "\e[1;31m";
my $COLOR_WARNING = "\e[1;34m";
my $COLOR_NORMAL  = "\e[0;39m";

sub success { return $COLOR_SUCCESS . "@_" . $COLOR_NORMAL; }
sub failure { return $COLOR_FAILURE . "@_" . $COLOR_NORMAL; }
sub warning { return $COLOR_WARNING . "@_" . $COLOR_NORMAL; }

# Message printed at beginning of current step
my $step_host;
my @step_msg;

#
# Print a debug message
#
sub debug {
    print "DEBUG: @_\n" if $DEBUG;
}

#
# Build a message prefixed with the name of a target host (if any)
#
sub msg($@) {
    my ($target_host, @msg) = @_;
    return "@{msg}" if $target_host eq "";
    return "${target_host}: @{msg}";
}

#
# Report current step
#
sub step($@) {
    my ($target_host, @msg) = @_;

    print &msg($target_host, "@{msg}...") . "\n";

    $step_host = $target_host;
    @step_msg = @msg;
}

#
# Report success or failure of current step
#
sub result($$) {
    my ($err, $details) = @_;

    my $color = ($err ? \&failure : \&success);

    print &$color(&msg($step_host, "@{step_msg}: "
		       . ($err == 0 ? "OK" : "FAILED"))) . "\n";
    print "\t${details}\n" if defined($details);

    exit 1 if $err;
}

#
# Print an error message
#
sub print_error {
    print STDERR &failure("ERROR: @_") . "\n";
}

#
# Expand node names
#
sub name_expand($) {
  my $all_nodes    = shift;

  if($all_nodes eq "*") {
    return "*";
  }

  my @result;

  my @nodes_list = split(/ /, $all_nodes);

  foreach my $nodes (@nodes_list) {

    if($nodes =~ /\/\d+\//) {
      # Single // quoted number, just remove the //
      $nodes =~ s/\/(\d+)\//$1/g;
      push(@result, "$nodes");
    } elsif ($nodes !~ /\/\d+:|-\d+[:\-\d]*\//) {
      # Is there really a regexp there. If not, push the current node in the result stack
      push(@result, "$nodes");
    } else {
      # Extract the prefix
      my ($node_name, $node_expr, $remaining) = $nodes =~ /^(.*?)(\/\d+[0-9:\-]+\/)+?(.*)/;

      # Remove the //
      $node_expr =~ s/\///g;

      my @expr_numbers = split(/[:\-]/,$node_expr);

      my @expr_expr    = split(/[0-9]+/,$node_expr);
      my $lost = shift(@expr_expr);

      my $previous = -1;
      foreach my $number (@expr_numbers) {

	my $expr = shift(@expr_expr);
	if (!defined($expr)) {
	  $expr = ":";
	}

	if ($expr eq ":") {
	  if ($previous != -1) {
	    for (my $i = $previous ; $i <= $number ; $i++) {
	      # Recursively create an expansion if remaining regexp
	      if($remaining =~ /\//) {
		my @subnames = &name_expand("$remaining");
		foreach my $subname (@subnames) {
		  push(@result, "$node_name$i$subname");
		}
	      } else {
		push(@result, "$node_name$i$remaining");
	      }
	    }
	    $previous = -1;
	  } else {
	    # Recursively create an expansion if remaining regexp
	    if($remaining =~ /\//) {
	      my @subnames = &name_expand("$remaining");
	      foreach my $subname (@subnames) {
		push(@result, "$node_name$number$subname");
	      }
	    } else {
	      push(@result, "$node_name$number$remaining");
	    }
	  }
	} elsif ($expr eq "-") {
	  if ($previous != -1) {
	    &print_error("In number expansion.  Two characters dash ('-') found after number '$previous'. Use the ':' separator instead");
	    exit 1;
	  }
	  $previous = $number;
	} else {
	  &print_error("In number expansion. Don't know how to handle '$expr'");
	  exit 1;
	}
      }
    }
  }

  return @result;
}

#          /^\
# FIXME:  / ! \  End of ugly copy paste from pushexa.pl!
#        /__!__\

#
# Invoke `psexec` to execute a program on a remote host
#
sub psexec($@) {
    my ($host, $prog) = @_;
    my $psexec_cmd = "psexec \\\\${host} -u ${USER} -p ${PASSWORD} -w ${REMOTE_DIR} ${prog}";

    &debug("psexec_cmd: ${psexec_cmd}");

    my @lines = `$psexec_cmd 2>&1`;
    # WIFEXITED and WEXITSTATUS are not implemented on Windows.
    # Assume the command was not signaled and return exit code.
    my $status = $? >> 8;

    return ($status, @lines);
}


#
# Invoke `xcopy` to copy a file
#
sub xcopy($$) {
    my ($source, $dest) = @_;

    my $filename = basename($source);
    my $xcopy_cmd = "xcopy /Q /Y ${source} ${dest}\\";

    &debug("xcopy_cmd: ${xcopy_cmd}");

    `$xcopy_cmd 2>&1`;
    # WIFEXITED and WEXITSTATUS are not implemented on Windows.
    # Assume the command was not signaled and return exit code.
    my $status = $? >> 8;

    return $status;
}


#
# Check that all hosts are up
#
sub check_all_up(@) {
    my @given_nodes = @_;

    &step("", "Checking all hosts are up");

    my $err = 0;
    foreach my $node (@given_nodes) {
        my $p = Net::Ping->new();
        if ($p->ping($node)) {
            $err = 0;
        } else {
            $err = 1;
        }
        $p->close();

        if ($err) {
            &print_error("${node} is unreachable.");
            last;
        }
    }

    &result($err);
}


#
# Uninstall Exanodes, copy Exanodes' installer, and install Exanodes
# on the target host
#
sub install_on_host($) {
    my ($targethost) = @_;
    my $ret;
    my @lines;

    # Execute a dummy command to ensure that `psexec` works on this node.
    &step($targethost, "Checking psexec has proper credentials");
    ($ret, @lines) = psexec($targethost, "cmd /C ver");
    &result($ret);

    # Uninstall Exanodes
    &step($targethost, "Uninstalling Exanodes");
    my $msi_uninstall_log = "c:\\pushexawin-uninstall.log";
    my $msi_uninstall_cmd = "msiexec /quiet /x {${PRODUCT_ID}}"
                          . " /l*v ${msi_uninstall_log}";

    &debug("msi_uninstall_cmd: ${msi_uninstall_cmd}");

    ($ret, @lines) = psexec($targethost, $msi_uninstall_cmd);
    # FIXME Take care of failure cases
    &result(0);

    &step($targethost, "Copying installer ${INSTALLER}");
    my $remote_dir = $REMOTE_DIR;
    $remote_dir =~ s/C:/C\$/;
    my $dest = "\\\\${targethost}\\${remote_dir}";

    $ret = xcopy($INSTALLER, $dest);
    &result($ret);

    &step($targethost, "Running installer");
    my $components = "";
    if (scalar(@SELECTED_FEATURES)) {
        $components = join(',', @SELECTED_FEATURES);
    }
    my $msi_install_log = "c:\\pushexawin-install.log";
    my $installer_name = basename($INSTALLER);
    my $msi_install_cmd = "msiexec /quiet /i ${REMOTE_DIR}\\${installer_name}"
                        . " ADDDEFAULT=\"${components}\" /l*v ${msi_install_log}";

    &debug("msi_install_cmd: ${msi_install_cmd}");

    ($ret, @lines) = psexec($targethost, $msi_install_cmd);
    if ($ret != 0) {
        &result($ret, "Installer log available in ${msi_install_log}");
    } else {
        &result($ret);
    }
}


#
# Invoke install_on_host() in sequence for the given hosts.
#
sub install_on_multiple_hosts(@) {
    my @hosts = @_;

    foreach my $host (@hosts) {
        &install_on_host($host);
    }
}


#
# Main
#

# Child processes
my @children = ();

# Global result we'll return at exit
my $global_error = 0;

my $ret;

&init();

if (!defined($INSTALLER)) {
    &print_error("No installer provided with --source.");
    exit 1;
}

unless (-e $INSTALLER) {
    &print_error("missing file '${INSTALLER}'.");
    exit 1;
}

if (scalar(@NODENAMES) == 0) {
    &print_error("No nodes specified.");
    exit 1;
}

unless ($PLATFORM eq "MSWin32") {
    &print_error("${PROGNAME} can only be run on Windows.");
    exit 1;
}

#check_all_up(@NODENAMES);

&step("", "Getting product id from installer source");
if (!open(WIX_FILE, $WIX_SOURCE)) {
    &result(1, "Failed opening ${WIX_SOURCE}");
}
my @wix_lines = <WIX_FILE>;
my $wix_source = join("", @wix_lines);
close(WIX_FILE);
($PRODUCT_ID) = $wix_source =~ /<Product Name='Exanodes' Id='([0-9A-F-]+)'/;
if (!defined($PRODUCT_ID)) {
    &result(1, "Failed parsing ${WIX_SOURCE}");
}
&result(0, "Product id: ${PRODUCT_ID}");

# Execute a dummy command to ensure that `psexec` is installed.
&step("", "Checking psexec is installed");
`psexec >nul 2>&1`;
# psexec returns 255 when it has nothing to do
if (($? >> 8) == 255) {
    $ret = 0;
} else {
    $ret = 1;
}
&result($ret);

# FIXME WIN32: Split the list of nodes into several pools of nodes.
# Each pool will be handled by a worker. This is done in order not to
# fork too many workers. The Windows Perl fork emulation tends to hang
# when there are many forked workers invoking exec().
my $i = 0;
my @node_pools = part { $i++ % $NBMAX_WORKERS } @NODENAMES;

#
# Fork a worker for each pool of nodes
#
foreach my $pool (@node_pools) {

    # FIXME WIN32: Forget what you've been told about fork().
    # The Windows Perl fork emulation can return a *negative* PID.
    my $child_pid = fork();

    # When fork() fails, the PID is not defined.
    unless (defined($child_pid)) {
        &print_error("Failed forking child ($!)");
        $global_error = 1;
        last;
    }

    if ($child_pid == 0) {
	&install_on_multiple_hosts(@{$pool});
	exit 0;
    } else {
	push(@children, $child_pid);
    }
}

#
# Wait until all children completed
#
while (scalar(@children)) {
    my $died = waitpid(-1, 0);

    $global_error = 1 if $? >> 8;

    # Update children info
    for my $i (0 .. scalar(@children)-1) {
        splice(@children, $i, 1)
            if defined($children[$i]) && $children[$i] == $died;
    }
}

exit $global_error;
