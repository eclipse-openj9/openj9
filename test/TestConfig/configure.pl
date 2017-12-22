#!/usr/bin/perl

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

use Cwd;

#Configure makefile dir
my $configuredir="";
if (@ARGV) {
	$configuredir = $ARGV[0];
} else {
	$configuredir = cwd();
}
my $machineConfiguremk = $configuredir . "/machineConfigure.mk";
unlink($machineConfiguremk);
my $platform=`uname`;
if ($platform !~ /Linux/i) {
	exit;
}
my %capability_Hash;

# virtual infor
my $KVM_Image=`cat /proc/cpuinfo | grep -i QEMU`;
my $VMWare_Image=`lspci 2>/dev/null | grep -i VMware` ;
my $Virt_Sys="";

if ($KVM_Image ne "" ) {
	$Virt_Sys="KVM"
} elsif ($VMWare_Image ne "") {
	$Virt_Sys="VMWare";
}
$ENV{hypervisor} = $Virt_Sys;
$capability_Hash{hypervisor} = $Virt_Sys;

if (%capability_Hash) {
	open( my $fhout,  '>>',  $machineConfiguremk )  or die "Cannot create file $machineConfiguremk $!";
	while (my ($key, $value) = each %capability_Hash) {
		my $exportline = "export " . $key . "=" . $value. "\n";
		print $fhout $exportline;
		delete $capability_Hash{$key};
	}
	close $fhout;
}
