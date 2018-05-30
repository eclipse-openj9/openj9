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

use strict;

# FIXME: incorporate the variant name into the buidl too (prod vs debug)

die("\nUsage:\n  $0 rel_name\n") unless (@ARGV eq 1);

my $rel = $ARGV[0];

my $snapshot_name;
if (defined $ENV{"TR_BUILD_NAME"}) {
    $snapshot_name = $ENV{"TR_BUILD_NAME"};
} else {
    use POSIX;
    
    # FIXME: try to include a workspace name too
    # Optionally, check if the user has defined $USER_TR_VERSION, and incorporate
    # too.
    my $time = POSIX::strftime("\%Y\%m\%d_\%H\%M", localtime($^T));
    $snapshot_name = $rel . "_" . $time . "_" . $ENV{LOGNAME};
}

print "#include \"control/OMROptions.hpp\"\n\nconst char TR_BUILD_NAME[] = \"$snapshot_name\";\n\n";
