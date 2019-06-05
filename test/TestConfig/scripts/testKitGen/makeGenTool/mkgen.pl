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

use constant DEBUG => 0;
use Data::Dumper;
use feature 'say';
use strict;
use warnings;
use XML::Parser;

my $headerComments =
	"########################################################\n"
	. "# This is an auto generated file. Please do NOT modify!\n"
	. "########################################################\n"
	. "\n";

my $mkName = "autoGen.mk";
my $dependmk = "dependencies.mk";
my $utilsmk = "utils.mk";
my $countmk = "count.mk";
my $settings = "settings.mk";
my $projectRootDir = '';
my $testRoot = '';
my $allLevels = '';
my $allGroups = '';
my $allTypes = '';
my $output = '';
my $graphSpecs = '';
my $jdkVersion = '';
my $allImpls = '';
my $impl = '';
my $modes_hs = '';
my $sp_hs = '';
my %targetCount = ();
my $buildList = '';
my $iterations = 1;
my $testFlag = '';
my $parseResult = {};
my $parseTest = {};
$parseResult->{'tests'} = [];
my $parentEle = '';
my $currentEle = '';
my $eleStr = '';
my $eleArr = [];

sub runmkgen {
	( $projectRootDir, $allLevels, $allGroups, $allTypes, $output, $graphSpecs, $jdkVersion, $allImpls, $impl, my $modesxml, my $ottawacsv, $buildList, $iterations, $testFlag ) = @_;

	$testRoot = $projectRootDir;
	if ($output) {
		make_path($output . '/TestConfig');
		$testRoot = $output;
	}

	my $includeModesService = 1;
	eval qq{require "modesService.pl"; 1;} or $includeModesService = 0;
	my $serviceResponse;
	if ($includeModesService) {
		$serviceResponse = eval {
			$modes_hs = getDataFromService('http://testmgmt.stage1.mybluemix.net/modesDictionaryService/getAllModes', "mode");
			$sp_hs = getDataFromService('http://testmgmt.stage1.mybluemix.net/modesDictionaryService/getSpecPlatMapping', 'spec');
		};
	}

	if (!(($serviceResponse) && (%{$modes_hs}) && (%{$sp_hs}))) {
		print "Getting modes data from modes.xml and ottawa.csv...\n";
		require "parseFiles.pl";
		my $data = getFileData($modesxml, $ottawacsv);
		$modes_hs = $data->{'modes'};
		$sp_hs = $data->{'specPlatMapping'};
	} else {
		print "Getting modes data from modes services...\n";
	}

	# initialize counting hash targetCount
	$targetCount{"all"} = 0;
	my @allDisHead = ('', 'disabled.', 'echo.disabled.');
	foreach my $eachDisHead (@allDisHead) {
		foreach my $eachLevel (sort @{$allLevels}) {
			if (!defined $targetCount{$eachDisHead . $eachLevel}) {
				$targetCount{$eachDisHead . $eachLevel} = 0;
			}
			foreach my $eachGroup (sort @{$allGroups}) {
				if (!defined $targetCount{$eachDisHead . $eachGroup}) {
					$targetCount{$eachDisHead . $eachGroup} = 0;
				}
				my $lgKey = $eachLevel . '.' . $eachGroup;
				if (!defined $targetCount{$eachDisHead . $lgKey}) {
					$targetCount{$eachDisHead . $lgKey} = 0;
				}
				foreach my $eachType (sort @{$allTypes}) {
					if (!defined $targetCount{$eachDisHead . $eachType}) {
						$targetCount{$eachDisHead . $eachType} = 0;
					}
					my $ltKey = $eachLevel . '.' . $eachType;
					if (!defined $targetCount{$eachDisHead . $ltKey}) {
						$targetCount{$eachDisHead . $ltKey} = 0;
					}
					my $gtKey = $eachGroup . '.' . $eachType;
					if (!defined $targetCount{$eachDisHead . $gtKey}) {
						$targetCount{$eachDisHead . $gtKey} = 0;
					}
					my $lgtKey = $eachLevel . '.' . $eachGroup . '.' . $eachType;
					$targetCount{$eachDisHead . $lgtKey} = 0;
				}
			}
		}
	}

	generateOnDir();
	dependGen();
	utilsGen();
	countGen();
}

