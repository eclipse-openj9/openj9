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
# rem @summary collection of perl functions used in share classes testing.

use warnings;
use strict;

# greps the word in the dump file and returns true if the number of occurence
# of the word matches with the number passed in as arg
sub words_grep_from_file {
	my ($dump_file, $grep_word, $num_of_expected_occurences) = @_;
	my $counter = 0;

	open(my $in,  "<",  $dump_file)  or die "TEST FAILED. unable to  read $dump_file";

	while ( <$in> ) {
		if( index($_, $grep_word) >= 0 ) {
			 $counter++;
		}
	}

	close($in)  or die "TEST FAILED. unable to  close $dump_file";
	return ( $counter == $num_of_expected_occurences );
}

# removes a file if it exists and if it is not writable
sub remove_file_if_not_writable{
	my ($filename)=@_;

	if ( -e $filename && !(-w $filename)) {
		unlink $filename || die "TEST FAILED. unable to delete file $filename";
	}

	return 1;
}

# for a given test name get the corresponding dump file name .
# dump file is used to write stdout/stderr messages.
sub get_dump_file {
	my  ($test_name) = @_;
	return $test_name . ".out";
}

sub get_user_name {
	my $username="";

	if(is_windows_OS()) {
		# getpwuid did not work in Windows.
		# USERNAME environment variable is used if getlogin fails
		$username= getlogin() ||  $ENV{USERNAME};
	}else {
		# When the test was run as user j9build,
		# In AIX 6.1
		# getpwuid($<) returns "root" instead of "j9build" when invoked from java program.
		# In AIX 5.3
		# getpwuid($<) returns "j9build" when invoked from java program.
		# Test runs in AIX 5.3 correctly as expected while AIX 6.1 does not.
		# When java program calls a perl script that makes a call to getlogin( )
		# , getlogin( ) returns undefined.
		#
		# So a possible fix for CMVC defect:157110 is to
		# use variable $LOGNAME or effective user id $>.
		# We are intentionally not using real user id $< to avoid problem in AIX 6.1
		$username = $ENV{LOGNAME} || getpwuid($>);
	}

	if (!defined($username) || ($username eq "")) {
		die "\n unable to get username. TEST FAILED \n";
	}

	return $username;
}

sub get_group_name {
	if(is_windows_OS()) {
		# no group name in Windows;
		return "  ";
	}

	#execute command id and collect the output;
	my @output_list=`id`;
	my $search_line=$output_list[0];
	my $gid_str = "gid=";

	my $pos = index($search_line,"gid=");

	#search for gid string and then '(' char and then ')'.
	#Then return return the string enclosed the string in braces.

	if( $pos <  0) {
		die "unable to get group name ";
	}

	my $left_brace_pos = index($search_line,"(",$pos);

	if( $left_brace_pos <  0) {
		die "unable to get group name ";
	}

	my $right_brace_pos = index($search_line,")",$left_brace_pos);

	if( $right_brace_pos <  0) {
		die "unable to get group name ";
	}else {
		$pos = $right_brace_pos;
	}

	return substr($search_line, $left_brace_pos + 1, $right_brace_pos - $left_brace_pos - 1);
}

# get the postfix string to be appended to a command
sub get_postfix_string {
	my ($is_err_stream_needed, $is_output_stream_needed, $dump_file) = @_;
	my $postfix_string = "";

	if( $is_err_stream_needed && $is_output_stream_needed )  {
		$postfix_string =  "   > " . $dump_file . " 2>&1";
	}elsif($is_output_stream_needed ) {
		$postfix_string =  "  1> " . $dump_file;
	}elsif($is_output_stream_needed ) {
		$postfix_string =  "  2> " . $dump_file;
	}else {
		$postfix_string =  "  ";
	}

	return $postfix_string;
}

# read the lines from file and return the array of lines
sub read_lines_from_file {
	my ($dump_file) = @_;

	my $in;
	open($in,  "<",  $dump_file)  or die "TEST FAILED. unable to  read $dump_file";
	my @lines = <$in>;
	close($in)  or die "TEST FAILED. unable to  close $dump_file";

	return @lines;
}

# get the command to list all caches
sub get_listAllCaches_command{
	my ($java_bin,$test_name) = @_;
	my $postfix_string="";
	my $cmd_to_execute="";
	my $dump_file = get_dump_file($test_name) ;
	my $cmd = "$java_bin -Xshareclasses:listAllCaches";

	$postfix_string = get_postfix_string(1,1, $dump_file);
	$cmd_to_execute=$cmd . $postfix_string;

	return $cmd_to_execute;
}

# get the command to create cache
sub get_createCache_command {
	my ($java_bin,$cache_name,$test_name,$is_err_stream_needed,$is_output_stream_needed) = @_;
	my $postfix_string="";
	my $cmd_to_execute="";
	my $dump_file = get_dump_file($test_name);
	my $cmd = $java_bin;

	$cmd = $cmd . " -Xshareclasses:name=$cache_name HelloWorld ";

	$postfix_string = get_postfix_string($is_output_stream_needed, $is_err_stream_needed, $dump_file);
	$cmd_to_execute=$cmd . $postfix_string;

	return $cmd_to_execute;
}

