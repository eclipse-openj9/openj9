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
use IPC::SysV qw(IPC_STAT IPC_SET);
use Fcntl qw(SEEK_SET);

#
# FWD DECLARES
#
sub cleanupSysV;
sub cleanupSysVMem;
sub cleanupSysVSem;
sub listSysVSemaphores;
sub listSysVMemory;
sub forkAndRunInBackGround;
sub listFilesInFolder;
sub getSemaphoreCntlFile;
sub getMemoryCntlFile;
sub removeCntrlFiles;
sub Test1;
sub Test2;
sub Test3;
sub Test4;
sub Test5;
sub Test6;
sub Test7;
sub Test8;
sub Test9;
sub Test10;
sub Test11;
sub Test12;
sub Test13;
sub Test14;
sub Test15;
sub Test16;
sub Test17;
sub Test18;
sub Test19;
sub Test20;
sub Test21;
sub Test22;


#
# MAIN ...
#
my $osname = "";
my $numargs = @ARGV;
my @semaphoreList;
my @memoryList;
my $ITER;
my $verbose = 0; #0 for false, 1 for true
my $numprocs = 15;
my $READONLY = 0400;
my $testcachname = "SysVCacheTestFileName";
my $java = "./java";

#
# Java commands for testing ...
#
my $cmd = $java. " -Xshareclasses:nonpersistent,name=".$testcachname." -version 2>&1";
my $cmddumpdisabled = $java. " -Xshareclasses:disablecorruptcachedumps,nonpersistent,name=".$testcachname." -version 2>&1";
my $destroycmd = $java. " -Xshareclasses:nonpersistent,name=".$testcachname.",destroy 2>&1";
my $cmdNonfal = $java. " -Xshareclasses:nonpersistent,name=".$testcachname.",nonfatal -version 2>&1";

#
# Script argument handling ...
#
if ($numargs != 1) {
	print("Error: incorrect useage!\n");
	print("\tscriptname.pl zos|aix|linux\n");
	die ("Invalid arguments!");
} elsif ($numargs == 1) {
	$osname = $ARGV[0];
}

#
# Start tests
#
my $semPermIndex = &getIPCObjPermIndex($osname);
my $shmPermIndex = &getIPCObjPermIndex($osname);
my $passedTestCount = 0;
my $failedTestCount = 0;

&cleanupSysV($osname,$verbose);
&removeCntrlFiles($testcachname);
if (&Test1($osname, $cmd, $numprocs) == 1) {
	$passedTestCount+=1;
} else {
	$failedTestCount+=1;
}

&cleanupSysV($osname,$verbose);
&removeCntrlFiles($testcachname);
if (&Test2($osname, $cmd, $numprocs) == 1) {
	$passedTestCount+=1;
} else {
	$failedTestCount+=1;
}

&cleanupSysV($osname,$verbose);
&removeCntrlFiles($testcachname);
if (&Test3($osname, $cmd, $numprocs) == 1) {
	$passedTestCount+=1;
} else {
	$failedTestCount+=1;
}

&cleanupSysV($osname,$verbose);
&removeCntrlFiles($testcachname);
if (&Test4($osname, $cmd, $numprocs) == 1) {
	$passedTestCount+=1;
} else {
	$failedTestCount+=1;
}

&cleanupSysV($osname,$verbose);
&removeCntrlFiles($testcachname);
if (&Test5($osname, $cmd, $numprocs) == 1) {
	$passedTestCount+=1;
} else {
	$failedTestCount+=1;
}

&cleanupSysV($osname,$verbose);
&removeCntrlFiles($testcachname);
if (&Test6($osname, $cmd, $numprocs) == 1) {
	$passedTestCount+=1;
} else {
	$failedTestCount+=1;
}

&cleanupSysV($osname,$verbose);
&removeCntrlFiles($testcachname);
if (&Test7($osname, $cmd, $numprocs) == 1) {
	$passedTestCount+=1;
} else {
	$failedTestCount+=1;
}

&cleanupSysV($osname,$verbose);
&removeCntrlFiles($testcachname);
if (&Test8($osname, $cmd, $numprocs) == 1) {
	$passedTestCount+=1;
} else {
	$failedTestCount+=1;
}

&cleanupSysV($osname,$verbose);
&removeCntrlFiles($testcachname);
if (&Test9($osname, $cmd, $numprocs) == 1) {
	$passedTestCount+=1;
} else {
	$failedTestCount+=1;
}


&cleanupSysV($osname,$verbose);
&removeCntrlFiles($testcachname);
if (&Test10($osname, $cmd, $destroycmd, $numprocs) == 1) {
	$passedTestCount+=1;
} else {
	$failedTestCount+=1;
}

&cleanupSysV($osname,$verbose);
&removeCntrlFiles($testcachname);
if (&Test11($osname, $cmd, $destroycmd, $numprocs) == 1) {
	$passedTestCount+=1;
} else {
	$failedTestCount+=1;
}

&cleanupSysV($osname,$verbose);
&removeCntrlFiles($testcachname);
if (&Test12($osname, $cmd, $destroycmd, $numprocs) == 1) {
	$passedTestCount+=1;
} else {
	$failedTestCount+=1;
}

&cleanupSysV($osname,$verbose);
&removeCntrlFiles($testcachname);
if (&Test13($osname, $cmd, $destroycmd, $numprocs) == 1) {
	$passedTestCount+=1;
} else {
	$failedTestCount+=1;
}

&cleanupSysV($osname,$verbose);
&removeCntrlFiles($testcachname);
if (&Test14($osname, $cmd, $numprocs) == 1) {
	$passedTestCount+=1;
} else {
	$failedTestCount+=1;
}

&cleanupSysV($osname,$verbose);
&removeCntrlFiles($testcachname);
if (&Test15($osname, $cmd, $cmdNonfal, $numprocs) == 1) {
	$passedTestCount+=1;
} else {
	$failedTestCount+=1;
}

&cleanupSysV($osname,$verbose);
&removeCntrlFiles($testcachname);
if (&Test16($osname, $cmd, 100) == 1) {
	$passedTestCount+=1;
} else {
	$failedTestCount+=1;
}
&cleanupSysV($osname,$verbose);
&removeCntrlFiles($testcachname);
if (&Test17($osname, $java, $numprocs) == 1) {
	$passedTestCount+=1;
} else {
	$failedTestCount+=1;
}

&cleanupSysV($osname,$verbose);
&removeCntrlFiles($testcachname);
if (&Test18($osname, $java, $numprocs) == 1) {
	$passedTestCount+=1;
} else {
	$failedTestCount+=1;
}

&cleanupSysV($osname,$verbose);
&removeCntrlFiles($testcachname);
if (&Test19($osname, $cmd, $numprocs) == 1) {
	$passedTestCount+=1;
} else {
	$failedTestCount+=1;
}

&cleanupSysV($osname,$verbose);
&removeCntrlFiles($testcachname);
if (&Test20($osname, $cmd, $numprocs) == 1) {
	$passedTestCount+=1;
} else {
	$failedTestCount+=1;
}
&cleanupSysV($osname,$verbose);
&removeCntrlFiles($testcachname);
if (&Test21($osname, $cmddumpdisabled, $numprocs) == 1) {
	$passedTestCount+=1;
} else {
	$failedTestCount+=1;
}
&cleanupSysV($osname,$verbose);
&removeCntrlFiles($testcachname);
if (&Test22($osname, $cmddumpdisabled) == 1) {
	$passedTestCount+=1;
} else {
	$failedTestCount+=1;
}

print("COMPLETE (".$passedTestCount." PASSED TESTS ... ".$failedTestCount." FAILED TESTS.)"."\n");

exit 0;


sub getSemaphoreCntlFile
{
	my ($testcachname) = @_;
	my $testdir = "/tmp/javasharedresources/";
	my @listfolders = &listFilesInFolder($testdir, $numprocs);
	foreach $ITER (@listfolders) {
		#print "\t".$ITER."\n";
		if ($ITER =~ /$testcachname/) {
			if ($ITER =~ /semaphore/) {
				return $testdir . $ITER;
			}
		}
	}
	return "";
}

sub getMemoryCntlFile
{
	my ($testcachname) = @_;
	my $testdir = "/tmp/javasharedresources/";
	my @listfolders = &listFilesInFolder($testdir, $numprocs);
	foreach $ITER (@listfolders) {
		#print "\t".$ITER."\n";
		if ($ITER =~ /$testcachname/) {
			if ($ITER =~ /memory/) {
				return $testdir . $ITER;
			}
		}
	}
	return "";
}

