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
use XML::Parser;

use constant DEBUG => 0;

my @allGroups = ( "sanity", "extended", "openjdk", "external", "perf", "jck" );

my $headerComments =
	"########################################################\n"
	. "# This is an auto generated file. Please do NOT modify!\n"
	. "########################################################\n"
	. "\n";

my $mkName = "autoGenTest.mk";
my $projectRootDir = '';
my $JCL_VERSION = '';
my $allSubsets = '';
my $output = '';
my $graphSpecs = '';
my $modes_hs = '';
my $sp_hs = '';
my %tests = ();
my $parseTests = [];
my $parseTest = {};
my $parentEle = '';
my $currentEle = '';
my $eleStr = '';
my $eleArr = [];

sub runmkgen {
	$projectRootDir = $_[0];
	$allSubsets = $_[1];
	$output = $_[2];
	$graphSpecs = $_[3];
	my $modesxml = $_[4];
	my $ottawacsv = $_[5];
	my $includeModesService = 1;

	if ( exists $ENV{'JCL_VERSION'} ) {
		$JCL_VERSION = $ENV{'JCL_VERSION'};
	} else {
		$JCL_VERSION = "latest";
	}
	print "JCL_VERSION is $JCL_VERSION\n";

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

	generateOnDir();

	my $specToPlatmk = $projectRootDir . "/TestConfig/specToPlat.mk";
	my $jvmTestmk = $projectRootDir . "/TestConfig/jvmTest.mk";
	if ($output) {
		my $outputdir = $output . '/TestConfig';
		make_path($outputdir);
		$specToPlatmk = $outputdir . "/specToPlat.mk";
		$jvmTestmk = $outputdir . "/jvmTest.mk";
	}

	specToPlatGen( $specToPlatmk, $graphSpecs );
	print "\nGenerated $specToPlatmk\n";

	jvmTestGen( $jvmTestmk, \%tests, $allSubsets );
	print "Generated $jvmTestmk\n";
	return \%tests;
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
		# temporarily exclude projects for CCM build (i.e., when JCL_VERSION is latest)
		my $latestDisabledDir = "jvmtitests proxyFieldAccess classesdbgddrext dumpromtests jep178staticLinkingTest pltest Panama NativeTest SharedCPEntryInvokerTests gcCheck classvertest";
		# Temporarily exclude SVT_Modularity tests from integration build where we are still using b148 JCL level
		my $currentDisableDir= "SVT_Modularity OpenJ9_Jsr_292_API";
		if ((($JCL_VERSION eq "latest") and ($latestDisabledDir !~ $entry )) or (($JCL_VERSION eq "current") and ($currentDisableDir !~ $entry ))) {
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

	if ($playlistXML) {
		print "\nGenerating make file based on $playlistXML...\n";
		generate($playlistXML, $absolutedir, \@currentdirs, \@subdirsHavePlaylist);
		return 1;
	} elsif (@subdirsHavePlaylist) {
		print "\nGenerating make file based on subdirs...\n";
		subdir2make($absolutedir, \@currentdirs, \@subdirsHavePlaylist, $allSubsets);
		return 1;
	}

	return 0;
}

sub generate {
	my ($playlistXML, $absolutedir, $currentdirs, $subdirsHavePlaylist) = @_;

	my $projectName      = $currentdirs->[-1];
	my $makeFile         = $absolutedir . "/" . $mkName;

	if ($output) {
		my $outputdir = $output;
		if (@{$currentdirs}) {
			$outputdir .= '/' . join('/', @{$currentdirs});
		}		make_path($outputdir);
		$makeFile = $outputdir . "/" . $mkName;
	}

	my $project = xml2make(	$playlistXML, $makeFile, $currentdirs, $subdirsHavePlaylist );

	$tests{$projectName} = $project;
}

sub xml2make {
	my ( $playlistXML, $makeFile, $currentdirs, $subdirsHavePlaylist )
	  = @_;
	$parseTests = [];
	parseXML($playlistXML);
	my $testgroups =
	  genMK( $makeFile, $parseTests, $currentdirs, $subdirsHavePlaylist );
	return $testgroups;
}

sub handle_start {
	my ($p, $elt) = @_;
	if ($elt eq 'test') {
		$eleStr = '';
		$parseTest = {};
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
	} elsif ($elt eq 'tags') {
		$eleStr = '';
		$eleArr = [];
		$currentEle = 'tags';
		$parentEle = 'tags';
	} elsif ($elt eq 'tag') {
		$eleStr = '';
		$currentEle = 'tag';
	}  elsif ($elt eq 'subsets') {
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
	if ($elt eq 'test') {
		# variation defaults to noOption
		if (!defined $parseTest->{'variation'}) {
			$parseTest->{'variation'} = [''];
		}
		# tag defaults to 'sanity'
		if (!defined $parseTest->{'group'}) {
			$parseTest->{'group'} = 'sanity';
		}
		# subset defaults to all
		if (!defined $parseTest->{'subsets'}) {
			$parseTest->{'subsets'} = $allSubsets;
		}
		push( @{$parseTests}, $parseTest );
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
	} elsif (($elt eq 'tag') && ($currentEle eq 'tag') && ($parentEle eq 'tags')) {
		push (@{$eleArr}, $eleStr);
	} elsif (($elt eq 'tags') && ($parentEle eq 'tags')) {
		foreach my $eachtag ( @{$eleArr} ) {
			if ( grep(/^$eachtag$/, @allGroups) ) {
				$parseTest->{'group'} = $eachtag;
				last;
			}
		}
	} elsif (($elt eq 'subset') && ($currentEle eq 'subset') && ($parentEle eq 'subsets')) {
		push (@{$eleArr}, $eleStr);
	} elsif (($elt eq 'subsets') && ($parentEle eq 'subsets')) {
		foreach my $eachsubset ( @{$eleArr} ) {
			if ( grep(/^$eachsubset$/, @{$allSubsets}) ) {
				push (@{$parseTest->{'subsets'}}, $eachsubset)
			} else {
				die "Error: unrecognized subset " . $eachsubset . " defined.";
			}
		}
	}
}

sub handle_char {
	my ($p, $str) = @_;
	$eleStr .= $str;
}

sub parseXML {
	my ( $playlistXML ) = @_;
	my $parser = new XML::Parser(Handlers => {Start => \&handle_start,
										End   => \&handle_end,
										Char  => \&handle_char});
	$parser->parsefile($playlistXML);
}

sub genMK {
	my ( $makeFile, $tests, $currentdirs, $subdirsHavePlaylist ) = @_;
	open( my $fhOut, '>', $makeFile ) or die "Cannot create make file $makeFile";
	my %project = ();
	print $fhOut $headerComments;

	my %testgroups = ();
	foreach my $test ( @{ $tests } ) {
		my $count     = 0;
		my $group = $test->{'group'};
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

			my @allInvalidSpecs = getAllInvalidSpecs( $invalidSpecs,
				$test->{'platformRequirements'}, $graphSpecs );

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
				print Dumper(\%capabilityReqs_Hash);
			}

			# generate make target
			my $name = $test->{'testCaseName'};
			$name .= "_$count";

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

			print $fhOut "$name: TEST_GROUP=level.$group\n";
			my $indent .= "\t";
			print $fhOut "$name:\n";
			print $fhOut "$indent\@echo \"\" | tee -a TestTargetResult;\n";
			print $fhOut "$indent\@echo \"===============================================\" | tee -a TestTargetResult;\n";
			print $fhOut "$indent\@echo \"Running test \$\@ ...\" | tee -a TestTargetResult;\n";
			print $fhOut "$indent\@echo \"===============================================\" | tee -a TestTargetResult;\n";
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
				print $fhOut "$indent\@echo \"test with $var\" | tee -a TestTargetResult;\n";
			}
			else {
				print $fhOut "$indent\@echo \"test with NoOptions\" | tee -a TestTargetResult;\n";
			}
			my $command = $test->{'command'};
			$command =~ s/^\s+//;
			$command =~ s/\s+$//;
			print $fhOut "$indent\{ $command; \} 2>&1 | tee -a TestTargetResult\n";

			if (defined($capabilityReqs)) {
				foreach my $key (keys %capabilityReqs_Hash) {
					print $fhOut "else\n";
					print $fhOut "$indent\$(TEST_SKIP_STATUS) | tee -a TestTargetResult\n";
					print $fhOut "endif\n";
				}
			}
			if ($condition_platform) {
				print $fhOut "else\n";
				print $fhOut "$indent\$(TEST_SKIP_STATUS) | tee -a TestTargetResult\n";
				print $fhOut "endif\n";
			}
			print $fhOut "\n";
			my %testElement = ();
			$testElement{"name"} = $name;
			$testElement{"invalidSpecs"} = \@allInvalidSpecs;

			foreach my $subset (@{$test->{'subsets'}}) {
				#push test name to subset group
				push(@{$testgroups{$group}{$subset}}, \%testElement);
			}
			$count++;
		}
	}

	my $dirtarget = join('.', @{$currentdirs});
	foreach my $subdir (sort @{$subdirsHavePlaylist}) {
		my $currdirstr = join("\$(D)", @{$currentdirs});
		if ($currdirstr) {
			$currdirstr .= "\$(D)";
		}
		print $fhOut "-include \$(JVM_TEST_ROOT)\$(D)" . $currdirstr . "$subdir\$(D)autoGenTest.mk\n";
	}

	foreach my $eachsubset (sort @{$allSubsets}) {
		my $isempty = 1;
		my $dirtarget_subset = $dirtarget . "_" . $eachsubset;
		print $fhOut "\n.PHONY: $dirtarget_subset\n\n";
		print $fhOut "$dirtarget_subset: ";
		foreach my $subdir (sort @{$subdirsHavePlaylist}) {
			print $fhOut "\\\n" . $dirtarget . "." . $subdir . "_" . $eachsubset . " ";
			$isempty = 0;
		}
		foreach my $eachgroup (sort keys %testgroups) {
			if ($testgroups{$eachgroup}{$eachsubset}) {
				foreach my $eachtestelement (sort {$a->{"name"} cmp $b->{"name"}} @{$testgroups{$eachgroup}{$eachsubset}}) {
					print $fhOut "\\\n$eachtestelement->{\"name\"} ";
					$isempty = 0;
				}
			}
		}
		if ($isempty == 1) {
			print $fhOut ";";
		}

		print $fhOut "\n";
	}

	$project{'testgroups'} = \%testgroups;

	close $fhOut;
	print "Generated $makeFile\n";

	return \%project;
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

