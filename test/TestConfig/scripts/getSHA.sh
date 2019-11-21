#!/usr/bin/env bash
###############################################################################
# Copyright (c) 2019, 2019 IBM Corp. and others
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
###############################################################################

REPO_DIR=""
OUTPUT_FILE="../TestConfig/SHA.txt"


usage ()
{
	echo 'This script use git command to get sha in the provided REPO_DIR HEAD and write the info into the OUTPUT_FILE'
	echo 'Usage : '
	echo '                --repo_dir: local git repo dir'
	echo '                --output_file: the file to write the sha info to. Default is to ../TestConfig/SHA.txt'

}

parseCommandLineArgs()
{
	while [[ $# -gt 0 ]] && [[ ."$1" = .-* ]] ; do
		opt="$1";
		shift;
		case "$opt" in
			"--repo_dir" | "-d" )
				REPO_DIR="$1"; shift;;

			"--output_file" | "-o" )
				OUTPUT_FILE="$1"; shift;;

			"--help" | "-h" )
				usage; exit 0;;

			*) echo >&2 "Invalid option: ${opt}"; echo "This option was unrecognized."; usage; exit 1;
		esac
	done
	if [ -z "$REPO_DIR" ] || [ -z "$OUTPUT_FILE" ] || [ ! -d "$REPO_DIR" ]; then
		echo "Error, please see the usage and also check if $REPO_DIR is existing"
		usage
		exit 1
	fi
}

timestamp() {
  date +"%Y%m%d-%H%M%S"
}

getSHA()
{
	echo "Check sha in $REPO_DIR and store the info in $OUTPUT_FILE"
	if [ ! -e ${OUTPUT_FILE} ]; then
		echo "touch $OUTPUT_FILE"
		touch $OUTPUT_FILE
	fi

	cd $REPO_DIR
	# append the info into $OUTPUT_FILE
	{ echo "================================================"; echo "timestamp: $(timestamp)"; echo "repo dir: $REPO_DIR"; echo "git repo: "; git remote show origin -n | grep "Fetch URL:"; echo "sha:"; git rev-parse HEAD; }  2>&1 | tee -a $OUTPUT_FILE
}
parseCommandLineArgs "$@"
getSHA