sub generateOnDir {
	my @currentdirs = @_;
	my $absolutedir = $projectRootDir;
	my $currentdir = join('/', @currentdirs);
	if (@currentdirs) {
		$absolutedir .= '/' . $currentdir;
	}
	
	# only generate make files for projects that specificed in the build list, if build list is empty, generate on every project
	if ( $buildList ne '' ){
		my $inList = 0;
		my @buildListArr = split(',', $buildList);
		foreach my $build (@buildListArr) {
			$build =~ s/\\+/\//g;
			if (( index($currentdir, $build) == 0 ) || ( index($build, $currentdir) == 0 )) {
				$inList = 1;
				last;
			}
		}
		if ($inList == 0) {
			return 0;
		}
	}

	my $playlistXML;
	my @subdirsHavePlaylist = ();
	opendir( my $dir, $absolutedir );
	while ( my $entry = readdir $dir ) {
		next if $entry eq '.' or $entry eq '..';
		my $tempExclude = 0;
		# temporary exclusion, remove this block when JCL_VERSION separation is removed
		if (($jdkVersion ne "Panama") && ($jdkVersion ne "Valhalla")) {
			my $JCL_VERSION = '';
			if ( exists $ENV{'JCL_VERSION'} ) {
				$JCL_VERSION = $ENV{'JCL_VERSION'};
			} else {
				$JCL_VERSION = "latest";
			}
			# temporarily exclude projects for CCM build (i.e., when JCL_VERSION is latest)
			my $latestDisabledDir = "proxyFieldAccess Panama";

			# Temporarily exclude SVT_Modularity tests from integration build where we are still using b148 JCL level
			my $currentDisableDir= "SVT_Modularity OpenJ9_Jsr_292_API";
			$tempExclude = (($JCL_VERSION eq "latest") and ($latestDisabledDir =~ /\Q$entry\E/ )) or (($JCL_VERSION eq "current") and ($currentDisableDir =~ /\Q$entry\E/ ));
		}
		if (!$tempExclude) {
			my $projectDir  = $absolutedir . '/' . $entry;
			if (( -f $projectDir ) && ( $entry eq 'playlist.xml' )) {
				$playlistXML = $projectDir;
			} elsif (-d $projectDir) {
				push (@currentdirs, $entry);
				if (generateOnDir(@currentdirs) == 1) {
					push (@subdirsHavePlaylist, $entry);
				}
				pop @currentdirs;
			}
		}
	}
	closedir $dir;

	return generateMk($playlistXML, $absolutedir, \@currentdirs, \@subdirsHavePlaylist);
}

sub generateMk {
	my ($playlistXML, $absolutedir, $currentdirs, $subdirsHavePlaylist) = @_;
	
	# construct makeFile path
	my $makeFile = $absolutedir . "/" . $mkName;
	if ($output) {
		my $outputdir = $output;
		if (@{$currentdirs}) {
			$outputdir .= '/' . join('/', @{$currentdirs});
		}
		make_path($outputdir);
		$makeFile = $outputdir . "/" . $mkName;
	}

	my $rt = 0;
	if ($playlistXML || @{$subdirsHavePlaylist}) {
		writeVars($makeFile, $subdirsHavePlaylist, $currentdirs);
		if (@{$subdirsHavePlaylist}) {
			$rt = 1;
		}
		if ($playlistXML) {
			$rt |= xml2mk($makeFile, $playlistXML, $currentdirs, $subdirsHavePlaylist);
		}
	}
	return $rt;
}

sub xml2mk {
	my ($makeFile, $playlistXML, $currentdirs) = @_;
	$parseTest = {};
	$parseResult = {};
	parseXML($playlistXML);
	if (!%{$parseResult}) {
		return 0;
	}
	writeTargets($makeFile, $parseResult, $currentdirs);
	return 1;
}

sub parseXML {
	my ( $playlistXML ) = @_;
	my $parser = new XML::Parser(Handlers => {Start => \&handle_start,
										End   => \&handle_end,
										Char  => \&handle_char});
	$parser->parsefile($playlistXML);
}

sub handle_start {
	my ($p, $elt) = @_;
	if ($elt eq 'include') {
		$eleStr = '';
		$currentEle = 'include';
	} elsif ($elt eq 'test') {
		$eleStr = '';
		$parseTest = {};
	} elsif ($elt eq 'disabled') {
		$eleStr = '';
		$currentEle = 'disabled';
	} elsif ($elt eq 'testCaseName') {
		$eleStr = '';
		$currentEle = 'testCaseName';
	} elsif ($elt eq 'command') {
		$eleStr = '';
		$currentEle = 'command';
	} elsif ($elt eq 'platformRequirements') {
		$eleStr = '';
		$currentEle = 'platformRequirements';
	} elsif ($elt eq 'capabilities') {
		$eleStr = '';
		$currentEle = 'capabilities';
	} elsif ($elt eq 'variations') {
		$eleStr = '';
		$eleArr = [];
		$currentEle = 'variations';
		$parentEle = 'variations';
	} elsif ($elt eq 'variation') {
		$eleStr = '';
		$currentEle = 'variation';
	} elsif ($elt eq 'levels') {
		$eleStr = '';
		$eleArr = [];
		$currentEle = 'levels';
		$parentEle = 'levels';
	} elsif ($elt eq 'level') {
		$eleStr = '';
		$currentEle = 'level';
	} elsif ($elt eq 'groups') {
		$eleStr = '';
		$eleArr = [];
		$currentEle = 'groups';
		$parentEle = 'groups';
	} elsif ($elt eq 'group') {
		$eleStr = '';
		$currentEle = 'group';
	} elsif ($elt eq 'types') {
		$eleStr = '';
		$eleArr = [];
		$currentEle = 'types';
		$parentEle = 'types';
	} elsif ($elt eq 'type') {
		$eleStr = '';
		$currentEle = 'type';
	} elsif ($elt eq 'impls') {
		$eleStr = '';
		$eleArr = [];
		$currentEle = 'impls';
		$parentEle = 'impls';
	} elsif ($elt eq 'impl') {
		$eleStr = '';
		$currentEle = 'impl';
	} elsif ($elt eq 'aot') {
		$eleStr = '';
		$currentEle = 'aot';
	} elsif ($elt eq 'subsets') {
		$eleStr = '';
		$eleArr = [];
		$currentEle = 'subsets';
		$parentEle = 'subsets';
	} elsif ($elt eq 'subset') {
		$eleStr = '';
		$currentEle = 'subset';
	}
}

