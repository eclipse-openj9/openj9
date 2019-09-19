#!/usr/bin/perl

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

use strict;
use warnings;
use Digest::SHA;
use Getopt::Long;
use File::Copy;
use File::Spec;
use File::Path qw(make_path);

# define test src root path
my $path;
# define task
my $task = "default";
# define os
my $os;

GetOptions ("path=s" => \$path,
			"task=s" => \$task,
			"os=s"   => \$os)
	or die("Error in command line arguments\n");

if (not defined $path) {
	die "ERROR: Download path not defined!\n"
}

if (! -d $path) {
	print "$path does not exist, creating one.\n";
	my $isCreated = make_path($path, {chmod => 0777, verbose => 1,});
}

if (not defined $os) {
	die "ERROR: os not defined!\n"
}

# define directory path separator
my $sep = File::Spec->catfile('', '');

print "--------------------------------------------\n";
print "path is set to $path\n";
print "task is set to $task\n";
print "os   is set to $os\n";

# Define a a hash for each dependent jar
# Contents in the hash should be: 
#   url - required. url to download the dependent jar
#   fname - required. the dependent jar name
#   sha1 - sha1 value for the dependent jar (can be skipped if both shaurl and shafn are provided)
#   shaurl - optional. url to download the sha file
#   shafn  - optional. sha file name (has to be used with shaurl)
#   shaalg - optional. sha is calculated based on shaalg (default value is sha1)

my %asm_all = (
	url => 'http://central.maven.org/maven2/org/ow2/asm/asm-all/6.0_BETA/asm-all-6.0_BETA.jar',
	fname => 'asm-all.jar',
	sha1 => '535f141f6c8fc65986a3469839a852a3266d1025'
);
my %asm_7_0 = (
	url => 'https://repository.ow2.org/nexus/content/repositories/releases/org/ow2/asm/asm/7.0-beta/asm-7.0-beta.jar',
	fname => 'asm-7.0.jar',
	sha1 => '7804855908e0a5e4f98e3364fa537426d9f5329c'
);
my %commons_cli = (
	url => 'http://central.maven.org/maven2/commons-cli/commons-cli/1.2/commons-cli-1.2.jar',
	fname => 'commons-cli.jar',
	sha1 => '2bf96b7aa8b611c177d329452af1dc933e14501c'
);
my %commons_exec = (
	url => 'http://central.maven.org/maven2/org/apache/commons/commons-exec/1.1/commons-exec-1.1.jar',
	fname => 'commons-exec.jar',
	sha1 => '07dfdf16fade726000564386825ed6d911a44ba1'
);
my %javassist = (
	url => 'http://central.maven.org/maven2/org/javassist/javassist/3.20.0-GA/javassist-3.20.0-GA.jar',
	fname => 'javassist.jar',
	sha1 => 'a9cbcdfb7e9f86fbc74d3afae65f2248bfbf82a0'
);
my %junit4 = (
	url => 'http://central.maven.org/maven2/junit/junit/4.10/junit-4.10.jar',
	fname => 'junit4.jar',
	sha1 => 'e4f1766ce7404a08f45d859fb9c226fc9e41a861'
);
my %testng = (
	url => 'http://central.maven.org/maven2/org/testng/testng/6.14.2/testng-6.14.2.jar',
	fname => 'testng.jar',
	sha1 => '10c93c2c0d165e895a7582dfd8b165f108658db5'
);
my %jcommander = (
	url => 'http://central.maven.org/maven2/com/beust/jcommander/1.48/jcommander-1.48.jar',
	fname => 'jcommander.jar',
	sha1 => 'bfcb96281ea3b59d626704f74bc6d625ff51cbce'
);
my %asmtools = (
	url => 'https://ci.adoptopenjdk.net/view/Dependencies/job/asmtools/lastSuccessfulBuild/artifact/asmtools.jar',
	fname => 'asmtools.jar',
	shaurl => 'https://ci.adoptopenjdk.net/view/Dependencies/job/asmtools/lastSuccessfulBuild/artifact/asmtools.jar.sha256sum.txt',
	shafn => 'asmtools.jar.sha256sum.txt',
	shaalg => '256'
);
# this is needed for JDK11 and up
my %jaxb_api = (
	url => 'http://central.maven.org/maven2/javax/xml/bind/jaxb-api/2.3.0/jaxb-api-2.3.0.jar',
	fname => 'jaxb-api.jar',
	sha1 => '99f802e0cb3e953ba3d6e698795c4aeb98d37c48'
);

