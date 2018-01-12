#!/usr/bin/perl

# Copyright (c) 2009, 2017 IBM Corp. and others
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

# Script to build an HTML table from the output of type_conversion_explorer.
#
# Usage:
# type_conversion_explorer.exe | perl prettify_type_conversion_explorer_output.pl
#
# Output will appear on the console.

use strict;
use warnings;

my %type_by_letter;
my %letter_by_type;
my %combinations;

my $state = 'reading_type_codes';

while (my $line = <STDIN>) {
   chomp $line;

   if ($state eq 'reading_type_codes') {
      if ($line eq 'Combinations:') {
         $state = 'reading_combinations';
      } elsif ($line eq 'Type codes:') {
         next;
      } elsif ($line =~ /(.+?): (.+)/) {
         my $type = $1;
         my $letter = $2;
 #        print "Read $type == $letter\n";
         $type_by_letter{$letter} = $type;
         $letter_by_type{$type} = $letter;
         $combinations{$letter} = {};
      } else {
         die "Don't recognise $line";
      }
   } elsif ($state eq 'reading_combinations') {
      if ($line =~ /(.+?) \+ (.+?): (.+)/) {
         my ($one,$two,$result) = ($1,$2,$3);

         $combinations{$one}->{$two} = $result;
      }
   } else {
      die "Unknown state: $state";
   }
}

#print "End state = $state\n";

my @types = ('U_8', 'U_16', 'U_32', 'U_64', 'I_8', 'I_16', 'I_32', 'I_64');

print "<table border=\"1\">\n";
print "<tr>\n";
print '<th>&nbsp;</th>';
foreach my $one (@types) {
   print "<th>$one</th>";
}
print "\n";
print "</tr>\n";
foreach my $one (@types) {
   print "<tr>\n";
   print "<th>$one</th>";
   foreach my $two (@types) {
      my $one_letter = $letter_by_type{$one};
      die "No Didn't match letter for $one" unless defined $one_letter;
      my $two_letter = $letter_by_type{$two};
      die "No Didn't match letter for $two" unless defined $two_letter;
      die "Unmatched $one_letter" unless defined $combinations{$one_letter};
      my $letter = $combinations{$one_letter}->{$two_letter};
      die "Error, no letter: $one_letter, $two_letter\n" unless defined $letter;
      my $type = $type_by_letter{$letter};
      die "Error! $one, $two, $one_letter, $two_letter, $letter" unless defined $type;
      print "<td>$type</td>";
   }

   print "</tr>\n";
}
print "</table>\n";
