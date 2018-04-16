#
# Copyright (c) 2001, 2018 IBM Corp. and others
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

# ensure the new files are not created in the same second as the originals,
# otherwise the shared cache will not be able to determine the files have
# been modified
sleep 2
cd ./StaleOrphansTest
rm *.class
cp StaleOrphan.java StaleOrphan.java.bak
cp ./temp/StaleOrphan.java .
cp StaleOrphan1.java StaleOrphan1.java.bak
cp ./temp/StaleOrphan1.java .
cp StaleOrphan2.java StaleOrphan2.java.bak
cp ./temp/StaleOrphan2.java .
cp StaleOrphan3.java StaleOrphan3.java.bak
cp ./temp/StaleOrphan3.java .
cp StaleOrphan4.java StaleOrphan4.java.bak
cp ./temp/StaleOrphan4.java .
cp StaleOrphan5.java StaleOrphan5.java.bak
cp ./temp/StaleOrphan5.java .
cp StaleOrphan6.java StaleOrphan6.java.bak
cp ./temp/StaleOrphan6.java .
cp StaleOrphan7.java StaleOrphan7.java.bak
cp ./temp/StaleOrphan7.java .
cp StaleOrphan8.java StaleOrphan8.java.bak
cp ./temp/StaleOrphan8.java .
cp StaleOrphan9.java StaleOrphan9.java.bak
cp ./temp/StaleOrphan9.java .
$1/javac -classpath ../ *.java
cd ..