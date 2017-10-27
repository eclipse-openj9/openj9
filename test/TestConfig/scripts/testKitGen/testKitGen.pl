##############################################################################
#  Copyright (c) 2016, 2017 IBM Corp. and others
#
#  This program and the accompanying materials are made available under
#  the terms of the Eclipse Public License 2.0 which accompanies this
#  distribution and is available at https://www.eclipse.org/legal/epl-2.0/
#  or the Apache License, Version 2.0 which accompanies this distribution and
#  is available at https://www.apache.org/licenses/LICENSE-2.0.
#
#  This Source Code may also be made available under the following
#  Secondary Licenses when the conditions for such availability set
#  forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
#  General Public License, version 2 with the GNU Classpath
#  Exception [1] and GNU General Public License, version 2 with the
#  OpenJDK Assembly Exception [2].
#
#  [1] https://www.gnu.org/software/classpath/license.html
#  [2] http://openjdk.java.net/legal/assembly-exception.html
#
#  SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
##############################################################################

use Cwd;
use strict;
use warnings;
use lib "./makeGenTool";
require "mkgen.pl";
use File::Basename;
use File::Path qw/make_path/;
use Data::Dumper;
use feature 'say';

my $all            = 0;
my $projectRootDir = '';
my $modesxml       = "../../resources/modes.xml";
my $ottawacsv      = "../../resources/ottawa.csv";
my $graphSpecs     = '';
my $output         = '';
my @allSubsets = ( "SE80", "SE90", "Panama", "Valhalla" );

foreach my $argv (@ARGV) {
	if ( $argv =~ /^\-\-graphSpecs=/ ) {
		($graphSpecs) = $argv =~ /^\-\-graphSpecs=(.*)/;
	}
	elsif ( $argv =~ /^\-\-projectRootDir=/ ) {
		($projectRootDir) = $argv =~ /^\-\-projectRootDir=(.*)/;
	}
	elsif ( $argv =~ /^\-\-output=/ ) {
		($output) = $argv =~ /^\-\-output=(.*)/;
	}
	elsif ( $argv =~ /^\-\-modesXml=/) {
		($modesxml) = $argv =~ /^\-\-modesXml=(.*)/;
	}
	elsif ( $argv =~ /^\-\-ottawaCsv=/) {
		($ottawacsv) = $argv =~ /^\-\-ottawaCsv=(.*)/;
	}
	else {
		print
"This program will search projectRootDir provided and find/parse playlist.xml to generate \n"
		  . "makefile (per project)\n"
		  . "jvmTest.mk and specToPlat.mk - under TestConfig\n";
		print "Options:\n"
		  . "--graphSpecs=<specs>    Comma separated specs that the build will run on.\n"
		  . "--output=<path>         Path to output makefiles.\n"
		  . "--projectRootDir=<path> Root path for searching playlist.xml.\n"
		  . "--modesXml=<path>       Path to modes.xml file.\n"
		  . "                        If the modesXml is not provided, the program will try to find modes.xml under projectRootDir/TestConfig/resources.\n"
		  . "--ottawaCsv=<path>      Path to ottawa.csv file.\n"
  		  . "                        If the ottawaCsv is not provided, the program will try to find ottawa.csv under projectRootDir/TestConfig/resources.\n";
		die "Please specify valid options!";
	}
}

if ( !$projectRootDir ) {
	$projectRootDir = getcwd . "/../../..";
	if ( -e $projectRootDir ) {
		print
"projectRootDir is not provided. Set projectRootDir = $projectRootDir\n";
	}
	else {
		die
"Please specify a valid project directory using option \"--projectRootDir=\".";
	}
}

if ( !$graphSpecs ) {
	die "Please provide graphSpecs!"
}

# run make file generator
my $tests = runmkgen( $projectRootDir, \@allSubsets, $output, $graphSpecs, $modesxml, $ottawacsv );

print "\nTEST AUTO GEN SUCCESSFUL\n";