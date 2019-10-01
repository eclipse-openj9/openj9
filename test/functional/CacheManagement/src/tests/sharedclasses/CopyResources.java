package tests.sharedclasses;

import java.io.*;

/**
 * Supplied an input and output folder, will copy any resources (ignoring .java and .class files) from input to output.
 */
public class CopyResources {

	public static void main(String[] args) throws Exception {

		if (args==null || args.length!=2) {
			throw new IllegalArgumentException("CopyResources <inputfolder> <outputfolder>");
		}
		
		File fIn = new File(args[0]);
		File fOut = new File(args[1]);
		
		if (!fIn.exists() || !fIn.isDirectory()) {
			throw new IllegalArgumentException("inputfolder must be an existing directory");
		}
		if (!fOut.exists()) {
			fOut.mkdir();
		} else if (!fOut.isDirectory()) { 
			throw new IllegalArgumentException("outputfolder specifies an already existing file, when it should specify a directory");
		}
		
		copyStuff(fIn,fOut);
	}
	
	private static void copyStuff(File from,File to) throws Exception {
		String[] files = from.list(new StripNonResourcesFilter());
		if (files!=null) {
			for (int i = 0; i < files.length; i++) {
				File fFrom = new File(from,files[i]);
				File fTo = new File(to,files[i]);
				if (fFrom.isFile()) {
					copyFile(fFrom,fTo);
				} else if (fFrom.isDirectory()) {
					fTo.mkdir();
					copyStuff(fFrom,fTo);
				} else {
					System.out.println("how to handle? "+fFrom);
				}
			}
		}
	}
	
	static class StripNonResourcesFilter implements FilenameFilter {
		public boolean accept(File dir, String name) {
			if (name.endsWith(".java")) return false;
			if (name.indexOf("CVS")!=-1) return false;
			return true;
		}
	}
	
	private static void copyFile(File from,File to) throws Exception {
	  System.out.println("Copying '"+from+"' to '"+to+"'");
	  int i =0;
	  byte[] buff = new byte[1000];
	  FileInputStream fis = new FileInputStream(from);
	  FileOutputStream fos = new FileOutputStream(to);
	  int read=1;
	  while (true && read>0) {
		  read = fis.read(buff);
		  i+=read;
		  if (read>-1) fos.write(buff,0,read);
	  }
	  fos.flush();
	  fos.close();
	  fis.close();
	}
	
}
