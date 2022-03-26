#!/usr/bin/perl -w

=head1 NAME

import.pl

=head1 SYNOPSIS

import.pl [options] /path/to/edk2/edk2

Options:

    -h,--help		Display brief help message
    -v,--verbose	Increase verbosity
    -q,--quiet		Decrease verbosity

=cut

use File::Spec::Functions qw ( :ALL );
use File::Find;
use File::Path;
use Getopt::Long;
use Pod::Usage;
use FindBin;
use strict;
use warnings;

my $verbosity = 0;

sub try_import_file {
  my $wimbootdir = shift;
  my $edktop = shift;
  my $edkdirs = shift;
  my $filename = shift;

  # Skip everything except headers
  return unless $filename =~ /\.h$/;

  # Skip files that are wimboot native headers
  my $outfile = catfile ( $wimbootdir, $filename );
  if ( -s $outfile ) {
    open my $outfh, "<$outfile" or die "Could not open $outfile: $!\n";
    my $line = <$outfh>;
    close $outfh;
    chomp $line;
    return if $line =~ /^\#ifndef\s+_WIMBOOT_\S+_H$/;
  }

  # Search for importable header
  foreach my $edkdir ( @$edkdirs ) {
    my $infile = catfile ( $edktop, $edkdir, $filename );
    if ( -e $infile ) {
      # We have found a matching source file - import it
      print "$filename <- ".catfile ( $edkdir, $filename )."\n"
	  if $verbosity >= 1;
      open my $infh, "<$infile" or die "Could not open $infile: $!\n";
      ( undef, my $outdir, undef ) = splitpath ( $outfile );
      mkpath ( $outdir );
      open my $outfh, ">$outfile" or die "Could not open $outfile: $!\n";
      my @dependencies = ();
      while ( <$infh> ) {
	# Strip CR and trailing whitespace
	s/\r//g;
	s/\s*$//g;
	chomp;
	# Update include lines, and record included files
	if ( s/^\#include\s+[<\"](\S+)[>\"]/\#include "efi\/$1"/ ) {
	  push @dependencies, $1;
	}
	# Write out line
	print $outfh "$_\n";
      }
      close $outfh;
      close $infh;
      # Recurse to handle any included files that we don't already have
      foreach my $dependency ( @dependencies ) {
	if ( ! -e catfile ( $wimbootdir, $dependency ) ) {
	  print "...following dependency on $dependency\n" if $verbosity >= 1;
	  try_import_file ( $wimbootdir, $edktop, $edkdirs, $dependency );
	}
      }
      return;
    }
  }
  die "$filename has no equivalent in $edktop\n";
}

# Parse command-line options
Getopt::Long::Configure ( 'bundling', 'auto_abbrev' );
GetOptions (
  'verbose|v+' => sub { $verbosity++; },
  'quiet|q+' => sub { $verbosity--; },
  'help|h' => sub { pod2usage ( 1 ); },
) or die "Could not parse command-line options\n";
pod2usage ( 1 ) unless @ARGV == 1;
my $edktop = shift;

# Identify edk import directories
my $edkdirs = [ "MdePkg/Include", "IntelFrameworkPkg/Include",
		"MdeModulePkg/Include", "EdkCompatibilityPkg/Foundation" ];
foreach my $edkdir ( @$edkdirs ) {
  die "Directory \"$edktop\" does not appear to contain the EFI EDK2 "
      ."(missing \"$edkdir\")\n" unless -d catdir ( $edktop, $edkdir );
}

# Identify wimboot EFI includes directory
my $wimbootdir = $FindBin::Bin;
die "Directory \"$wimbootdir\" does not contain the wimboot EFI includes\n"
    unless -e catfile ( $wimbootdir, "../wimboot.h" );

if ( $verbosity >= 1 ) {
  print "Importing EFI headers into $wimbootdir\nfrom ";
  print join ( "\n and ", map { catdir ( $edktop, $_ ) } @$edkdirs )."\n";
}

# Import headers
find ( { wanted => sub {
  try_import_file ( $wimbootdir, $edktop, $edkdirs,
		    abs2rel ( $_, $wimbootdir ) );
}, no_chdir => 1 }, $wimbootdir );
