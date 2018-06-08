################################################################################
# Copyright (c) 2017, 2018 IBM Corp. and others
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
################################################################################

add_jar(jpp
	SOURCES
		com/ibm/jpp/commandline/CommandlineBuilder.java
		com/ibm/jpp/commandline/CommandlineLogger.java
		com/ibm/jpp/commandline/CommandlineNowarnLogger.java
		com/ibm/jpp/commandline/NullOutputStream.java
		com/ibm/jpp/om/BuilderConfigurationException.java
		com/ibm/jpp/om/BuilderExtension.java
		com/ibm/jpp/om/Builder.java
		com/ibm/jpp/om/ClassPathEntry.java
		com/ibm/jpp/om/ConfigObject.java
		com/ibm/jpp/om/ConfigurationRegistry.java
		com/ibm/jpp/om/ConfigXMLHandler.java
		com/ibm/jpp/om/EclipseMetadataExtension.java
		com/ibm/jpp/om/ExternalMessagesExtension.java
		com/ibm/jpp/om/InputStreamMerger.java
		com/ibm/jpp/om/JavaPreprocessor.java
		com/ibm/jpp/om/JitAttributesExtension.java
		com/ibm/jpp/om/JitAttributes.java
		com/ibm/jpp/om/JxeRulesExtension.java
		com/ibm/jpp/om/Logger.java
		com/ibm/jpp/om/MacroExtension.java
		com/ibm/jpp/om/MetaRegistry.java
		com/ibm/jpp/om/NullLogger.java
		com/ibm/jpp/om/PhantomOutputStream.java
		com/ibm/jpp/om/PreprocessorException.java
		com/ibm/jpp/om/PreprocessorFactory.java
		com/ibm/jpp/om/PreprocessorWarning.java
		com/ibm/jpp/om/SimpleCopy.java
		com/ibm/jpp/om/Src.java
		com/ibm/jpp/om/TagExtension.java
		com/ibm/jpp/om/TestsProjectExtension.java
		com/ibm/jpp/xml/IXMLDocumentHandler.java
		com/ibm/jpp/xml/XMLException.java
		com/ibm/jpp/xml/XMLParser.java
		com/ibm/jpp/xml/XMLSpec.java
		com/ibm/jpp/xml/XMLStringBuffer.java
)
