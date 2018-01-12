/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2017 IBM Corp. and others
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
package com.ibm.dtfj.javacore.builder.javacore;

import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.image.javacore.JCImage;
import com.ibm.dtfj.image.javacore.JCImageAddressSpace;
import com.ibm.dtfj.image.javacore.JCImageSection;
import com.ibm.dtfj.java.javacore.JCInvalidArgumentsException;
import com.ibm.dtfj.javacore.builder.BuilderFailureException;
import com.ibm.dtfj.javacore.builder.IImageAddressSpaceBuilder;
import com.ibm.dtfj.javacore.builder.IImageProcessBuilder;

/**
 * @see com.ibm.dtfj.javacore.builder.IImageAddressSpaceBuilder
 * @see com.ibm.dtfj.javacore.builder.AbstractBuilderComponent
 */
public class ImageAddressSpaceBuilder extends AbstractBuilderComponent implements IImageAddressSpaceBuilder {

	private JCImageAddressSpace fImageAddressSpace;
	private BuilderContainer fBuilderContainer;

	/**
	 * 
	 * @param image valid com.ibm.dtfj.javacore.image.JCImage associated with this image address factory. Must not be null.
	 * @param id unique id for this factory. Must not be null.
	 * @throws JCInvalidArgumentsException if image is null
	 */
	public ImageAddressSpaceBuilder(JCImage image, String id) throws JCInvalidArgumentsException {
		super(id);
		if (image == null) {
			throw new JCInvalidArgumentsException("An image address space builder must have a valid image");
		}
		fImageAddressSpace = new JCImageAddressSpace(image);
		fBuilderContainer = getBuilderContainer();
	}

	/**
	 * @see com.ibm.dtfj.javacore.builder.IImageAddressSpaceBuilder#getCurrentImageProcessBuilder()
	 * @return current image process builder. Must not be null.
	 */
	public IImageProcessBuilder getCurrentImageProcessBuilder() {
		return (IImageProcessBuilder) fBuilderContainer.getLastAdded();
	}
	
	/**
	 * @see com.ibm.dtfj.javacore.builder.IImageAddressSpaceBuilder#generateImageProcessBuilder(String)
	 * @param id unique id for the new image address space factory
	 * @return generated image process factory. must not be null.
	 * @throws BuilderFailureException if image process factory not generated.
	 */
	public IImageProcessBuilder generateImageProcessBuilder(String id) throws BuilderFailureException {
		IImageProcessBuilder imageProcessBuilder = null;
		if (getImageProcessBuilder(id) == null) {
			try {
				imageProcessBuilder = new ImageProcessBuilder(fImageAddressSpace, id);
				if (imageProcessBuilder instanceof AbstractBuilderComponent) {
					fBuilderContainer.add((AbstractBuilderComponent)imageProcessBuilder);
				}
				else {
					throw new BuilderFailureException(imageProcessBuilder.toString() + " must also be an " + AbstractBuilderComponent.class);
				}
			} catch (JCInvalidArgumentsException e) {
				throw new BuilderFailureException(e);
			}
		}
		return imageProcessBuilder;
	}
	
	/**
	 * @see com.ibm.dtfj.javacore.builder.IImageAddressSpaceBuilder#getImageProcessBuilder(String)
	 * @return found image process factory, or null if not found
	 */
	public IImageProcessBuilder getImageProcessBuilder(String builderID) {
		return (IImageProcessBuilder) fBuilderContainer.findComponent(builderID);
	}
	
	public ImageSection addImageSection(String name, long base, long size) {
		ImagePointer basePointer = fImageAddressSpace.getPointer(base);
		ImageSection section = new JCImageSection(name, basePointer, size);
		fImageAddressSpace.addImageSection(section);
		return section;
	}
}
