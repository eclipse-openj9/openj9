/*[INCLUDE-IF Sidecar19-SE]*/
/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package jdk.tools.jlink.internal.plugins;

import jdk.tools.jlink.plugin.Plugin;
import jdk.tools.jlink.plugin.ResourcePool;
import jdk.tools.jlink.plugin.ResourcePoolBuilder;
import java.util.EnumSet;
import java.util.Set;

/**
 * We override the default GenerateJLIClassesPlugin with this
 * one because the default relies on Hotspot-specific code.
 * Removing this class entirely causes the build to fail when
 * using the default module-info.java file for the jlink module.
 * So we create the class, but include no code that affects any 
 * change.
 */
public class GenerateJLIClassesPlugin implements Plugin
{
 /**
   * This method makes a copy of the ResourcePool.
   * Since this plugin is intended to have no effect,
   * it lacks any transformational logic.
   * @param rp            Pool of resources
   * @param rpb           Builder to create a copy of the pool of resources.
   * @return ResourcePool Built copy of the pool of resources.
   */
   public ResourcePool transform(ResourcePool rp, ResourcePoolBuilder rpb){
      return null;
   }
   
  /**
   * This method tells the caller that this plugin is disabled,
   * and should not be used.
   * @return Set<State> State of the Plugin.
   */
   @Override
   public Set<State> getState() {
       return EnumSet.of(State.DISABLED, State.FUNCTIONAL);
   }
}