sub specToPlatGen {
	my ( $specToPlatmk ) = @_;
	open( my $fhOut, '>', $specToPlatmk ) or die "Cannot create file $specToPlatmk";
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
}

sub jvmTestGen {
	my ( $jvmTestmk, $tests ) = @_;
	my %container = ();

	open( my $fhOut, '>', $jvmTestmk ) or die "Cannot create file $jvmTestmk";
	print $fhOut $headerComments;

	foreach my $projectName (sort keys %{$tests}) {
		my $testGroups = $tests->{$projectName}->{'testgroups'};
		foreach my $group (@allGroups) {
			if ( $testGroups->{$group} ) {
				my %subgroups = %{$testGroups->{$group}};
				foreach my $subset ( sort keys %subgroups ) {
					if (defined $subgroups{$subset}) {
						splice @{$container{$group}{$subset}}, 0, 0, @{$subgroups{$subset}};
					}
				}
			}
		}
	}

	my %hashSet = ();
	
	foreach my $group ( sort keys %container ) {
		$hashSet{$group} = 1;
		foreach my $testsubset ( @{$allSubsets} ) {
			my $targetName = $group . "_" . $testsubset;
			print $fhOut $targetName . ":";
			my $testElement = $container{$group}{$testsubset};
			if (defined $testElement) {
				foreach my $value ( sort {$a->{"name"} cmp $b->{"name"}} @{$testElement} ) {
					print $fhOut " \\\n" . $value->{"name"};
				}
			} else {
				print $fhOut " ;";
			}
			print $fhOut "\n\n";
		}
	}

	foreach my $group ( @allGroups ) {
		if (!exists $hashSet{$group}) {
			foreach my $testsubset ( @{$allSubsets} ) {
				my $targetName = $group . "_" . $testsubset;
				print $fhOut $targetName . ": ;\n\n";
			}
		}
	}
	
	close $fhOut;
}

