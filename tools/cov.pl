#!/usr/bin/perl

use strict;
use warnings;

use File::Basename;
use Cwd;

my $self = basename($0);

my $debugging = $ENV{"COV_DEBUG"};

my $rel_source_path = shift;
if (!defined($rel_source_path)) {
    print STDERR <<EOF;
Get the coverage of a module.

Usage: ${self} <rel source path>

    The script assumes the working directory is the toplevel *build*
    directory.

    <rel source path> is the source path of the module to get
    the coverage of, relative to the toplevel *source* directory
    as if you were in that diretory.

    The script runs the unit test of the module, generates an
    HTML page showing the line-by-line coverage of the module,
    and prints out the page URL.

Example: From the toplevel build directory, get the coverage of
         module os_dir.c:

    ${self} os/src-linux/os_dir.c
EOF
    exit 1;
}

if ($rel_source_path !~ /\.(c|cpp)$/) {
    print STDERR "missing extension (.c or .cpp)\n";
    exit 1;
}

my $build_dir = cwd();
my $source_dir = `grep exanodes_SOURCE_DIR CMakeCache.txt | sed s/^.*=//`;
chomp($source_dir);

my $source_path = "${source_dir}/${rel_source_path}";
my $source_name = basename($rel_source_path);

my $module_name = $source_name;
$module_name =~ s/\.(c|cpp)$//;

my $ut_name = $module_name;
$ut_name = "ut_" . basename($ut_name);

my $rel_ut_path = dirname(dirname($rel_source_path)) . "/test";
$rel_ut_path = "${rel_ut_path}/${ut_name}";

if ($debugging) {
    print "rel_source_path = ${rel_source_path}\n";
    print "source_path = ${source_path}\n";
    print "source_name = ${source_name}\n";
    print "module_name = ${module_name}\n";
    print "ut_name = ${ut_name}\n";
    print "rel_ut_path = ${rel_ut_path}\n";
}

sub sys {
    my @cmd = @_;

    print "sys: @{cmd}\n" if $debugging;
    return system("@{cmd} >/dev/null 2>&1");
}

my $r = &sys("lcov --zerocounters --directory .");
if ($r != 0) {
    print STDERR "failed resetting coverage data.\n";
    exit 1;
}

$r = system("${rel_ut_path}");
if ($r != 0) {
    print STDERR "failed running unit test\n";
    exit 1;
}

$r = &sys("lcov --capture --directory . --test-name ${ut_name} -o ${module_name}.tmp.cov");
if ($r != 0) {
    print STDERR "failed capturing coverage data\n";
    exit 1;
}

$r = &sys("lcov --extract ${module_name}.tmp.cov ${module_name} ${source_path} -o ${module_name}.cov");
unlink("${module_name}.tmp.cov");
if ($r != 0) {
    print STDERR "failed extracting coverage data\n";
    exit 1;
}

$r = &sys("genhtml --title ${module_name} --output-directory ./Coverage ${module_name}.cov");
unlink("${module_name}.cov");
if ($r != 0) {
    print STDERR "failed generating HTML coverage page\n";
    exit 1;
}
# No need to keep that
unlink("Coverage/" . dirname($rel_source_path) . "/index.html");

# FIXME This is a workaround. Don't know why genthml drops the "ui/"
# prefix...
my $rel_html_path = $rel_source_path;
$rel_html_path =~ s/^ui\///;

print "=======================================\n";
print "Resulting coverage page:\n";
print "    file://${build_dir}/Coverage/${rel_html_path}.gcov.html\n";
