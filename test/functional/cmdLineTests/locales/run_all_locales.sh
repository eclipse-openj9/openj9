#! /bin/bash

#
# Copyright (c) 2016, 2020 IBM Corp. and others
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

# don't try ZOS xplink and lp64 locales
# Various IBM-XXXX code pages cause java.io.UnsupportedEncodingsException on ZOS 
# see CMVC 189169 and 189170
for locale in `locale -a | grep -v -E "xplink|lp64|IBM"`;
do
  # vi_VN.tcvn currently causes problems in SLES10 and 11
  # see CMVC 188505 and https://bugzilla.linux.ibm.com/show_bug.cgi?id=78610
  if [[ "$locale" != "vi_VN.tcvn" && \
  # hy_AM.armsc, ka_GE, ka_GE.georg, tg_TJ, tg_TJ.koi8t currently cause problems on Linux
  # as of SDK.java7sr1hrt = 20120213_01
  # see CMVC 188906
	"$locale" != "Ar_AA" && \
	"$locale" != "hy_AM.armscii8" && \
	"$locale" != "ka_GE" && \
	"$locale" != "ka_GE.georgianps" && \
	"$locale" != "tg_TJ" && \
	"$locale" != "tg_TJ.koi8t" ]]; then

    echo
    echo "Locale: " $locale;

    # also print locale to stderr so it appears with the JAVA errors
    echo 1>&2;
    echo "Locale: " $locale 1>&2;
    
    export LC_ALL=$locale;
    export LANG=$locale;
    
    # run first argument and pass remaining arguments
    $1 "${@:2}";
    
    if [ "$?" != 0 ]; then
      echo "bad return code"
    fi
  fi
done
