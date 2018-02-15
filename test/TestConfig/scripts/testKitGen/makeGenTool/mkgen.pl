##############################################################################
#  Copyright (c) 2016, 2018 IBM Corp. and others
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
use feature 'say';

use constant DEBUG => 0;

my $headerComments =
	"########################################################\n"
	. "# This is an auto generated file. Please do NOT modify!\n"
	. "########################################################\n"
	. "\n";

my $mkName = "autoGen.mk";
my $utilsmk = "utils.mk";
my $settings = "settings.mk";
my $projectRootDir = '';
my $testRoot = '';
my $allLevels = '';
my $allGroups = '';
my $allSubsets = '';
my $output = '';
my $graphSpecs = '';
my $javaVersion = '';
my $allImpls = '';
my $impl = '';
my $modes_hs = '';
my $sp_hs = '';
my %targetGroup = ();

sub runmkgen {
	( $projectRootDir, $allLevels, $allGroups, $allSubsets, $output, $graphSpecs, $javaVersion, $allImpls, $impl, my $modesxml, my $ottawacsv ) = @_;

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
		print "Cannot get data from modes service! Getting data from modes.xml and ottawa.csv...\n";
		require "parseFiles.pl";
		my $data = getFileData($modesxml, $ottawacsv);
		$modes_hs = $data->{'modes'};
		$sp_hs = $data->{'specPlatMapping'};
	}

	foreach my $eachLevel (sort @{$allLevels}) {
		foreach my $eachGroup (sort @{$allGroups}) {
			my $groupTargetKey = $eachLevel . '.' . $eachGroup;
			$targetGroup{$groupTargetKey} = 0;
		}
	}

	generateOnDir();
	utilsGen();
}

