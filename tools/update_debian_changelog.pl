#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

my $PLATFORM = $^O;
chomp($PLATFORM);
my $DEV_NULL = $PLATFORM eq "MSWin32" ? "nul" : "/dev/null";

$ENV{LC_ALL} = "C";

my $CHANGELOG_FILE = "debian/changelog";
my $SRCDIR = $ARGV[0];

exit(0) if system("git --version > $DEV_NULL") != 0;

if (-d "$SRCDIR/.git")
{
    # compute the branch/tag
    my $CHANGELOG_DATE = `cd $SRCDIR && git log -1 --format=%cd`;
    chomp($CHANGELOG_DATE);
    open(OLD_FILE, "<", "$CHANGELOG_FILE");
    read(OLD_FILE, $content, 1024);
    close(OLD_FILE);
    $content =~ s/( -- .*>  ).*$/\1$CHANGELOG_DATE/;
    open(NEW_FILE, ">", "$CHANGELOG_FILE");
    print(NEW_FILE $content);
    close(NEW_FILE);
}
else
{
    print("Not a git working directory\n");
}