sub removeCntrlFiles
{
	#
	# Ruthless clean up of any cache using the same cache name as we
	# use for this test
	#
	my ($testcachname) = @_;
	my $testdir = "/tmp/javasharedresources/";
	my @listfolders = &listFilesInFolder($testdir, $numprocs);
	foreach $ITER (@listfolders) {
		#print "\t".$ITER."\n";
		if ($ITER =~ /$testcachname/) {
			my $cmd = "rm -f ". $testdir . $ITER;
			#print $cmd . "\n";
			`$cmd`;
		}
	}
}

sub Test1
{
	my ($osname, $cmd, $numprocs) = @_;
	my @pidList;
	my $count = 0;
	my $waitedfor=0;

	print "Test 1: verify 1 only semaphore created\n";

	my @semaphoreList = &listSysVSemaphores($osname);
	my $startCount = scalar(@semaphoreList);
	my $endCount = 0;

	for ($count = 0; $count < $numprocs; $count++) {
		my $pid = &forkAndRunInBackGround($cmd);
		push(@pidList, $pid);
	}

	while (wait() != -1) {
		$waitedfor = $waitedfor + 1;
	}
	printf("\tWe succesfully ran " . $waitedfor . " JVMs with shared classes.\n");

	@semaphoreList = &listSysVSemaphores($osname);
	$endCount = scalar(@semaphoreList);

	if ($endCount == $startCount+1) {
		print "\tThere is only one semaphore (as expected)\n";
		print "\tTEST PASS\n";
		return 1;
	} else {
		print "\tThere are ".$endCount." semaphores (we expect 1)\n";
		print "\tTEST FAIL\n";
		return 0;
	}
}

sub Test2
{
	my ($osname, $cmd, $numprocs) = @_;
	my @pidList;
	my $count = 0;
	my $waitedfor=0;

	print "Test 2: verify 1 only shared memory created\n";

	my @memoryList = &listSysVMemory($osname);
	my $startCount = scalar(@memoryList);
	my $endCount = 0;

	for ($count = 0; $count < $numprocs; $count++) {
		my $pid = &forkAndRunInBackGround($cmd);
		push(@pidList, $pid);
	}

	while (wait() != -1) {
		$waitedfor = $waitedfor + 1;
	}
	printf("\tWe succesfully ran " . $waitedfor . " JVMs with shared classes.\n");

	@memoryList = &listSysVMemory($osname);
	$endCount = scalar(@memoryList);

	if ($endCount == $startCount+1) {
		print "\tThere is only one shared memory (as expected)\n";
		print "\tTEST PASS\n";
		return 1;
	} else {
		print "\tThere are ".$endCount." shared memories (we expect 1)\n";
		print "\tTEST FAIL\n";
		return 0;
	}
}


sub Test3
{
	my ($osname, $cmd, $numprocs) = @_;
	my @pidList;
	my $count = 0;
	my $waitedfor=0;

	print "Test 3: verify no new shared memory are created on 2nd run of JVM\n";

	for ($count = 0; $count < $numprocs; $count++) {
		my $pid = &forkAndRunInBackGround($cmd);
		push(@pidList, $pid);
	}

	$waitedfor = 0;
	while (wait() != -1) {
		$waitedfor = $waitedfor + 1;
	}
	printf("\tWe succesfully ran " . $waitedfor . " JVMs with shared classes.\n");


	my $semCount = scalar(&listSysVSemaphores($osname));
	my $memCount = scalar(&listSysVMemory($osname));

	for ($count = 0; $count < $numprocs; $count++) {
		my $pid = &forkAndRunInBackGround($cmd);
		push(@pidList, $pid);
	}

	$waitedfor = 0;
	while (wait() != -1) {
		$waitedfor = $waitedfor + 1;
	}
	printf("\tWe succesfully ran " . $waitedfor . " JVMs with shared classes.\n");

	if  ($semCount != scalar(&listSysVSemaphores($osname))) {
		print "\tTEST FAIL\n";
		return 0;
	} elsif ($memCount != scalar(&listSysVMemory($osname))) {
		print "\tTEST FAIL\n";
		return 0;
	} else {
		print "\tTEST PASS\n";
	}
	return 1;

}

sub Test4
{
	my ($osname, $cmd, $numprocs) = @_;
	my @pidList;
	my $count = 0;
	my $waitedfor=0;

	print "Test 4: Create semaphore successfully when stale semaphores exist\n";
	`$cmd`;
	&removeCntrlFiles($testcachname);
	`$cmd`;
	&removeCntrlFiles($testcachname);
	`$cmd`;
	&removeCntrlFiles($testcachname);

	if (&getSemaphoreCntlFile($testcachname) ne "") {
		print "\tTEST FAIL file ".&getSemaphoreCntlFile($testcachname)." found\n";
		return 0;
	}
	if (&getMemoryCntlFile($testcachname) ne "") {
		print "\tTEST FAIL file ".&getMemoryCntlFile($testcachname)." found\n";
		return 0;
	}

	my $semCount = scalar(&listSysVSemaphores($osname));
	my $memCount = scalar(&listSysVMemory($osname));

	#print "\t".$cmd."\n";
	`$cmd`;

	if ($memCount == scalar(&listSysVMemory($osname))) {
		print "\tTEST FAIL mem count ".$memCount."\n";
		return 0;
	}

	if ($semCount == scalar(&listSysVSemaphores($osname))) {
		print "\tTEST FAIL sem count ".$semCount." vs ".scalar(&listSysVSemaphores($osname))."\n";
		return 0;
	}

	print "\tTEST PASS\n";
	return 1;

}


sub Test5
{
	my ($osname, $cmd, $numprocs) = @_;
	my @pidList;
	my $count = 0;
	my $waitedfor=0;
	my $touchcmd;


	print "Test 5: Test empty control files.\n";
	`$cmd`;
	my $semfile = &getSemaphoreCntlFile($testcachname);
	my $memfile = &getMemoryCntlFile($testcachname);
	&removeCntrlFiles($testcachname);

	$touchcmd = "touch ".$semfile;
	`$touchcmd`;
	$touchcmd = "touch ".$memfile;
	`$touchcmd`;
	my $semCount = scalar(&listSysVSemaphores($osname));
	my $memCount = scalar(&listSysVMemory($osname));
	`$cmd`;
	if ($memCount == scalar(&listSysVMemory($osname))) {
		print "\tTEST FAIL mem count ".$memCount."\n";
		return 0;
	}
	if ($semCount == scalar(&listSysVSemaphores($osname))) {
		print "\tTEST FAIL sem count ".$semCount." vs ".scalar(&listSysVSemaphores($osname))."\n";
		return 0;
	}
	print "\tTEST PASS\n";
	return 1;

}

sub Test6
{
	my ($osname, $cmd, $numprocs) = @_;
	my @pidList;
	my $count = 0;
	my $waitedfor=0;
	my $chmodcmd;


	print "Test 6: Open cache with readonly control file.\n";
	`$cmd`;
	my $semfile = &getSemaphoreCntlFile($testcachname);
	my $memfile = &getMemoryCntlFile($testcachname);

	$chmodcmd = "chmod a-wx ".$semfile;
	`$chmodcmd`;
	$chmodcmd = "chmod a-wx ".$memfile;
	`$chmodcmd`;
	my $semCount = scalar(&listSysVSemaphores($osname));
	my $memCount = scalar(&listSysVMemory($osname));

	`$cmd`;

	$chmodcmd = "chmod ug+w ".$semfile;
	`$chmodcmd`;
	$chmodcmd = "chmod ug+w ".$memfile;
	`$chmodcmd`;

	if ($memCount != scalar(&listSysVMemory($osname))) {
		print "\tTEST FAIL mem count ".$memCount."\n";
		return 0;
	}
	if ($semCount != scalar(&listSysVSemaphores($osname))) {
		print "\tTEST FAIL sem count ".$semCount." vs ".scalar(&listSysVSemaphores($osname))."\n";
		return 0;
	}
	print "\tTEST PASS\n";
	return 1;

}


sub Test7
{
	my ($osname, $cmd, $numprocs) = @_;
	my @pidList;
	my $count = 0;
	my $waitedfor=0;
	my $chmodcmd;


	print "Test 7: Open cache with readonly control file and no corresponding SysV objs.\n";
	`$cmd`;
	my $semfile = &getSemaphoreCntlFile($testcachname);
	my $memfile = &getMemoryCntlFile($testcachname);

	$chmodcmd = "chmod a-wx ".$semfile;
	`$chmodcmd`;
	$chmodcmd = "chmod a-wx ".$memfile;
	`$chmodcmd`;

	&cleanupSysV($osname,$verbose);
	my $semCount = scalar(&listSysVSemaphores($osname));
	my $memCount = scalar(&listSysVMemory($osname));


	`$cmd`;

	$chmodcmd = "chmod ug+w ".$semfile;
	`$chmodcmd`;
	$chmodcmd = "chmod ug+w ".$memfile;
	`$chmodcmd`;

	if ($memCount != scalar(&listSysVMemory($osname))) {
		print "\tTEST FAIL mem count ".$memCount." vs ".scalar(&listSysVMemory($osname))."\n";
		return 0;
	}
	if ($semCount != scalar(&listSysVSemaphores($osname))) {
		print "\tTEST FAIL sem count ".$semCount." vs ".scalar(&listSysVSemaphores($osname))."\n";
		return 0;
	}

	$cmd = "unlink ".$semfile;
	`$cmd`;
	$cmd = "unlink ".$memfile;
	`$cmd`;

	print "\tTEST PASS\n";
	return 1;

}

