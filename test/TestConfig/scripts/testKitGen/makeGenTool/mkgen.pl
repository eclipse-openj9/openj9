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
use warnings;
use Data::Dumper;
use feature 'say';

use constant DEBUG => 0;

my @allGroups = ( "sanity", "extended", "openjdk", "external", "perf", "jck" );

my $headerComments =
	"########################################################\n"
	. "# This is an auto generated file. Please do NOT modify!\n"
	. "########################################################\n"
	. "\n";

my $mkName = "autoGenTest.mk";
my $projectRootDir = '';
my $allSubsets = '';
my $output = '';
my $graphSpecs = '';
my $javaVersion = '';
my $modes_hs = '';
my $sp_hs = '';
my %tests = ();

sub runmkgen {
	( $projectRootDir, $allSubsets, $output, $graphSpecs, $javaVersion, my $modesxml, my $ottawacsv ) = @_;
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

	jvmTestGen( $jvmTestmk, \%tests );
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
		my $tempExclude = 0;
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

	if ($playlistXML) {
		print "\nGenerating make file based on $playlistXML...\n";
		generate($playlistXML, $absolutedir, \@currentdirs, \@subdirsHavePlaylist);
		return 1;
	} elsif (@subdirsHavePlaylist) {
		print "\nGenerating make file based on subdirs...\n";
		subdir2make($absolutedir, \@currentdirs, \@subdirsHavePlaylist);
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

	#clean stale makefile before generating
	unlink $makeFile or warn "Cannot not unlink $makeFile: $!";
	my $project = xml2make(	$playlistXML, $makeFile, $currentdirs, $subdirsHavePlaylist );

	$tests{$projectName} = $project;
}

sub xml2make {
	my ( $playlistXML, $makeFile, $currentdirs, $subdirsHavePlaylist )
	  = @_;
	my $tests = parseXML($playlistXML);
	my $testgroups =
	  genMK( $makeFile, $tests, $currentdirs, $subdirsHavePlaylist );
	return $testgroups;
}

sub parseXML {
	my ($playlistXML) = @_;
	open( my $fhIn, '<', $playlistXML ) or die "Cannot open file $_[0]";
	my @tests = ();
	while ( my $line = <$fhIn> ) {
		my %test;
		my $testlines;
		if ( $line =~ /\<test\>/ ) {
			$testlines .= $line;
			while ( my $testline = <$fhIn> ) {
				$testlines .= $testline;
				if ( $testline =~ /\<\/test\>/ ) {
					last;
				}
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

			my $tags = getElementByTag( $testlines, 'tags' );
			my $tag = getElementsByTag( $testlines, 'tag' );
			foreach my $eachtag ( @{$tag} ) {
				if ( grep(/^$eachtag$/, @allGroups) ) {
					$test{'group'} = $eachtag;
					last;
				}
			}
			# defaults to 'sanity'
			if (!defined $test{'group'}) {
				$test{'group'} = 'sanity';
			}

			my $subsets = getElementsByTag( $testlines, 'subset' );
			# defaults to all subsets
			if (!$subsets) {
				$subsets = $allSubsets;
			}
			# do not generate make taget if subset doesn't match javaVersion
			if ( !grep(/^$javaVersion$/, @{$subsets}) ) {
				next;
			}
			push( @tests, \%test );
		}
	}
	close $fhIn;
	return \@tests;
}

sub getElementByTag {
	my ($element) = $_[0] =~ /\<$_[1]\>(.*)\<\/$_[1]\>/s;
	return $element;
}

sub getElementsByTag {
	my (@elements) = $_[0] =~ /\<$_[1]\>(.*?)\<\/$_[1]\>/sg;
	return \@elements;
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
					print $fhOut "$indent\@echo \"Skipped due to capabilities ($capabilityReqs) => \$(TEST_SKIP_STATUS)\" | tee -a TestTargetResult\n";
					print $fhOut "endif\n";
				}
			}
			if ($condition_platform) {
				print $fhOut "else\n";
				if (defined($platformRequirements)) {
					print $fhOut "$indent\@echo \"Skipped due to jvm options (\$(JVM_OPTIONS)) and/or platform requirements ($platformRequirements) => \$(TEST_SKIP_STATUS)\"\n";
				} else{
					print $fhOut "$indent\@echo \"Skipped due to jvm options (\$(JVM_OPTIONS)) => \$(TEST_SKIP_STATUS)\"\n";
				}
				print $fhOut "endif\n";
			}
			print $fhOut "\n";
			my %testElement = ();
			$testElement{"name"} = $name;
			$testElement{"invalidSpecs"} = \@allInvalidSpecs;

			push(@{$testgroups{$group}}, \%testElement);
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

	my $isempty = 1;
	print $fhOut "\n.PHONY: $dirtarget\n\n";
	print $fhOut "$dirtarget: ";
	foreach my $subdir (sort @{$subdirsHavePlaylist}) {
		print $fhOut "\\\n" . $dirtarget . "." . $subdir . " ";
		$isempty = 0;
	}
	foreach my $eachgroup (sort keys %testgroups) {
		if ($testgroups{$eachgroup}) {
			foreach my $eachtestelement (sort {$a->{"name"} cmp $b->{"name"}} @{$testgroups{$eachgroup}}) {
				print $fhOut "\\\n$eachtestelement->{\"name\"} ";
				$isempty = 0;
			}
		}
	}
	if ($isempty == 1) {
		print $fhOut ";";
	}

	print $fhOut "\n";


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
				splice @{$container{$group}}, 0, 0, @{$testGroups->{$group}};
			}
		}
	}

	my %hashSet = ();
	
	foreach my $group ( sort keys %container ) {
		$hashSet{$group} = 1;
		print $fhOut "\n.PHONY: $group\n\n";
		print $fhOut $group . ":";
		my $testElement = $container{$group};
		if (defined $testElement) {
			print $fhOut " \\\nsetup_" . $group;
			print $fhOut " \\\nrmResultFile";
			foreach my $value ( sort {$a->{"name"} cmp $b->{"name"}} @{$testElement} ) {
				print $fhOut " \\\n" . $value->{"name"};
			}
			print $fhOut " \\\nresultsSummary";
		} else {
			print $fhOut " ;";
		}
		print $fhOut "\n\n";
		
	}

	foreach my $group ( @allGroups ) {
		if (!exists $hashSet{$group}) {
			print $fhOut $group . ": ;\n\n";
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

	#clean stale makefile before generating
	unlink $makeFile or warn "Cannot not unlink $makeFile: $!";
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

	print $fhOut "\n.PHONY: $dirtarget\n\n";
	print $fhOut "$dirtarget: ";
	foreach my $subdir (sort @{$subdirsHavePlaylist}) {
		if ($dirtarget eq 'alltargets') {
			print $fhOut "\\\n" . $subdir . " ";
		} else {
			print $fhOut "\\\n" . $dirtarget . "." . $subdir . " ";
		}
	}
	print $fhOut "\n";

	close $fhOut;
	print "Generated $makeFile\n";
}

1;
