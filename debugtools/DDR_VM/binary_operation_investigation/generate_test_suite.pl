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

# Script to build a C testcase for generating test cases for scalar types

# Usage: perl generate_test_suite.pl <name of cpp file to generate>

# Andrew Hall

use strict;
use warnings;

my $output_file = shift;

die "You didn't specify an output file" unless defined $output_file;

open(my $out,'>',$output_file) or die "Couldn't open $output_file: $!";

#Operations to generate data for
my @ops = ({op=>'add',operator=>'+'}, {op=>'sub',operator=>'-'});

my @bitwise_ops = ({op=>'bitAnd',operator=>'&'}, {op=>'bitOr',operator=>'|'});

#Types to generate data for
my @types = qw{U_8 U_16 U_32 U_64 I_8 I_16 I_32 I_64 UDATA IDATA};

#Conversion table. Equivalent of the HTML table produced by the prettify script
#Tells us what format to expect all the output in
my %type_conversion_table = (U_8 => {U_8=>'U_8',
									U_16=>'I_32',
									U_32=>'U_32',
									U_64=>'U_64',
									I_8=>'I_32',
									I_16=>'I_32',
									I_32=>'I_32',
									I_64=>'I_64',
									UDATA=>'UDATA',
									IDATA=>'IDATA'
									},

							U_16=>{U_8=>'I_32',
									U_16=>'U_16',
									U_32=>'U_32',
									U_64=>'U_64',
									I_8=>'I_32',
									I_16=>'I_32',
									I_32=>'I_32',
									I_64=>'I_64',
									UDATA=>'UDATA',
									IDATA=>'IDATA'
									},
							
							U_32=>{U_8=>'U_32',
									U_16=>'U_32',
									U_32=>'U_32',
									U_64=>'U_64',
									I_8=>'U_32',
									I_16=>'U_32',
									I_32=>'U_32',
									I_64=>'I_64',
									UDATA=>'UDATA',
									IDATA=>'IDATA'
									},
							
							U_64=>{U_8=>'U_64',
									U_16=>'U_64',
									U_32=>'U_64',
									U_64=>'U_64',
									I_8=>'U_64',
									I_16=>'U_64',
									I_32=>'U_64',
									I_64=>'U_64',
									UDATA=>'U_64',
									IDATA=>'ERROR'
									},
							
							I_8=>{U_8=>'I_32',
									U_16=>'I_32',
									U_32=>'U_32',
									U_64=>'U_64',
									I_8=>'I_32',
									I_16=>'I_32',
									I_32=>'I_32',
									I_64=>'I_64',
									UDATA=>'UDATA',
									IDATA=>'IDATA'
									},
							
							I_16=>{U_8=>'I_32',
									U_16=>'I_32',
									U_32=>'U_32',
									U_64=>'U_64',
									I_8=>'I_32',
									I_16=>'I_16',
									I_32=>'I_32',
									I_64=>'I_64',
									UDATA=>'UDATA',
									IDATA=>'IDATA'
									},
							
							I_32=>{U_8=>'I_32',
									U_16=>'I_32',
									U_32=>'U_32',
									U_64=>'U_64',
									I_8=>'I_32',
									I_16=>'I_32',
									I_32=>'I_32',
									I_64=>'I_64',
									UDATA=>'UDATA',
									IDATA=>'IDATA'
									},
							
							I_64=>{U_8=>'I_64',
									U_16=>'I_64',
									U_32=>'I_64',
									U_64=>'U_64',
									I_8=>'I_64',
									I_16=>'I_64',
									I_32=>'I_64',
									I_64=>'I_64',
									UDATA=>'ERROR',
									IDATA=>'I_64'
									},
							
							UDATA=>{U_8=>'UDATA',
									U_16=>'UDATA',
									U_32=>'UDATA',
									U_64=>'U_64',
									I_8=>'UDATA',
									I_16=>'UDATA',
									I_32=>'UDATA',
									I_64=>'ERROR',
									UDATA=>'UDATA',
									IDATA=>'UDATA'
									},
							
							IDATA=>{U_8=>'IDATA',
									U_16=>'IDATA',
									U_32=>'IDATA',
									U_64=>'ERROR',
									I_8=>'IDATA',
									I_16=>'IDATA',
									I_32=>'IDATA',
									I_64=>'I_64',
									UDATA=>'UDATA',
									IDATA=>'IDATA'
									},);