sub Test8
{
	my ($osname, $cmd, $numprocs) = @_;
	my @pidList;
	my $count = 0;
	my $waitedfor=0;
	my @semaphoreListBefore;
	my @semaphoreListAfter;
	my $ITER;
	my $ITER2;

	print "Test 8: when no permissions to access a semaphore make sure do not we create a new one.\n";
	#system("ipcs");
	@semaphoreListBefore = &listSysVSemaphores($osname);
	`$cmd`;
	@semaphoreListAfter = &listSysVSemaphores($osname);

	#we expect one semaphore to be created
	if (scalar(@semaphoreListBefore)+1 != scalar(@semaphoreListAfter)) {
		print "\tTEST FAIL (bad sem count)\n";
		return 0;
	}

	#find the newly created semaphore
	my $semid = 0;
	BREAK_FROM_LOOP: {
		foreach $ITER (@semaphoreListAfter) {
			my $found = 0;
			foreach $ITER2 (@semaphoreListBefore) {
				if ($ITER == $ITER2) {
					$found = $ITER;
				}
			}
			if ($found == 0) {
					$semid = $ITER;
					#this equivalent to a break statement in C ...
					last BREAK_FROM_LOOP;
			}
		}
	}

	if ($semid == 0) {
		print "\tTEST FAIL (not found)\n";
		return 0;
	}


	my $statInfo;
	semctl($semid, 0, IPC_STAT, $statInfo);
	my @mdata = unpack("l*",$statInfo);

    #my $key = @mdata[0];
    #my $ uid = @mdata[1];
    #my $gid = @mdata[2];
    #my $cuid = @mdata[3];
    #my $cgid = @mdata[4];
    #my $mode = @mdata[5];
    #my $seq = @mdata[6];

	#set permissions to O000 (no permissions) ...
	my $oldperm =  @mdata[$semPermIndex];
	print "\tChange permissions from ".sprintf("O%o",$oldperm)." to ".$READONLY."\n";
    @mdata[$semPermIndex] = $READONLY;
	$statInfo = pack("l*",@mdata);
	semctl($semid, 0, IPC_SET, $statInfo);

	#system("ipcs");
	@semaphoreListBefore = @semaphoreListAfter;
	#print $cmd."\n";
	`$cmd`;
	@semaphoreListAfter = &listSysVSemaphores($osname);
	#system("ipcs");

	#No new semaphore should be created ... and nothing deleted ...
	if (scalar(@semaphoreListBefore) != scalar(@semaphoreListAfter)) {
		print "\tTEST FAIL (bad sem count ... before:".scalar(@semaphoreListBefore)." after:".scalar(@semaphoreListAfter).")\n";
		return 0;
	}

	# Set the permission back here
	@mdata[$semPermIndex] = $oldperm;
	$statInfo = pack("l*",@mdata);
	semctl($semid, 0, IPC_SET, $statInfo) or die('Can not change permissions back');
	print "\tTEST PASS\n";
	return 1;

}


sub Test9
{
	my ($osname, $cmd, $numprocs) = @_;
	my @pidList;
	my $count = 0;
	my $waitedfor=0;
	my @memListBefore;
	my @memListAfter;
	my $ITER;
	my $ITER2;

	print "Test 9: when no permissions to access a shared memory make sure we create a new one. ";
	print "\n";

	@memListBefore = &listSysVMemory($osname);
	`$cmd`;
	@memListAfter = &listSysVMemory($osname);

	#we expect one semaphore to be created
	if (scalar(@memListBefore)+1 != scalar(@memListAfter)) {
		print "\tTEST FAIL (bad sem count)\n";
		return 0;
	}

	#find the newly created mem
	my $shmid = 0;
	BREAK_FROM_LOOP: {
		foreach $ITER (@memListAfter) {
			my $found = 0;
			foreach $ITER2 (@memListBefore) {
				if ($ITER == $ITER2) {
					$found = $ITER;
				}
			}
			if ($found == 0) {
					$shmid = $ITER;
					#this equivalent to a break statement in C ...
					last BREAK_FROM_LOOP;
			}
		}
	}
	if ($shmid == 0) {
		print "\tTEST FAIL (not found)\n";
		return 0;
	}

	my $statInfo;
	shmctl($shmid, IPC_STAT, $statInfo);
	my @mdata = unpack("l*",$statInfo);

	#set permissions to 0400 (no permissions) ...
	my $oldperm =  @mdata[$shmPermIndex];
	print "\tChange permissions from ".sprintf("O%o",$oldperm)." to ".$READONLY."\n";
    @mdata[$shmPermIndex] = $READONLY;
	$statInfo = pack("l*",@mdata);
	shmctl($shmid, IPC_SET, $statInfo);


	@memListBefore = @memListAfter;
	#print $cmd."\n";
	#return 0;
	my @stdout = `$cmd`;
	#print @stdout, "\n";
	@memListAfter = &listSysVMemory($osname);


	# No new memory should be created if current is read only
	if (scalar(@memListBefore) != scalar(@memListAfter)) {
		print "\tTEST FAIL (bad mem count ... before:".scalar(@memListBefore)." after:".scalar(@memListAfter).")\n";
		return 0;
	}

	# Set the permission back here
	@mdata[$shmPermIndex] = $oldperm;
	$statInfo = pack("l*",@mdata);
	shmctl($shmid, IPC_SET, $statInfo) or die('Can not change permissions back');

	print "\tTEST PASS\n";
	return 1;

}

sub Test10
{
	my ($osname, $cmd, $destroycmd, $numprocs) = @_;
	my @pidList;
	my $count = 0;
	my $waitedfor=0;
	my @memListBefore;
	my @memListAfter;
	my @semListBefore;
	my @semListAfter;
	my $ITER;
	my $ITER2;

	print "Test 10: Verify a simple case of destroy";
	print "\n";

	#
	# Create a cache
	#
	@memListBefore = &listSysVMemory($osname);
	@semListBefore = &listSysVSemaphores($osname);
	`$cmd`;
	@memListAfter = &listSysVMemory($osname);
	@semListAfter = &listSysVSemaphores($osname);

	if (scalar(@memListBefore)+1 != scalar(@memListAfter)) {
		print "\tTEST FAIL (bad mem count before: ".scalar(@memListBefore)." after:". scalar(@memListAfter)." )\n";
		return 0;
	}
	if (scalar(@semListBefore)+1 != scalar(@semListAfter)) {
		print "\tTEST FAIL (bad sem count before: ".scalar(@semListBefore)." after:". scalar(@semListAfter)." )\n";
		return 0;
	}
	if (&getSemaphoreCntlFile($testcachname) eq "") {
		print "\tTEST FAIL file ".&getSemaphoreCntlFile($testcachname)." not found\n";
		return 0;
	}
	if (&getMemoryCntlFile($testcachname) eq "") {
		print "\tTEST FAIL file ".&getMemoryCntlFile($testcachname)." not found\n";
		return 0;
	}

	#
	# Destroy the cache
	#
	@memListBefore = &listSysVMemory($osname);
	@semListBefore = &listSysVSemaphores($osname);
	`$destroycmd`;
	@memListAfter = &listSysVMemory($osname);
	@semListAfter = &listSysVSemaphores($osname);

	if (scalar(@memListBefore)-1 != scalar(@memListAfter)) {
		print "\tTEST FAIL (bad mem count before: ".scalar(@memListBefore)." after:". scalar(@memListAfter)." )\n";
		return 0;
	}
	if (scalar(@semListBefore)-1 != scalar(@semListAfter)) {
		print "\tTEST FAIL (bad sem count before: ".scalar(@semListBefore)." after:". scalar(@semListAfter)." )\n";
		return 0;
	}
	if (&getSemaphoreCntlFile($testcachname) ne "") {
		print "\tTEST FAIL file ".&getSemaphoreCntlFile($testcachname)." found\n";
		return 0;
	}
	if (&getMemoryCntlFile($testcachname) ne "") {
		print "\tTEST FAIL file ".&getMemoryCntlFile($testcachname)." found\n";
		return 0;
	}

	print "\tTEST PASS\n";
	return 1;
}

