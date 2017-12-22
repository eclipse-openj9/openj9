#
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
#

# @test 1.0
# @outputpassed
# @author Radhakrishnan Thangamuthu
# @summary check if a cache can be created with maximum allowed length
# @summary and that the destroy command works. The cache name used for
# @summary testing contains %g.

require "sharedClassesUtil.pl";

use strict;
use warnings;

sub nameOption15test{
	my ($java_bin,$cache_max_len_string)=@_;
	my $test_name = "nameOption15";

	my $append_token=get_short_string_for_group();
#   expanded_token contains the expansion of chars stored in append_token
	my $expanded_token="";
	
	if (is_windows_OS( )){
		my $error_msg = "Escape character g not valid for cache name";
		$expanded_token= "  "; #  dummy string with the same length as append_token
		long_cache_name_fail_cases_test($java_bin, $test_name, $cache_max_len_string, $error_msg,
			$append_token, $expanded_token);
	}else {
		my $group_name=get_group_name();
		$expanded_token = $group_name;
		cache_name_with_fixed_length_test($java_bin, $test_name, $cache_max_len_string, $append_token, 
			$expanded_token);
	}
}		
	
nameOption15test((@ARGV));