sub handle_end {
	my ($p, $elt) = @_;
	my $neglectedFlag = 0;
	if (($elt eq 'include') && ($currentEle eq 'include')) {
		$parseResult->{'include'} = $eleStr;
	} elsif ($elt eq 'test') {
		# do not generate make taget if impl doesn't match the exported impl
		if ((defined $parseTest->{'impls'}) && !grep(/^$impl$/, @{$parseTest->{'impls'}}) ) {
			$neglectedFlag = 1;
		}
		# do not generate make target if subset doesn't match the exported jdk_version
		if (defined $parseTest->{'subsets'}) {
			my $isSubsetValid = 0;
			foreach my $eachSubset ( @{$parseTest->{'subsets'}} ) {
				if ( $eachSubset =~ /^(.*)\+$/ ) {
					if ( $1 <=  $jdkVersion ) {
						$isSubsetValid = 1;
						last;
					}
				} elsif ( $eachSubset eq $jdkVersion) {
					$isSubsetValid = 1;
					last;
				}
			}
			if ( $isSubsetValid == 0) {
				$neglectedFlag = 1;
			}
		}
		# Do not generate make taget if the test is aot not applicable when test flag is set to AOT.
		if ($neglectedFlag == 0 && (!defined $parseTest->{'aot'} || $parseTest->{'aot'} ne 'nonapplicable')) {
			# variation defaults to noOption
			if (!defined $parseTest->{'variation'}) {
				$parseTest->{'variation'} = ['NoOptions'];
			}
			# level defaults to 'extended'
			if (!defined $parseTest->{'levels'}) {
				$parseTest->{'levels'} = ['extended'];
			}
			# group defaults to 'functional'
			if (!defined $parseTest->{'groups'}) {
				$parseTest->{'groups'} = ['functional'];
			}
			# type defaults to 'regular'
			if (!defined $parseTest->{'types'}) {
				$parseTest->{'types'} = ['regular'];
			}
			# impl defaults to all
			if (!defined $parseTest->{'impls'}) {
				$parseTest->{'impls'} = $allImpls;
			}
			# aot defaults to applicable when testFlag contains AOT
			if (( $testFlag =~ /AOT/ ) && ( !defined $parseTest->{'aot'} )) {
				$parseTest->{'aot'} = 'applicable';
			}
			push( @{$parseResult->{'tests'}}, $parseTest );
		}
	} elsif (($elt eq 'disabled') && ($currentEle eq 'disabled')) {
		$parseTest->{'disabled'} = $eleStr;
	} elsif (($elt eq 'testCaseName') && ($currentEle eq 'testCaseName')) {
		$parseTest->{'testCaseName'} = $eleStr;
	} elsif (($elt eq 'command') && ($currentEle eq 'command')) {
		$parseTest->{'command'} = $eleStr;
	} elsif (($elt eq 'platformRequirements') && ($currentEle eq 'platformRequirements')) {
		$parseTest->{'platformRequirements'} = $eleStr;
	} elsif (($elt eq 'capabilities') && ($currentEle eq 'capabilities')) {
		$parseTest->{'capabilities'} = $eleStr;
	} elsif (($elt eq 'variation') && ($currentEle eq 'variation') && ($parentEle eq 'variations')) {
		push (@{$eleArr}, $eleStr);
	} elsif (($elt eq 'variations') && ($parentEle eq 'variations')) {
		if (@{$eleArr}) {
			$parseTest->{'variation'} = $eleArr;
		}
	} elsif (($elt eq 'level') && ($currentEle eq 'level') && ($parentEle eq 'levels')) {
		if ( !grep(/^$eleStr$/, @{$allLevels}) ) {
			die "The level: " . $eleStr . " for test " . $parseTest->{'testCaseName'} . " is not valid, the valid level strings are " . join(",", @{$allLevels}) . ".";
		}
		push (@{$eleArr}, $eleStr);
	} elsif (($elt eq 'levels') && ($parentEle eq 'levels')) {
		if (@{$eleArr}) {
			$parseTest->{'levels'} = $eleArr;
		}
	} elsif (($elt eq 'group') && ($currentEle eq 'group') && ($parentEle eq 'groups')) {
		if ( !grep(/^$eleStr$/, @{$allGroups}) ) {
			die "The group: " . $eleStr . " for test " . $parseTest->{'testCaseName'} . " is not valid, the valid group strings are " . join(",", @{$allGroups}) . ".";
		}
		push (@{$eleArr}, $eleStr);
	} elsif (($elt eq 'groups') && ($parentEle eq 'groups')) {
		if (@{$eleArr}) {
			$parseTest->{'groups'} = $eleArr;
		}
	} elsif (($elt eq 'type') && ($currentEle eq 'type') && ($parentEle eq 'types')) {
		if ( !grep(/^$eleStr$/, @{$allTypes}) ) {
			die "The type: " . $eleStr . " for test " . $parseTest->{'testCaseName'} . " is not valid, the valid type strings are " . join(",", @{$allTypes}) . ".";
		}
		push (@{$eleArr}, $eleStr);
	} elsif (($elt eq 'types') && ($parentEle eq 'types')) {
		if (@{$eleArr}) {
			$parseTest->{'types'} = $eleArr;
		}
	} elsif (($elt eq 'impl') && ($currentEle eq 'impl') && ($parentEle eq 'impls')) {
		if ( !grep(/^$eleStr$/, @{$allImpls}) ) {
			die "The impl: " . $eleStr . " for test " . $parseTest->{'testCaseName'} . " is not valid, the valid impl strings are " . join(",", @{$allImpls}) . ".";
		}
		push (@{$eleArr}, $eleStr);
	} elsif (($elt eq 'impls') && ($parentEle eq 'impls')) {
		if (@{$eleArr}) {
			$parseTest->{'impls'} = $eleArr;
		}
	} elsif (($elt eq 'aot') && ($currentEle eq 'aot')) {
		if ( $testFlag =~ /AOT/ ) {
			if ( $eleStr eq 'nonapplicable' ) {
				$parseTest->{'aot'} = $eleStr;
			} elsif ( $eleStr eq 'applicable' ) {
				$parseTest->{'aot'} = $eleStr;
			} elsif ( $eleStr eq 'explicit' ) {
				$parseTest->{'aot'} = $eleStr;
			} else {
				die "The aot tag: " . $eleStr . " for test " . $parseTest->{'testCaseName'} . " is not valid, the valid subset strings are nonapplicable, applicable and explicit.";
			}
		}
	} elsif (($elt eq 'subset') && ($currentEle eq 'subset') && ($parentEle eq 'subsets')) {
		push (@{$eleArr}, $eleStr);
	} elsif (($elt eq 'subsets') && ($parentEle eq 'subsets')) {
		if (@{$eleArr}) {
			$parseTest->{'subsets'} = $eleArr;
		}
	}
}