sub subdir2make {
	my ($absolutedir, $currentdirs, $subdirsHavePlaylist) = @_;

	my $makeFile = $absolutedir . "/" . $mkName;
	if ($output) {
		my $outputdir = $output;
		if (@{$currentdirs}) {
			$outputdir .= '/' . join('/', @{$currentdirs});
		}
		make_path($outputdir);
		$makeFile = $outputdir . "/" . $mkName;
	}

	open( my $fhOut, '>', $makeFile ) or die "Cannot create make file $makeFile";
	my $mkname = basename($makeFile);
	my %project = ();
	print $fhOut $headerComments;

	foreach my $subdir (sort @{$subdirsHavePlaylist}) {
		my $currdirstr = join("\$(D)", @{$currentdirs});
		if ($currdirstr) {
			$currdirstr .= "\$(D)";
		}
		print $fhOut "-include \$(JVM_TEST_ROOT)\$(D)" . $currdirstr . "$subdir\$(D)$mkname\n";
	}

	print $fhOut "\n";

	my $dirtarget = 'alltargets';
	if (@{$currentdirs}) {
		$dirtarget = join('.', @{$currentdirs});
	}

	foreach my $eachsubset (sort @{$allSubsets}) {
		my $dirtarget_subset = $dirtarget . "_" . $eachsubset;
		print $fhOut "\n.PHONY: $dirtarget_subset\n\n";
		print $fhOut "$dirtarget_subset: ";
		foreach my $subdir (sort @{$subdirsHavePlaylist}) {
			if ($dirtarget eq 'alltargets') {
				print $fhOut "\\\n" . $subdir . "_" . $eachsubset . " ";
			} else {
				print $fhOut "\\\n" . $dirtarget . "." . $subdir . "_" . $eachsubset . " ";
			}
		}
		print $fhOut "\n";
	}

	close $fhOut;
	print "Generated $makeFile\n";
}

1;
