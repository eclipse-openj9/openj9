##############################################################################
# Copyright (c) 2016, 2019 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
##############################################################################

use Cwd;
use strict;
use warnings;
use lib "./makeGenTool";
use File::Basename;
use File::Path qw/make_path/;
use Data::Dumper;
use feature 'say';
require "mkgen.pl";

my $all            = 0;
my $projectRootDir = '';
my $modesxml       = "../../resources/modes.xml";
my $ottawacsv      = "../../resources/ottawa.csv";
my $graphSpecs     = '';
my $jdkVersion     = '';
my $impl           = '';
my $output         = '';
my $buildList      = '';
my $iterations     = 1;
my $testFlag       = '';
my @allLevels      = ( "sanity", "extended", "special" );
my @allGroups      = ( "functional", "openjdk", "external", "perf", "jck", "system" );
my @allTypes       = ( "regular", "native" );
my @allImpls       = ( "openj9", "ibm", "hotspot", "sap" );

foreach my $argv (@ARGV) {
	if ( $argv =~ /^--graphSpecs=(.*)/ ) {
		$graphSpecs = $1;
	} elsif ( $argv =~ /^--jdkVersion=(.*)/ ) {
		$jdkVersion = $1;
	} elsif ( $argv =~ /^--impl=(.*)/ ) {
		$impl = $1;
	} elsif ( $argv =~ /^--projectRootDir=(.*)/ ) {
		$projectRootDir = $1;
	} elsif ( $argv =~ /^--output=(.*)/ ) {
		$output = $1;
	} elsif ( $argv =~ /^--modesXml=(.*)/ ) {
		$modesxml = $1; 
	} elsif ( $argv =~ /^--ottawaCsv=(.*)/ ) {
		$ottawacsv = $1;
	} elsif ( $argv =~ /^--buildList=(.*)/ ) {
		$buildList = $1;
	} elsif ( $argv =~ /^--iterations=(.*)/ ) {
		$iterations = $1;
	} elsif ( $argv =~ /^--testFlag=(.*)/ ) {
		$testFlag = $1;
	} else {
		print "This program will search projectRootDir provided and find/parse playlist.xml to generate\n"
			. "makefile (per project)\n"
			. "jvmTest.mk and specToPlat.mk - under TestConfig\n";
		print "Usage:\n"
			. "  perl testKitGen.pl --graphSpecs=[linux_x86-64|linux_x86-64_cmprssptrs|...] --jdkVersion=[8|9|...] --impl=[openj9|ibm|hotspot|sap] [options]\n"
			. "Options:\n"
			. "  --graphSpecs=<specs>      Comma separated specs that the build will run on.\n"
			. "  --jdkVersion=<version>    Jdk version that the build will run on.\n"
			. "  --impl=<implementation>   Java Implementation, e.g., openj9, ibm, hotspot, sap.\n"
			. "  --projectRootDir=<path>   Root path for searching playlist.xml.\n"
			. "                            The path defaults to openj9/test.\n"
			. "  --output=<path>           Path to output makefiles.\n"
			. "                            The path defaults to projectRootDir.\n"
			. "  --modesXml=<path>         Path to modes.xml file.\n"
			. "                            If the modesXml is not provided, the program will try to find modes.xml under projectRootDir/TestConfig/resources.\n"
			. "  --ottawaCsv=<path>        Path to ottawa.csv file.\n"
			. "                            If the ottawaCsv is not provided, the program will try to find ottawa.csv under projectRootDir/TestConfig/resources.\n"
			. "  --buildList=<paths>       Comma separated project paths to search playlist.xml. The paths are relative to projectRootDir.\n"
			. "                            If the buildList is not provided, the program will search all files under projectRootDir.\n"
			. "  --iterations=<number>     Repeatedly generate test command based on iteration number. The default value is 1.\n"
			. "  --testFlag=<string>       Comma separated string to specify different test flag. The default is empty.\n";
			die "Please specify valid options!";
	}
}

if ( !$projectRootDir ) {
	$projectRootDir = getcwd . "/../../..";
	if ( -e $projectRootDir ) {
		print "Using projectRootDir: $projectRootDir\n";
	} else {
		die "Please specify a valid project directory using option \"--projectRootDir=\".";
	}
}

if ( !$graphSpecs ) {
	die "Please provide graphSpecs!"
}

if ( !$jdkVersion ) {
	die "Please provide jdkVersion!"
}

if ( !$impl ) {
	die "Please provide impl!"
}

# run make file generator

runmkgen( $projectRootDir, \@allLevels, \@allGroups, \@allTypes, $output, $graphSpecs, $jdkVersion, \@allImpls, $impl, $modesxml, $ottawacsv, $buildList, $iterations, $testFlag );

print "\nTEST AUTO GEN SUCCESSFUL\n";