sub Test11
{
	my ($osname, $cmd, $destroycmd, $numprocs) = @_;
	my @pidList;
	my $count = 0;
	my $waitedfor=0;
	my @memListBefore;
	my @memListAfter;
	my @semListBefore;
	my @semListAfter;
	my $ITER;
	my $ITER2;

	print "Test 11: Call destroy with read only semaphore";
	print "\n";

	#
	# Create a cache
	#
	@memListBefore = &listSysVMemory($osname);
	@semListBefore = &listSysVSemaphores($osname);
	`$cmd`;
	@memListAfter = &listSysVMemory($osname);
	@semListAfter = &listSysVSemaphores($osname);

	if (scalar(@memListBefore)+1 != scalar(@memListAfter)) {
		print "\tTEST FAIL (bad mem count before: ".scalar(@memListBefore)." after:". scalar(@memListAfter)." )\n";
		return 0;
	}
	if (scalar(@semListBefore)+1 != scalar(@semListAfter)) {
		print "\tTEST FAIL (bad sem count before: ".scalar(@semListBefore)." after:". scalar(@semListAfter)." )\n";
		return 0;
	}
	if (&getSemaphoreCntlFile($testcachname) eq "") {
		print "\tTEST FAIL file ".&getSemaphoreCntlFile($testcachname)." not found\n";
		return 0;
	}
	if (&getMemoryCntlFile($testcachname) eq "") {
		print "\tTEST FAIL file ".&getMemoryCntlFile($testcachname)." not found\n";
		return 0;
	}


	#find the newly created semaphore
	my $semid = 0;
	BREAK_FROM_LOOP: {
		foreach $ITER (@semListAfter) {
			my $found = 0;
			foreach $ITER2 (@semListBefore) {
				if ($ITER == $ITER2) {
					$found = $ITER;
				}
			}
			if ($found == 0) {
					$semid = $ITER;
					#this equivalent to a break statement in C ...
					last BREAK_FROM_LOOP;
			}
		}
	}

	if ($semid == 0) {
		print "\tTEST FAIL (not found)\n";
		return 0;
	}

	# mod to be read only
	my $statInfo;
	semctl($semid, 0, IPC_STAT, $statInfo);
	my @mdata = unpack("l*",$statInfo);
	my $oldperm =  @mdata[$semPermIndex];
	print "\tChange permissions from ".sprintf("O%o",$oldperm)." to ".$READONLY."\n";
    @mdata[$semPermIndex] = $READONLY;
	$statInfo = pack("l*",@mdata);
	semctl($semid, 0, IPC_SET, $statInfo);

	#
	# Destroy the cache
	#
	@memListBefore = &listSysVMemory($osname);
	@semListBefore = &listSysVSemaphores($osname);
	`$destroycmd`;
	@memListAfter = &listSysVMemory($osname);
	@semListAfter = &listSysVSemaphores($osname);

	#NOTE:
	# - the vm will not create a new semaphore or shared memory
	# - the vm will fail to destroy semaphore but will remove the shared memory
	if (scalar(@memListBefore)-1 != scalar(@memListAfter)) {
		print "\tTEST FAIL (bad mem count before: ".scalar(@memListBefore)." after:". scalar(@memListAfter)." )\n";
		return 0;
	}
	if (scalar(@semListBefore) != scalar(@semListAfter)) {
		print "\tTEST FAIL (bad sem count before: ".scalar(@semListBefore)." after:". scalar(@semListAfter)." )\n";
		return 0;
	}
	if (&getSemaphoreCntlFile($testcachname) eq "") {
		print "\tTEST FAIL file ".&getSemaphoreCntlFile($testcachname)." not found\n";
		return 0;
	}
	if (&getMemoryCntlFile($testcachname) ne "") {
		print "\tTEST FAIL shared memory control file ".&getMemoryCntlFile($testcachname)." was not deleted\n";
		return 0;
	}

	# Set the permission back here
	@mdata[$semPermIndex] = $oldperm;
	$statInfo = pack("l*",@mdata);
	semctl($semid, 0, IPC_SET, $statInfo) or die('Can not change permissions back');
	print "\tTEST PASS\n";
	return 1;
}

sub Test12
{
	my ($osname, $cmd, $destroycmd, $numprocs) = @_;
	my @pidList;
	my $count = 0;
	my $waitedfor=0;
	my @memListBefore;
	my @memListAfter;
	my @semListBefore;
	my @semListAfter;
	my $ITER;
	my $ITER2;

	print "Test 12: Call destroy with read only memory";
	print "\n";

	#
	# Create a cache
	#
	@memListBefore = &listSysVMemory($osname);
	@semListBefore = &listSysVSemaphores($osname);
	`$cmd`;
	@memListAfter = &listSysVMemory($osname);
	@semListAfter = &listSysVSemaphores($osname);

	if (scalar(@memListBefore)+1 != scalar(@memListAfter)) {
		print "\tTEST FAIL (bad mem count before: ".scalar(@memListBefore)." after:". scalar(@memListAfter)." )\n";
		return 0;
	}
	if (scalar(@semListBefore)+1 != scalar(@semListAfter)) {
		print "\tTEST FAIL (bad sem count before: ".scalar(@semListBefore)." after:". scalar(@semListAfter)." )\n";
		return 0;
	}
	if (&getSemaphoreCntlFile($testcachname) eq "") {
		print "\tTEST FAIL file ".&getSemaphoreCntlFile($testcachname)." not found\n";
		return 0;
	}
	if (&getMemoryCntlFile($testcachname) eq "") {
		print "\tTEST FAIL file ".&getMemoryCntlFile($testcachname)." not found\n";
		return 0;
	}


	#find the newly created semaphore
	my $objid = 0;
	BREAK_FROM_LOOP: {
		foreach $ITER (@memListAfter) {
			my $found = 0;
			foreach $ITER2 (@memListBefore) {
				if ($ITER == $ITER2) {
					$found = $ITER;
				}
			}
			if ($found == 0) {
					$objid = $ITER;
					#this equivalent to a break statement in C ...
					last BREAK_FROM_LOOP;
			}
		}
	}

	if ($objid == 0) {
		print "\tTEST FAIL (not found)\n";
		return 0;
	}

	# mod to be read only
	my $statInfo;
	shmctl($objid, IPC_STAT, $statInfo);
	my @mdata = unpack("l*",$statInfo);
	my $oldperm =  @mdata[$shmPermIndex];
	print "\tChange permissions from ".sprintf("O%o",$oldperm)." to ".$READONLY."\n";
    @mdata[$shmPermIndex] = $READONLY;
	$statInfo = pack("l*",@mdata);
	shmctl($objid, IPC_SET, $statInfo);

	#
	# Destroy the cache
	#
	@memListBefore = &listSysVMemory($osname);
	@semListBefore = &listSysVSemaphores($osname);
	print "\t".$destroycmd."\n";
	`$destroycmd`;
	@memListAfter = &listSysVMemory($osname);
	@semListAfter = &listSysVSemaphores($osname);

	#NOTE:
	# -
	if (scalar(@memListBefore) != scalar(@memListAfter)) {
		print "\tTEST FAIL (bad mem count before: ".scalar(@memListBefore)." after:". scalar(@memListAfter)." )\n";
		return 0;
	}
	if (scalar(@semListBefore)-1 != scalar(@semListAfter)) {
		print "\tTEST FAIL (bad sem count before: ".scalar(@semListBefore)." after:". scalar(@semListAfter)." )\n";
		return 0;
	}
	if (&getSemaphoreCntlFile($testcachname) ne "") {
		print "\tTEST FAIL file ".&getSemaphoreCntlFile($testcachname)." found\n";
		return 0;
	}
	if (&getMemoryCntlFile($testcachname) eq "") {
		print "\tTEST FAIL file ".&getMemoryCntlFile($testcachname)." not found\n";
		return 0;
	}

	# Set the permission back here
	@mdata[$shmPermIndex] = $oldperm;
	$statInfo = pack("l*",@mdata);
	shmctl($objid, IPC_SET, $statInfo) or die('Can not change permissions back');
	print "\tTEST PASS\n";
	return 1;
}