sub handle_char {
	my ($p, $str) = @_;
	$eleStr .= $str;
}

sub writeVars {
	my ($makeFile, $subdirsHavePlaylist, $currentdirs) = @_;
	print "\nGenerating make file $makeFile\n";
	open( my $fhOut, '>', $makeFile ) or die "Cannot create make file $makeFile";
	
	my $realtiveRoot = "";
	my $subdirlevel = scalar @{$currentdirs};
	if ($subdirlevel == 0) {
		$realtiveRoot = ".";
	} else {
		for my $i (1..$subdirlevel) {
			$realtiveRoot .= ($i == $subdirlevel) ? ".." : "..\$(D)";
		}
	}
	
	print $fhOut $headerComments ."\n";
	print $fhOut "D=/\n\n";
	print $fhOut "ifndef TEST_ROOT\n";
	print $fhOut "\tTEST_ROOT := $testRoot\n";
	print $fhOut "endif\n\n";
	print $fhOut "SUBDIRS = " . join(" ", @{$subdirsHavePlaylist}) . "\n\n";
	print $fhOut 'include $(TEST_ROOT)$(D)TestConfig$(D)' . $settings . "\n\n";
	close $fhOut;
}

sub writeTargets {
	my ( $makeFile, $result, $currentdirs ) = @_;
	open( my $fhOut, '>>', $makeFile ) or die "Cannot create make file $makeFile";
	my %project = ();
	my %groupTargets = ();
	if (defined $result->{'include'}) {
		print $fhOut "-include " . $result->{'include'} . "\n\n";
	}
	print $fhOut 'include $(TEST_ROOT)$(D)TestConfig$(D)' . $dependmk . "\n\n";
	foreach my $test ( @{ $result->{'tests'} } ) {
		my $count    = 0;
		my @subtests = ();
		my $testIterations = $iterations;
		foreach my $var ( @{ $test->{'variation'} } ) {
			my $jvmoptions = ' ' . $var . ' ';
			$jvmoptions =~ s/\ NoOptions\ //x;

			my %invalidSpecs_hs = ();
			while ( my ($mode) = $jvmoptions =~ /\ Mode(.*?)\ /x ) {
				my $vardoc = $modes_hs->{$mode};

				my $clArg_arr = $vardoc->{'clArg'};
				if ( !$clArg_arr ) {
					die "Error: Cannot find mode $mode in database";
				}

				my $clArgs = join(' ', @{$clArg_arr});
				$clArgs =~ s/\ $//x;
				$jvmoptions =~ s/Mode$mode/$clArgs/;

				my $invalidSpecs_arr = $vardoc->{'invalidSpecs'};
				foreach my $invalidSpec ( @{$invalidSpecs_arr} ) {
					$invalidSpecs_hs{$invalidSpec} = 1;
				}
			}
			$jvmoptions =~ s/^\s+|\s+$//g;

			my $invalidSpecs = '';
			foreach my $invalidSpec ( sort keys %invalidSpecs_hs ) {
				$invalidSpecs .= $invalidSpec . ' ';
			}

			$invalidSpecs =~ s/^\s+|\s+$//g;

			my $platformRequirements = $test->{'platformRequirements'};
			my @allInvalidSpecs = getAllInvalidSpecs( $invalidSpecs,
				$platformRequirements, $graphSpecs );

			my $capabilityReqs = $test->{'capabilities'};
			my @capabilityReqs_Arr = ();
			my %capabilityReqs_Hash;
			if (defined($capabilityReqs)) {
				@capabilityReqs_Arr = split( ',', $capabilityReqs );
				foreach my $capabilityReq (@capabilityReqs_Arr) {
					$capabilityReq =~ s/^\s+|\s+$//g;
					my ( $key, $value ) = split( /:/, $capabilityReq );
					$capabilityReqs_Hash{$key} = $value;
				}
			}

			# generate make target
			my $name = $test->{'testCaseName'};
			$name .= "_$count";
			push (@subtests, $name);

			my $condition_platform = undef;
			if (@allInvalidSpecs) {
				my $string = join( ' ', @allInvalidSpecs );
				$condition_platform = "$name\_INVALID_PLATFORM_CHECK";
				print $fhOut "$condition_platform=\$(filter $string, \$(SPEC))\n";
			}

			my @condition_capabilities = ();
			if (@capabilityReqs_Arr) {
				foreach my $capa_key (keys %capabilityReqs_Hash) {
					my $condition_capsReqs = $name . "_" . $capa_key. "_CHECK";
					print $fhOut "$condition_capsReqs=\$($capa_key)\n";
				}
			}

			my $jvmtestroot = "\$(JVM_TEST_ROOT)\$(D)" . join("\$(D)", @{$currentdirs});
			print $fhOut "$name: TEST_RESROOT=$jvmtestroot\n";

			my $aotOptions = '';
			# AOT_OPTIONS only needs to be appended when TEST_FLAG contains AOT and the test is aot applicable.
			if ( defined $test->{'aot'} ) {
				if ( $test->{'aot'} eq 'applicable' ) {
					$aotOptions = '$(AOT_OPTIONS) ';
				} elsif ( $test->{'aot'} eq 'explicit' ) {
					# When test tagged with aot explicit, its test command has aot options and runs multiple times explicitly.
					$testIterations = 1;
					print $fhOut "$name: TEST_ITERATIONS=1\n";
				}
			}

			if ($jvmoptions) {
				print $fhOut "$name: JVM_OPTIONS?=$aotOptions\$(RESERVED_OPTIONS) $jvmoptions \$(EXTRA_OPTIONS)\n";
			} else {
				print $fhOut "$name: JVM_OPTIONS?=$aotOptions\$(RESERVED_OPTIONS) \$(EXTRA_OPTIONS)\n";
			}

			my $levelStr = '';
			foreach my $level (@{$test->{'levels'}}) {
				if ($levelStr ne '') {
					$levelStr .= ',';
				}
				$levelStr .= "level." . $level;
			}

			print $fhOut "$name: TEST_GROUP=" . $levelStr . "\n";
			my $indent .= "\t";
			print $fhOut "$name:\n";
			print $fhOut "$indent\@echo \"\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";
			print $fhOut "$indent\@echo \"===============================================\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";
			print $fhOut "$indent\@echo \"Running test \$\@ ...\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";
			print $fhOut "$indent\@echo \"===============================================\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";
			print $fhOut "$indent\@perl \'-MTime::HiRes=gettimeofday\' -e \'print \"$name Start Time: \" . localtime() . \" Epoch Time (ms): \" . int (gettimeofday * 1000) . \"\\n\"\' | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";			
			if (defined $test->{'disabled'}) {
				#This line is also the key words to match runningDisabled
				print $fhOut "$indent\@echo \"Test is disabled due to:\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";
				my @disabledReasons = split("[\t\n]", $test->{'disabled'});
				foreach my $dReason (@disabledReasons) {
					if ($dReason ne "") {
						print $fhOut "$indent\@echo \"$dReason\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";
					}	
				}
			}
			if ($condition_platform) {
				print $fhOut "ifeq (\$($condition_platform),)\n";
			}
			if (defined($capabilityReqs)) {
				while (my ($key, $value) = each(%capabilityReqs_Hash)) {
					my $condition_capsReqs = $name . "_" . $key. "_CHECK";
					print $fhOut "ifeq (\$($condition_capsReqs), $value)\n";
				}
			}

			print $fhOut "$indent\$(TEST_SETUP);\n";

			print $fhOut "$indent\@echo \"variation: $var\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";
			print $fhOut "$indent\@echo \"JVM_OPTIONS: \$(JVM_OPTIONS)\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";

			my $command = $test->{'command'};
			$command =~ s/^\s+//;
			$command =~ s/\s+$//;

			print $fhOut "$indent\{ ";
			for (my $i = 1; $i <= $testIterations; $i++) {
				print $fhOut "itercnt=$i; \\\n$indent\$(MKTREE) \$(REPORTDIR); \\\n$indent\$(CD) \$(REPORTDIR); \\\n";
				print $fhOut "$indent$command;";
				if ($i ne $testIterations) {
					print $fhOut " \\\n$indent";
				}
			}
			print $fhOut " \} 2>&1 | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";

			print $fhOut "$indent\$(TEST_TEARDOWN);\n";

			if (defined($capabilityReqs)) {
				foreach my $key (keys %capabilityReqs_Hash) {
					print $fhOut "else\n";
					print $fhOut "$indent\@echo \"Skipped due to capabilities ($capabilityReqs) => \$(TEST_SKIP_STATUS)\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";
					print $fhOut "endif\n";
				}
			}
			if ($condition_platform) {
				print $fhOut "else\n";
				if (defined($platformRequirements)) {
					print $fhOut "$indent\@echo \"Skipped due to jvm options (\$(JVM_OPTIONS)) and/or platform requirements ($platformRequirements) => \$(TEST_SKIP_STATUS)\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";
				} else{
					print $fhOut "$indent\@echo \"Skipped due to jvm options (\$(JVM_OPTIONS)) => \$(TEST_SKIP_STATUS)\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";
				}
				print $fhOut "endif\n";
			}
			print $fhOut "$indent\@perl \'-MTime::HiRes=gettimeofday\' -e \'print \"$name Finish Time: \" . localtime() . \" Epoch Time (ms): \" . int (gettimeofday * 1000) . \"\\n\"\' | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q)\n";

			print $fhOut "\n.PHONY: $name\n\n"; 

			foreach my $eachGroup (@{$test->{'groups'}}) {
				foreach my $eachLevel (@{$test->{'levels'}}) {
					foreach my $eachType (@{$test->{'types'}}) {
						if (!defined $test->{'disabled'}) {
							my $lgtKey = $eachLevel . '.' . $eachGroup . '.' . $eachType;
							push(@{$groupTargets{$lgtKey}}, $name);
						}
						else {
							my $lgtKey = $eachLevel . '.' . $eachGroup . '.' . $eachType;
							my $dlgtKey = 'disabled.' . $lgtKey;
							my $echodlgtKey = 'echo.' . $dlgtKey;
							push(@{$groupTargets{$dlgtKey}}, $name);
							push(@{$groupTargets{$echodlgtKey}}, "echo.disabled." . $name);
							print $fhOut "echo.disabled." . $name . ":\n";
							print $fhOut "$indent\@echo \"\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";
							print $fhOut "$indent\@echo \"===============================================\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";
							print $fhOut "$indent\@echo \"Running test $name ...\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";
							print $fhOut "$indent\@echo \"===============================================\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";
							print $fhOut "$indent\@perl \'-MTime::HiRes=gettimeofday\' -e \'print \"$name Start Time: \" . localtime() . \" Epoch Time (ms): \" . int (gettimeofday * 1000) . \"\\n\"\' | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";
							print $fhOut "$indent\@echo \"" . $name . "_DISABLED\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";
							print $fhOut "$indent\@echo \"Disabled Reason:\"\n";
							my @disabledReasons = split("[\t\n]", $test->{'disabled'});
							foreach my $dReason (@disabledReasons) {
								if ($dReason ne "") {
									print $fhOut "$indent\@echo \"$dReason\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";
								}	
							}
							print $fhOut "$indent\@perl \'-MTime::HiRes=gettimeofday\' -e \'print \"$name Finish Time: \" . localtime() . \" Epoch Time (ms): \" . int (gettimeofday * 1000) . \"\\n\"\' | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q)\n";
							print $fhOut "\n.PHONY: echo.disabled." . "$name\n\n";
						}
					}
				}
			}

			$count++;
		}
		print $fhOut $test->{'testCaseName'} . ":";
		foreach my $subTest (@subtests) {
			print $fhOut " \\\n$subTest";
		}
		print $fhOut "\n\n.PHONY: " . $test->{'testCaseName'} . "\n\n";
	}

	my @allDisHead = ('', 'disabled.', 'echo.disabled.');
	foreach my $eachDisHead (@allDisHead) { 
		foreach my $eachLevel (sort @{$allLevels}) {
			foreach my $eachGroup (sort @{$allGroups}) {
				foreach my $eachType (sort @{$allTypes}) {
					my $lgtKey = $eachLevel . '.' . $eachGroup . '.' . $eachType;
					my $hlgtKey = $eachDisHead . $eachLevel . '.' . $eachGroup . '.' . $eachType;
					my $tests = $groupTargets{$hlgtKey};
					print $fhOut "$hlgtKey:";
					foreach my $test (@{$tests}) {
						print $fhOut " \\\n$test";
					}
					# The 'all' / lgtKey  contain normal and echo.disabled.					
					$targetCount{$hlgtKey} += @{$tests};
					if ($eachDisHead eq '') {
						print $fhOut " \\\necho.disabled." . $lgtKey;
						$targetCount{"all"} += @{$tests};
					}
					if ($eachDisHead eq 'echo.disabled.') {
						# normal key contains echo.disabled key
						$targetCount{$lgtKey} += @{$tests};
						$targetCount{"all"} += @{$tests};
					}
					print $fhOut "\n\n.PHONY: $hlgtKey\n\n";
				}
			}
		}
	}

	close $fhOut;
}

