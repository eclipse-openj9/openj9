##############################################################################
#  Copyright (c) 2016, 2019 IBM Corp. and others
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
use Data::Dumper;
use File::Basename;
use File::Path qw/make_path/;

my $resultFile;
my $failuremkarg;
my $tapFile;
my $diagnostic = 'failure';
my $jdkVersion = "";
my $jdkImpl = "";
my $buildList = "";
my $spec = "";
my $customTarget = "";
my %spec2jenkinsFile = (
	'linux_x86-64_cmprssptrs'      => 'openjdk_x86-64_linux',
	'linux_x86-64'                 => 'openjdk_x86-64_linux_xl',
	'linux_arm'                    => 'openjdk_aarch32_linux',
	'linux_aarch64_cmprssptrs'     => 'openjdk_aarch64_linux',
	'linux_aarch64'                => 'openjdk_aarch64_linux_xl',
	'linux_ppc-64_cmprssptrs_le'   => 'openjdk_ppc64le_linux',
	'linux_390-64_cmprssptrs'      => 'openjdk_s390x_linux',
	'aix_ppc-64_cmprssptrs'        => 'openjdk_ppc64_aix',
	'zos_390-64_cmprssptrs'        => 'openjdk_s390x_zos',
	'osx_x86-64_cmprssptrs'        => 'openjdk_x86-64_mac',
	'osx_x86-64'                   => 'openjdk_x86-64_mac_xl',
	'win_x86-64_cmprssptrs'        => 'openjdk_x86-64_windows',
	'win_x86-64'                   => 'openjdk_x86-64_windows_xl',
	'win_x86'                      => 'openjdk_x86-32_windows',
	'sunos_sparcv9-64_cmprssptrs'  => 'openjdk_sparcv9_solaris',
);

for (my $i = 0; $i < scalar(@ARGV); $i++) {
	my $arg = $ARGV[$i];
	if ($arg =~ /^\-\-failuremk=/) {
		($failuremkarg) = $arg =~ /^\-\-failuremk=(.*)/;
	} elsif ($arg =~ /^\-\-resultFile=/) {
		($resultFile) = $arg =~ /^\-\-resultFile=(.*)/;
	} elsif ($arg =~ /^\-\-tapFile=/) {
		($tapFile) = $arg =~ /^\-\-tapFile=(.*)/;
	} elsif ($arg =~ /^\-\-diagnostic=/) {
		($diagnostic) = $arg =~ /^\-\-diagnostic=(.*)/;
	} elsif ($arg =~ /^\-\-jdkVersion=/) {
		($jdkVersion) = $arg =~ /^\-\-jdkVersion=(.*)/;
	} elsif ($arg =~ /^\-\-jdkImpl=/) {
		($jdkImpl) = $arg =~ /^\-\-jdkImpl=(.*)/;
	} elsif ($arg =~ /^\-\-buildList=/) {
		($buildList) = $arg =~ /^\-\-buildList=(.*)/;
	} elsif ($arg =~ /^\-\-spec=/) {
		($spec) = $arg =~ /^\-\-spec=(.*)/;
	} elsif ($arg =~ /^\-\-customTarget=/) {
		($customTarget) = $arg =~ /^\-\-customTarget=(.*)/;
	}
}

if (!$failuremkarg) {
	die "Please specify a valid file path using --failuremk= option!";
}

my $failures = resultReporter();
failureMkGen($failuremkarg, $failures);

