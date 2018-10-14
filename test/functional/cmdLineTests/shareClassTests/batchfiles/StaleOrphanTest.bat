
rem
rem Copyright (c) 2001, 2018 IBM Corp. and others
rem
rem This program and the accompanying materials are made available under
rem the terms of the Eclipse Public License 2.0 which accompanies this
rem distribution and is available at https://www.eclipse.org/legal/epl-2.0/
rem or the Apache License, Version 2.0 which accompanies this distribution and
rem is available at https://www.apache.org/licenses/LICENSE-2.0.
rem
rem This Source Code may also be made available under the following
rem Secondary Licenses when the conditions for such availability set
rem forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
rem General Public License, version 2 with the GNU Classpath
rem Exception [1] and GNU General Public License, version 2 with the
rem OpenJDK Assembly Exception [2].
rem
rem [1] https://www.gnu.org/software/classpath/license.html
rem [2] http://openjdk.java.net/legal/assembly-exception.html
rem
rem SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
rem

# ensure the new files are not created in the same second as the originals,
# otherwise the shared cache will not be able to determine the files have
# been modified
sleep 2
cd .\StaleOrphansTest
del *.class
copy StaleOrphan.java StaleOrphan.java.bak
copy .\temp\StaleOrphan.java .
copy StaleOrphan1.java StaleOrphan1.java.bak
copy .\temp\StaleOrphan1.java .
copy StaleOrphan2.java StaleOrphan2.java.bak
copy .\temp\StaleOrphan2.java .
copy StaleOrphan3.java StaleOrphan3.java.bak
copy .\temp\StaleOrphan3.java .
copy StaleOrphan4.java StaleOrphan4.java.bak
copy .\temp\StaleOrphan4.java .
copy StaleOrphan5.java StaleOrphan5.java.bak
copy .\temp\StaleOrphan5.java .
copy StaleOrphan6.java StaleOrphan6.java.bak
copy .\temp\StaleOrphan6.java .
copy StaleOrphan7.java StaleOrphan7.java.bak
copy .\temp\StaleOrphan7.java .
copy StaleOrphan8.java StaleOrphan8.java.bak
copy .\temp\StaleOrphan8.java .
copy StaleOrphan9.java StaleOrphan9.java.bak
copy .\temp\StaleOrphan9.java .
%1%\javac -classpath ..\ *.java
cd ..