# return an array of invalid specs
sub getAllInvalidSpecs {
	my @invalidSpecPerMode   = ();
	my @platformRequirements = ();
	my @specs                = ();
	my @invalidSpecs         = ();
	if ( defined $_[0] ) {
		@invalidSpecPerMode = split( ' ', $_[0] );
	}
	if ( defined $_[1] ) {
		@platformRequirements = split( /,/, $_[1] );
	}
	if ( defined $_[2] ) {
		@specs = split( ',', $_[2] );
	}

	foreach my $pr (@platformRequirements) {
		$pr =~ s/^\s+|\s+$//g;
		my ( $key, $value ) = split( /\./, $pr );

		# copy @specs into @specArray
		my @specArray = @specs;
		foreach my $i ( 0 .. $#specArray ) {
			if ( $specArray[$i] ) {
				my $tmpVal = $specArray[$i];

				# special case, specs with 32 or 31 bits do not have 32 or 31 in the name (i.e. aix_ppc)
				if ( $tmpVal !~ /-64/ ) {
					if ( $tmpVal =~ /390/ ) {
						$tmpVal = $specArray[$i] . "-31";
					}
					else {
						$tmpVal = $specArray[$i] . "-32";
					}
				}

				if ( $key =~ /^\^/ ) {
					if ( $tmpVal =~ /$value/ ) {
						push @invalidSpecs, $specArray[$i];

						# remove the element from @specs
						my $index = 0;
						$index++ until $specs[$index] eq $specArray[$i];
						splice( @specs, $index, 1 );
					}
				}
				else {
					if ( $tmpVal !~ /$value/ ) {
						push @invalidSpecs, $specArray[$i];

						# remove the element from @specs
						my $index = 0;
						$index++ until $specs[$index] eq $specArray[$i];
						splice( @specs, $index, 1 );
					}
				}
			}
		}

		# reset @specsArray
		@specArray = @specs;
	}

	# find intersection of runable specs and invalid specs
	my %specs = map { $_ => 1 } @specs;
	my @intersect = grep { $specs{$_} } @invalidSpecPerMode;
	push( @invalidSpecs, @intersect );
	return @invalidSpecs;
}

