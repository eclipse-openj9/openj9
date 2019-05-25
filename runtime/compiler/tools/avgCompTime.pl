##############################################################################
#  Copyright (c) 2019, 2019 IBM Corp. and others
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

#script to compute the average compilation time for methods in the vlog

use strict;
use File::Find;

my $compTimeThreshold = 1000000; # 100 ms

if (@ARGV < 1) {
    die "must provide at least one parameter: vlog_file\n";
}


# hash of methods; each entry is an array of lines containing all occurrences of that method in the vlog
my %methodHash;


# process each file given as argument
my $totalCompTime = 0;
foreach (map { glob } @ARGV) {
  my $compTime =  processVlog($_);
  $totalCompTime += $compTime/1000; # convert to ms
}
print "total compilation time = $totalCompTime ms\n";




sub UpdateStats {
    $_[1] += 1;  # update samples
    $_[2] += $_[0];   # update totalTime
    if ($_[0] > $_[3]) { #update MAX
        $_[3] = $_[0];
    }
    if ($_[0] < $_[4]) { #update MIN
        $_[4] = $_[0];
    }
}

# subroutine to print the stats for an array
sub PrintStats {
    my ($hotness, $samples, $sum, $maxVal, $minVal, $stddev) = @_;
    if ($samples > 0) {
        #                                                   samples max         sum         avg          min
        printf "$hotness\t%d\t%.1f\t%.1f\t\t%.0f\t\t%.0f\n", $samples, $maxVal/1000, $sum/1000, $sum/$samples, $minVal;
    }
}


