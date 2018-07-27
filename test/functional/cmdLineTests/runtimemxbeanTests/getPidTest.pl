#!/usr/bin/perl

##############################################################################
#  Copyright (c) 2017, 2018 IBM Corp. and others
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
use warnings;
use IPC::Open3;

my $javaCmd = join(' ', @ARGV);

my ($in, $out, $err);

my $perlPid;

my $javaPid;

if ($^O eq 'cygwin') {
	open3($in, $out, $err, $javaCmd);
	$javaPid = <$out>;
	# Command gets the list of PIDs of the Java processes system is running
	$perlPid = `wmic process where "name='java.exe'" get ProcessID`;
	print $in "getPid finished";
	# String trim both sides of javaPid and perlPid for the index check
	$javaPid =~ s/^\s+|\s+$//g;
	$perlPid =~ s/^\s+|\s+$//g;
	# After trimming, check if javaPid is in the list of PIDs system returns
	# index() would return an index if it is in the list, return -1 otherwise
	# (PASS if not -1)
	if (index($perlPid, $javaPid) != -1) {
		$perlPid = $javaPid;
	}
} else {
	$perlPid = open3($in, $out, $err, $javaCmd);
	$javaPid = <$out>;
	print $in "getPid finished";
}

if ($perlPid == $javaPid) {
	print "PASS: RuntimeMXBean.getPid() returned correct PID.";
}
else {
	print "FAIL: RuntimeMXBean.getPID() returned ${javaPid} instead of ${perlPid}";
}