sub Test13
{
	my ($osname, $cmd, $destroycmd, $numprocs) = @_;
	my @pidList;
	my $count = 0;
	my $waitedfor=0;
	my @memListBefore;
	my @memListAfter;
	my @semListBefore;
	my @semListAfter;
	my $ITER;
	my $ITER2;
	my $chmodcmd;

	print "Test 13: Verify a simple case of destroy with readonly control files";
	print "\n";

	#
	# Create a cache
	#
	@memListBefore = &listSysVMemory($osname);
	@semListBefore = &listSysVSemaphores($osname);
	`$cmd`;
	@memListAfter = &listSysVMemory($osname);
	@semListAfter = &listSysVSemaphores($osname);

	if (scalar(@memListBefore)+1 != scalar(@memListAfter)) {
		print "\tTEST FAIL (bad mem count before: ".scalar(@memListBefore)." after:". scalar(@memListAfter)." )\n";
		return 0;
	}
	if (scalar(@semListBefore)+1 != scalar(@semListAfter)) {
		print "\tTEST FAIL (bad sem count before: ".scalar(@semListBefore)." after:". scalar(@semListAfter)." )\n";
		return 0;
	}
	if (&getSemaphoreCntlFile($testcachname) eq "") {
		print "\tTEST FAIL file ".&getSemaphoreCntlFile($testcachname)." not found\n";
		return 0;
	}
	if (&getMemoryCntlFile($testcachname) eq "") {
		print "\tTEST FAIL file ".&getMemoryCntlFile($testcachname)." not found\n";
		return 0;
	}


	my $semfile = &getSemaphoreCntlFile($testcachname);
	my $memfile = &getMemoryCntlFile($testcachname);

	$chmodcmd = "chmod a-wx ".$semfile;
	`$chmodcmd`;
	$chmodcmd = "chmod a-wx ".$memfile;
	`$chmodcmd`;
	#system("ls -la /tmp/javasharedresources/");

	#
	# Destroy the cache
	#
	@memListBefore = &listSysVMemory($osname);
	@semListBefore = &listSysVSemaphores($osname);
	`$destroycmd`;
	@memListAfter = &listSysVMemory($osname);
	@semListAfter = &listSysVSemaphores($osname);

	#
	# Destroy will kill control files regardless if they are readonly
	#
	if (scalar(@memListBefore)-1 != scalar(@memListAfter)) {
		print "\tTEST FAIL (bad mem count before: ".scalar(@memListBefore)." after:". scalar(@memListAfter)." )\n";
		return 0;
	}
	if (scalar(@semListBefore)-1 != scalar(@semListAfter)) {
		print "\tTEST FAIL (bad sem count before: ".scalar(@semListBefore)." after:". scalar(@semListAfter)." )\n";
		return 0;
	}
	if (&getSemaphoreCntlFile($testcachname) eq "") {
		print "\tTEST FAIL file ".&getSemaphoreCntlFile($testcachname)." not found\n";
		return 0;
	}
	if (&getMemoryCntlFile($testcachname) eq "") {
		print "\tTEST FAIL file ".&getMemoryCntlFile($testcachname)." not found\n";
		return 0;
	}

	$cmd = "unlink ".$semfile;
	`$cmd`;
	$cmd = "unlink ".$memfile;
	`$cmd`;

	print "\tTEST PASS\n";
	return 1;
}


sub Test14
{
	my ($osname, $cmd, $numprocs) = @_;
	my @pidList;
	my $count = 0;
	my $waitedfor=0;
	my @memListBefore;
	my @memListAfter;
	my @semListBefore;
	my @semListAfter;
	my $ITER;
	my $ITER2;
	my $chmodcmd;

	print "Test 14: Verify a simple case of start up with readonly control files, readonly memory";
	print "\n";

	#
	# Create a cache
	#
	@memListBefore = &listSysVMemory($osname);
	@semListBefore = &listSysVSemaphores($osname);
	`$cmd`;
	@memListAfter = &listSysVMemory($osname);
	@semListAfter = &listSysVSemaphores($osname);

	if (scalar(@memListBefore)+1 != scalar(@memListAfter)) {
		print "\tTEST FAIL (bad mem count before: ".scalar(@memListBefore)." after:". scalar(@memListAfter)." )\n";
		return 0;
	}
	if (scalar(@semListBefore)+1 != scalar(@semListAfter)) {
		print "\tTEST FAIL (bad sem count before: ".scalar(@semListBefore)." after:". scalar(@semListAfter)." )\n";
		return 0;
	}
	if (&getSemaphoreCntlFile($testcachname) eq "") {
		print "\tTEST FAIL file ".&getSemaphoreCntlFile($testcachname)." not found\n";
		return 0;
	}
	if (&getMemoryCntlFile($testcachname) eq "") {
		print "\tTEST FAIL file ".&getMemoryCntlFile($testcachname)." not found\n";
		return 0;
	}

	#find the newly created shared memory
	my $objid = 0;
	BREAK_FROM_LOOP: {
		foreach $ITER (@memListAfter) {
			my $found = 0;
			foreach $ITER2 (@memListBefore) {
				if ($ITER == $ITER2) {
					$found = $ITER;
				}
			}
			if ($found == 0) {
					$objid = $ITER;
					#this equivalent to a break statement in C ...
					last BREAK_FROM_LOOP;
			}
		}
	}

	if ($objid == 0) {
		print "\tTEST FAIL (not found)\n";
		return 0;
	}

	# mod to be read only
	my $statInfo;
	shmctl($objid, IPC_STAT, $statInfo);
	my @mdata = unpack("l*",$statInfo);
	my $oldperm =  @mdata[$shmPermIndex];
	print "\tChange permissions from ".sprintf("O%o",$oldperm)." to ".$READONLY."\n";
    @mdata[$shmPermIndex] = $READONLY;
	$statInfo = pack("l*",@mdata);
	shmctl($objid, IPC_SET, $statInfo);

	my $semfile = &getSemaphoreCntlFile($testcachname);
	my $memfile = &getMemoryCntlFile($testcachname);

	$chmodcmd = "chmod a-wx ".$semfile;
	`$chmodcmd`;
	$chmodcmd = "chmod a-wx ".$memfile;
	`$chmodcmd`;
	#system("ls -la /tmp/javasharedresources/");

	#
	# Open the cache
	#
	@memListBefore = &listSysVMemory($osname);
	@semListBefore = &listSysVSemaphores($osname);
	`$cmd`;
	@memListAfter = &listSysVMemory($osname);
	@semListAfter = &listSysVSemaphores($osname);

	#
	# Open of the cache will cause 'permission denied' on control file and memory ...
	# OSCache.cpp will then attempt to delete it ... only the semaphore will be deleted
	#
	if (scalar(@memListBefore) != scalar(@memListAfter)) {
		print "\tTEST FAIL (bad mem count before: ".scalar(@memListBefore)." after:". scalar(@memListAfter)." )\n";
		return 0;
	}
	if (scalar(@semListBefore)-1 != scalar(@semListAfter)) {
		print "\tTEST FAIL (bad sem count before: ".scalar(@semListBefore)." after:". scalar(@semListAfter)." )\n";
		return 0;
	}
	if (&getSemaphoreCntlFile($testcachname) eq "") {
		print "\tTEST FAIL file ".&getSemaphoreCntlFile($testcachname)." not found\n";
		return 0;
	}
	if (&getMemoryCntlFile($testcachname) eq "") {
		print "\tTEST FAIL file ".&getMemoryCntlFile($testcachname)." not found\n";
		return 0;
	}

	# Set the permission back here
	@mdata[$shmPermIndex] = $oldperm;
	$statInfo = pack("l*",@mdata);
	shmctl($objid, IPC_SET, $statInfo) or die('Can not change permissions back');
	$chmodcmd = "chmod a+w ".$semfile;
	`$chmodcmd`;
	$chmodcmd = "chmod a+w ".$memfile;
	`$chmodcmd`;

	$cmd = "unlink ".$semfile;
	`$cmd`;
	$cmd = "unlink ".$memfile;
	`$cmd`;

	print "\tTEST PASS\n";
	return 1;
}