# get the command to print cache stats
sub get_printStats_command {
	my ($java_bin,$cache_name,$test_name) = @_;
	my $postfix_string="";
	my $cmd_to_execute="";
	my $is_output_stream_needed = 1;
  	my $is_err_stream_needed = 1;
	my $dump_file = get_dump_file($test_name) ;
	my $cmd = "$java_bin -Xshareclasses:name=$cache_name,printStats ";

	$postfix_string = get_postfix_string($is_output_stream_needed, $is_err_stream_needed, $dump_file);
	$cmd_to_execute=$cmd . $postfix_string;

	return $cmd_to_execute;
}

# create a cache with the given cache_name
sub do_create_cache {
	my ($java_bin,$cache_name,$test_name,$is_err_stream_needed,$is_output_stream_needed) = @_;
	my @lines = undef;
	my $dump_file = get_dump_file($test_name);

	my $cmd_to_execute = " ";

	$cmd_to_execute = get_createCache_command($java_bin,$cache_name,
						$test_name,$is_err_stream_needed
						,$is_output_stream_needed);

	#leaving debug messages commented
	printf "\n command to execute :$cmd_to_execute: \n" ;

	if( !$is_output_stream_needed && !$is_err_stream_needed ) {
		`$cmd_to_execute`;
	} else {
		remove_file_if_not_writable($dump_file);

		#execute the command to be tested
		`$cmd_to_execute`;

		@lines = read_lines_from_file($dump_file);
	}

	return @lines;
}

# destroy the specified shared class cache
sub do_destroy_cache {
	my ($java_bin,$test_name, $cache_name) = @_;
	my $cmd = "$java_bin -Xshareclasses:name=$cache_name,destroy " . " > "  . get_dump_file($test_name) . " 2>&1";

	`$cmd`;
}

# returns true if a shared class cache name is present
sub is_cache_present {
	my ($java_bin,$cache_name,$test_name) = @_;
	my $cmd_to_execute = get_listAllCaches_command($java_bin, $test_name);
	my $dump_file = get_dump_file($test_name);

	remove_file_if_not_writable($dump_file);
	#execute the command to be tested
	`$cmd_to_execute`;

	return words_grep_from_file($dump_file, $cache_name, 1);
}

# returns true if a shared class cache name is no more in the list of available caches.
sub is_cache_absent{
	my ($java_bin,$cache_name,$test_name) = @_;
	my $cmd_to_execute = get_printStats_command($java_bin,$cache_name, $test_name);
	my $dump_file = get_dump_file($test_name);

	remove_file_if_not_writable($dump_file);

	#execute the command to be tested
	`$cmd_to_execute`;

	return words_grep_from_file($dump_file, $cache_name, 0);
}

# returns true if OS is Windows
sub is_windows_OS {
	return (index($^O,"MSWin") >= 0);
}

# returns the path separator
sub get_path_separator {
	if ( is_windows_OS( )) {
		return "\\";
	}

	return "/";
}

# Given a cache name it validates that the fact creating the cache with the given name
# fails with the error message specified in the argument list
sub test_unsuccessful_cache_creation {
	my ($java_bin, $test_name, $cache_name, $error_msg)=@_;
	my $return_status;
	my $create_status;
	my $destroy_status;
	my $dump_file = get_dump_file($test_name);
	my $is_create_failed=0;

	do_create_cache($java_bin, $cache_name, $test_name, 1, 1);

	$is_create_failed =words_grep_from_file($dump_file, $error_msg, 1);

	if ($is_create_failed) {
		$return_status = 1;
		printf "\n TEST PASSED \n";
	} else 	{
		$return_status = 0;
		printf "\n TEST FAILED : missing error message \n";
	}

	return $return_status;
}

# Given a cache name it checks for the creation of the cache and declares pass/fail.
sub test_successful_cache_creation {
	my ($java_bin, $test_name, $cache_name, $cache_grep_name)=@_;
	my $return_status=0;
	my $create_status=0;
	my $destroy_status=0;

	do_create_cache($java_bin, $cache_name,$test_name, 1, 0);

	$create_status=is_cache_present($java_bin, $cache_grep_name,$test_name);

	if ($create_status) {
		do_destroy_cache($java_bin, $test_name, $cache_name) ;
		$destroy_status = is_cache_absent($java_bin, $cache_name,$test_name);
	}

	if ($create_status && $destroy_status) {
		$return_status = 1;
		printf "\n TEST PASSED \n";
	} else 	{
		$return_status = 0;
		printf "\n TEST FAILED : create_status:$create_status destroy_status:$destroy_status \n";
	}

	return $return_status;
}

