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
# rem @author Radhakrishnan Thangamuthu
# rem @summary check if a cache is not created with a length  
# rem @summary greater than the allowed length. The cache name used for testing contains %u

require "sharedClassesUtil.pl";

use strict;
use warnings;

sub nameOption18test{
	my ($java_bin, $cache_max_len_string)=@_;

	my $test_name = "nameOption18";	
	my $user_name=get_user_name();
	my $append_token = get_short_string_for_user();
	my $expanded_token=$user_name;
	my $error_msg=copy_username_err_msg ();

	# increment the length by 1 and convert to string 
	my $new_cache_len = ($cache_max_len_string + 0 ) + 1;
    my $new_cache_len_string = $new_cache_len . ' ';
	long_cache_name_fail_cases_test( $java_bin, $test_name, $new_cache_len_string,
		$error_msg, $append_token, $expanded_token);
}		
	
nameOption18test((@ARGV));