# Put all dependent jars hash to array to prepare downloading
my @jars_info = (
	\%asm_all,
	\%asm_7_0,
	\%commons_cli,
	\%commons_exec,
	\%javassist,
	\%junit4,
	\%testng,
	\%jcommander,
	\%asmtools,
	\%jaxb_api
);

print "--------------------------------------------\n";
print "Starting download third party dependent jars\n";
print "--------------------------------------------\n";

if ($task eq "clean") {
	my $output = `rm $path/*.jar`;
	print "cleaning jar task completed.\n"
} elsif ($task eq "default") {
	print "downloading dependent third party jars to $path\n";
	for my $i (0 .. $#jars_info) {
		my $url = $jars_info[$i]{url};
		my $fn = $jars_info[$i]{fname};
		my $filename = $path . $sep . $fn;
		my $shaurl = $jars_info[$i]{shaurl};
		my $shafn = $jars_info[$i]{shafn};
		my $shaalg = $jars_info[$i]{shaalg};
		if (!$shaalg) {
			$shaalg = "sha1";
		}
		my $sha = Digest::SHA->new($shaalg);
		my $digest = "";

		if (-e $filename) {
			$sha->addfile($filename);
			$digest = $sha->hexdigest;
		}

		my $expectedsha = $jars_info[$i]{sha1};
		if (!$expectedsha) {
			$shafn = $path . $sep . $shafn;
			# if the sha file exists, parse the file and get the expected sha
			if (-e $shafn) {
				$expectedsha = getShaFromFile($shafn, $fn);
			} 

			# if expectedsha is not set above and shaurl is provided, download the sha file 
			# and parse the file to get the expected sha
			if (!$expectedsha && $shaurl) {
				downloadFile($shaurl, $shafn);
				$expectedsha = getShaFromFile($shafn, $fn);
			}
		}

		if ($expectedsha && $digest eq $expectedsha) {
			print "$filename exists with correct hash, not downloading\n";
			next;
		}

		# download the dependent third party jar
		downloadFile($url, $filename);

		# if shaurl is provided, re-download the sha file and reset the expectedsha value
		# as the dependent third party jar is newly downloadeded
		if ($shaurl) {
			downloadFile($shaurl, $shafn);
			$expectedsha = getShaFromFile($shafn, $fn);
		}

		if (!$expectedsha) {
			die "ERROR: cannot get the expected sha for file $fn.\n";
		}

		# validate dependencies sha sum
		$sha = Digest::SHA->new($shaalg);
		$sha->addfile($filename);
		$digest = $sha->hexdigest;

		if ($digest ne $expectedsha) {
			print "Expected sha is: $expectedsha,\n";
			print "Actual sha is  : $digest.\n";
			print "Please delete $filename and rerun the program!";
			die "ERROR: sha checksum error.\n";
		}
	}
	print "downloaded dependent third party jars successfully\n";
} else {
	die "ERROR: task unsatisfied!\n";
}


sub getShaFromFile {
	my ( $shafile, $fn ) = @_;
	my $sha = "";
	open my $fh, '<', $shafile or print "Can't open file $!";
	my $content = do { local $/; <$fh> };
	my @token = split / /, $content;
	if (@token > 1) {
		$sha = $token[0];
	} else {
		print "WARNING: cannot get the sha from $shafile.\nFile content: $content\n";
	}
	return $sha;
}

sub downloadFile {
	my ( $url, $filename ) = @_;
	print "downloading $url\n";
	my $output;
	# ToDo: should use curl only
	if ($os eq 'os.zos') {
		$output = qx{curl -k -o $filename $url 2>&1};	
	} else {
		$output = qx{wget --no-check-certificate --quiet --output-document=$filename $url 2>&1};
	}
	my $returnCode = $?;
	if ($returnCode == 0) {
		print "--> file downloaded to $filename\n";
	} else {
		print $output;
		unlink $filename or die "Can't delete '$filename': $!\n";
		die "ERROR: downloading $url failed, return code: $returnCode\n";
	}
}