sub Test15
{
	my ($osname, $cmd, $cmd2, $numprocs) = @_;
	my @pidList;
	my $count = 0;
	my $waitedfor=0;
	my @memListBefore;
	my @memListAfter;
	my @semListBefore;
	my @semListAfter;
	my $ITER;
	my $ITER2;
	my $chmodcmd;

	print "Test 15: Same test as 14 ... but we run 2nd command with non fatal and check stdout/err for dump of vm version info ...";
	print "\n";

	#
	# Create a cache
	#
	@memListBefore = &listSysVMemory($osname);
	@semListBefore = &listSysVSemaphores($osname);
	`$cmd`;
	@memListAfter = &listSysVMemory($osname);
	@semListAfter = &listSysVSemaphores($osname);

	if (scalar(@memListBefore)+1 != scalar(@memListAfter)) {
		print "\tTEST FAIL (bad mem count before: ".scalar(@memListBefore)." after:". scalar(@memListAfter)." )\n";
		return 0;
	}
	if (scalar(@semListBefore)+1 != scalar(@semListAfter)) {
		print "\tTEST FAIL (bad sem count before: ".scalar(@semListBefore)." after:". scalar(@semListAfter)." )\n";
		return 0;
	}
	if (&getSemaphoreCntlFile($testcachname) eq "") {
		print "\tTEST FAIL file ".&getSemaphoreCntlFile($testcachname)." not found\n";
		return 0;
	}
	if (&getMemoryCntlFile($testcachname) eq "") {
		print "\tTEST FAIL file ".&getMemoryCntlFile($testcachname)." not found\n";
		return 0;
	}

	#find the newly created semaphore
	my $objid = 0;
	BREAK_FROM_LOOP: {
		foreach $ITER (@memListAfter) {
			my $found = 0;
			foreach $ITER2 (@memListBefore) {
				if ($ITER == $ITER2) {
					$found = $ITER;
				}
			}
			if ($found == 0) {
					$objid = $ITER;
					#this equivalent to a break statement in C ...
					last BREAK_FROM_LOOP;
			}
		}
	}

	if ($objid == 0) {
		print "\tTEST FAIL (not found)\n";
		return 0;
	}

	# mod to be read only
	my $statInfo;
	shmctl($objid, IPC_STAT, $statInfo);
	my @mdata = unpack("l*",$statInfo);
	my $oldperm =  @mdata[$shmPermIndex];
	print "\tChange permissions from ".sprintf("O%o",$oldperm)." to ".$READONLY."\n";
    @mdata[$shmPermIndex] = $READONLY;
	$statInfo = pack("l*",@mdata);
	shmctl($objid, IPC_SET, $statInfo);

	my $semfile = &getSemaphoreCntlFile($testcachname);
	my $memfile = &getMemoryCntlFile($testcachname);

	$chmodcmd = "chmod a-wx ".$semfile;
	`$chmodcmd`;
	$chmodcmd = "chmod a-wx ".$memfile;
	`$chmodcmd`;
	#system("ls -la /tmp/javasharedresources/");

	#
	# Open the cache
	#
	#system("ipcs");
	@memListBefore = &listSysVMemory($osname);
	@semListBefore = &listSysVSemaphores($osname);
	#print $cmd2."\n";
	my @stdout = `$cmd2`;
	#print @stdout,"\n";
	#system("ipcs");
	@memListAfter = &listSysVMemory($osname);
	@semListAfter = &listSysVSemaphores($osname);

	my $found = 0;
	foreach $ITER (@stdout) {
		if ($ITER =~ /java version/) {
			$found = 1;
		}
	}
	if ($found == 0) {
		print "\tTEST FAIL success string not found in output\n";
		return 0;
	}

	#
	# Open of the cache will cause 'permission denied' on control file and memory ...
	# OSCache.cpp will then attempt to delete it ... only the semaphore will be deleted
	#
	if (scalar(@memListBefore) != scalar(@memListAfter)) {
		print "\tTEST FAIL(2) (bad mem count before: ".scalar(@memListBefore)." after:". scalar(@memListAfter)." )\n";
		return 0;
	}
	#not minus one b/c retries readonly with nonfatal
	if (scalar(@semListBefore) != scalar(@semListAfter)) {
		print "\tTEST FAIL(2) (bad sem count before: ".scalar(@semListBefore)." after:". scalar(@semListAfter)." )\n";
		return 0;
	}
	if (&getSemaphoreCntlFile($testcachname) eq "") {
		print "\tTEST FAIL(2) file ".&getSemaphoreCntlFile($testcachname)." not found\n";
		return 0;
	}
	if (&getMemoryCntlFile($testcachname) eq "") {
		print "\tTEST FAIL(2) file ".&getMemoryCntlFile($testcachname)." not found\n";
		return 0;
	}

	# Set the permission back here
	@mdata[$shmPermIndex] = $oldperm;
	$statInfo = pack("l*",@mdata);
	shmctl($objid, IPC_SET, $statInfo) or die('Can not change permissions back');
	#semaphore control file should be deleted
	#$chmodcmd = "chmod a+w ".$semfile;
	#`$chmodcmd`;
	$chmodcmd = "chmod a+w ".$memfile;
	`$chmodcmd`;

	$cmd = "unlink ".$semfile;
	`$cmd`;
	$cmd = "unlink ".$memfile;
	`$cmd`;

	print "\tTEST PASS\n";
	return 1;
}

sub Test16
{
	my ($osname, $cmd, $numprocs) = @_;
	my @pidList;
	my $count = 0;
	my $waitedfor=0;
	my $chmodcmd;


	print "Test 16: Same as test 7 but with multiple threads (its a tinch random).\n";
	`$cmd`;
	my $semfile = &getSemaphoreCntlFile($testcachname);
	my $memfile = &getMemoryCntlFile($testcachname);

	&cleanupSysV($osname,$verbose);

	my $semCount = scalar(&listSysVSemaphores($osname));
	my $memCount = scalar(&listSysVMemory($osname));

	for ($count = 0; $count < $numprocs; $count++) {
		$chmodcmd = "chmod a-wx ".$semfile . " 2>&1";
		`$chmodcmd`;
		$chmodcmd = "chmod a-wx ".$memfile . " 2>&1";
		`$chmodcmd`;
		&cleanupSysV($osname,$verbose);
		my $pid = &forkAndRunInBackGround($cmd);
		push(@pidList, $pid);
	}

	while (wait() != -1) {
		$waitedfor = $waitedfor + 1;
	}
	printf("\tWe succesfully ran " . $waitedfor . " JVMs with shared classes.\n");

	#$chmodcmd = "chmod ug+w ".$semfile;
	#`$chmodcmd`;
	#$chmodcmd = "chmod ug+w ".$memfile;
	#`$chmodcmd`;

	#we can't create new sysv obj if cntrl file is readonly
	if ($memCount != scalar(&listSysVMemory($osname))) {
		print "\tTEST FAIL mem count ".$memCount." vs ".scalar(&listSysVMemory($osname))."\n";
		return 0;
	}
	if ($semCount != scalar(&listSysVSemaphores($osname))) {
		print "\tTEST FAIL sem count ".$semCount." vs ".scalar(&listSysVSemaphores($osname))."\n";
		return 0;
	}

	$cmd = "unlink ".$semfile;
	`$cmd`;
	$cmd = "unlink ".$memfile;
	`$cmd`;

	print "\tTEST PASS (we wrote a new control file)\n";
	return 1;

}

sub Test17
{
	my ($osname, $javabin, $numprocs) = @_;
	my @pidList;
	my $count = 0;
	my $waitedfor=0;
	my $cachename = "testSCSysVTest17num";

	print "Test 17: Run ".$numprocs." JVMs and create same number of caches. The try again and make sure no new ones show up\n";

	&cleanupSysV($osname,$verbose);

	for ($count = 0; $count < $numprocs; $count++) {
		my $cmd = $javabin . " -Xshareclasses:nonpersistent,name=".$cachename.$count." -version ". " 2>&1";;
		my $pid = &forkAndRunInBackGround($cmd);
		push(@pidList, $pid);
	}

	while (wait() != -1) {
		$waitedfor = $waitedfor + 1;
	}
	printf("\tWe succesfully ran " . $waitedfor . " JVMs with shared classes.\n");

	my @memList = &listSysVMemory($osname);
	my @semList = &listSysVSemaphores($osname);

	if (scalar(@memList) == 0) {
		print "\tTEST FAIL mem count ".scalar(@memList)." vs 0"."\n";
		return 0;
	}
	if (scalar(@semList) == 0) {
		print "\tTEST FAIL sem count ".scalar(@semList)." vs 0"."\n";
		return 0;
	}

	for ($count = 0; $count < $numprocs; $count++) {
		my $cmd = $javabin . " -Xshareclasses:nonpersistent,name=".$cachename.$count." -version ". " 2>&1";
		#print $cmd."\n";
		my $pid = &forkAndRunInBackGround($cmd);
		push(@pidList, $pid);
	}

	$waitedfor = 0;
	while (wait() != -1) {
		$waitedfor = $waitedfor + 1;
	}
	printf("\tWe succesfully ran " . $waitedfor . " JVMs with shared classes.\n");

	my $cleanupCmd = "rm -f /tmp/javasharedresources/*".$cachename."*";
	`$cleanupCmd`;

	if (scalar(@memList) != scalar(&listSysVMemory($osname))) {
		print "\tTEST FAIL mem count ".scalar(@memList)." vs ".scalar(&listSysVMemory($osname))."\n";
		return 0;
	}

	if (scalar(@semList) != scalar(&listSysVSemaphores($osname))) {
		print "\tTEST FAIL sem count ".scalar(@semList)." vs ".scalar(&listSysVSemaphores($osname))."\n";
		return 0;
	}

	print "\tTEST PASS\n";
	print "\t\tmem count ".scalar(@memList)."\n";
	print "\t\tsem count ".scalar(@semList)."\n";

	return 1;

}

