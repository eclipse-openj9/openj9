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

cd FindStore
del /Q *.jar
del /Q %2\I.jar %2\J.jar %2\K.jar %2\L.jar
del /Q M_Classes\jnurlcldr\shared\findstore\*.class
del /Q N_Classes\jnurlcldr\shared\findstore\*.class
del /Q O_Classes\jnurlcldr\shared\findstore\*.class
del /Q P_Classes\jnurlcldr\shared\findstore\*.class
%1\javac jnurlcldr\shared\findstore\*.java
%1\jar -cvf A.jar jnurlcldr\shared\findstore\A*.class
%1\jar -cvf B.jar jnurlcldr\shared\findstore\B*.class
%1\jar -cvf C.jar jnurlcldr\shared\findstore\C*.class
%1\jar -cvf D.jar jnurlcldr\shared\findstore\D*.class
%1\jar -cvf E.jar jnurlcldr\shared\findstore\E*.class
%1\jar -cvf F.jar jnurlcldr\shared\findstore\F*.class
%1\jar -cvf G.jar jnurlcldr\shared\findstore\G*.class
%1\jar -cvf H.jar jnurlcldr\shared\findstore\H*.class
%1\jar -cvf %2\I.jar jnurlcldr\shared\findstore\I*.class
%1\jar -cvf %2\J.jar jnurlcldr\shared\findstore\J*.class
%1\jar -cvf %2\K.jar jnurlcldr\shared\findstore\K*.class
%1\jar -cvf %2\L.jar jnurlcldr\shared\findstore\L*.class
copy jnurlcldr\shared\findstore\M*.class M_Classes\jnurlcldr\shared\findstore\
copy jnurlcldr\shared\findstore\N*.class N_Classes\jnurlcldr\shared\findstore\
copy jnurlcldr\shared\findstore\O*.class O_Classes\jnurlcldr\shared\findstore\
copy jnurlcldr\shared\findstore\P*.class P_Classes\jnurlcldr\shared\findstore\
%1\jar -cvf Nothing.jar nothing.txt
dir *.jar
cd ..
