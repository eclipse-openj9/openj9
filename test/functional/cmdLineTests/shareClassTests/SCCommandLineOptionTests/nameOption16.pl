#
# Copyright (c) 2009, 2019 IBM Corp. and others
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
#

# @test 1.0
# @outputpassed
# @author Radhakrishnan Thangamuthu
# @summary check if a cache can be created with maximum allowed length
# @summary and that the destroy command works

use lib ".";
require "sharedClassesUtil.pl";

use strict;
use warnings;

sub nameOption16test{
	my ($java_exe, $cache_max_len_string)=@_;
	my $test_name = "nameOption16";
	cache_name_with_fixed_length_test( $java_exe, $test_name, $cache_max_len_string);
}		
	
nameOption16test((@ARGV));
