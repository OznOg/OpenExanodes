#!/usr/bin/perl

#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

#
# Send, compile and install the given Exanodes.tgz dist file to the given host
#

# TODO:
#  - --debug is mysteriously eaten up by Getopt() oO
#  - rename rootdir to build_dir and rpmdir to root_dir !
#  - --nodeps in uninstall_all_rpms is bad(c)(tm)
#  - make target_host global after forking
#  - use SIG{DIE} etc for safer node cleanup (needs global target_host)

package pushexa;

use strict;
use warnings;

use Getopt::Long;
use File::Basename;
use Sys::Hostname;
use Cwd;
use Socket;
use POSIX ":sys_wait_h";
use Env;
use Errno;

# Path and name of this script
my $self_path = dirname($0);
my $self = basename($0);

# The root name of the rpm being generated
my $product_name = "exanodes";

# The specfile in the dist tarball
my $specfile = $product_name . ".spec";

#
# Colors.
#
my $COLOR_SUCCESS = "\e[1;32m";
my $COLOR_FAILURE = "\e[1;31m";
my $COLOR_WARNING = "\e[1;34m";
my $COLOR_NORMAL  = "\e[0;39m";

sub init_no_colors
{
    $COLOR_SUCCESS = "";
    $COLOR_FAILURE = "";
    $COLOR_WARNING = "";
    $COLOR_NORMAL  = "";
}

sub success {return $COLOR_SUCCESS . "@_" . $COLOR_NORMAL;}
sub failure {return $COLOR_FAILURE . "@_" . $COLOR_NORMAL;}
sub warning {return $COLOR_WARNING . "@_" . $COLOR_NORMAL;}

# Platform this script is running on
my $PLATFORM = $^O;
chomp($PLATFORM);
if ($PLATFORM eq "MSWin32") {
    &print_error("${self} can't be run on Windows; use pushexawin.pl instead.");
    exit 1;
}

# Script debugging
my $SCRIPTDEBUG   = 0;
my $RSYNC_DEBUG   = 0;
my $RSYNC_EXCLUDE = "";

# Compilation options
my $TEST       = 0;
my $TOOLS      = 1;
my $DKMS       = 0;
my $MONITORING = 0;

# RPM options
my @rpm_options  = ();
my @spec_options = ();
my %spec_help;

my $RPMBUILD_ENV     = $ENV{'EXABUILD_ENV'} || "";
my $RPMBUILD_DEBUG   = 0;
my $RPMBUILD_SELINUX = 0;
my $NO_DEPS          = "";

# Generated debug RPM
my $rpm_debug;

# Installation prefix
my $INSTALL_PREFIX = "/usr";

# Connection to the target nodes
my $PROTO = "ssh";
my $USER  = "pusher";
my $SCP   = "scp -q";

# The directories we will work in on the remote nodes
my $userdir = "";
my $rootdir = "";
my $rpmdir  = "";

# Distfile (source tarball)
my $distfile   = "";
my $distfile_n = "";
my $rpmfile    = "";
my $rpmfile_n  = "";
my $distname   = "";

# Trace file's default prefix
my $tracefile = "pushexa";

# Installation options
use constant INSTALL_CLI     => 1 << 0;
use constant INSTALL_SERVER  => 1 << 2;
use constant INSTALL_SELINUX => 1 << 3;
use constant INSTALL_TM      => 1 << 4;

# Actions to perform
my $COPY       = 1;
my $DECOMPRESS = 0;
my $BUILD      = 1;
my $INSTALL    = 0;
my $UNINSTALL  = 0;

# Push options
my $KEEPCODE      = 0;
my $IGNORE_OWNER  = 0;
my $PRETEND       = 0;
my $SILENT        = 1;
my $SHOW_TIME     = 1;
my $CHECK_UP      = 1;
my $RSYNC         = 0;

my $CLEANUP_FIRST = 0;

# Exanodes' config file
my $EXA_CONFIG_FILE = "/var/cache/exanodes/exanodes.conf";

# Get user login
my $login = (getpwuid($<))[0] || "Intruder!!";

# Nodes given on the commandline
my @given_nodes;

# Print a debug message if --debug-push
sub debug
{
    print "DEBUG: @_" if $SCRIPTDEBUG;
}

# Get current date in english
sub date
{
    my $date = `LC_ALL=C date`;
    chop($date);
    return $date;
}

# Message printed at beginning of current step
my $step_host;
my @step_msg;

#
# Build a message prefixed with the name of a target host (if any)
#
sub msg($@)
{
    my ($target_host, @msg) = @_;
    return "@{msg}" if $target_host eq "";
    return "${target_host}: @{msg}";
}

#
# Report current step
#
sub step($@)
{
    my ($target_host, @msg) = @_;

    print &msg($target_host, "@{msg}...") . "\n";

    $step_host = $target_host;
    @step_msg  = @msg;
}

#
# Report success or failure of current step
#
sub result($$)
{
    my ($err, $details) = @_;

    my $color = ($err ? \&failure : \&success);

    print &$color(&msg($step_host, "@{step_msg}: " . ($err == 0 ? "OK" : "FAILED")))
      . "\n";
    print "\t${details}\n" if defined($details);

    exit 1 if $err;
}

# Cleanup in progress ?
my $CLEANING_UP = 0;

#
# !!! IMPORTANT: This function removes the code on the target nodes.
# It must be called before any 'die' after the code has been copied !
#
sub cleanup($)
{
    my $targethost = shift;

    return if $CLEANING_UP;

    $CLEANING_UP = 1;

    if (!$KEEPCODE)
    {
        my $err = 0;
        &step($targethost, "Erasing ${product_name} source");
        if (!$RSYNC)
        {
            $err = 1
              if &my_system($PROTO, $targethost,
                      "cd ${rootdir} && " . "rm -f ${distfile_n} && "
                                          . "rm -rf ${distname} && "
                                          . "rm -rf ${distname}-build");
            $err = 1
              if &my_system($PROTO, $targethost,
                            "cd ${rpmdir}/BUILD && " . "rm -rf ${distname}");
        } else {
            $err = 1
              if &my_system($PROTO, $targethost, "rm -rf ${userdir}/rsync");
        }

        &result($err);
    }

    $CLEANING_UP = 0;
}

#
# Print an error message
#
sub print_error
{
    print STDERR &failure("ERROR: @_") . "\n";
}

#
# Print an error message and die
#
sub error_die
{
    &print_error(@_);
    exit 1;
}

#
# Cleanup and die
#
sub die_cleanup($@)
{
    my ($targethost, @message) = @_;

    &cleanup($targethost);
    &error_die(@message);
}