sub dependGen {
	my $dependmkpath = $testRoot . "/TestConfig/" . $dependmk;
	open( my $fhOut, '>', $dependmkpath ) or die "Cannot create file $dependmkpath";
	print $fhOut $headerComments;

	my @allDisHead = ('', 'disabled.', 'echo.disabled.');
	foreach my $eachDisHead (@allDisHead) {
		foreach my $eachLevel (sort @{$allLevels}) {
			foreach my $eachGroup (sort @{$allGroups}) {
				my $hlgKey = $eachDisHead . $eachLevel . '.' . $eachGroup;
				print $fhOut "$hlgKey:";
				foreach my $eachType (sort @{$allTypes}) {
					my $hlgtKey = $eachDisHead. $eachLevel . '.' . $eachGroup . '.' . $eachType; 
					print $fhOut " \\\n$hlgtKey";
					$targetCount{$hlgKey} += $targetCount{$hlgtKey};
					$targetCount{$eachDisHead . $eachLevel} += $targetCount{$hlgtKey};
				}
				print $fhOut "\n\n.PHONY: $hlgKey\n\n";
			}
		}	

		foreach my $eachGroup (sort @{$allGroups}) {
			foreach my $eachType (sort @{$allTypes}) {
				my $gtKey = $eachDisHead . $eachGroup . '.' . $eachType;
				print $fhOut "$gtKey:";
				foreach my $eachLevel (sort @{$allLevels}) {
					my $lgtKey = $eachDisHead . $eachLevel . '.' . $eachGroup . '.' . $eachType; 
					print $fhOut " \\\n$lgtKey";
					$targetCount{$gtKey} += $targetCount{$lgtKey};
					$targetCount{$eachDisHead . $eachGroup} += $targetCount{$lgtKey};
				}
				print $fhOut "\n\n.PHONY: $gtKey\n\n";
			}
		}

		foreach my $eachType (sort @{$allTypes}) {
			foreach my $eachLevel (sort @{$allLevels}) {
				my $ltKey = $eachDisHead . $eachLevel . '.' . $eachType;
				print $fhOut "$ltKey:";
				foreach my $eachGroup (sort @{$allGroups}) {
					my $lgtKey = $eachDisHead . $eachLevel . '.' . $eachGroup . '.' . $eachType;
					print $fhOut " \\\n$lgtKey";
					$targetCount{$ltKey} += $targetCount{$lgtKey};
					$targetCount{$eachDisHead . $eachType} += $targetCount{$lgtKey};
				}
				print $fhOut "\n\n.PHONY: $ltKey\n\n";
			}
		}

		foreach my $eachLevel (sort @{$allLevels}) {
			my $lKey = $eachDisHead . $eachLevel;
			print $fhOut "$lKey:";
			foreach my $eachGroup (sort @{$allGroups}) {
				print $fhOut " \\\n" . $eachDisHead . $eachLevel . '.' . $eachGroup;
			}
			print $fhOut "\n\n.PHONY: $lKey\n\n";
		}	

		foreach my $eachGroup (sort @{$allGroups}) {
			my $gKey = $eachDisHead . $eachGroup;
			print $fhOut "$gKey:";
			foreach my $eachLevel (sort @{$allLevels}) {
				print $fhOut " \\\n" . $eachDisHead . $eachLevel . '.' . $eachGroup;
			}
			print $fhOut "\n\n.PHONY: $gKey\n\n";
		}

		foreach my $eachType (sort @{$allTypes}) {
			my $tKey = $eachDisHead . $eachType;
			print $fhOut "$tKey:";
			foreach my $eachLevel (sort @{$allLevels}) {
				print $fhOut " \\\n" . $eachDisHead . $eachLevel . '.' . $eachType;
			}
			print $fhOut "\n\n.PHONY: $tKey\n\n";
		}

		my $allKey = $eachDisHead . "all";
		print $fhOut "$allKey:";
		foreach my $eachLevel (sort @{$allLevels}) {
			print $fhOut " \\\n" . $eachDisHead . $eachLevel;
		}
		print $fhOut "\n\n.PHONY: $allKey\n\n";
	}

	close $fhOut;
	print "\nGenerated $dependmk\n";
}

