#!/usr/bin/perl
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
#  SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
##############################################################################

use strict;

#
# This script cleans up SysV memory and semaphores.
#

my $numargs = @ARGV;
if ($numargs > 2) {
	print("Error: incorrect usage!\n");
	print("\tscriptname.pl userid zos\n");
	print("\t\tuserid: user you wish to clean up SysV objects for\n");
	print("\t\tzos: set this if your cleaning up on zos\n");
}

#
# The user we are cleaning up ipcs for ...
#

my $cleanupuser = "j9build";
# a1 is for Attach API, ad is for shared classes, 0a is for pltest (and for cloud at one time)
my $semaphoreProjid = "(0xad|0xa1|0x41|0x42|0x43|0x44|0x45|0x46|0x47|0x48|0x49|0x4a|0x4b|0x4c|0x4d|0x4e|0x4f|0x50|0x51|0x52|0x53|0x54|".
					"0x81|0x81|0x82|0x83|0x84|0x85|0x86|0x87|0x88|0x89|0x8a|0x8b|0x8c|0x8d|0x8e|0x8f|0x90|0x91|0x92|0x93|0x94|0x0a)";

my $memoryProjid = "(0xde|0x21|0x22|0x23|0x24|0x25|0x26|0x27|0x28|0x29|0x2a|0x2b|0x2c|0x2d|0x2e|0x2f|0x30|0x31|0x32|0x33|0x34|".
					"0x61|0x61|0x62|0x63|0x64|0x65|0x66|0x67|0x68|0x69|0x6a|0x6b|0x6c|0x6d|0x6e|0x6f|0x70|0x71|0x72|0x73|0x74)";

my $keyindex = 0;
my $idindex = 1;
my $ownerindex = 2;

my @stdoutvar = ();
my $iter = "";
my $foundSemaphoreHeader = 0;
my $foundMemoryHeader = 0;

my $memsremoved=0;
my $semsremoved=0;

if ($numargs>0) {
	$cleanupuser=$ARGV[0];
}
if (($numargs>1) && ($ARGV[1] =~ /zos/))
{
	$keyindex=2;
	$idindex=1;
	$ownerindex=4;
}

if (($numargs>1) && ($ARGV[1] =~ /aix/))
{
	$keyindex=2;
	$idindex=1;
	$ownerindex=4;
}


printf("Start SysV clean up for user:".$cleanupuser." ".$ARGV[1]."\n");

#
# 1.) LIST MEMORY:
# - Call ipcs and store stdout in a local variable.
# - If the command fails we exit ASAP
#
@stdoutvar = `ipcs -m` or die("Error: could not execute ipcs!");

#
# 2.) CLEAN UP SYSV MEMORY
# Loop through stdout and perform clean up of semaphores owned by 'user'
#
$iter = "";
$foundSemaphoreHeader = 0;
$foundMemoryHeader = 0;
foreach $iter (@stdoutvar) {

	if ($iter =~ /Shared Memory/) {
		$foundMemoryHeader=1;
	}
	if (($iter =~ /$memoryProjid/) && ($foundMemoryHeader==1)) {
		#remove any sequence of whitespace and insert a :
		$iter =~s/[\s]+/:/g;

		#split the string around :
		my @vals = split(/:/,$iter);

		#we expect 3 entries in this row ... the 1st three are kei,id,owner ...
		#the rest are don't cares ...
		if (@vals<3) {
			die("ipcs did not return expected ouput");
		}

		my $key = $vals[$keyindex];
		my $id  = $vals[$idindex];
		my $owner = $vals[$ownerindex];
		if (($owner =~ /$cleanupuser/) || ($cleanupuser =~ /all/)) {
			my $cleanmem = "ipcrm -m $id";
			my $out = `$cleanmem`;
			print($out);
			$memsremoved = $memsremoved + 1;
		}
	}
}

#
# 3.) LIST SEMAPHORES:
# - Call ipcs and store stdout in a local variable.
# - If the command fails we exit ASAP
#
@stdoutvar = `ipcs -s` or die("Error: could not execute ipcs!");

#
# 4.) CLEAN UP SYSV MEMORY
# Loop through stdout and perform clean up of semaphores owned by 'user'
#
$iter = "";
$foundSemaphoreHeader = 0;
$foundMemoryHeader = 0;
foreach $iter (@stdoutvar) {

	if ($iter =~ /Semaphore/) {
		$foundSemaphoreHeader=1;
	}
	if (($iter =~ /$semaphoreProjid/) && ($foundSemaphoreHeader==1)) {
		#remove any sequence of whitespace and insert a :
		$iter =~s/[\s]+/:/g;

		#split the string around :
		my @vals = split(/:/,$iter);

		#we expect 3 entries in this row ... the 1st three are kei,id,owner ...
		#the rest are don't cares ...
		if (@vals<3) {
			die("ipcs did not return expected ouput");
		}
		my $key = $vals[$keyindex];
		my $id  = $vals[$idindex];
		my $owner = $vals[$ownerindex];
		if (($owner =~ /$cleanupuser/)  || ($cleanupuser =~ /all/)) {
			my $cleansem = "ipcrm -s $id";
			my $out = `$cleansem`;
			print($out);
			$semsremoved = $semsremoved + 1;
		}
	}
}

printf("Clean up is finished:\n");
printf("\tShared Memory Segments Removed:".$memsremoved."\n");
printf("\tShared Semaphores Removed:".$semsremoved."\n");

0;