sub Test18
{
	my ($osname, $javabin, $numprocs) = @_;
	my @pidList;
	my $count = 0;
	my $waitedfor=0;
	my $cachename = "testSCSysVTest18num";

	print "Test 18: Run ".$numprocs." JVMs and create same number of caches. Delete the control files and try again. \n";

	&cleanupSysV($osname,$verbose);

	for ($count = 0; $count < $numprocs; $count++) {
		my $cmd = $javabin . " -Xshareclasses:nonpersistent,name=".$cachename.$count." -version ". " 2>&1";;
		my $pid = &forkAndRunInBackGround($cmd);
		push(@pidList, $pid);
	}

	while (wait() != -1) {
		$waitedfor = $waitedfor + 1;
	}
	printf("\tWe succesfully ran " . $waitedfor . " JVMs with shared classes.\n");

	#my @stdout = `ipcs`;
	#print @stdout,"\n";

	my @memList = &listSysVMemory($osname);
	my @semList = &listSysVSemaphores($osname);
	my $cleanupCmd = "rm -f /tmp/javasharedresources/*".$cachename."*";
	`$cleanupCmd`;

	for ($count = 0; $count < $numprocs; $count++) {
		my $cmd = $javabin . " -Xshareclasses:nonpersistent,name=".$cachename.$count." -version ". " 2>&1";;
		my $pid = &forkAndRunInBackGround($cmd);
		push(@pidList, $pid);
		#print $cmd."\n";
		#my @stdout = `$cmd`;
		#print @stdout,"\n";

	}

	$waitedfor = 0;
	while (wait() != -1) {
		$waitedfor = $waitedfor + 1;
	}
	printf("\tWe succesfully ran " . $waitedfor . " JVMs with shared classes.\n");

	#my @stdout = `ipcs`;
	#print @stdout,"\n";

	my $cleanupCmd = "rm -f /tmp/javasharedresources/*".$cachename."*";
	`$cleanupCmd`;

	if ((scalar(@memList)+$numprocs) != scalar(&listSysVMemory($osname))) {
		print "\tTEST FAIL mem count ".scalar(@memList)." vs ".scalar(&listSysVMemory($osname))."\n";
		return 0;
	}

	if ((scalar(@semList)+$numprocs) != scalar(&listSysVSemaphores($osname))) {
		print "\tTEST FAIL sem count ".scalar(@semList)." vs ".scalar(&listSysVSemaphores($osname))."\n";
		return 0;
	}

	print "\tTEST PASS\n";
	print "\t\tmem count ".scalar(&listSysVMemory($osname))."\n";
	print "\t\tsem count ".scalar(&listSysVSemaphores($osname))."\n";

	return 1;

}

sub Test19
{
	my ($osname, $cmd, $numprocs) = @_;
	my @pidList;
	my $count = 0;
	my $waitedfor=0;
	my $touchcmd;


	print "Test 19: Test non empty corrupted control files.\n";
	`$cmd`;
	my $semfile = &getSemaphoreCntlFile($testcachname);
	my $memfile = &getMemoryCntlFile($testcachname);
	&removeCntrlFiles($testcachname);

	$touchcmd = "echo \"abcdefghijklmnopqrstuvwxyz\" > ".$semfile;
	`$touchcmd`;
	$touchcmd = "echo \"abcdefghijklmnopqrstuvwxyz\" > ".$memfile;
	`$touchcmd`;

	my $semCount = scalar(&listSysVSemaphores($osname));
	my $memCount = scalar(&listSysVMemory($osname));
	#printf $cmd."\n";
	my @stdout = `$cmd`;
	#printf @stdout,"\n";
	if ($memCount == scalar(&listSysVMemory($osname))) {
		print "\tTEST FAIL mem count ".$memCount."\n";
		return 0;
	}
	if ($semCount == scalar(&listSysVSemaphores($osname))) {
		print "\tTEST FAIL sem count ".$semCount." vs ".scalar(&listSysVSemaphores($osname))."\n";
		return 0;
	}
	print "\tTEST PASS\n";
	return 1;

}

sub Test20
{
	my ($osname, $javabin, $numprocs) = @_;
	my @pidList;
	my $count = 0;
	my $waitedfor=0;
	my $cachename = "testSCSysVTest20num";

	print "Test 20: Run ".$numprocs." JVMs and create same number of caches. Delete the ipcs objs and try again. \n";

	&cleanupSysV($osname,$verbose);

	for ($count = 0; $count < $numprocs; $count++) {
		my $cmd = $javabin . " -Xshareclasses:nonpersistent,name=".$cachename.$count." -version &> ./out.txt";
		my $cmd = $javabin . " -Xshareclasses:nonpersistent,name=".$cachename.$count." -version";
		my $pid = &forkAndRunInBackGround($cmd);
		push(@pidList, $pid);
	}

	while (wait() != -1) {
		$waitedfor = $waitedfor + 1;
	}
	printf("\tWe succesfully ran " . $waitedfor . " JVMs with shared classes.\n");

	my @memList = &listSysVMemory($osname);
	my @semList = &listSysVSemaphores($osname);
	&cleanupSysV($osname,$verbose);


	for ($count = 0; $count < $numprocs; $count++) {
		my $cmd = $javabin . " -Xshareclasses:nonpersistent,name=".$cachename.$count." -version &> ./out.txt";
		my $pid = &forkAndRunInBackGround($cmd);
		push(@pidList, $pid);
	}

	$waitedfor = 0;
	while (wait() != -1) {
		$waitedfor = $waitedfor + 1;
	}
	printf("\tWe succesfully ran " . $waitedfor . " JVMs with shared classes.\n");

	my $cleanupCmd = "rm -f /tmp/javasharedresources/*".$cachename."*";
	`$cleanupCmd`;

	if (scalar(@memList) != scalar(&listSysVMemory($osname))) {
		print "\tTEST FAIL mem count ".scalar(@memList)." vs ".scalar(&listSysVMemory($osname))."\n";
		return 0;
	}

	if (scalar(@semList) != scalar(&listSysVSemaphores($osname))) {
		print "\tTEST FAIL sem count ".scalar(@semList)." vs ".scalar(&listSysVSemaphores($osname))."\n";
		return 0;
	}

	print "\tTEST PASS\n";
	print "\t\tmem count ".scalar(&listSysVMemory($osname))."\n";
	print "\t\tsem count ".scalar(&listSysVSemaphores($osname))."\n";

	return 1;

}

sub Test21
{
	my ($osname, $cmd, $numprocs) = @_;
	my @pidList;
	my $count = 0;
	my $waitedfor=0;
	my $chmodcmd;
	my $ITER;

	print "Test 21: Check semid in memory is valid ... it should not be!\n";
	`$cmd`;
	my $semfile = &getSemaphoreCntlFile($testcachname);
	my $memfile = &getMemoryCntlFile($testcachname);

	&cleanupSysVSem($osname,$verbose);

	my @stdout = `$cmd`;
	#print @stdout,"\n";

	my $found = 0;
	foreach $ITER (@stdout) {
		if ($ITER =~ /different semaphore/) {
			$found = 1;
		}
	}
	if ($found == 0) {
		print "\tTEST FAIL success string not found in output\n";
		return 0;
	}

	my $semCount = scalar(&listSysVSemaphores($osname));
	my $memCount = scalar(&listSysVMemory($osname));

	# if semid mismatch is found the shared memory is destroyed ...
	if ($memCount != scalar(&listSysVMemory($osname))) {
		print "\tTEST FAIL mem count ".$memCount." vs ".scalar(&listSysVMemory($osname))."\n";
		return 0;
	}
	if ($semCount != scalar(&listSysVSemaphores($osname))) {
		print "\tTEST FAIL sem count ".$semCount." vs ".scalar(&listSysVSemaphores($osname))."\n";
		return 0;
	}
	print "\tTEST PASS\n";
	return 1;
}