#List of generated functions (called by main at the end)
my @function_list;

generate_file_header();

generate_arithmetic_functions();

generate_bitwise_functions();

generate_equals_functions();

generate_main_function();

close($out);

exit;

sub generate_bitwise_functions
{
	foreach my $type (@types) {
		
		foreach my $operation (@bitwise_ops) {
			my $op_name = $operation->{op};
			my $operator = $operation->{operator};
			my $function_name = "test${op_name}${type}";
			push @function_list, $function_name;
		
			generate_type_function_header ($out,$function_name);
			my $base_var = generate_locals ($out,$type);
			generate_post_local_header($out,$type,$op_name);
	
			foreach my $second_arg_type (@types) {
		
				print $out <<END;

	cout << "SUBSECTION $type WITH $second_arg_type" << endl;
END

				my $var_1 = $base_var;
				my $var_2 = lc $second_arg_type;
				my $expected_type = $type_conversion_table{$type}->{$second_arg_type};
				die "No expected type for $type, $second_arg_type" unless defined $expected_type;
			
				if ($expected_type eq 'ERROR') {
					print $out <<END;

	cout << "INVALID: $type,$op_name,$second_arg_type" << endl;

END
					next;
				}
			
				my $bitpattern1a = get_pattern_1a($type);
				my $bitpattern1b = get_pattern_1b($second_arg_type);
				
				#Test max+1
				print $out <<END;
	$var_1 = $bitpattern1a;
	$var_2 = $bitpattern1b;
END
				write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);
				
				my $bitpattern2a = get_pattern_2a($type);
				my $bitpattern2b = get_pattern_2b($second_arg_type);
				
				#Test max+1
				print $out <<END;
	$var_1 = $bitpattern2a;
	$var_2 = $bitpattern2b;
END
				write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);
			}

			generate_type_function_footer($out);
		}
	}
}

sub generate_equals_functions
{
	foreach my $type (@types) {
		
		my $op_name = "equals";
		my $operator = "==";
		my $function_name = "test${op_name}${type}";
		push @function_list, $function_name;
		
		generate_type_function_header ($out,$function_name);
		my $base_var = generate_locals ($out,$type);
		generate_post_local_header($out,$type,$op_name);
	
		foreach my $second_arg_type (@types) {
			print $out <<END;
	cout << "SUBSECTION $type WITH $second_arg_type" << endl;
END

			my $var_1 = $base_var;
			my $var_2 = lc $second_arg_type;
			my $expected_type = $type_conversion_table{$type}->{$second_arg_type};
			die "No expected type for $type, $second_arg_type" unless defined $expected_type;
		
			if ($expected_type eq 'ERROR') {
				print $out <<END;

	cout << "INVALID: $type,$op_name,$second_arg_type" << endl;

END
				next;
			}
			
			$expected_type = 'bool';
	
			#Test 0=0
			print $out <<END;
	$var_1 = 0;
	$var_2 = 0;
END

			write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);
			
			if ( isSigned($type) && isSigned($second_arg_type) ) {
				#Try min, max and -1
				
							print $out <<END;
	$var_1 = -1;
	$var_2 = -1;
END
				write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);

				my $min = (getWidth($type) > getWidth($second_arg_type)) ? getMin($second_arg_type) :  getMin($type);
				my $max = (getWidth($type) > getWidth($second_arg_type)) ? getMax($second_arg_type) :  getMax($type);
				
				print $out <<END;
	$var_1 = $min;
	$var_2 = $min;
END
				write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);
				
				print $out <<END;
	$var_1 = $min - 1;
	$var_2 = $min - 1;
END
				write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);
				
				print $out <<END;
	$var_1 = $max;
	$var_2 = $max;
END
				write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);
				
				print $out <<END;
	$var_1 = $max + 1;
	$var_2 = $max + 1;
END
				write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);
				
			} elsif ( !isSigned($type) && isSigned($second_arg_type) ) {
				#Test -1 vs. MAX
				my $max = getMax($type);
				
				print $out <<END;
	$var_1 = $max;
	$var_2 = -1;
END
				write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);

				print $out <<END;
	$var_1 = $max + 1;
	$var_2 = -1;
