#!/usr/bin/perl
#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

# TODO: refactor regexes as much as possible

use strict;
use warnings;

use File::Basename;
use File::Temp;
use Data::Dumper;

my $self = basename($0);

my $PLATFORM = $^O;
chomp($PLATFORM);

my $debugging = $ENV{ADM_COMMAND_EXTRACTION_DEBUG};

sub debug {
    print "@_" if $debugging;
}

sub error {
    print STDERR "@_\n";
}

if (@ARGV < 2) {
    print STDERR "Script to extract exported admin commands.\n";
    print STDERR "\n";
    print STDERR "Usage: ${self} <compile-command> <source>\n";
    print STDERR "\n";
    print STDERR "    <compile-command>  Full compilation commandline: compiler with flags,\n";
    print STDERR "                       include directives, etc.\n";
    print STDERR "    <source-list-file>  File containing a list of source files to extract\n";
    print STDERR "                        the commands from (one file per line).\n";
    print STDERR "\n";
    print STDERR "The description of the exported admin commands are printed to standard output\n";
    print STDERR "as Perl data for direct use by other scripts.\n";

    exit 1;
}

my $filename = pop @ARGV;
my $lineno;

my @compile_cmd = @ARGV;

# Ensure arguments with spaces are properly quoted
my @quoted_compile_cmd = ();
foreach my $token (@compile_cmd) {
    $token = "\"${token}\"" if $token =~ / /;
    push(@quoted_compile_cmd, $token);
}
@compile_cmd = @quoted_compile_cmd;

&debug("compile cmd: @{compile_cmd}\n");
&debug("filename: ${filename}\n");

################################################################################
# Parsing
################################################################################

sub syntax_error {
    &error("${filename}: ${lineno}: @_");
}

sub __debug_directives($) {
    my ($directives) = @_;

    return if !$debugging;

    print "--- DIRECTIVES BEGIN ---\n";
    foreach my $direc (@{$directives}) {
        print "${direc}\n";
    }
    print "--- DIRECTIVES END ---\n";
}

sub __debug_exported_lines($) {
    my ($lines) = @_;

    return if !$debugging;

    print "--- EXPORT BEGIN ---\n";
    foreach my $line (@{$lines}) {
        print $line, "\n";
    }
    print "--- EXPORT END ---\n";
}

