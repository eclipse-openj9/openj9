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

# ensure the new jar files are not created in the same second as the originals,
# otherwise the shared cache will not be able to determine the jar files have
# been modified
sleep 2
cd JarExt
del *.jar
%1\javac VerifyClasses\jnurlcldr\shared\jarexttests\*.java
cd VerifyClasses
%1\jar -cvf ..\A.jar jnurlcldr\shared\jarexttests\A*.class
%1\jar -cvfm ..\B.jar ..\Manifests\B\manifest.mf jnurlcldr\shared\jarexttests\B*.class
%1\jar -cvf ..\C.jar jnurlcldr\shared\jarexttests\C*.class
%1\jar -cvf ..\D.jar jnurlcldr\shared\jarexttests\D*.class
%1\jar -cvfm ..\E.jar ..\Manifests\E\manifest.mf jnurlcldr\shared\jarexttests\E_*.class 
%1\jar -cvf ..\E1.jar jnurlcldr\shared\jarexttests\E1_*.class
%1\jar -cvfm ..\F.jar ..\Manifests\F\manifest.mf jnurlcldr\shared\jarexttests\F_*.class 
%1\jar -cvf ..\F1.jar jnurlcldr\shared\jarexttests\F1_*.class
%1\jar -cvf ..\G.jar jnurlcldr\shared\jarexttests\G_*.class 
%1\jar -cvf ..\G1.jar  jnurlcldr\shared\jarexttests\G1_*.class
%1\jar -cvfm ..\H.jar ..\Manifests\H\manifest.mf jnurlcldr\shared\jarexttests\H_*.class 
%1\jar -cvf ..\H1.jar jnurlcldr\shared\jarexttests\H1_*.class
cd ..
dir *.jar
cd ..