END
				write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);
				
			} elsif  ( isSigned($type) && !isSigned($second_arg_type) ) {
				#Test -1 vs. MAX
				my $max = getMax($second_arg_type);
				
				print $out <<END;
	$var_1 = -1;
	$var_2 = $max;
END
				write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);

				print $out <<END;
	$var_1 = -1;
	$var_2 = $max + 1;
END
				write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);
				
			} else {
				my $min = (getWidth($type) > getWidth($second_arg_type)) ? getMin($second_arg_type) :  getMin($type);
				my $max = (getWidth($type) > getWidth($second_arg_type)) ? getMax($second_arg_type) :  getMax($type);
				
				print $out <<END;
	$var_1 = $max;
	$var_2 = $max;
END
				write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);
				
				print $out <<END;
	$var_1 = $max + 1;
	$var_2 = $max + 1;
END
				write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);
				
			}
			
			#Generate some bit patterns to trigger funny sign extensions etc.
			my $bit_pattern1 = (getWidth($type) > getWidth($second_arg_type)) ? get_pattern_1a($second_arg_type) : get_pattern_1a($type);
			my $bit_pattern2 = (getWidth($type) > getWidth($second_arg_type)) ? get_pattern_1b($second_arg_type) : get_pattern_1b($type);
			my $bit_pattern3 = (getWidth($type) > getWidth($second_arg_type)) ? get_pattern_2a($second_arg_type) : get_pattern_2a($type);
			my $bit_pattern4 = (getWidth($type) > getWidth($second_arg_type)) ? get_pattern_2b($second_arg_type) : get_pattern_2b($type);
			
			print $out <<END;
	$var_1 = $bit_pattern1;
	$var_2 = $bit_pattern1;
END
			write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);
			
			print $out <<END;
	$var_1 = $bit_pattern2;
	$var_2 = $bit_pattern2;
END
			write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);
			
			print $out <<END;
	$var_1 = $bit_pattern3;
	$var_2 = $bit_pattern3;
END
			write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);
			
			print $out <<END;
	$var_1 = $bit_pattern4;
	$var_2 = $bit_pattern4;
END
			write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);
			
		}
		generate_type_function_footer($out);
	}
}


sub get_pattern_1a
{
	my $type = shift;
	
	my $width = getWidth($type);
		
	$width /= 8;
		
	return '0x'.('AA'x$width);
}

sub get_pattern_1b
{
	my $type = shift;
	
	my $width = getWidth($type);
		
	$width /= 8;
		
	return '0x'.('55'x$width);
}

sub get_pattern_2a
{
	my $type = shift;
	return get_pattern_1a($type);
}

sub get_pattern_2b
{
	my $type = shift;
	
	my $width = getWidth($type);
		
	$width /= 8;
		
	return '0x'.('A5'x$width);
}

sub generate_arithmetic_functions
{
	foreach my $type (@types) {
		
		foreach my $operation (@ops) {
			my $op_name = $operation->{op};
			my $operator = $operation->{operator};
			my $function_name = "test${op_name}${type}";
			push @function_list, $function_name;
		
			generate_type_function_header ($out,$function_name);
			my $base_var = generate_locals ($out,$type);
			generate_post_local_header($out,$type,$op_name);
	
			foreach my $second_arg_type (@types) {
		
				print $out <<END;

	cout << "SUBSECTION $type WITH $second_arg_type" << endl;
END

				#Generate 0 + 1 and 0 + 2 operations
				my $var_1 = $base_var;
				my $var_2 = lc $second_arg_type;
				my $expected_type = $type_conversion_table{$type}->{$second_arg_type};
				die "No expected type for $type, $second_arg_type" unless defined $expected_type;
			
				if ($expected_type eq 'ERROR') {
					print $out <<END;

	cout << "INVALID: $type,$op_name,$second_arg_type" << endl;

END
					next;
				}
			
				#Test 0+0
				print $out <<END;
	$var_1 = 0;
	$var_2 = 0;
END
				write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);
			
				#Test 0+1
			
				print $out <<END;
	$var_1 = 0;
	$var_2 = 1;
END
				write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);
			
				#Test 0+2
				print $out <<END;
	$var_1 = 0;
	$var_2 = 2;
END
				write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);
				
				#For signed secondary types, try 0 + -1
				
				if (isSigned($second_arg_type)) {
					print $out <<END;
	$var_1 = 0;
	$var_2 = -1;
END
					write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);
				}
			
				my $min = getMin($type);
				my $max = getMax($type);
				
				if ($min ne '0') {
					#Test min+1
					print $out <<END;
	$var_1 = $min;
	$var_2 = 1;
END
					write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);
					
					if (isSigned($second_arg_type)) {
						#Test min-1
						print $out <<END;
	$var_1 = $min;
	$var_2 = -1;
END
						write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);
					}					
				}
				
				
				#Test max+1
				print $out <<END;
	$var_1 = $max;
	$var_2 = 1;