sub resultReporter {
	my $numOfExecuted = 0;
	my $numOfFailed = 0;
	my $numOfPassed = 0;
	my $numOfSkipped = 0;
	my $numOfDisabled = 0;
	my $numOfTotal = 0;
	my $runningDisabled = 0;
	my @passed;
	my @failed;
	my @disabled;
	my @capSkipped;
	my $tapString = '';
	my $fhIn;

	print "\n\n";

	if (open($fhIn, '<', $resultFile)) {
		while ( my $result = <$fhIn> ) {
			if ($result =~ /===============================================\n/) {
				my $output = "    output:\n      |\n";
				$output .= '        ' . $result;
				my $testName = '';
				my $startTime = 0;
				my $endTime = 0;
				while ( $result = <$fhIn> ) {
					# remove extra carriage return
					$result =~ s/\r//g;
					$output .= '        ' . $result;
					if ($result =~ /Running test (.*) \.\.\.\n/) {
						$testName = $1;
					} elsif ($result =~ /^\Q$testName\E Start Time: .* Epoch Time \(ms\): (.*)\n/) {
						$startTime = $1;
					} elsif ($result =~ /^\Q$testName\E Finish Time: .* Epoch Time \(ms\): (.*)\n/) {
						$endTime = $1;
						$tapString .= "    duration_ms: " . ($endTime - $startTime) . "\n  ...\n";
						last;
					} elsif ($result eq ($testName . "_PASSED\n")) {
						$result =~ s/_PASSED\n$//;
						push (@passed, $result);
						$numOfPassed++;
						$numOfTotal++;
						$tapString .= "ok " . $numOfTotal . " - " . $result . "\n";
						$tapString .= "  ---\n";
						if ($diagnostic eq 'all') {
							$tapString .= $output;
						}
					} elsif ($result eq ($testName . "_FAILED\n")) {
						$result =~ s/_FAILED\n$//;
						push (@failed, $result);
						$numOfFailed++;
						$numOfTotal++;
						$tapString .= "not ok " . $numOfTotal . " - " . $result . "\n";
						$tapString .= "  ---\n";
						if (($diagnostic eq 'failure') || ($diagnostic eq 'all')) {
							$tapString .= $output;
						}
					} elsif ($result eq ($testName . "_DISABLED\n")) {
						$result =~ s/_DISABLED\n$//;
						push (@disabled, $result);
						$numOfDisabled++;
						$numOfTotal++;
					} elsif ($result eq ("Test is disabled due to:\n")) {
						$runningDisabled = 1;
					} elsif ($result =~ /(capabilities \(.*?\))\s*=>\s*${testName}_SKIPPED\n/) {
						my $capabilities = $1;
						push (@capSkipped, "$testName - $capabilities");
						$numOfSkipped++;
						$numOfTotal++;
						$tapString .= "ok " . $numOfTotal . " - " . $testName . " # skip\n";
						$tapString .= "  ---\n";
					} elsif ($result =~ /(jvm options|platform requirements).*=>\s*${testName}_SKIPPED\n/) {
						$numOfSkipped++;
						$numOfTotal++;
						$tapString .= "ok " . $numOfTotal . " - " . $testName . " # skip\n";
						$tapString .= "  ---\n";
					}
				}
			}
		}

		close $fhIn;
	}

	#generate console output
	print "TEST TARGETS SUMMARY\n";
	print "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";

	if ($numOfDisabled != 0) {
		printTests(\@disabled, "DISABLED test targets");
		print "\n";
	}

	# only print out skipped due to capabilities in console
	if (@capSkipped) {
		printTests(\@capSkipped, "SKIPPED test targets due to capabilities");
		print "\n";
	}

	if ($numOfPassed != 0) {
		printTests(\@passed, "PASSED test targets");
		print "\n";
	}

	if ($numOfFailed != 0) {
		printTests(\@failed, "FAILED test targets");
		print "\n";
	}

	$numOfExecuted = $numOfTotal - $numOfSkipped - $numOfDisabled;

	print "TOTAL: $numOfTotal   EXECUTED: $numOfExecuted   PASSED: $numOfPassed   FAILED: $numOfFailed";
	# Hide numOfDisabled when running disabled tests list.
	if ($runningDisabled == 0) {
		print "   DISABLED: $numOfDisabled";   
	}
	print "   SKIPPED: $numOfSkipped";
	print "\n";
	if ($numOfTotal > 0) {
		#generate tap output
		my $dir = dirname($tapFile);
		if (!(-e $dir and -d $dir)) {
			make_path($dir);
		}
		open(my $fhOut, '>', $tapFile) or die "Cannot open file $tapFile!";
		print $fhOut "1.." . $numOfTotal . "\n";
		print $fhOut $tapString;
		close $fhOut;
		if ($numOfFailed == 0) {
			print "ALL TESTS PASSED\n";
		}
	}

	print "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";

	if ($numOfFailed != 0) {
		my $buildParam =  "";
		if ($buildList ne '') {
			$buildParam = "&BUILD_LIST=" . $buildList;
		}
		my $jenkinFileParam = "";
		if (exists $spec2jenkinsFile{$spec}) {
			$jenkinFileParam = "&JenkinsFile=" . $spec2jenkinsFile{$spec};
		}
		my $customTargetParam = "";
		if ($customTarget ne '') {
			$customTargetParam = "&CUSTOM_TARGET=" . $customTarget;
		}
		print "To rebuild the failed test in a jenkins job, copy the following link and fill out the <Jenkins URL> and <FAILED test target>:\n";
		print "<Jenkins URL>/parambuild/?JDK_VERSION=$jdkVersion&JDK_IMPL=$jdkImpl$buildParam$jenkinFileParam$customTargetParam&TARGET=<FAILED test target>\n\n";
		print "For example, to rebuild the failed tests in <Jenkins URL>=https://ci.adoptopenjdk.net/job/Grinder, use the following links:\n";
		foreach my $failedTarget (@failed) {
			print "https://ci.adoptopenjdk.net/job/Grinder/parambuild/?JDK_VERSION=$jdkVersion&JDK_IMPL=$jdkImpl$buildParam$jenkinFileParam$customTargetParam&TARGET=$failedTarget\n";
		}
		print "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";
	}
	unlink($resultFile);

	return \@failed;
}

sub printTests() {
	my ($tests, $msg) = @_;
	print "$msg:\n\t";
	print join("\n\t", @{$tests});
	print "\n";
}

sub failureMkGen {
	my $failureMkFile = $_[0];
	my $failureTargets = $_[1];
	if (@$failureTargets) {
		open( my $fhOut, '>', $failureMkFile ) or die "Cannot create make file $failureMkFile!";
		my $headerComments =
		"########################################################\n"
		. "# This is an auto generated file. Please do NOT modify!\n"
		. "########################################################\n"
		. "\n";
		print $fhOut $headerComments;
		print $fhOut ".DEFAULT_GOAL := failed\n\n"
					. "D = /\n\n"
					. "ifndef TEST_ROOT\n"
					. "\tTEST_ROOT := \$(shell pwd)\$(D)..\n"
					. "endif\n\n"
					. "failed:\n";
		foreach my $target (@$failureTargets) {
			print $fhOut '	@$(MAKE) -C $(TEST_ROOT) -f autoGen.mk ' . $target . "\n";
		}
		print $fhOut "\n.PHONY: failed\n"
					. ".NOTPARALLEL: failed";
		close $fhOut;
	}
}