#  Used to generate any cache name of length less than 101.
#  current allowed max cache name length is 64.
#  cache_prefix is trimmed and add_token is appended to the
#  end of the resultant string such that length of the final string is
#  equal to requested length specified in the args
#  expanded_token contains the string that would result if add_token is expanded.
#  eg: if "%u" is add_token then expanded_token should be given the value of username.
#  if "%g" is add_token then expanded_token should be given the value "  "
sub get_cache_name {
	my ($reqd_len,$add_token,$expanded_token,$cache_prefix)=@_;

	if(!defined($reqd_len)) {
		$reqd_len = 64;
	}

	if(!defined($add_token)) {
		$add_token = "";
	}

	if(!defined($expanded_token)) {
		$expanded_token = "";
	}

	if(!defined($cache_prefix))  {
		#100 char string
		$cache_prefix= "123456789A123456789B123456789C123456789D123456789E123456789F123456789G123456789H123456789I123456789J";
	}

	my $expanded_token_len = length($expanded_token);
	my $cache_prefix_len = length($cache_prefix);

	# number of characters to trim on the right hand side of cache prefix
	my $left_trim_char_count = $expanded_token_len  - ( $reqd_len - $cache_prefix_len );
	my $right_trim_char_count = $cache_prefix_len - $left_trim_char_count;

	# leaving debug messages commented
	# printf " reqd_len = $reqd_len cache_prefix_len = $cache_prefix_len  ltrimmed = $left_trim_char_count \n";
	my $new_cache_prefix = substr($cache_prefix,0, $right_trim_char_count) ;

	# name of the cache name to be specified in the command line
	my $new_cache_name = $new_cache_prefix . $add_token;

	# name of the cache name to be created when the new_cache_name is specified in the command line
	my $new_expanded_cache_name = $new_cache_prefix . $expanded_token;

	my @arr={ "" , ""};
   	$arr[0] = $new_cache_name;
	$arr[1] = $new_expanded_cache_name;
	return @arr;
}

# test successful cache creation/deletion scenario where cache name is equal to the
# length of cache_name_len passed in as arg.
sub cache_name_with_fixed_length_test {
	my ($java_bin, $test_name, $cache_max_len_string, $add_token, $expanded_token) = @_;

	my $specified_cache_name = "" ;
	my $expanded_cache_name = "" ;
	my $cache_max_len =0;

	if(!defined($cache_max_len_string)) {
		die "TEST FAILED . wrong usage. specify a cache max length ";
	} else {
		$cache_max_len = $cache_max_len_string + 0; # convert to number
	}

	my @arr = get_cache_name($cache_max_len , $add_token, $expanded_token);

	# cache name to be used in the command line
	$specified_cache_name = $arr[0];

	# name of the cache that is obtained by expanding %u or %g ( if any ) in the specified_cache_name
	$expanded_cache_name = $arr[1];

	printf "cache name to create: " . $specified_cache_name . ":to grep: " . "$expanded_cache_name  \n";
	test_successful_cache_creation ($java_bin , $test_name, $specified_cache_name, $expanded_cache_name);
}

# tests the failures in cache creation where cache name exceeds the max length
sub long_cache_name_fail_cases_test {
	my ($java_bin, $test_name, $cache_max_len_string, $error_msg, $add_token, $expanded_token) = @_;

	my $specified_cache_name = "" ;
	my $cache_max_len = 0;

	if(!defined($cache_max_len_string)) {
		die "TEST FAILED . wrong usage. specify a cache max length ";
	} else {
		$cache_max_len = $cache_max_len_string + 0; # convert to number
	}

	if(!defined($add_token)){
		$add_token="";
	}

	if(!defined($expanded_token)){
		$expanded_token="";
	}

	my @arr = get_cache_name($cache_max_len , $add_token, $expanded_token);
	$specified_cache_name = $arr[0];

	#leaving debug messages commented
	#printf "\ncache name to create & grep :$arr[0]:  grep:$arr[1]\n";
	test_unsuccessful_cache_creation ($java_bin , $test_name, $specified_cache_name, $error_msg);
}

# error msg to indicate cache name is too long
sub long_cache_name_err_msg {
	return "Cache name should not be longer than 64 chars."
		. " Cache not created.";
}

# error msg to indicate that the cache name could not accomodate the expansion of %g
sub copy_groupname_err_msg {
	return "Error copying groupname into cache name";
}

# error msg to indicate that the cache name could not accomodate the expansion of %u
sub copy_username_err_msg {
	return "The cache name is to long when the user name is included";
}

sub get_short_string_for_user{
	# do not use %u with double-quotes as "%u" since it will result in the variable
	# interpolation ( substitution of variable %u with its value)
	return '%u';
}

sub get_short_string_for_group{
	# do not use %g with double-quotes as "%g" since it will result in the variable
	# interpolation ( substitution of variable %g with its value)
	return '%g';
}

# return 1 so that this file can be imported and used in other perl scripts
1;