END
				write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);
					
				if (isSigned($second_arg_type)) {
					#Test max-1
					print $out <<END;
	$var_1 = $max;
	$var_2 = -1;
END
					write_testcase_output_line($out,$type,$var_1,$second_arg_type,$var_2,$expected_type,$op_name,$operator);
				}
				
			}

			generate_type_function_footer($out);
		}
	}
}

sub isSigned
{
	my $type = shift;
	
	my $firstChar = substr($type,0,1);
	
	return (lc $firstChar) eq 'i';
}

sub getWidth
{
	my $type = shift;
	
	if ($type =~ /[UI]_(\d+)/) {
		return $1;
	} elsif (substr($type,1,4) eq 'DATA') {
		#TODO THINK ABOUT TREATING UDATA AS 32 BIT
		return 32;
	} else {
		die "Unknown type $type";
	}
}

sub getMax
{
	my $type = shift;
	
	if (isSigned($type)) {
		my $width = getWidth($type);
		
		$width /= 8;
		
		return "0x7f".('ff'x($width-1));
	} else {
		my $width = getWidth($type);
		return "0x".('ff'x($width/8));
	}
}

sub getMin
{
	my $type = shift;
		
	if (isSigned($type)) {
		my $width = getWidth($type);
		
		$width /= 8;
		
		return "0x80".('00'x($width-1));
	} else {
		return "0";
	}
}

sub protect_chars
{
	my ($type,$var) = @_;
	
	if ($type eq 'U_8') {
		return "(unsigned int)$var";
	} elsif ($type eq 'I_8') {
		return "(int)$var";
	} else {
		return $var;
	}
}

sub write_testcase_output_line
{
	my ($out,$type_1,$var_1,$type_2,$var_2,$expected_type,$op_name,$operator) = @_;
	
	# When printing the values for chars, they have to be converted to a numeric type so they will print as integers
	my $print_var_1 = protect_chars($type_1,$var_1);
	my $print_var_2 = protect_chars($type_2,$var_2);
	my $print_result1 = protect_chars($expected_type,"result1");
	my $print_result2 = protect_chars($expected_type,"result2");
	
	print $out <<"END";
	{
		$expected_type result1 = $var_1 $operator $var_2;
		$expected_type result2 = $var_2 $operator $var_1;
		cout << "$type_1," << hex << $print_var_1 << ",$op_name,$type_2," << hex << $print_var_2 << ",$expected_type," << hex << $print_result1 << "," << hex << $print_result2 << endl;
	}
END
}

sub generate_locals {
	my $out = shift;
	my $type = shift;
	
	print $out <<END;
	$type base;
END

	#Generate an automatic variable for each type
	foreach my $local_type (@types) {
		my $var_name = lc $local_type;
		print $out <<END;
	$local_type $var_name;
END
	}
	
	return "base";
}


sub generate_type_function_header {
	my $out = shift;
	my $function_name = shift;

	print $out <<END;

static void $function_name(void)	
{
	//Locals
END

}

sub generate_type_function_footer {
	my $out = shift;
	
	#Footer of function
	print $out <<END;

}
END
}

sub generate_post_local_header {
	my $out = shift;
	my $type = shift;
	my $op = shift;
	
	print $out <<END;

	cout << "START SECTION $type $op" << endl;
END
}


sub generate_main_function
{

#	Header of main function
	print $out <<END;

int main(int argc, char * argv[]) 
{
END

#Call each function in turn
	foreach my $function (@function_list) {
		print $out <<"END"
	
	${function}();

END

	}

	#Footer of main method
	print $out <<END;

	return 0;
}

END

}



sub generate_file_header
{
	# Print the header
	print $out <<END;

/* Automatically generated by generate_test_suit.pl. */

/* DO NOT MODIFY THIS GENERATED FILE. CHANGE THE generate_test_suite.pl SCRIPT INSTEAD */

#include <iostream>

#include "j9_types.hpp"

using namespace std;

END

}