#
# TODO: Move this function out in a module (common to all push procedures, etc)
#
# Given a node name parameter that includes regexp style node numbering.
# returns a list of nodes.
# Nodes are returned in the parsed order, not sorted.
#
# The numbering format is:
# NODES       ::= {<NODEREG>}
# NODEREG     ::= NODEPREFIX{<NUMBERING>}{<NODEREG>}
# <NUMBERING> ::= <digit> { [-<digit>] | [:] }
#
# Example:
# sam/1-4/          => sam1 sam2 sam3 sam4
# sam/1-4/ sam10    => sam1 sam2 sam3 sam4 sam10
# sam/3:8-9/ sam1   => sam3 sam8 sam9 sam1
# sam-/3-5:7:10-12/ => sam-3 sam-4 sam-5 sam-7 sam-10 sam-11 sam-12
#
# Example with multiple ranges:
# sam/1-2/-/3-4/    => sam1-3 sam1-4 sam2-3 sam2-4
#
# If a * is passed in all_nodes, nothing is done, the * is returned.
#
# \returns a list of nodes.
#
sub name_expand($)
{
    my $all_nodes = shift;

    if ($all_nodes eq "*")
    {
        return "*";
    }

    my @result;

    my @nodes_list = split(/ /, $all_nodes);

    foreach my $nodes (@nodes_list)
    {

        if ($nodes =~ /\/\d+\//)
        {

            # Single // quoted number, just remove the //
            $nodes =~ s/\/(\d+)\//$1/g;
            push(@result, "$nodes");
        }
        elsif ($nodes !~ /\/\d+:|-\d+[:\-\d]*\//)
        {

            # Is there really a regexp there. If not, push the current node in the result stack
            push(@result, "$nodes");
        }
        else
        {

            # Extract the prefix
            my ($node_name, $node_expr, $remaining) =
              $nodes =~ /^(.*?)(\/\d+[0-9:\-]+\/)+?(.*)/;

            # Remove the //
            $node_expr =~ s/\///g;

            my @expr_numbers = split(/[:\-]/, $node_expr);

            my @expr_expr = split(/[0-9]+/, $node_expr);
            my $lost = shift(@expr_expr);

            my $previous = -1;
            foreach my $number (@expr_numbers)
            {

                my $expr = shift(@expr_expr);
                if (!defined($expr))
                {
                    $expr = ":";
                }

                if ($expr eq ":")
                {
                    if ($previous != -1)
                    {
                        for (my $i = $previous ; $i <= $number ; $i++)
                        {

                            # Recursively create an expansion if remaining regexp
                            if ($remaining =~ /\//)
                            {
                                my @subnames = &name_expand("$remaining");
                                foreach my $subname (@subnames)
                                {
                                    push(@result, "$node_name$i$subname");
                                }
                            }
                            else
                            {
                                push(@result, "$node_name$i$remaining");
                            }
                        }
                        $previous = -1;
                    }
                    else
                    {

                        # Recursively create an expansion if remaining regexp
                        if ($remaining =~ /\//)
                        {
                            my @subnames = &name_expand("$remaining");
                            foreach my $subname (@subnames)
                            {
                                push(@result, "$node_name$number$subname");
                            }
                        }
                        else
                        {
                            push(@result, "$node_name$number$remaining");
                        }
                    }
                }
                elsif ($expr eq "-")
                {
                    if ($previous != -1)
                    {
                        &print_error(
                            "In number expansion.  Two characters dash ('-') found after number '$previous'. Use the ':' separator instead"
                        );
                        exit 1;
                    }
                    $previous = $number;
                }
                else
                {
                    &print_error("In number expansion. Don't know how to handle '$expr'");
                    exit 1;
                }
            }
        }
    }

    &debug("===> Name expand: got $all_nodes, returning @result\n");
    return @result;
}

#
# Display usage help
# TODO: make options consistant (both <opt> and no-<opt>, w/ default)
#
sub usage
{
    print "Usage : pushexa.pl [OPTION] -s 'SOURCE PACKAGE' NODENAME...

Push the version of Exanodes found in the given source package file
to the given host list target.

The source package file is created with 'make dist'.
NODENAME... is a list of regexes for the nodes on which to install exanodes
Pushexa copies the source package on each node, recompiles and installs it.

A trace file pushexa.<node> is created for each node.\n\n";

    print "RPM/configure options:\n";

    if (@spec_options)
    {
        my $maxlen = 0;
        for my $opt (@spec_options)
        {
            my $len = length($opt) + length("--with-");
            $maxlen = $len if $len > $maxlen;
        }

        for my $opt (@spec_options)
        {
            my $help = $spec_help{$opt} || "???";
            $opt =~ s/^/--with-/;
            printf("  %-*s  %s\n", $maxlen, $opt, $help);
        }

        # XXX This should be extracted from the spec file too
        printf("  %-*s  %s\n",
             $maxlen, "--prefix=PREFIX",
             "Set installation prefix: one of /usr (default), /usr/local, /opt/exanodes");

        print "\n";
    }
    else
    {
        print "  ** Need source package to show these options **\n\n";
    }

    print "Push options:
  -s, --source=FILE	Specify the source package file
  --keep-code           Do not erase the code on the target node once installed
                        WARNING: DO NOT USE OFFSITE
  -p, --protocol=proto	Run the remote command using the protocol (proto=rsh|ssh|local)
                        ssh is the default; local will use regular local commands
  --no-color		Set the output color off (default=on)
  --rsync               Fast installation and build using rsync (default=NO)
  --cleanup-first       Cleanup source on nodes prior to pushing (default=NO)
  --no-copy             Don't copy the source package
                        (assume it's already on the target nodes)
  --no-build            Don't build RPMs
  --no-install          Don't install RPMs built
  --no-deps             Don't check for RPM dependencies
  --decompress          Decompress archive (use with --no-build and --keep-code)
  --uninstall           Uninstall RPMs
  --user=USER           Push as user USER (default=${USER})
  --tracefile=PREFIX    Set the trace file prefix (default=${tracefile})
  --no-time             Don't show time at end of push
  --no-check-up         Don't check whether all nodes are up
  --no-check-stopped    Don't check whether Exanodes is stopped
  --ignore-owner        Don't check for node ownership
  --pretend             Pretend, don't actually do anything on the nodes
  -h, --help		Display this help and exit
  --debug-push          Debug the push procedure
  --debug-rsync         Debug rsync

Note: You can pass an optional build environment by using the EXABUILD_ENV
      environment variable:

          EXABUILD_ENV=<settings> pushexa.pl ...

Examples:
 - Push to nodes as user toto:
       pushexa.pl --with-nodes -s exanodes-gen-3.0.tar.gz --user=toto node/1-4/ node6
 - Push to CLI node:
       pushexa.pl --with-cli -s exanodes-gen-3.0.tar.gz localhost
 - Push the token manager:
       pushexa.pl --with-tm -s exanodes-gen-3.0.tar.gz node5
 - Push with specified QT environment:
       export EXABUILD_ENV='QTDIR=/usr/lib64/qt-3.3/'
       pushexa.pl -s exanodes-gen-3.0.tar.gz node/1:3:5/\n";

    exit 1;
}

#
# Check for duplicate nodes
#
sub check_duplicates
{
    my @given_nodes = @_;

    my $last_node = undef;
    foreach my $node (@given_nodes)
    {
        &error_die("Some nodes appear more than once")
          if defined($last_node) && $node eq $last_node;
        $last_node = $node;
    }
}

#
# Get list of nodes owned
#
sub owned_nodes
{
    return split(" ", `nodereserve --list mine-short 2>/dev/null | xargs echo -n`);
}

#
# Check for node ownership
#
sub check_ownership
{
    my @given_nodes = @_;

    return if $IGNORE_OWNER;

    &error_die(  "Can't ensure node ownership: couldn't find nodereserve"
               . "\nUse --ignore-owner if you know what you're doing")
      if (!prog_exists("nodereserve"));

    my $errs        = 0;
    my @owned_nodes = &owned_nodes;
    foreach my $node (@given_nodes)
    {
        if ($node ne "localhost" && !grep {/$node/} @owned_nodes)
        {
            &print_error("You don't own node ${node}");
            $errs++;
        }
    }

    exit 1 if $errs;
}

#
# Check that all hosts are up
#
sub check_all_up
{
    my @given_nodes = @_;

    &error_die("Can't check node status: couldn't find fping")
      if (!prog_exists("fping"));

    &step("", "Checking all hosts are up");

    my $err = 0;
    if (!$PRETEND)
    {
        system("fping @{given_nodes} >/dev/null 2>&1");
        $err = $? >> 8;
    }

    &result($err);
}

#
# Check existence of pusher on all hosts
#
sub check_pusher
{
    my ($pusher, @given_nodes) = @_;

    &step("", "Checking pusher exists on all hosts");

    my $errs = 0;
    foreach my $node (@given_nodes)
    {
        next if $node eq "localhost";
        if (&as_root(\&my_system, $PROTO, $node, "id ${pusher}"))
        {
            &print_error("No user '${pusher}' on host ${node} (or maybe no SSH)");
            $errs++;
        }
    }

    &result($errs);
}

#
# Check disk space on all hosts
#
sub check_disk_space
{
    my @given_nodes = @_;

    &step("", "Checking disk space on all hosts");

    my $errs = 0;
    foreach my $node (@given_nodes)
    {
        my @disk_info = split(" ", `${PROTO} -l ${USER} ${node} "df -h | grep '/\$'"`);
        my $avail_space = $disk_info[3];
        if ($avail_space eq "0")
        {
            &print_error("No space left on host ${node}");
            $errs++;
        }
    }

    &result($errs);
}

#
# Get the name of the tarball defined in the specfile.
#
sub get_spec_tarball_name
{
    return unless open(SPEC, "<${specfile}");

    my $tarball_name;
    while (my $line = <SPEC>)
    {
        chomp($line);
        ($tarball_name) = $line =~ /^%define\s+tarball_name\s+(.*)$/;
        last if defined($tarball_name);
    }

    close(SPEC);

    return $tarball_name;
}

#
# Get the options recognized by the RPM spec
#
sub get_spec_options
{
    if ($RSYNC)
    {
        return if !open(SPEC, "${specfile}.in");
    }
    else
    {
        return
          if !$distname
              || !open(SPEC, "tar zxf ${distfile} --to-stdout ${distname}/${specfile} |");
    }

    my $help;
    while (my $line = <SPEC>)
    {
        chomp($line);

        my ($h) = $line =~ /# \@Option:\s*(.*)/;
        if ($h)
        {
            $help = $h;
            next;
        }

        my ($prefix, $opt) = $line =~ /^%bcond_(with|without) (.*)$/;
        if ($prefix && $opt)
        {
            my $option = "${opt}";
            push(@spec_options, $option) if !grep(/$option/, @spec_options);

            # Invert default values that differ between pushexa and rpm.
            if (grep(/$option/, "nodes cli test tools selinux"))
            {
                $help =~ s/default=yes/default=no/;
            }
            $spec_help{$option} = $help;
            $help = undef;
        }
    }

    close(SPEC);
}

#
# Handling of RPM/configure options
#

# Check whether a --with RPM option is present
sub with($)
{
    my $opt = shift;
    return grep(/^--with ${opt}$/, @rpm_options) ? 1 : 0;
}

# Check whether a --without RPM option is present
sub without($)
{
    my $opt = shift;
    return grep(/^--without ${opt}$/, @rpm_options) ? 1 : 0;
}

# Report conflict on an RPM option
sub option_conflict_error($)
{
    my $opt = shift;
    &error_die("Conflict between --with ${opt} and --without ${opt}");
}

# Add an option with given prefix to RPM options
sub add_option($$)
{
    my ($prefix, $opt) = @_;
    push(@rpm_options, "--${prefix} ${opt}");
}

# Add a --with option to RPM options
sub add_with($)
{
    my $opt = shift;
    if (!&with($opt))
    {
        &option_conflict_error($opt) if &without($opt);
        &add_option("with", $opt);
    }
}

# Add a --without option to RPM options
sub add_without($)
{
    my $opt = shift;
    if (!&without($opt))
    {
        &option_conflict_error($opt) if &with($opt);
        &add_option("without", $opt);
    }
}

# Warn about deprecated option
sub warn_deprecated($$)
{
    my ($old_opt, $new_opt) = @_;
    print &warning("Option '${old_opt}' is deprecated, use '${new_opt}' instead"), "\n";
}

#
# Filter RPM options out of a list and add them to the RPM options list
#
sub filter_rpm_options
{
    my @args = @_;

    my @not_passed;

    &debug("spec options: @{spec_options}\n");

    while (my $arg = shift(@args))
    {
        my ($prefix, $opt) = $arg =~ /^--(with|without)-(.*)/;
        if ($opt && $opt eq "server")
        {
            warn_deprecated("server", "nodes");
            $opt = "nodes";
        }
        if ($prefix && $opt)
        {
            &error_die("Unknown RPM option: '${arg}'\n")
                if !grep(/${opt}/, @spec_options);
            &add_option($prefix, $opt);
        }
        else
        {
            push(@not_passed, $arg);
        }
    }

    return @not_passed;
}

#
# Initialization of push procedure
#
sub init
{
    my %options;

    #
    # Get commandline options
    # TODO: Needs readability improvement
    #
    Getopt::Long::Configure("pass_through");
    Getopt::Long::Configure("no_auto_abbrev");
    if (!GetOptions(\%options,          "protocol|p=s",
                    "source|s=s",       "keep-code",
                    "no-copy",          "no-build",
                    "no-install",       "decompress",
                    "no-deps",          "uninstall",
                    "user=s",           "tracefile=s",
                    "no-time",          "no-check-up",
                    "no-check-stopped", "ignore-owner",
                    "pretend",          "no-color",
                    "rsync",            "help|h",
                    "debug-push",       "debug-rsync",
                    "cleanup-first",
                    "prefix=s"))
    {
        print("Try `$0 --help' for more information.\n");
        exit 1;
    }

    &init_no_colors if $options{"no-color"};

    $RSYNC = 1 if $options{"rsync"};
    $CLEANUP_FIRST = 1 if $options{"cleanup-first"};

    $distfile = $options{"source"};

    if ($RSYNC)
    {
        &error_die("Source package useless with --rsync")
            if defined($distfile);

        if (!-f "${specfile}.in" && -f "Makefile")
        {
            my $source_dir=`grep "^CMAKE_SOURCE_DIR = " Makefile`;
            $source_dir =~ s/^CMAKE_SOURCE_DIR = //;
            chomp($source_dir);
            &debug("Found source directory ${source_dir}");
            chdir(${source_dir})
                or &error_die("Current directory seems not to be an ".
                    "Exanodes source directory (no ${specfile}.in), ".
                    "and can't figure out CMAKE_SOURCE_DIR from Makefile.");
        } else {
            &error_die("Current directory seems not to be Exanodes source or ".
                       "build directory (no ${specfile}.in or Makefile).")
              if !-f "${specfile}.in";
        }

        # XXX distname here is <user>--<dir>, where <user> is the name of
        # the user pushing and <dir> is the name of the current directory.
        $distname = $login . "--" . basename(cwd());
    }
    else
    {
        &error_die("No source package specified")
            if !defined($distfile) || $distfile eq "";

       &error_die("Cannot open file source package '${distfile}'")
            if !-f $distfile;

        #
        # Source package
        #
        $distfile_n = $distfile;
        $distfile_n =~ s/^.*\///g;

        $distname = $distfile_n;
        $distname =~ s/.tar.gz//g;

        # Ensure versions of this script and the pushed distfile are identical
        my $spec_tarball_name = &get_spec_tarball_name();

        &error_die("Version mismatch between pushexa (${spec_tarball_name}) and ${distfile}")
            if $distfile !~ /${spec_tarball_name}/;

        &error_die("Tarball name should begin with '$product_name'")
            if $distfile !~ /(^|\/)${product_name}/;
    }

    # Extract RPM options from spec file
    &get_spec_options;

    &usage if $options{help};

    # Filter out RPM options
    @ARGV = &filter_rpm_options(@ARGV);

    add_without("nodes")      unless with("nodes");
    add_without("monitoring") unless with("monitoring");
    add_without("cli")        unless with("cli");
    add_without("selinux")    unless with("selinux");
    add_without("test")       unless with("test");
    add_without("selinux")    unless with("selinux");
    add_without("tm")         unless with("tm");

    # Error if there are remaining options
    for my $arg (@ARGV)
    {
        &error_die("Unknown option: '${arg}'") if $arg =~ /^--?/;
    }

    $tracefile = $options{"tracefile"} if $options{"tracefile"};

    # Installation prefix
    $INSTALL_PREFIX = $options{"prefix"} if $options{"prefix"};

    #
    # Actions to perform
    #
    $PRETEND     = 1 if $options{"pretend"};
    $SCRIPTDEBUG = 1 if $options{"debug-push"};
    $RSYNC_DEBUG = 1 if $options{"debug-rsync"};

    $INSTALL |= INSTALL_CLI     if &with("cli");
    $INSTALL |= INSTALL_SELINUX if &with("selinux");
    $INSTALL |= INSTALL_SERVER  if &with("nodes");
    $INSTALL |= INSTALL_TM      if &with("tm");

    $CHECK_UP      = 0 if $options{"no-check-up"};

    # FIXME: these options are meaningless with --rsync
    $COPY       = 0 if $options{"no-copy"};
    $DECOMPRESS = 1 if $options{"decompress"};
    $BUILD      = 0 if $options{"no-build"};
    $INSTALL    = 0 if $options{"no-install"};

    $UNINSTALL = 1 if $options{"uninstall"};
    if ($UNINSTALL)
    {
        $COPY = $DECOMPRESS = $BUILD = $INSTALL = 0;
    }

    $NO_DEPS = "--nodeps" if $options{"no-deps"};

    $SHOW_TIME = 0 if $options{"no-time"};

    &error_die("Nothing to do :p\n")
      if !$COPY && !$BUILD && !$INSTALL && !$UNINSTALL;

    if (!$UNINSTALL)
    {

        # Error if nothing to compile
        &error_die("You have to select at least one component (nodes and/or cli or token manager)")
            if !with("nodes") && !with("cli") && !with("tm");
    }

    # Careful with this one
    $KEEPCODE = 1 if $options{"keep-code"};

    #
    # User to push as
    #
    $USER = $options{"user"} if $options{"user"};
    if ($USER eq "root")
    {
        $userdir = "/root";
        if ($RSYNC)
        {
            $rpmdir  = "";
            $rootdir = "${userdir}/rsync";
        }
        else
        {
            $rpmdir  = "\\`rpm --eval %_topdir\\`";
            $rootdir = "${rpmdir}/BUILD";
        }
    }
    else
    {
        $userdir = "~${USER}";
        if ($RSYNC)
        {
            $rootdir = "${userdir}/rsync";
            $rpmdir  = "";
        }
        else
        {
            $rootdir = "${userdir}/tmp";
            $rpmdir  = "${userdir}/rpm";
        }
    }

    #
    # Network options
    #
    $PROTO = $options{"protocol"} if $options{"protocol"};
    &error_die("Unsupported protocol '${PROTO}'")
        if $PROTO ne "ssh" && $PROTO ne "rsh" && $PROTO ne "local";

    # Special cases
    $TEST       = &with("test");
    $TOOLS      = !&without("tools");
    $DKMS       = !&without("dkms");
    $MONITORING = &with("monitoring");
    if (!$RSYNC)
    {
        $RPMBUILD_DEBUG   = !&without("build_rpm_debug") && !&with("symbols");
        $RPMBUILD_SELINUX = &with("selinux");

        if (&with("debug") || &with("symbols"))
        {

            # Exporting STRIP prevents configure & autotools from stripping
            # during make install. Exporting DONT_STRIP prevents newest rpm
            # from stripping during post.
            # But this is not enough :(
            # In the spec file we must overide __os_install_post for some old
            # RedHat distributions. This is done using the --with noRHstripping
            # directive while building the spec.
            $RPMBUILD_ENV = join(" ", ($RPMBUILD_ENV, "DONT_STRIP=1", "STRIP=/bin/true"));
        }

        &debug(">>> passed to rpmbuild: @{rpm_options}\n");
    } else {
        $DKMS = 0;
        print &warning("Disabling DKMS which is incompatible with rsync push\n");
    }

    # Whether to ignore node ownership
    $IGNORE_OWNER = 1 if $options{"ignore-owner"};

    #
    # Get nodes pushing to. If none specified, use all owned nodes.
    #
    @given_nodes = sort(&name_expand(join(" ", @ARGV)));
    if (scalar(@given_nodes) < 1)
    {
        @given_nodes  = &owned_nodes;
        $IGNORE_OWNER = 1;
        print &warning("No nodes specified, using all owned nodes: @{given_nodes}"), "\n";
        if (scalar(@given_nodes) < 1)
        {
            &error_die("You don't own any node !");
        }
    }

    # Play safe
    &check_duplicates(@given_nodes);
    &check_ownership(@given_nodes);
    &check_all_up(@given_nodes) if $CHECK_UP;
    &check_pusher($USER, @given_nodes);
    &check_disk_space(@given_nodes) if !$UNINSTALL;
}

#
# Execute a function as root
#
sub as_root($@)
{
    my ($fun, @args) = @_;

    my $pusher = $USER;
    $USER = "root";

    my $r = &$fun(@args);

    $USER = $pusher;

    return $r;
}

#
# Return the name of the tracefile for the specified host
#
sub host_tracefile($)
{
    my $host = shift;
    return "${tracefile}.${host}";
}

#
# Get the root directory of a target host
#
sub remote_dir($$)
{
    my ($target_host, $dir) = @_;

    my $rem_dir;
    if ($PROTO ne "local")
    {
        $rem_dir = `${PROTO} -l ${USER} ${target_host} "echo ${dir}"`;
    }
    else
    {
        $rem_dir = `echo ${dir}`;
    }
    chomp($rem_dir);

    return $rem_dir;
}

#
# Copy to a remote host
#
sub remote_copy($$$)
{
    my ($src_file, $target_host, $target_dir) = @_;

    return 0 if $PRETEND;

    if ($PROTO ne "local")
    {
        $target_dir = &remote_dir($target_host, $target_dir);
        return
          system(  "${SCP} -r ${src_file} ${USER}\@${target_host}:${target_dir}/"
                 . " 2>/dev/null");
    }
    else
    {
        return system("${SCP} -r ${src_file} ${target_dir}/ 2>/dev/null");
    }
}

#
# Remote 'system' call
#
sub my_system($$$)
{
    my ($proto, $target_host, $command) = @_;

    my $redirect = "";
    if ($tracefile eq "")
    {
        $redirect = "2>/dev/null" if $SILENT;
    }
    else
    {
        $redirect = ">> " . &host_tracefile($target_host) . " 2>&1";
    }

    my $sys_cmd;

    $proto = "local" if $target_host eq "localhost";
    if ($proto ne "local")
    {
        $sys_cmd = "${proto} -l ${USER} ${target_host} \"${command}\" ${redirect}";
    }
    else
    {
        $command = "sudo ${command}" if $USER eq "root";
        $sys_cmd = "{ ${command}; } ${redirect}";
    }

    &debug("my_system: '${target_host}' => '${sys_cmd}'\n");

    return 0 if $PRETEND;

    my $r = system($sys_cmd);
    &die_cleanup($target_host, "Failed to run ${command} on ${target_host}")
      if $r == -1;

    return $r >> 8;
}

#
# Check whether a given program is found in the path
#
sub prog_exists($)
{
    my $prog = shift;
    return system("which ${prog} >/dev/null 2>&1") >> 8 == 0;
}

my $date_begin = &date;

&init;

#
# Warn about symbols being left on the nodes
#
sub warn_symbols
{
    my $color = shift;

    if (&with("symbols"))
    {
        print $color || $COLOR_WARNING;
        print "** ----------------------------------------------------\n";
        print "** WARNING: CODE COMPILED WITH SYMBOLS\n";
        print "** ----------------------------------------------------\n";
        print $COLOR_NORMAL;
    }
    elsif (!&without("build_rpm_debug"))
    {
        print $color || $COLOR_WARNING;
        print "** ----------------------------------------------------\n";
        print "** WARNING: DEBUG RPM GENERATED\n";
        print "**     RPM in ${rpmdir}/RPMS/<arch>/\n";
        print "** ----------------------------------------------------\n";
        print $COLOR_NORMAL;
    }
    else
    {
        print "Symbols stripped from code\n";
    }
}

#
# Warn about the source code being left on the nodes
#
sub warn_keep_code
{
    my $color = shift;

    if ($KEEPCODE)
    {
        print $color || $COLOR_WARNING;
        print "** ----------------------------------------------------\n";
        print "** WARNING: SOURCE CODE *NOT* REMOVED FROM NODES\n";
        print "**     Code is in ${rootdir}/${distfile_n}\n";
        print "**     Expanded in ${rpmdir}/BUILD/${distname}\n";
        print "** ----------------------------------------------------\n";
        print $COLOR_NORMAL;
    }
    else
    {
        print "Code removed from nodes\n";
    }
}

#
# Print an option setting
#
sub print_opt($$@)
{
    my ($desc, $setting, $on_str, $off_str) = @_;

    $on_str  = "enabled"  if !defined($on_str);
    $off_str = "disabled" if !defined($off_str);

    my $state = $off_str;
    $state = $on_str if $setting;

    print "${desc}: ${state}\n";
}

print("----------------------------------------\n");
print("Date                    : ${date_begin}\n");
if (!$RSYNC)
{
    print("Source file             : ${distfile} ($distfile_n)\n");
}
print("Source name             : ${distname}\n");
print("Protocol                : ${PROTO}\n");
print("Rsync                   : ", $RSYNC ? "yes" : "no", "\n");

#print("Pushing as user         : ${USER} (user dir: ${userdir}, rpm build dir: ${rpmdir}, temp dir: ${rootdir})\n");
print_opt("Superblock endianness   ", &with("bigendiansb"), "big endian",
          "little endian");
print_opt("Yaourt usage            ", &with("yaourt"));
print("----------------------------------------\n");

if (!$RSYNC && $INSTALL)
{
    &warn_keep_code();
    &warn_symbols();

    &debug("rpm_options: @{rpm_options}\n");
}

# Change the scp command depending on the protocol
if ($PROTO eq "rsh")
{
    $SCP = "rcp";
}
elsif ($PROTO eq "local")
{
    $SCP = "cp";
}

#
# Create temporary root directory on target host
#
sub create_tmp_rootdir($)
{
    my $target_host = shift;

    &step($target_host, "Creating ${rootdir}");
    &result(&my_system($PROTO, $target_host, "mkdir -p ${rootdir}"));
}

#
# Create directories used for building RPMs
#
sub create_rpm_build_dirs($)
{
    my $target_host = shift;

    return if $USER eq "root";

    &step($target_host, "Creating RPM build directories");
    &result(&my_system($PROTO, $target_host,
                       "mkdir -p ${rpmdir}/BUILD && "
                         . "mkdir -p ${rpmdir}/RPMS && "
                         . "mkdir -p ${rpmdir}/SOURCES && "
                         . "mkdir -p ${rpmdir}/SPECS && "
                         . "mkdir -p ${rpmdir}/SRPMS && "
                         . "mkdir -p ${rpmdir}/tmp"));
}

#
# Create RPM macro file
#
sub create_rpm_macro_file($)
{
    my $target_host = shift;

    return if $USER eq "root";

    &step($target_host, "Creating RPM macro file");
    &result(&my_system($PROTO, $target_host,
                     "echo '%_topdir ${rpmdir}' > ${userdir}/.rpmmacros && "
                       . "echo '%_tmppath ${rpmdir}/tmp' >> ${userdir}/.rpmmacros && "
                       . "echo '%_sysconfdir            /etc' >> ${userdir}/.rpmmacros &&"
                       . "echo '%_localstatedir         /var' >> ${userdir}/.rpmmacros"));
}

#
# Remove previous distfile on target host, if any
#
sub remove_previous_distfile($)
{
    my $target_host = shift;

    &step($target_host,
          "Removing previous ${rootdir}/${distfile_n} and ${rootdir}/${distname}");
    &result(&my_system($PROTO, $target_host,
                       "rm -rf ${rootdir}/${distfile_n} ${rootdir}/${distname}"));
}

#
# Copy distfile to target host
#
sub copy_distfile($)
{
    my $target_host = shift;

    &step($target_host, "Copying ${distfile}");
    &result(&remote_copy($distfile, $target_host, $rootdir));
}

#
# Update git.h to ensure it has the actual current code revision
#
sub update_git_header
{
    &step("", "Bringing git.h up to date");
    &result(system("perl tools/generate_git_h.pl . >/dev/null 2>&1"));
}

#
# Remove git.h from the sources
#
sub remove_git_header
{
    &step("", "Removing local git.h");
    &result(system("rm git.h"));
}

#
# Calculate list of files that must be excluded from rsyncing.
#
sub calculate_rsync_exclude()
{
    &step("", "Calculating rsync excludes");

    # Exclude all files ignored by subversion *and* some other files,
    # with a notable exception: 'git.h', which is ignored by subversion,
    # *must* be present on the nodes in order to compile.
    my @excluded = (".*", "*.git*", "*~", "#*", &get_subversion_ignored);
    for my $i (0 .. scalar(@excluded) - 1)
    {
        splice(@excluded, $i, 1)
            if defined($excluded[$i]) && $excluded[$i] eq "git.h";
    }
    map {$_ = "--exclude='$_'";} @excluded;

    &result(0);

    return @excluded;
}

#
# Rsync current directory to target host
#
sub rsync($)
{
    my $target_host = shift;

    &step($target_host, "Rsync'ing");

    if ($PRETEND)
    {
        &result(0);
    }
    else
    {
        my $target_dir = "${USER}\@${target_host}:${rootdir}/${distname}";

        # --delete can safely be added since excluded files aren't deleted
        # by default (and thus, .o files and the like are kept)
        my $rsync_opts = "--archive --compress --delete";
        $rsync_opts .= " --verbose" if $RSYNC_DEBUG;

        my $rsync_cmd = "rsync ${rsync_opts} . ${RSYNC_EXCLUDE} ${target_dir}";
        if ($RSYNC_DEBUG)
        {
            $rsync_cmd .= " | grep -v 'is uptodate'";
        }
        else
        {
            $rsync_cmd .= " >/dev/null";
        }

        &result(system($rsync_cmd));
    }
}

#
# Return the definitions corresponding to the given installation prefix
#
sub rpm_defs_for_prefix($)
{
    my ($prefix) = @_;

    if ($prefix eq "/usr")
    {
        return ("--define='_prefix /usr'",
                "--define='cache_dir /var/cache'",
                "--define='log_dir /var/log'",
                "--define='pid_dir /var/run'");
    }
    elsif ($prefix eq "/usr/local")
    {
        return ("--define='_prefix /usr/local'",
                "--define='cache_dir /var/cache'",
                "--define='log_dir /var/log'",
                "--define='pid_dir /var/run'");
    }
    elsif ($prefix eq "/opt/exanodes")
    {
        return (" --define='_prefix /opt/exanodes'",
                " --define='_sysconfdir /etc/opt/exanodes'",
                " --define='cache_dir /var/opt/exanodes/cache'",
                " --define='log_dir /var/opt/exanodes/log'",
                " --define='pid_dir /var/run'");
    }
    else
    {
        return ();
    }
}

#
# Build RPMs on target host
#
sub build_rpm($)
{
    my ($target_host) = @_;

    my @rpm_defs = &rpm_defs_for_prefix($INSTALL_PREFIX);
    if (!@rpm_defs)
    {
        &result(1, "Invalid prefix: ${INSTALL_PREFIX}");
    }

    my $build_opt = ($KEEPCODE ? "-ta" : "-tb");

    # http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=400889
    my $tar_workaround = "TAR_OPTIONS=--wildcards";
    my $build_cmd = "${tar_workaround} rpmbuild ${build_opt} ${NO_DEPS} ${distfile_n}"
      . " @{rpm_options} @{rpm_defs}";

    &step($target_host, "Rebuilding RPM Exanodes: ${build_cmd}");
    &result(&my_system($PROTO, $target_host,
                       "export LANG=C && "
                         . "export LC_MESSAGES=C && "
                         . "export LC_ALL=C && "
                         . "cd ${rootdir} && "
                         . "${RPMBUILD_ENV} ${build_cmd}"));
}

#
# Uninstall old RPMs on target host
#
sub uninstall_all_rpms($)
{
    my $target_host = shift;

    &step($target_host, "Uninstalling all Exanodes RPMs");
    if (&my_system($PROTO, $target_host, "rpm -qa | grep -i exanodes") == 0)
    {
        &result(&my_system($PROTO, $target_host,
                           "rpm -qa | grep -i exanodes | xargs rpm --nodeps -e"));
    }
    else
    {
        &result(0);
    }
}

#
# Install new RPM on target host.
# An optional environment can be given as a third argument
# (string of space-separated <var>=<value>)
#
sub install_rpm($$$)
{
    my ($target_host, $rpm, $env) = @_;

    $env = "" if !defined($env);

    &step($target_host, "Installing RPM ${rpm}");
    &result(&my_system($PROTO, $target_host, "${env} rpm -Uvh ${NO_DEPS} ${rpm}"));
}

#
# Get the list of subversion's global ignores from its config
#
sub get_subversion_global_ignores
{

    # Default value of subversion's 'global-ignores' variable
    my @default_ignores =
      ("*.o", "*.lo", "*.la", "#*#", ".*.rej", "*.rej", ".*~", "*~", ".#*", ".DS_Store");

    return @default_ignores
      if !open(SVN_CONFIG, "< /home/${login}/.subversion/config");

    my @global_ignores;
    while (my $line = <SVN_CONFIG>)
    {
        chomp($line);
        my ($value) = $line =~ /^global-ignores\s*=\s*(.*)/;
        if (defined($value))
        {
            @global_ignores = split(" ", $value);
            last;
        }
    }

    close(SVN_CONFIG);

    return @global_ignores if @global_ignores;
    return @default_ignores;
}

#
# Build list of file patterns ignored by subversion
# FIXME this should be moved to git but I do not know how to test it
#
sub get_subversion_ignored
{
    my @global_ignores = &get_subversion_global_ignores;

    # Get actual svn:ignore properties recursively
    my @raw     = split("\n", `svn propget -R svn:ignore`);
    my @cooked  = @global_ignores;
    my $cur_dir = ".";

    foreach my $line (@raw)
    {
        my ($dir, $pattern) = $line =~ /(.*) - (.*)/;

        if (defined($dir))
        {
            $cur_dir = $dir;
        }
        else
        {
            $pattern = $line;
        }

        next if !$pattern;
        next if grep {$_ eq $pattern} @global_ignores;

        if ($cur_dir eq ".")
        {
            push(@cooked, $pattern);
        }
        else
        {
            push(@cooked, "${cur_dir}/${pattern}");
        }
    }

    return @cooked;
}

#
# Get string of configure options previously used on specified target host
#
sub get_prev_opt_str($)
{
    my $target_host = shift;

    my $cmd       = "cat ${rootdir}/${distname}.options";
    my $prev_opts = `${PROTO} -l ${USER} ${target_host} "${cmd}" 2>/dev/null`;
    chomp($prev_opts);

    return $prev_opts;
}

#
# Get string of cmake options from rpm options
#
sub get_cmake_opts($)
{
    my $opts = shift;
    my @options = split(/ /, $opts);
    my $cmake_opts = "";

    foreach my $opt (@options)
    {
        my $output;
        $opt =~ s/--with-/with_/g;
        $opt =~ s/--without-/!with_/g;
        $output = `grep '?${opt}:-D' ${specfile}.in`;
        if ($output eq "")
        {
            # reverse the with/without logic to get the -D define.
            if ($opt  =~ m/!with_/)
            {
                $opt =~ s/!with_/with_/g;
            } else {
                $opt =~ s/with_/!with_/g;
            }
            $output = `grep '?${opt}:-D' ${specfile}.in`;
            $output =~ s/=TRUE/=WASTRUE/g;
            $output =~ s/=FALSE/=TRUE/g;
            $output =~ s/=WASTRUE/=FALSE/g;
        }
        $output =~ s/^.*:(.*)}.*$/$1/;
        chomp($output);
        $cmake_opts .= "${output} ";
    }

    if ($cmake_opts !~ m/-DWITH_DKMS=FALSE/)
    {
        $cmake_opts .= "-DWITH_DKMS=FALSE ";
    }

    &debug ("options ${opts} translated to ${cmake_opts}\n");

    return $cmake_opts;
}

#
# Save string of configure options on the specified target host
#
sub save_opt_str($$)
{
    my $target_host = shift;
    my $opt_str     = shift;

    return &my_system($PROTO, $target_host,
                      "echo '${opt_str}' > ${rootdir}/${distname}.options");
}

sub do_cmake($@)
{
    my $target_host = shift;
    my @options     = sort(@_);
    my $cmake_opts;

    # Translate RPM-style options to configure-style options
    my @config_options = @options;
    map {$_ =~ s/ /-/;} @config_options;

    &debug("./configure options: @{config_options}\n");

    my $cur_opts = join(" ", @config_options);
    my $prev_opts = &get_prev_opt_str($target_host);

    $cmake_opts = &get_cmake_opts($cur_opts);

    &debug(" cur opts: '${cmake_opts}'\n");
    &debug("prev opts: '${prev_opts}'\n");

    if ($cmake_opts eq $prev_opts)
    {
        print &msg($target_host, "Build options unchanged, cmake skipped\n");
        return;
    }

    print &msg($target_host, "Build options changed, running cmake\n");

    &step($target_host, "Running 'cmake'");
    my $r = &result(&my_system($PROTO, $target_host, "mkdir -p ${rootdir}/${distname}-build"));
    if ($r == 0)
    {
        $r = &my_system($PROTO, $target_host,
            "cd ${rootdir}/${distname}-build && cmake -G 'Unix Makefiles' "
            ."${cmake_opts} ../${distname}");
    }
    if ($r == 0)
    {
        $r = &save_opt_str($target_host, $cmake_opts);
    }
    &result($r);
}

#
# 'Make' on target host
#
sub make($)
{
    my $target_host = shift;

    &step($target_host, "Building with 'make'");

    my $r = &my_system($PROTO, $target_host, "cd ${rootdir}/${distname}-build && make -k -j6");
    if ($r != 0)
    {
        my $trace = &host_tracefile($target_host);
        my $missing_makefile = "No targets specified and no makefile found";
        if (system("grep '${missing_makefile}' ${trace} >/dev/null 2>&1") == 0)
        {
            &print_error(  "If pushing with rsync for the first time, "
                         . "you *must* specify configure options");
        }
    }

    &result($r);
}

#
# 'Make install' on target host
# FIXME: sandboxing
#
sub make_install($)
{
    my $target_host = shift;

    &step($target_host, "Installing with 'make install'");
    &result(&my_system($PROTO, $target_host, "cd ${rootdir}/${distname}-build && make install"));

    &step($target_host, "Updating module dependencies");
    &result(&my_system($PROTO, $target_host, "depmod -ae"));

    &step($target_host, "Adding Exanodes service");
    &result(&my_system($PROTO, $target_host,
                       "chkconfig --add exanodes || update-rc.d exanodes defaults 98 33"));
}

#
# 'Make uninstall' on target host
#
sub make_uninstall($)
{
    my $target_host = shift;

    &step($target_host, "Uninstalling with 'make uninstall' in ${rootdir}/${distname}");
    &result(&my_system($PROTO, $target_host, "cd ${rootdir}/${distname}-build && make uninstall"));
}

#
# Start/restart exanodes after it's been installed
#
sub has_exanodes_init($)
{
    my $target_host = shift;

    return &my_system($PROTO, $target_host, "test -x /etc/init.d/exanodes") == 0;
}

#
# Start/restart exanodes after it's been installed
#
sub start_exanodes($)
{
    my $target_host = shift;

    &step($target_host, "Restarting Exanodes service");
    return &result(&my_system($PROTO, $target_host, "/etc/init.d/exanodes start"));
}

#
# Stop Exanodes on a target host
#
sub stop_exanodes($)
{
    my $target_host = shift;
    return &my_system($PROTO, $target_host, "/etc/init.d/exanodes stop");
}

#
# Copy, patch, build, install and restart on target host
#
sub install_on_host($)
{
    my $targethost = shift;

    #
    # Check whether targethost is localhost
    #
    my $localhost = hostname();
    my $is_local  = 0;
    if ($localhost =~ m/^${targethost}/)
    {
        print &warning(
            "WARNING: TARGET HOST (${targethost}) and LOCAL HOST (${localhost}) are the same,"
              . " using local protocol")
          . "\n";
        $is_local = 1;
    }
    elsif ($targethost eq "localhost")
    {
        print &warning("WARNING: TARGET HOST is localhost, using local protocol") . "\n";
        $is_local = 1;
    }

    if ($is_local)
    {
        $USER  = $login;
        $PROTO = "local";
        $SCP   = "cp";
    }

    # Get the actual dirs on the target host
    $userdir = &remote_dir($targethost, $userdir);
    $rpmdir  = &remote_dir($targethost, $rpmdir);
    $rootdir = &remote_dir($targethost, $rootdir);

    # Remove old tracefile, if any
    my $tracefile = &host_tracefile($targethost);
    unlink $tracefile if -f $tracefile;


    &step($targethost, "Stopping exanodes");
    if (has_exanodes_init($targethost))
    {
        &result(1) if &as_root(\&stop_exanodes, $targethost);
    }
    &result(0);

    my $error;

    if ($CLEANUP_FIRST)
    {
        my $should_keepcode_after_push = $KEEPCODE;
        $KEEPCODE = 0;
        &cleanup($targethost);
        $KEEPCODE = $should_keepcode_after_push;
    }

    # Make sure the tmp rootdir exists
    &create_tmp_rootdir($targethost);

    if ($RSYNC)
    {
        if ($INSTALL)
        {
            # Rsync to target host
            &rsync($targethost);
        }
    }
    else
    {
        # Copy+patch distfile to target host
        if ($COPY)
        {
            &remove_previous_distfile($targethost);
            &copy_distfile($targethost);
        }

        # Decompress distfile if required
        if ($DECOMPRESS)
        {
            &step("Decompressing distfile");
            &result(1)
              if &my_system($PROTO,
                            $targethost,
                            "cd ${rootdir}"
                              . " && rm -rf ${distname}"
                              . " && tar -xzf ${distfile_n}");
        }
    }

    #------------------------------------------------------------
    # NOW THE CODE IS ON THE TARGET NODE.
    # CLEANUP MUST BE CALLED BEFORE ANY exit OR die
    #------------------------------------------------------------

    my $output;

    my ($rpm, $rpm_admind, $rpm_monitoring, $rpm_kernel, $rpm_test,
        $rpm_tools_nodes);
    my ($rpm_cli, $rpm_policy, $rpm_selinux, $rpm_tools_cli, $rpm_token_manager);

    my $RPM_BUILD_FAILED = "Failed to build RPM on ${targethost}";

    if ($RSYNC)
    {
        if ($INSTALL)
        {
            &do_cmake($targethost, @rpm_options);
            &make($targethost);
        }
    }
    else
    {
        #
        # Build RPMs
        #
        if ($BUILD)
        {
            # Ensure the directory structure for building RPMs is there
            &create_rpm_build_dirs($targethost);
            &create_rpm_macro_file($targethost);

            &build_rpm($targethost);

            # Extract the "Wrote:" list to determine where RPMs have been built.
            # Adjusted to get the package needed on the nodes
            $output = `grep "Wrote:" ${tracefile}`;

            &step($targethost, "Checking generated RPMs");

            if ($INSTALL & INSTALL_SERVER)
            {
                if ($DKMS)
                {
                    ($rpm_kernel) = $output =~
                      /^Wrote: ([a-zA-Z0-9\/_]+dkms\-${product_name}\-.*\.rpm)/mg;
                    &result(1, "Missing RPM dkms-$product_name") if !defined($rpm_kernel);
                }
                else
                {
                    ($rpm_kernel) = $output =~
                      /^Wrote: ([a-zA-Z0-9\/_]+${product_name}\-kernel\-.*\.rpm)/mg;
                    &result(1, "Missing RPM $product_name-kernel")
                      if !defined($rpm_kernel);
                }

                ($rpm) =
                  $output =~ /^Wrote: ([a-zA-Z0-9\/_]+${product_name}\-nodes\-.*\.rpm)/mg;
                &result(1, "Missing RPM $product_name-nodes") if !defined($rpm);

                ($rpm_admind) = $output =~
                  /^Wrote: ([a-zA-Z0-9\/_]+${product_name}\-admind\-.*\.rpm)/mg;
                &result(1, "Missing RPM $product_name-admind") if !defined($rpm_admind);

                if ($MONITORING)
                {
                    ($rpm_monitoring) = $output =~
                      /^Wrote: ([a-zA-Z0-9\/_]+${product_name}\-monitoring\-.*\.rpm)/mg;
                    &result(1, "Missing RPM $product_name-monitoring")
                      if !defined($rpm_monitoring);
                }

                if ($TEST)
                {
                    ($rpm_test) = $output =~
                      /^Wrote: ([a-zA-Z0-9\/_]+${product_name}\-test\-nodes\-.*\.rpm)/mg;
                    &result(1, "Missing RPM $product_name-test-nodes")
                      if !defined($rpm_test);
                }

                if ($TOOLS)
                {
                    ($rpm_tools_nodes) = $output =~
                      /^Wrote: ([a-zA-Z0-9\/_]+${product_name}\-tools-nodes\-.*\.rpm)/mg;
                    &result(1, "Missing RPM $product_name-tools-nodes")
                      if !defined($rpm_tools_nodes);
                }

                if ($RPMBUILD_DEBUG)
                {
                    ($rpm_debug) = $output =~
                      /^Wrote: ([a-zA-Z0-9\/_]+${product_name}\-debug\-.*\.rpm)/mg;
                    &result(1, "Missing RPM $product_name-debug") if !defined($rpm_debug);
                }

                if ($RPMBUILD_SELINUX)
                {
                    ($rpm_selinux) = $output =~
                      /^Wrote: ([a-zA-Z0-9\/_]+${product_name}\-selinux\-.*\.rpm)/mg;
                    &result(1, "Missing RPM $product_name-selinux-policy")
                      if !defined($rpm_debug);
                }
            }

            if ($INSTALL & INSTALL_CLI)
            {
                ($rpm_cli) =
                  $output =~ /^Wrote: ([a-zA-Z0-9\/_]+${product_name}\-cli\-.*\.rpm)/mg;
                &result(1, "Missing RPM $product_name-cli") if !defined($rpm_cli);

                ($rpm_policy) = $output =~
                  /^Wrote: ([a-zA-Z0-9\/_]+${product_name}\-policy-empty\-.*\.rpm)/mg;
                &result(1, "Missing RPM $product_name-policy-empty")
                  if !defined($rpm_policy);

                if ($TOOLS)
                {
                    ($rpm_tools_cli) = $output =~
                      /^Wrote: ([a-zA-Z0-9\/_]+${product_name}\-tools-cli\-.*\.rpm)/mg;
                    &result(1, "Missing RPM $product_name-tools-cli")
                      if !defined($rpm_tools_cli);
                }
            }

            if ($INSTALL & INSTALL_TM)
            {
                ($rpm_token_manager) = $output =~
                  /^Wrote: ([a-zA-Z0-9\/_]+${product_name}\-token-manager\-.*\.rpm)/mg;
                &result(1, "Missing RPM $product_name-token-manager")
                  if !defined($rpm_token_manager);
            }
            &result(0);
        }
    }

    #
    # Uninstall *all* old RPMs, even if pushing with --rsync:
    # we don't want to mix versions
    #
    &as_root(\&uninstall_all_rpms, $targethost) if $INSTALL || $UNINSTALL;

    if ($RSYNC)
    {
        if ($INSTALL)
        {
            &as_root(\&make_install,     $targethost);
            &as_root(\&start_exanodes,   $targethost);
        }
        elsif ($UNINSTALL)
        {
            &as_root(\&make_uninstall, $targethost);
        }
    }
    else
    {

        #
        # Install the RPMs we just built
        #

        # Install new nodes RPMs
        if ($INSTALL & INSTALL_SERVER)
        {
            &as_root(\&install_rpm, $targethost, "$rpm $rpm_admind $rpm_kernel",
                     "exanodes=nostart");
            &as_root(\&install_rpm, $targethost, $rpm_monitoring)  if $MONITORING;
            &as_root(\&install_rpm, $targethost, $rpm_test)        if $TEST;
            &as_root(\&install_rpm, $targethost, $rpm_tools_nodes) if $TOOLS;
            &as_root(\&install_rpm, $targethost, $rpm_selinux)     if $RPMBUILD_SELINUX;

            &as_root(\&start_exanodes, $targethost);
        }

        # Install new CLI RPMs
        if ($INSTALL & INSTALL_CLI)
        {
            &as_root(\&install_rpm, $targethost, "$rpm_cli $rpm_policy");
            &as_root(\&install_rpm, $targethost, $rpm_tools_cli) if $TOOLS;
        }

        if ($INSTALL & INSTALL_TM)
        {
            &as_root(\&install_rpm, $targethost, $rpm_token_manager);
        }
    }

    if (!$RSYNC)
    {

        # Install is done, do some housekeeping
        &cleanup($targethost);
    }

    #
    # Update the installation trace file
    #
    my $logline = scalar(localtime) . " ";
    $logline .= $login || "Unknown";
    $logline .= " " . scalar(hostname()) . " ";
    $logline .= "${distfile_n} options=@{rpm_options}";

    &my_system($PROTO, $targethost,
               "echo '${logline}' >> ${rootdir}/exanodes_install.log");

    if ($SHOW_TIME)
    {
        print &msg($targethost, "Push finished") . "\n";
        my $date_end = &date;
        print &msg($targethost,
                   "Push started at ${date_begin}," . " finished at ${date_end}")
          . "\n";
    }

    exit 0;
}

# Child processes
my @children        = ();
my $CHILD_ABORT_SIG = 'KILL';

# Global result we'll return at exit
my $global_error = 0;

#
# A child process has terminated
#
sub child_terminated
{

    # Loop, as there may be several dead children
    while (1)
    {
        my $died = waitpid(-1, WNOHANG);
        last if $died <= 0;

        $global_error = 1 if $? >> 8;

        # Update children info
        for my $i (0 .. scalar(@children) - 1)
        {
            splice(@children, $i, 1)
              if defined($children[$i]) && $children[$i] == $died;
        }
    }
}

# Handler for child termination
$SIG{CHLD} = \&child_terminated;

if ($RSYNC)
{

    # Make sure 'git.h' is up to date
    &update_git_header;
    $RSYNC_EXCLUDE = join(" ", &calculate_rsync_exclude());
}

#
# Fork a child for each target host
#
foreach my $targethost (@given_nodes)
{
    my $child_pid = fork();

    if ($child_pid < 0)
    {
        &print_error("Failed forking child for target host ${targethost}");
        $global_error = 1;
        last;
    }

    if ($child_pid == 0)
    {
        $SIG{CHLD} = 'DEFAULT';
        &install_on_host($targethost);
        exit 0;
    }
    else
    {
        push(@children, $child_pid);
    }
}

#
# Wait until all children completed or abort required
#
sleep(1) while scalar(@children) > 0 && !$global_error;

if ($RSYNC)
{
    &remove_git_header;
}

if ($global_error)
{
    &debug("### Killing child processes: @{children} ###\n");
    while (scalar(@children) > 0)
    {
        kill($CHILD_ABORT_SIG, @children);
        sleep(1);
    }

    print $COLOR_FAILURE;
    print "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
    print "!!! ERRORS - SEE MESSAGES ABOVE !!!\n";
    print "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
    print $COLOR_NORMAL;
}
else
{
    if (!$RSYNC && $INSTALL)
    {
        &warn_keep_code($COLOR_FAILURE);
        &warn_symbols($COLOR_FAILURE);
    }
}

exit $global_error;