sub processVlog {
   # samples, sum, max, min, stddev
   my @noopt=     (0,0,0,10000000,0);
   my @aotload=   (0,0,0,10000000,0);
   my @aotcold=   (0,0,0,10000000,0);
   my @aotwarm=   (0,0,0,10000000,0);
   my @aothot=    (0,0,0,10000000,0);
   my @cold=      (0,0,0,10000000,0);
   my @warm=      (0,0,0,10000000,0);
   my @hot=       (0,0,0,10000000,0);
   my @phot=      (0,0,0,10000000,0);
   my @veryhot=   (0,0,0,10000000,0);
   my @pveryhot=  (0,0,0,10000000,0);
   my @scorching= (0,0,0,10000000,0);
   my @jni=       (0,0,0,10000000,0);
   my @unknown=   (0,0,0,10000000,0);
   my @failures=  (0,0,0,10000000,0);
   my @allt=      (0,0,0,10000000,0);
   my $numGCR = 0;
   my $numGCRNoServer = 0;
   my $numPushy = 0;
   my $numDLT = 0;
   my $numSync = 0;
   my $numColdUpgrade = 0;
   my $numForcedAOTUpgrade = 0;
   my $numAOTUpgrade = 0;
   my $numNonAOTUpgrade = 0;
   my $numInvalidate = 0;
   my $numFailures = 0;
   my $max = 0;
   my $maxLine = "";
   my $maxQSZ = "";
   my $numAOTLoadsNotRecompiled = 0;
   my $lastTimestamp = 0; # in ms since JVM start
   # clear the hash
   %methodHash = ();

   my $vlog_filename = shift;
   open(VLOG, $vlog_filename) || die "Cannot open $vlog_filename: $!\n";
   my @vlog_lines = <VLOG>; # read all lines from VLOG

   # process each line from the file
   foreach my $line (@vlog_lines) {
      # filter the lines that start with +. 
      my $compLevel = "";
      my $methodName = "";
      my $usec = 0;
      my $matchFound = 0;
      if ($line =~ /\+ \((.+)\) (\S+) \@ .+ time=(\d+)us/) {
         $compLevel = $1;
         $methodName = $2;
         $usec = $3;
         $matchFound = 1;
      }
      # Note that AOT loads have a different format:
      elsif ($line =~ /\+ \(AOT load\) (\S+).+\@ /) {
         $compLevel = "AOT load";
         $methodName = $1;
         $matchFound = 1;
      }
      # If the method name is very long there might be no space between the method name and location in memory
      elsif ($line =~ /\+ \((.+)\) (\S+)0x[0-9A-F].+time=(\d+)us/) {
         $compLevel = $1;
         $methodName = $2;
         $usec = $3;
         $matchFound = 1;
      }
      elsif($line =~ /^\! (\S+) time=(\d+)us/) {
         $usec = $2;
	 &UpdateStats($usec, @allt);
	 &UpdateStats($usec, @failures);
      }

     
      if ($matchFound) {
      
         if ($usec > $max) {
            $max = $usec;
            $maxLine = $line;
         }
         
         # add the method to a hash where the method name is the key
         if (exists $methodHash{$methodName}) {
            # retrieve the array ref from the hash and then do a push
            push(@{$methodHash{$methodName}}, $line);
         }
         else { # first instance in the hash
            # add an anonymous array ref to the hash
            $methodHash{$methodName} = [$line];
         }
         
         if ($usec > $compTimeThreshold) {
            print $line;
         }

         &UpdateStats($usec, @allt);
         if ($line =~ / JNI /) {
            &UpdateStats($usec, @jni);
         }elsif ($compLevel eq "warm") {
            &UpdateStats($usec, @warm);
         }elsif ($compLevel eq "cold") {
            &UpdateStats($usec, @cold);
         }elsif ($compLevel eq "AOT load") {
            &UpdateStats($usec, @aotload);
         }elsif ($compLevel eq "AOT cold") {
            &UpdateStats($usec, @aotcold);
         }elsif ($compLevel eq "AOT warm") {
            &UpdateStats($usec, @aotwarm);
	      }elsif ($compLevel eq "AOT hot") {
            &UpdateStats($usec, @aothot);
         }elsif ($compLevel eq "hot") {
            &UpdateStats($usec, @hot);
         }elsif ($compLevel eq "profiled hot") {
            &UpdateStats($usec, @phot);
         }elsif ($compLevel eq "very-hot") {
            &UpdateStats($usec, @veryhot);
         }elsif ($compLevel eq "profiled very-hot") {
            &UpdateStats($usec, @pveryhot);
         }elsif ($compLevel eq "scorching") {
            &UpdateStats($usec, @scorching);
         }elsif ($compLevel eq "no-opt") {
            &UpdateStats($usec, @noopt);
         }else {
            &UpdateStats($usec, @unknown);
         }
         # look at the reason for recompilation
         if ($line =~ / G /) {
            $numGCR++;
         }
         elsif ($line =~ / g /) {
            $numGCRNoServer++;
         }
         elsif ($line =~ / P /) {
            $numPushy++;
         }
         elsif ($line =~ / I /) {
            $numInvalidate++;
         }
         elsif ($line =~ / C /) {
            $numColdUpgrade++;
         }
		 if ($line =~ / A /) {
            $numForcedAOTUpgrade++;
         }
         if ($line =~ / sync /) {
            $numSync++;
         }
         if ($line =~ / DLT /) {
            $numDLT++;
         }
         if ($line =~ /Q\_SZ=(\d+)/) {
            if ($1 > $maxQSZ) {
               $maxQSZ = $1;
            }
         }
      }
      elsif ($line =~ /^\! /) { # lines starting with !
            $numFailures++;
      }
      elsif ($line =~ /^\+/) { # lines that start with '+' and haven't been tracked by now are errors
         print "Unaccounted line: $line";
      }
      # Track timestamps
      if ($line =~ /\st=\s*(\d+)\s/) {
         $lastTimestamp = $1;
      }
   }
   print "$vlog_filename\n";
   print  "\tSamples MAX(ms) TOTAL(ms)\tAVG(usec)\tMIN(usec)\n";
   &PrintStats("Total", @allt);
   &PrintStats("aotl", @aotload);
   &PrintStats("noopt", @noopt);
   &PrintStats("cold", @cold);
   &PrintStats("aotc", @aotcold);
   &PrintStats("aotw", @aotwarm);
   &PrintStats("aoth", @aothot);
   &PrintStats("warm", @warm);
   &PrintStats("hot", @hot);
   &PrintStats("phot", @phot);
   &PrintStats("vhot", @veryhot);
   &PrintStats("pvhot", @pveryhot);
   &PrintStats("scorc", @scorching);
   &PrintStats("jni", @jni);
   &PrintStats("unkn", @unknown);
   &PrintStats("fails", @failures);

   
   # compute how many upgrades come from AOT and how many from JIT
   for my $methodName (keys %methodHash) {          # for each method
      my $compArrayRef = $methodHash{$methodName};  # get its table of compilations
      my $numComp = scalar(@$compArrayRef);         # number of compilations for this method
      for (my $i = $numComp-1; $i >= 0; $i--) {     # for each compilation of said method
         if ($compArrayRef->[$i] =~ / C /) {        # if the compilation is a cold upgrade
            #print $compArrayRef->[$i];
            if ($i == 0) {                          # cold upgrades cannot appear first because thei are recompilations
               print "Some error occurred for" . $compArrayRef->[$i];
            } else {
               if ($compArrayRef->[$i-1] =~ /\+ \(AOT load\)/) {
                  $numAOTUpgrade++;
               } else {
                  $numNonAOTUpgrade++;
               }
               #print $compArrayRef->[$i-1] . "\n";
            }
            next;
         }
      }
      # now look to see how many AOT loads were recompiled
      if ($compArrayRef->[0] =~ /AOT load/) { # this is an AOT load
         # if there are other compilations, then they are recompilations
         if ($numComp == 1) {
            $numAOTLoadsNotRecompiled++;
         }
      }
   }  
   

   print "MAXLINE=$maxLine";
   print "numCGR   recompilations = $numGCR\n";
   print "numgcr   recompilations = $numGCRNoServer\n";
   print "pushy    recompilations = $numPushy\n";
   print "coldupgr recompilations = $numColdUpgrade (AOT=$numAOTUpgrade, JIT=$numNonAOTUpgrade\n";
   print "forcedAOTupgrades       = $numForcedAOTUpgrade\n";
   print "numAOTLoadsNotRecompiled= $numAOTLoadsNotRecompiled\n";
   print "inv      recompilations = $numInvalidate\n";
   print "sync compilations = $numSync\n";
   print "DLT compilations  = $numDLT\n";
   print "numFailures       = $numFailures\n";
   print "max_Q_SZ          = $maxQSZ\n";
   print "lastTimestamp     = $lastTimestamp\n"; 
  
   
   return $allt[1];
}

