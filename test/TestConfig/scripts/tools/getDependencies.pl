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
#  SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
##############################################################################

use strict;
use warnings;
use Getopt::Long;
use File::Spec;
use File::Path qw(make_path);


#define test src root path
my $path;
#define task
my $task = "default";

GetOptions ("path=s"	=>	\$path,
			"task=s"	=>	\$task)
or die("Error in command line arguments\n");

if (not defined $path) {
	die "ERROR: Download path not defined! \n"
}

if ( ! -d $path ) {
	print "$path does not exist, creating one. \n";
	my $isCreated = make_path($path, {chmod => 0777, verbose => 1,});
}

#define directory path separator
my $sep = File::Spec->catfile('', '');

print "-------------------------------------------- \n";
print "path is set to $path \n";
print "task is set to $task \n";

#define downlaod links
my @urls = (
	'http://central.maven.org/maven2/org/ow2/asm/asm-all/5.0.1/asm-all-5.0.1.jar',
	'http://central.maven.org/maven2/commons-cli/commons-cli/1.2/commons-cli-1.2.jar',
	'http://central.maven.org/maven2/org/apache/commons/commons-exec/1.1/commons-exec-1.1.jar',
	'http://central.maven.org/maven2/org/javassist/javassist/3.20.0-GA/javassist-3.20.0-GA.jar',
	'http://central.maven.org/maven2/junit/junit/4.10/junit-4.10.jar',
	'http://central.maven.org/maven2/org/testng/testng/6.10/testng-6.10.jar',
	'http://central.maven.org/maven2/com/beust/jcommander/1.48/jcommander-1.48.jar',
	'https://adopt-openjdk.ci.cloudbees.com/view/OpenJDK/job/asmtools/ws/asmtools-6.0-build/release/lib/asmtools.jar'
	);

#define jar file names stored under TestConfig/lib,
#this array should be in the same order of @urls
my @filenames = (
	'asm-all.jar',
	'commons-cli.jar',
	'commons-exec.jar',
	'javassist.jar',
	'junit4.jar',
	'testng.jar',
	'jcommander.jar',
	'asmtools.jar'
	);

print "-------------------------------------------- \n";
print "Starting download third party dependent jars \n";
print "-------------------------------------------- \n";
if ( $task eq "default" ) {

	print "downloading dependent third party jars to $path \n";
	for my $i (0 .. $#urls) {
		my $url = $urls[$i];
		my $filename = $path . $sep . $filenames[$i];

		if ( -e $filename) {
			print "$filename exits, skip downloading \n"
		} else {
			print "downloading $url \n";
			my $output = qx{wget --quiet --output-document=$filename $url 2>&1};
			if ($? == 0 ) {
				print "--> file downloaded to $filename \n";
			} else {
				die "ERROR: downloading $url failed, return code: $? \n";
			}
		}
	}
	print "downloaded dependent third party jars successfully \n";

} else {
	if ( $task eq "clean" ) {
		my $output = `rm $path/*.jar `;
		print "cleaning jar task completed. \n"
	} else {
		die "ERROR: task unsatisfied! \n";
	}
}


