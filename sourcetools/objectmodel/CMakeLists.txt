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

add_jar(objectmodel
	SOURCES
		com/ibm/j9tools/om/io/AbstractParser.java
		com/ibm/j9tools/om/io/BuildInfoParser.java
		com/ibm/j9tools/om/io/BuildSpecParser.java
		com/ibm/j9tools/om/io/FeatureParser.java
		com/ibm/j9tools/om/io/FileExtensionFilter.java
		com/ibm/j9tools/om/io/FlagDefinitionParser.java
		com/ibm/j9tools/om/io/Messages.java
		com/ibm/j9tools/om/io/messages.properties

		com/ibm/j9tools/om/AsmBuilder.java
		com/ibm/j9tools/om/BuildInfo.java
		com/ibm/j9tools/om/BuildSpec.java
		com/ibm/j9tools/om/ConfigDirectory.java
		com/ibm/j9tools/om/DefaultSizes.java
		com/ibm/j9tools/om/FeatureDefinition.java
		com/ibm/j9tools/om/Feature.java
		com/ibm/j9tools/om/FlagDefinition.java
		com/ibm/j9tools/om/FlagDefinitions.java
		com/ibm/j9tools/om/Flag.java
		com/ibm/j9tools/om/FlagVerifier.java
		com/ibm/j9tools/om/IFlagContainer.java
		com/ibm/j9tools/om/InvalidAsmBuilderException.java
		com/ibm/j9tools/om/InvalidBuildInfoException.java
		com/ibm/j9tools/om/InvalidBuildSpecException.java
		com/ibm/j9tools/om/InvalidConfigurationException.java
		com/ibm/j9tools/om/InvalidDefaultSizesException.java
		com/ibm/j9tools/om/InvalidFactoryException.java
		com/ibm/j9tools/om/InvalidFeatureDefinitionException.java
		com/ibm/j9tools/om/InvalidFeatureException.java
		com/ibm/j9tools/om/InvalidFlagDefinitionsException.java
		com/ibm/j9tools/om/InvalidFlagException.java
		com/ibm/j9tools/om/InvalidJCLException.java
		com/ibm/j9tools/om/InvalidSourceException.java
		com/ibm/j9tools/om/InvalidSpecIDException.java
		com/ibm/j9tools/om/ISourceContainer.java
		com/ibm/j9tools/om/JclConfiguration.java
		com/ibm/j9tools/om/Messages.java
		com/ibm/j9tools/om/messages.properties
		com/ibm/j9tools/om/MissingPropertyException.java
		com/ibm/j9tools/om/ObjectFactory.java
		com/ibm/j9tools/om/OMElementException.java
		com/ibm/j9tools/om/OMException.java
		com/ibm/j9tools/om/OMObjectException.java
		com/ibm/j9tools/om/OMObject.java
		com/ibm/j9tools/om/Owner.java
		com/ibm/j9tools/om/Property.java
		com/ibm/j9tools/om/Source.java
		com/ibm/j9tools/om/SourceLocation.java
		com/ibm/j9tools/om/SourceVerifier.java

		schema/archive-v1.xsd
		schema/build-v1.xsd
		schema/build-v2.xsd
		schema/build-v3.xsd
		schema/build-v4.xsd
		schema/build-v5.xsd
		schema/build-v6.xsd
		schema/feature-v1.xsd
		schema/feature-v2.xsd
		schema/flags-v1.xsd
		schema/spec-v1.xsd

	OUTPUT_NAME om
)