sub generateOnDir {
	my @currentdirs = @_;
	my $absolutedir = $projectRootDir;
	if (@currentdirs) {
		$absolutedir .= '/' . join('/', @currentdirs);
	}
	my $playlistXML;
	my @subdirsHavePlaylist = ();
	opendir( my $dir, $absolutedir );
	while ( my $entry = readdir $dir ) {
		next if $entry eq '.' or $entry eq '..';
		my $tempExclude = 0;
		# tmporary exclusion, remove this block when JCL_VERSION separation is removed
		if (($javaVersion eq "SE90") || ($javaVersion eq "SE80")) {
			my $JCL_VERSION = '';
			if ( exists $ENV{'JCL_VERSION'} ) {
				$JCL_VERSION = $ENV{'JCL_VERSION'};
			} else {
				$JCL_VERSION = "latest";
			}
			# temporarily exclude projects for CCM build (i.e., when JCL_VERSION is latest)
			my $latestDisabledDir = "jvmtitests proxyFieldAccess classesdbgddrext dumpromtests jep178staticLinkingTest pltest Panama NativeTest SharedCPEntryInvokerTests gcCheck classvertest";
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
	if (@{$subdirsHavePlaylist}) {
		writeVars($makeFile, $subdirsHavePlaylist, $currentdirs);
		$rt = 1;
	} elsif ($playlistXML) {
		$rt = xml2mk($makeFile, $playlistXML, $currentdirs, $subdirsHavePlaylist);
	}

	return $rt;
}

sub xml2mk {
	my ($makeFile, $playlistXML, $currentdirs, $subdirsHavePlaylist) = @_;
	my $result = parseXML($playlistXML);
	if (!%{$result}) {
		return 0;
	}
	writeVars($makeFile, $subdirsHavePlaylist, $currentdirs);
	writeTargets($makeFile, $result, $currentdirs, $subdirsHavePlaylist);
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
	print $fhOut ".DEFAULT_GOAL := all\n\n";
	print $fhOut "D=/\n\n";
	print $fhOut "ifndef TEST_ROOT\n";
	print $fhOut "\tTEST_ROOT := $testRoot\n";
	print $fhOut "endif\n\n";
	print $fhOut "SUBDIRS = " . join(" ", @{$subdirsHavePlaylist}) . "\n\n";
	print $fhOut 'include $(TEST_ROOT)$(D)TestConfig$(D)' . $settings . "\n\n";
	close $fhOut;
}

sub parseXML {
	my ($playlistXML) = @_;
	open( my $fhIn, '<', $playlistXML ) or die "Cannot open file $_[0]";
	my %result = ();
	my @tests = ();
	while ( my $line = <$fhIn> ) {
		my %test;
		my $testlines;
		if ( $line =~ /\<include\>/ ) {
			my $include = getElementByTag( $line, 'include' );
			$result{'include'} = $include;
		} elsif ( $line =~ /\<test\>/ ) {
			$testlines .= $line;
			while ( my $testline = <$fhIn> ) {
				$testlines .= $testline;
				if ( $testline =~ /\<\/test\>/ ) {
					last;
				}
			}

			my $disabled = getElementByTag( $testlines, 'disabled' );
			if (defined $disabled) {
				next;
			}

			$test{'testCaseName'} =
			  getElementByTag( $testlines, 'testCaseName' );
			$test{'command'} = getElementByTag( $testlines, 'command' );
			$test{'platformRequirements'} =
			  getElementByTag( $testlines, 'platformRequirements' );
			$test{'capabilities'} = getElementByTag( $testlines, 'capabilities');
			my $variations = getElementByTag( $testlines, 'variations' );
			my $variation = getElementsByTag( $testlines, 'variation' );
			if ( !@{$variation} ) {
				$variation = [''];
			}
			$test{'variation'} = $variation;

			$test{'levels'} = getElementsByTag( $testlines, 'level' );
			# defaults to extended
			if (!@{$test{'levels'}}) {
				$test{'levels'} = ['extended'];
			}
			foreach my $level ( @{$test{'levels'}} ) {
				if ( !grep(/^$level$/, @{$allLevels}) ) {
					die "The level: " . $level . " for test " . $test{'testCaseName'} . " is not valid, the valid level strings are " . join(",", @{$allLevels}) . ".";
				}
			}

			$test{'groups'} = getElementsByTag( $testlines, 'group' );
			# defaults to functional
			if (!@{$test{'groups'}}) {
				$test{'groups'} = ['functional'];
			}
			foreach my $group ( @{$test{'groups'}} ) {
				if ( !grep(/^$group$/, @{$allGroups}) ) {
					die "The group: " . $group . " for test " . $test{'testCaseName'} . " is not valid, the valid group strings are " . join(",", @{$allGroups}) . ".";
				}
			}

			my $impls = getElementsByTag( $testlines, 'impl' );
			# defaults to all impls
			if (!@{$impls}) {
				$impls = $allImpls;
			}
			foreach my $impl ( @{$impls} ) {
				if ( !grep(/^$impl$/, @{$allImpls}) ) {
					die "The impl: " . $impl . " for test " . $test{'testCaseName'} . " is not valid, the valid impl strings are " . join(",", @{$allImpls}) . ".";
				}
			}
			# do not generate make taget if impl doesn't match the exported impl
			if ( !grep(/^$impl$/, @{$impls}) ) {
				next;
			}

			my $subsets = getElementsByTag( $testlines, 'subset' );
			# defaults to all subsets
			if (!@{$subsets}) {
				$subsets = $allSubsets;
			}
			foreach my $subset ( @{$subsets} ) {
				if ( !grep(/^$subset$/, @{$allSubsets}) ) {
					die "The subset: " . $subset . " for test " . $test{'testCaseName'} . " is not valid, the valid subset strings are " . join(",", @{$allSubsets}) . ".";
				}
			}
			# do not generate make taget if subset doesn't match javaVersion
			if ( !grep(/^$javaVersion$/, @{$subsets}) ) {
				next;
			}
			push( @tests, \%test );
		}
	}
	close $fhIn;
	$result{'tests'} = \@tests;
	return \%result;
}

sub getElementByTag {
	my ($element) = $_[0] =~ /\<$_[1]\>(.*)\<\/$_[1]\>/s;
	return $element;
}

sub getElementsByTag {
	my (@elements) = $_[0] =~ /\<$_[1]\>(.*?)\<\/$_[1]\>/sg;
	return \@elements;
}

sub writeTargets {
	my ( $makeFile, $result, $currentdirs, $subdirsHavePlaylist ) = @_;
	open( my $fhOut, '>>', $makeFile ) or die "Cannot create make file $makeFile";
	my %project = ();
	my %groupTargets = ();
	if (defined $result->{'include'}) {
		print $fhOut "-include " . $result->{'include'} . "\n\n";
	}
	foreach my $test ( @{ $result->{'tests'} } ) {
		my $count     = 0;
		my @subtests = ();
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

			if ($jvmoptions) {
				print $fhOut "$name: JVM_OPTIONS=\$(RESERVED_OPTIONS) $jvmoptions \$(EXTRA_OPTIONS)\n";
			} else {
				print $fhOut "$name: JVM_OPTIONS=\$(RESERVED_OPTIONS) \$(EXTRA_OPTIONS)\n";
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
			print $fhOut "$indent\@\$(MKTREE) \$(REPORTDIR);\n";
			print $fhOut "$indent\@echo \"\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";
			print $fhOut "$indent\@echo \"===============================================\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";
			print $fhOut "$indent\@echo \"Running test \$\@ ...\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";
			print $fhOut "$indent\@echo \"===============================================\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";
			if ($condition_platform) {
				print $fhOut "ifeq (\$($condition_platform),)\n";
			}
			if (defined($capabilityReqs)) {
				while (my ($key, $value) = each(%capabilityReqs_Hash)) {
					my $condition_capsReqs = $name . "_" . $key. "_CHECK";
					print $fhOut "ifeq (\$($condition_capsReqs), $value)\n";
				}
			}

			if ($jvmoptions) {
				print $fhOut "$indent\@echo \"test with $var\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";
			}
			else {
				print $fhOut "$indent\@echo \"test with NoOptions\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q);\n";
			}
			my $command = $test->{'command'};
			$command =~ s/^\s+//;
			$command =~ s/\s+$//;
			print $fhOut "$indent\{ $command; \} 2>&1 | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q)\n";

			if (defined($capabilityReqs)) {
				foreach my $key (keys %capabilityReqs_Hash) {
					print $fhOut "else\n";
					print $fhOut "$indent\@echo \"Skipped due to capabilities ($capabilityReqs) => \$(TEST_SKIP_STATUS)\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q)\n";
					print $fhOut "endif\n";
				}
			}
			if ($condition_platform) {
				print $fhOut "else\n";
				if (defined($platformRequirements)) {
					print $fhOut "$indent\@echo \"Skipped due to jvm options (\$(JVM_OPTIONS)) and/or platform requirements ($platformRequirements) => \$(TEST_SKIP_STATUS)\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q)\n";
				} else{
					print $fhOut "$indent\@echo \"Skipped due to jvm options (\$(JVM_OPTIONS)) => \$(TEST_SKIP_STATUS)\" | tee -a \$(Q)\$(TESTOUTPUT)\$(D)TestTargetResult\$(Q)\n";
				}
				print $fhOut "endif\n";
			}
			print $fhOut "\n.PHONY: $name\n\n";

			foreach my $eachGroup (@{$test->{'groups'}}) {
				foreach my $eachLevel (@{$test->{'levels'}}) {
					my $groupTargetKey = $eachLevel . '.' . $eachGroup;
					push(@{$groupTargets{$groupTargetKey}}, $name);
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

	foreach my $eachLevel (sort @{$allLevels}) {
		foreach my $eachGroup (sort @{$allGroups}) {
			my $groupTargetKey = $eachLevel . '.' . $eachGroup;
			my $tests = $groupTargets{$groupTargetKey};
			print $fhOut "$groupTargetKey:";
			foreach my $test (@{$tests}) {
				print $fhOut " \\\n$test";
			}
			$targetGroup{$groupTargetKey} += @{$tests};
			print $fhOut "\n\n.PHONY: $groupTargetKey\n\n";
		}
	}

	foreach my $eachLevel (sort @{$allLevels}) {
		print $fhOut "$eachLevel:";
		foreach my $eachGroup (sort @{$allGroups}) {
			my $groupTargetKey = $eachLevel . '.' . $eachGroup;
			print $fhOut " \\\n$groupTargetKey";
		}
		print $fhOut "\n\n.PHONY: $eachLevel\n\n";
	}

	foreach my $eachGroup (sort @{$allGroups}) {
		print $fhOut "$eachGroup:";
		foreach my $eachLevel (sort @{$allLevels}) {
			my $groupTargetKey = $eachLevel . '.' . $eachGroup;
			print $fhOut " \\\n$groupTargetKey";
		}
		print $fhOut "\n\n.PHONY: $eachGroup\n\n";
	}

	print $fhOut "all:";
	foreach my $eachLevel (sort @{$allLevels}) {
		print $fhOut " \\\n$eachLevel";
	}
	print $fhOut "\n\n.PHONY: all\n";

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
	
	foreach my $eachLevel (sort @{$allLevels}) {
		$targetGroup{$eachLevel} = 0;
		foreach my $eachGroup (sort @{$allGroups}) {
			my $groupTargetKey = $eachLevel . '.' . $eachGroup;
			$targetGroup{$eachLevel} += $targetGroup{$groupTargetKey};
		}
	}
	
	foreach my $eachGroup (sort @{$allGroups}) {
		$targetGroup{$eachGroup} = 0;
		foreach my $eachLevel (sort @{$allLevels}) {
			my $groupTargetKey = $eachLevel . '.' . $eachGroup;
			$targetGroup{$eachGroup} += $targetGroup{$groupTargetKey};
		}
	}

	print $fhOut "_GROUPTARGET = \$(firstword \$(MAKECMDGOALS))\n\n";
	print $fhOut "GROUPTARGET = \$(patsubst _%,%,\$(_GROUPTARGET))\n\n";
	print $fhOut "TOTALCOUNT := 0\n\n";
	foreach my $targetGroupKey (keys %targetGroup) {
		print $fhOut "ifeq (\$(GROUPTARGET),$targetGroupKey)\n";
		print $fhOut "\tTOTALCOUNT := $targetGroup{$targetGroupKey}\n";
		print $fhOut "endif\n\n";
	}

	close $fhOut;
	print "\nGenerated $utilsmk\n";
}

1;
