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
use Data::Dumper;
use feature 'say';

my $resultFile;
my $failuremkarg;
my $tapFile;
my $diagnostic = 'failure';

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
	}
}

if (!$failuremkarg) {
	die "Please specify a vaild file path using --failuremk= option!";
}

my $failures = resultReporter();
failureMkGen($failuremkarg, $failures);

sub resultReporter {
	my $numOfExecuted = 0;
	my $numOfFailed = 0;
	my $numOfPassed = 0;
	my $numOfSkipped = 0;
	my $numOfTotal = 0;
	my @passed;
	my @failed;
	my @skipped;
	my $tapString = '';
	my $fhIn;

	print "\n\n";

	if (open($fhIn, '<', $resultFile)) {
		while ( my $result = <$fhIn> ) {
			if ($result =~ /===============================================\n/) {
				my $output = "  ---\n    output:\n      |\n";
				$output .= '        ' . $result;
				my $testName = '';
				while ( $result = <$fhIn> ) {
					if ($result =~ /Running test (.*) \.\.\.\n/) {
						$testName = $1;
					} elsif ($result eq ($testName . "_PASSED\n")) {
						$result =~ s/_PASSED\n$//;
						push (@passed, $result);
						$numOfPassed++;
						$numOfTotal++;
						$tapString .= "ok " . $numOfTotal . " - " . $result . "\n";
						if ($diagnostic eq 'all') {
							$output .= "  ...\n";
							$tapString .= $output;
						}
						last;
					} elsif ($result eq ($testName . "_FAILED\n")) {
						$result =~ s/_FAILED\n$//;
						push (@failed, $result);
						$numOfFailed++;
						$numOfTotal++;
						$tapString .= "not ok " . $numOfTotal . " - " . $result . "\n";
						if (($diagnostic eq 'failure') || ($diagnostic eq 'all')) {
							$output .= "  ...\n";
							$tapString .= $output;
						}
						last;
					} elsif ($result eq ($testName . "_SKIPPED\n")) {
						$result =~ s/_SKIPPED\n$//;
						push (@skipped, $result);
						$numOfSkipped++;
						$numOfTotal++;
						$tapString .= "ok " . $numOfTotal . " - " . $result . " # skip\n";
						last;
					}
					$output .= '        ' . $result;
				}
			}
		}

		close $fhIn;

		#generate tap output
		if ($tapFile) {
			open(my $fhOut, '>', $tapFile) or die "Cannot open file $tapFile!";
			print $fhOut "1.." . $numOfTotal . "\n";
			print $fhOut $tapString;
			close $fhOut;
		}
	}

	#generate console output
	print "TEST TARGETS SUMMARY\n";
	print "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";

	if ($numOfPassed != 0) {
		printTests(\@passed, "PASSED");
		print "\n";
	}

	if ($numOfFailed != 0) {
		printTests(\@failed, "FAILED");
		print "\n";
	}

	if ($numOfSkipped != 0) {
		printTests(\@skipped, "SKIPPED");
		print "\n";
	}

	$numOfExecuted = $numOfTotal - $numOfSkipped;

	print "TOTAL: $numOfTotal   EXECUTED: $numOfExecuted   PASSED: $numOfPassed   FAILED: $numOfFailed   SKIPPED: $numOfSkipped\n";
	if (($numOfTotal > 0) && ($numOfFailed == 0)) {
		print "ALL TESTS PASSED\n";
	}
	print "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";

	unlink($resultFile);

	return \@failed;
}

sub printTests() {
	my ($tests, $tag) = @_;
	print "$tag test targets:\n\t";
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
		print $fhOut "failed:";
		print $fhOut " \\\nrmResultFile";
		foreach my $target (@$failureTargets) {
			print $fhOut " \\\n" . $target;
		}
		print $fhOut " \\\nresultsSummary";
		close $fhOut;
	}
}