sub parse_field($) {
    my ($line) = @_;

    my ($optional, $type, $field, $size, @default);
    my $opt;

    # Is it just a comment?
    if ($line =~ /^\s*\/\*.*\*\/\s*$/) {
        return { field => undef, kind => 'comment' };
    }

    # Is the parameter optional?
    if ($line =~ /^\s*__optional\s+/) {
        &debug("optional parameter\n");
        $optional = 1;
    } else {
        $optional = 0;
    }

    #
    # Try parsing an array field
    #
    ($opt, $type, $field, $size, @default) =
        $line =~ /^\s*(__optional\s+)?([a-zA-Z_0-9]+)\s+([a-zA-Z_0-9]+)\[([a-zA-Z_0-9 +\-\*\/\(\)]+)\](\s+__default\(([a-zA-Z_0-9"\-]+)\))?\s*;/;

    if ($optional) {
        if (defined($opt) && (!@default || !defined($default[1]))) {
            &syntax_error("missing default value for optional param");
            return undef;
        }
    }

    if (defined($type) && defined($field) && defined($size)) {
        &debug("found array field '${field}' of size '${size}' of type '${type}'",
               ($optional ? ", optional (defaults to '${default[1]}')" : ""), "\n");
        my $field_desc = { field => $field, kind => 'array', size => $size, type => $type, optional => $optional,
                           default => $default[1] };
        return $field_desc;
    }

    #
    # Try parsing an atom field
    #
    ($opt, $type, $field, @default) =
	$line =~ /^\s*(__optional\s+)?([a-zA-Z_0-9]+)\s+([a-zA-Z_0-9]+)(\s+__default\(([a-zA-Z_0-9"\-]+)\))?\s*;/;

    if ($optional) {
	if (!defined($opt)) {
	    &syntax_error("missing type?");
	    return undef;
	}
	if (defined($opt) && (!@default || !defined($default[1]))) {
	    &syntax_error("missing default value for optional param");
	    return undef;
	}
    }

    if (defined($type) && defined($field)) {
	&debug("found atom field '${field}' of type '${type}'",
	       ($optional ? ", optional (defaults to '${default[1]}')" : ""), "\n");
	my $field_desc = { field => $field, kind => 'atom', type => $type, optional => $optional,
			   default => $default[1] };
	return $field_desc;
    }

    return undef;
}

# XXX Swap arguments: fields first, includes second?
sub preprocess_fields($$) {
    my ($directives, $fields) = @_;

    # String used to mark the beginning of fields in the preprocessed output
    # (everything above the marker is ignored)
    my $field_marker = "__EXTRACTED_FIELDS_BEGIN__";

    my $filename = tmpnam() . ".c";

    # Create a temporary file to be preprocessed
    if (!open(FILE, "> ${filename}")) {
        &error("failed creating temporary preprocessing file");
        return ();
    }
    foreach my $direc (@{$directives}) {
        print FILE "${direc}\n";
    }
    print FILE "#define ${field_marker} ${field_marker}\n";
    print FILE "${field_marker}\n";
    my $i = 0;
    foreach my $field (@{$fields}) {
        # Print preprocessor directives but instead of printing the fields,
        # just print their index in the structure: we'll use these indexes
        # to build the final field list.
        if ($field =~ /^#/) {
            print FILE "${field}\n";
        } else {
            print FILE "${i}\n";
        }
        $i++;
    }
    close(FILE);

    # Let the preprocessor do the work
    my $preprocess_cmd = "@{compile_cmd} -E ${filename}";
    if ($PLATFORM ne "MSWin32") {
        $preprocess_cmd = "LC_ALL=C " . $preprocess_cmd;
    }
    &debug("preprocessing cmd: ${preprocess_cmd}\n");
    my @preprocessed_source = `${preprocess_cmd}`;
    my $prepro_result = $?;

    unlink($filename);

    if ($prepro_result != 0) {
        &error("failed preprocessing fields");
        return ();
    }

    map(chomp, @preprocessed_source);

    # Get the field indexes that remain after the file has been preprocessed
    my $in_fields = 0;
    my @indexes = ();
    foreach my $line (@preprocessed_source) {
        if ($line eq "__EXTRACTED_FIELDS_BEGIN__") {
            $in_fields = 1;
            next;
        }

        if ($in_fields && $line ne "") {
            # A line is merely an index (into the original fields) at this point
            push(@indexes, $line);
        }
    }

    # The resulting list of fields is built from the *original* source: the
    # preprocessed source can't be used because constants have been replaced.
    my @result = ();
    foreach my $index (@indexes) {
        push(@result, @{$fields}[$index]);
    }

    return @result;
}

# Only call if the export actually has params (ie, is not specified
# with __no_param).
# XXX Return error instead of exiting
sub parse_export($$$) {
    my ($cur_export, $lines, $directives) = @_;

    __debug_directives($directives);
    __debug_exported_lines($lines);

    # Each line contains a field except the first two (export header, opening
    # brace) and the last one (closing brace).
    my @fields = @{$lines}[2..scalar(@{$lines})-2];

    # Check whether we need to preprocess the fields. It's nice to have on
    # Windows as ICC is dog slow.
    my $has_direc = 0;
    foreach my $field (@fields) {
        if ($field =~ /^\s*#(ifdef|ifndef|if|endif)/) {
            $has_direc = 1;
            last;
        }
    }

    my @preprocessed_fields;
    if ($has_direc) {
        @preprocessed_fields = &preprocess_fields($directives, \@fields);
    } else {
        @preprocessed_fields = @fields;
    }

    if (!@preprocessed_fields) {
        exit 1;
    }

    foreach my $field (@preprocessed_fields) {
        my $field_desc = &parse_field($field);
        if (!defined($field_desc)) {
            &error("failed parsing field: ${field}");
            exit 1;
        }

        if ($field_desc->{kind} ne 'comment') {
            push(@{$cur_export->{fields}}, $field_desc);
        }
    }
}

my $parsing = 0;
my @directives = ();
my @export_lines = ();
my @exports = ();
my %names = ();

my $cur_export;

sub remember_directive($$$) {
    my ($directives, $direc, $line) = @_;

    if ($direc eq "endif") {
        if (@{$directives}[-1] =~ /^\s*#(ifdef|ifndef)/) {
            pop(@{$directives});
        } else {
            push(@{$directives}, $line);
        }
    } elsif ($direc eq "include") {
        if (!grep { $_ eq $line } @{$directives}) {
            push(@{$directives}, $line);
        }
    } else {
        push(@{$directives}, $line);
    }
}


#
# 1. Read the list of C source files from the specified file.
#
if (!open(LIST_FILE, "< ${filename}")) {
    &error("failed opening source list file '${filename}'");
    exit 1;
}
my @source_list = <LIST_FILE>;
map(chomp, @source_list);
close(LIST_FILE);



foreach my $source (@source_list) {
    &debug("opening file '${source}'\n");
    if (!open(FILE, "< ${source}")) {
        &error("failed opening '${source}'");
        exit 1;
    }
    my @lines = <FILE>;
    close(FILE);

    &debug("parsing command-exported parameters...\n");

    $lineno = 0;
    foreach my $line (@lines) {
        chomp($line);
        $lineno++;

        if (!$parsing) {
            # Remember include directives and their surrounding conditionals, if
            # any (get rid of #if[n]def/#endif with nothing in between).
            my ($direc) = $line =~ /^\s*#(ifdef|ifndef|if|endif|include)/;
            if (defined($direc)) {
                &remember_directive(\@directives, $direc, $line);
                next;
            }
        }

        my ($symbol, $typedef, $struct);

        # Declaration of exported struct ?
        if ($line =~ /^\s*__export/) {
	    ($symbol, $typedef, $struct) =
	        $line =~ /^\s*__export\(([A-Z_0-9]+)\)\s+(typedef\s+)?struct\s+([a-zA-Z_0-9]+)/;

	    if (defined($symbol) && defined($struct)) {
	        &debug("beginning of exported struct: '${symbol}' => '${struct}'\n");
	        $cur_export = { symbol => $symbol, struct => $struct, fields => [] };
                push(@export_lines, $line);
	        push(@exports, $cur_export);
	        $parsing = 1;
	        next;
	    }

	    ($symbol) = $line =~ /^\s*__export\(([A-Z_0-9]+)\)\s+__no_param\s*;/;
	    if (defined($symbol)) {
	        &debug("command '${symbol}' has no params\n");
	        $cur_export = { symbol => $symbol, struct => undef };
                @export_lines = ();
	        push(@exports, $cur_export);
	        next;
	    }

	    &syntax_error("invalid __export directive");
	    exit 1;
        }

        next if !$parsing;

        push(@export_lines, $line);

        # End of non-typedef'ed exported struct ?
        if ($line =~ /\s*};/) {
	    &debug("end of exported struct (no typedef)\n");
            &parse_export($cur_export, \@export_lines, \@directives);
            @export_lines = ();
	    $parsing = 0;
	    next;
        }

        # End of typedef'ed exported struct ?
        ($typedef) = $line =~ /\s*}\s*([a-zA-Z_0-9]+)\s*;/;
        if (defined($typedef)) {
	    &debug("end of exported struct (typedef'ed '${typedef}')\n");
            &parse_export($cur_export, \@export_lines, \@directives);
            @export_lines = ();
	    $cur_export->{typedef} = $typedef;
	    $parsing = 0;
	    next;
        }
    }

    ################################################################################
    # Display debugging info
    ################################################################################

    if ($debugging) {
      &debug("==============\n");
      foreach my $export (@exports) {
        &debug("symbol: '$export->{symbol}' ");

        if (!defined($export->{struct})) {
          &debug("no_param\n");
          next;
        }

        &debug("struct: '$export->{struct}'");

        if (defined($export->{typedef})) {
          &debug(" typedef: '$export->{typedef}'");
        }
        &debug("\n");

        foreach my $field_desc (@{$export->{fields}}) {
          if ($field_desc->{kind} eq 'array') {
	    &debug("\tarray: '$field_desc->{field}' '$field_desc->{size}'",
	      "'$field_desc->{type}'");
          } else {
	    &debug("\t atom: '$field_desc->{field}' '$field_desc->{type}'");
          }
          if ($field_desc->{optional}) {
	    &debug(", optional with default '$field_desc->{default}'");
          }
          &debug("\n");
        }
      }
    }

    ################################################################################
    # Build the (command identifier => command name) mappings
    ################################################################################

    &debug("==============\n");
    &debug("parsing command definitions...\n");

    my $definition = undef;
    my $in_cmd_def = 0;
    my ($code, $name) = (undef, undef);

    $lineno = 0;
    for my $line (@lines) {
        chomp($line);
        $lineno++;

        my ($tmp) = $line =~ /^const\s+AdmCommand\s+([a-z_]+)/;
        if (defined($tmp)) {
	    $definition = $tmp;
	    $in_cmd_def = 1;
	    &debug("found definition '${definition}' at line ${lineno}\n");
	    next;
        }

        if ($in_cmd_def) {
	    if ($line =~ /^};/) {
	        &debug("end of definition at line ${lineno}\n");
	        if (!$code) {
		    &error("missing code in definition '${definition}'");
		    exit 1;
	        }
	        if (!$name) {
		    &error("missing name in definition '${definition}'");
		    exit 1;
	        }
	        $names{$code} = $name;
	        $definition = undef;
	        $in_cmd_def = 0;
	        ($code, $name) = (undef, undef);
	    }

	    ($tmp)= $line =~ /\s+.code\s+=\s+([A-Z_0-9]+),/;
	    if (defined($tmp)) {
	        &debug("\tfound code '${tmp}' at line ${lineno}\n");
	        $code = $tmp;
	    }
	    # XXX Yes, the name field is really called 'msg' -<:o
	    ($tmp) = $line =~ /\s+.msg\s+=\s+"([a-z_ ]+)",/;
	    if (defined($tmp)) {
	        &debug("\tfound name '${tmp}' at line ${lineno}\n");
	        $name = $tmp;
	    }
        }
    }

    # Debugging of the (command identifier => command name) mappings

    if ($debugging) {
        &debug("==============\n");
        &debug("(command identifier => command name) mappings:\n");
        foreach my $identifier (keys %names) {
	    &debug("\t${identifier} -> $names{$identifier}\n");
        }
    }
}

#use generate_xml_parsing;

#generate_xml_parsing::generate_code(\@exports, \%names);
print Data::Dumper->Dump([\@exports], ["EXPORTS"]);
print Data::Dumper->Dump([\%names], ["NAMES"]);