sub Test22
{
	my ($osname, $cmd) = @_;
	my $chmodcmd;

	print "Test 22: Check that JVM removes read-only control files having modlevel such that major level=0 and minor level=1.\n";
	`$cmd`;
	my $semfile = &getSemaphoreCntlFile($testcachname);
	my $memfile = &getMemoryCntlFile($testcachname);

	&cleanupSysVSem($osname,$verbose);
	&cleanupSysVMem($osname,$verbose);

	my $semCount = scalar(&listSysVSemaphores($osname));
	my $memCount = scalar(&listSysVMemory($osname));

	# update control files such that major level=0 and minor level=1
	&updateSemControlFileModLevel($semfile);
	&updateShmControlFileModLevel($memfile);

	# make control files read-only
	$chmodcmd = "chmod a-wx ".$semfile;
	`$chmodcmd`;
	$chmodcmd = "chmod a-wx ".$memfile;
	`$chmodcmd`;

	# Using read-only control files having major level=0 and minor level=1 without any SysV objects
	# will cause JVM to remove the control files and recreate new shared class cache.
	`$cmd`;

	if ($memCount+1 != scalar(&listSysVMemory($osname))) {
		print "\tTEST FAIL mem count expected ".($memCount+1)." vs found ".scalar(&listSysVMemory($osname))."\n";
		return 0;
	}
	if ($semCount+1 != scalar(&listSysVSemaphores($osname))) {
		print "\tTEST FAIL sem count expected ".($semCount+1)." vs found ".scalar(&listSysVSemaphores($osname))."\n";
		return 0;
	}

	`$destroycmd`;

	print "\tTEST PASS\n";
	return 1;
}

#-----------------------------------------------------
# listFilesInFolder()
#-----------------------------------------------------
sub listFilesInFolder
{
	my ($folder) = @_;
	my @FILES;
	opendir(DIR, $folder) or return @FILES;
	@FILES= readdir(DIR);
	closedir(DIR) or die ("can not close dir");
	return @FILES;
}

#-----------------------------------------------------
# forkAndRunInBackGround()
#-----------------------------------------------------
sub forkAndRunInBackGround
{
	my ($cmd) = @_;
	my $child_pid;
	if (!defined($child_pid = fork())) {
    	die "cannot fork: $!";
	} elsif ($child_pid) {
    	return $child_pid;
	} else {
    	`$cmd`;
    	exit 0;
	}
}

#-----------------------------------------------------
# cleanupSysV()
#-----------------------------------------------------
sub cleanupSysV
{
	my ($osname, $verbose) = @_;
	&cleanupSysVSem($osname,$verbose);
	&cleanupSysVMem($osname,$verbose);
}

#-----------------------------------------------------
# cleanupSysV()
#-----------------------------------------------------
sub cleanupSysVSem
{
	my ($osname,$verbose) = @_;
	my @semaphoreList;
	my @memoryList;
	my $ITER;

	@semaphoreList = &listSysVSemaphores($osname);
	if ($verbose == 1) {
		print "Semaphores Being Removed: ".scalar(@semaphoreList)."\n";
	}
	foreach $ITER (@semaphoreList) {
		my $cmd = "ipcrm -s " . $ITER . " 2>&1";
		if ($verbose == 1) {
			print "\t".$cmd."\n";
		}
		`$cmd`;
	}
}

#-----------------------------------------------------
# cleanupSysV()
#-----------------------------------------------------
sub cleanupSysVMem
{
	my ($osname,$verbose) = @_;
	my @memoryList;
	my $ITER;

	@memoryList = &listSysVMemory($osname);
	if ($verbose == 1) {
		print "Shared Memory  Being Removed: ".scalar(@memoryList)."\n";
	}
	foreach $ITER (@memoryList) {
		my $cmd = "ipcrm -m " . $ITER . " 2>&1";
		if ($verbose == 1) {
			print "\t".$cmd."\n";
		}
		`$cmd`;
	}
}

#-----------------------------------------------------
# listSysVSemaphores()
#-----------------------------------------------------
sub listSysVSemaphores
{
	my $osname = $_[0];
	my @retval;
	my $semaphoreProjid = "(0x81|0x81|0x82|0x83|0x84|0x85|0x86|0x87|0x88|0x89|0x8a|0x8b|0x8c|0x8d|0x8e|0x8f|0x90|0x91|0x92|0x93|0x94)";
	my $memoryProjid = "(0x61|0x61|0x62|0x63|0x64|0x65|0x66|0x67|0x68|0x69|0x6a|0x6b|0x6c|0x6d|0x6e|0x6f|0x70|0x71|0x72|0x73|0x74)";
	my $keyindex = 0;
	my $idindex = 1;
	my $ownerindex = 2;

	my @stdoutvar = ();
	my $iter = "";
	my $foundSemaphoreHeader = 0;
	my $foundMemoryHeader = 0;

	my $memsremoved=0;
	my $semsremoved=0;

	if ($osname =~ /zos/)
	{
		$keyindex=2;
		$idindex=1;
		$ownerindex=4;
	}

	if ($osname =~ /aix/)
	{
		$keyindex=2;
		$idindex=1;
		$ownerindex=4;
	}


	#
	# GET LIST OF SEMAPHORES:
	# - Call ipcs and store stdout in a local variable.
	# - If the command fails we exit ASAP
	#
	@stdoutvar = `ipcs -s` or die("Error: could not execute ipcs!");


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
			push(@retval,$id);
		}
	}
	return @retval;
}


#-----------------------------------------------------
# listSysVMemory()
#-----------------------------------------------------
sub listSysVMemory
{
	my $osname = $_[0];
	my @retval;
	my $semaphoreProjid = "(0x81|0x81|0x82|0x83|0x84|0x85|0x86|0x87|0x88|0x89|0x8a|0x8b|0x8c|0x8d|0x8e|0x8f|0x90|0x91|0x92|0x93|0x94)";
	my $memoryProjid = "(0x61|0x61|0x62|0x63|0x64|0x65|0x66|0x67|0x68|0x69|0x6a|0x6b|0x6c|0x6d|0x6e|0x6f|0x70|0x71|0x72|0x73|0x74)";

	my $keyindex = 0;
	my $idindex = 1;
	my $ownerindex = 2;

	my @stdoutvar = ();
	my $iter = "";
	my $foundHeader = 0;

	my $memsremoved=0;
	my $semsremoved=0;

	if ($osname =~ /zos/)
	{
		$keyindex=2;
		$idindex=1;
		$ownerindex=4;
	}

	if ($osname =~ /aix/)
	{
		$keyindex=2;
		$idindex=1;
		$ownerindex=4;
	}


	#
	# GET LIST OF MEMS:
	# - Call ipcs and store stdout in a local variable.
	# - If the command fails we exit ASAP
	#
	@stdoutvar = `ipcs -m` or die("Error: could not execute ipcs!");


	$iter = "";
	$foundHeader = 0;
	foreach $iter (@stdoutvar) {

		if ($iter =~ /Memory/) {
			$foundHeader=1;
		}
		if (($iter =~ /$memoryProjid/) && ($foundHeader==1)) {
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
			push(@retval,$id);
		}
	}
	return @retval;
}

sub getIPCObjPermIndex
{
	my $osname = $_[0];
	if ($osname =~ /zos/)
	{
		return 4;
	}

	if ($osname =~ /aix/)
	{
		return 4;
	}
	return 5;
}

#-----------------------------------------------------
# updateSemControlFileModLevel() - updates the modlevel of control file to major level=0 and minor level=1.
# Such readonly control files can be unlinked by JVM which creates control files with higher modlevel.
#-----------------------------------------------------
sub updateSemControlFileModLevel
{
	my ($controlFile) = @_;
	my $modlevel;
	my $oldMajorLevel = 0;
	my $oldMinorLevel = 1;
	my $modLevelOffset = 4;	# modlevel in semaphore control file is at offset 4

	open my $fh, '+<', $controlFile or die("failed to open semaphore control file".$controlFile."\n");

	$modlevel = ($oldMajorLevel << 16) | $oldMinorLevel;
	$modlevel = pack("I", $modlevel);
	sysseek($fh, $modLevelOffset, SEEK_SET);
	syswrite($fh, $modlevel) == 4 or die("failed to update semaphore control file".$controlFile."\n");
	close $fh;
	return 0;
}

#-----------------------------------------------------
# updateShmControlFileModLevel() - updates the modlevel of control file to major level=0 and minor level=1.
# Such readonly control files can be unlinked by JVM which creates control files with higher modlevel.
#-----------------------------------------------------
sub updateShmControlFileModLevel
{
	my ($controlFile) = @_;
	my $modlevel;
	my $oldMajorLevel = 0;
	my $oldMinorLevel = 1;
	my $modLevelOffset = 4;	# modlevel in shared memory control file is at offset 4
	my $majorLevelMask = 0xFFFF0000;

	open my $fh, '+<', $controlFile or die("failed to open shared memory control file".$controlFile."\n");

	$modlevel = ($oldMajorLevel << 16) | $oldMinorLevel;
	$modlevel = pack("I", $modlevel);
	sysseek($fh, $modLevelOffset, SEEK_SET);
	syswrite($fh, $modlevel) == 4 or die("failed to update shared memory control file".$controlFile."\n");
	close $fh;
	return 0;
}