sub utilsGen {
	my $utilsmkpath = $testRoot . "/TestConfig/" . $utilsmk;
	open( my $fhOut, '>', $utilsmkpath ) or die "Cannot create file $utilsmkpath";
	print $fhOut $headerComments;
	print $fhOut "PLATFORM=\n";
	my $spec2platform = '';
	my @graphSpecs_arr = split( ',', $graphSpecs );
	foreach my $graphSpec (@graphSpecs_arr) {
		if ( defined $sp_hs->{$graphSpec} ) {
			$spec2platform .= "ifeq" . " (\$(SPEC),$graphSpec)\n\tPLATFORM=" . $sp_hs->{$graphSpec}->{"platform"} . "\nendif\n\n";
		} else {
			print "\nWarning: cannot find spec $graphSpec in ModesDictionaryService or ottawa.csv file\n" if DEBUG;
		}
	}
	print $fhOut $spec2platform;

	close $fhOut;
	print "\nGenerated $utilsmk\n";
}

sub countGen {
	my $countmkpath = $testRoot . "/TestConfig/" . $countmk;
	open( my $fhOut, '>', $countmkpath ) or die "Cannot create file $countmkpath";
	print $fhOut $headerComments;

	print $fhOut "_GROUPTARGET = \$(firstword \$(MAKECMDGOALS))\n\n";
	print $fhOut "GROUPTARGET = \$(patsubst _%,%,\$(_GROUPTARGET))\n\n";
	foreach my $lgtKey (keys %targetCount) {
		print $fhOut "ifeq (\$(GROUPTARGET),$lgtKey)\n";
		print $fhOut "\tTOTALCOUNT := $targetCount{$lgtKey}\n";
		print $fhOut "endif\n\n";
	}

	close $fhOut;
	print "\nGenerated $countmk\n";
}


1;
