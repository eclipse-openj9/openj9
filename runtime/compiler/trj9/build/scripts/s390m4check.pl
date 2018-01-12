#!/bin/perl

# Copyright (c) 2000, 2017 IBM Corp. and others
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

use warnings;
use strict;
use bytes;    # avoid using Unicode internally

die "usage: $0 filename\n" unless @ARGV == 1;
my $inputFile = pop @ARGV;
open FILE, $inputFile or die "unable to open file $inputFile";

my $MAX_LENGTH = 71;
my $is_m4      = ( $inputFile =~ m/\.m4$/ );
my $is_ascii   = 1 if ord('[') == 91;

while (<FILE>)
   {
   chomp;
   my $line   = $_;
   my $length = length($line);
   die "line longer than $MAX_LENGTH characters ($length)"
      if ( $length > $MAX_LENGTH );

   # check that only valid characters show up in the string
   # %goodchars is a hash of characters allowed
   my %goodchars = $is_ascii ? 
      map { $_ => 1 } ( 10, 13, 31 .. 127 ) : # ascii
      map { $_ => 1 } ( 13, 21, 64 .. 249 );  # ebcdic
   my $newline = $line;
   $newline =~ s/(.)/$goodchars{ord($1)} ? $1 : ' '/eg;
   die "invalid character(s)"
      if $newline ne $line;
      
   my $comment_regex =
        $is_m4    ? "ZZ"  # M4
      : $is_ascii ? "#"   # GAS
      : "\\*\\*";         # HLASM 

   # skip comment lines
   next if $line =~ m/^$comment_regex/;
   die "comment lines should not have preceding whitespace"
      if $line =~ m/^\s+$comment_regex/;

   # strip the comments out
   $line =~ s/$comment_regex.*$//;

   # the # character is considered a comment everywhere
   $line =~ s/\#.*$//;

   die "comma in first column"    if $line =~ m/^,/;
   die "whitespace next to comma" if $line =~ m/\s,|,\s/;
   die "whitespace within parentheses"
      if $line =~ m/\(.*\s.*\)/;
   die "whitespace before parentheses"
      if $line =~ m/,\s+\(/;
   }
