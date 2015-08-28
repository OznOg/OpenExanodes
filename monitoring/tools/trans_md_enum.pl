#!/usr/bin/perl

use strict;
use warnings;
use Data::Dumper;

sub convert
{
    my ($header_file) = @_;

    open(my $F, '<', $header_file) or die("unable to open file");
    my @tmp = <$F>;
    close($F);

    my @slurp;

    # We eliminate every single line which is not a code line.
    foreach (@tmp)
    {
        chomp;
        next if /^$/;
        next if /^#/;
        next if /^\s+\*/;
        next if /\*\/$/;
        next if /^\s*\//;
        push @slurp, $_;
    }

    my %enums;

    my $continue = 1;
    my $last_idx = scalar(@slurp) - 1;
    my $curr_idx = 0;

    # Big terrible function just to create a hash table enum_name => [values]
    # from C declarations.
    # We just get status_type enum (which are useful for us).
    while ($curr_idx <= $last_idx)
    {
        if (grep(/typedef enum {/, $slurp[$curr_idx]))
        {
            my $end_enum = 0;
            my @enum_content;
            my $enum_name;
            my $enum_idx;
            while (not($end_enum))
            {
                if (grep(/;/, $slurp[$curr_idx]))
                {
                    ($enum_name) = $slurp[$curr_idx] =~ m/\s*\}\s*(\S+);/;
                    $end_enum = 1;
                }
                else
                {
                    if (not grep(/typedef enum {/, $slurp[$curr_idx]))
                    {
                        $slurp[$curr_idx] =~ s/\s//gix;
                        $slurp[$curr_idx] =~ s/,$//gix;
                        push(@enum_content, $slurp[$curr_idx]);
                    }
                }
                $curr_idx++;
            }
            if (grep(/_status_type/, $enum_name))
            {
                $enums{$enum_name} = \@enum_content;
            }
        }
        else
        {
            $curr_idx++;
        }
    }

    # finally we transform a little the hash table
    #  - names are not those we expect in MIB
    #  - we need statuses to have a number, as this's how we get them.
    my $enum_name;
    my $enum_data_r;

    # The new hash (ref) we'll use, containing ready-to-use data.
    my $enum_for_traps_r;

    while (($enum_name, $enum_data_r) = each(%enums))
    {
        my $new_name = $enum_name;

        # do not swap steps
        $new_name =~ s/type_t//gix;
        $new_name =~ s/md_//gix;
        $new_name =~ s/_(\w)/uc($1)/e;
        $new_name =~ s/_//gix;
        $new_name =~ s/^(\w)/uc($1)/e;
        $new_name =~ s/iskgroup/iskGroup/gix;
        $new_name = 'exa' . $new_name;

        my $idx = 1;
        foreach my $el (@{$enum_data_r})
        {
            $el =~ s/=1//gix;
            $el =~ s/MD.*STATUS_//gix;
            $enum_for_traps_r->{$new_name}->{$idx} = $el;
            $idx++;
        }
    }
    return $enum_for_traps_r;
}

my $header_file = shift;
die "md_types.h file must be provided" if (not defined($header_file));
my $r = convert($header_file);
print Dumper $r;
