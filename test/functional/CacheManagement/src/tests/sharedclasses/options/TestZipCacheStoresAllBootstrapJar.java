package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;
import java.io.File;

/**
 * This test checks for the content of the zipcache in the shared cache.
 * It creates a persistent shared cache and runs
 * java -Xshareclasses:printstats=zipcache
 * on the shared cache.
 * The list of boot strap jar is got from the system property sun.boot.class.path
 * and we check that every file in the sun.boot.class.path is found in the
 * shared cache
 * 
 * @author jeanpb
 */
public class TestZipCacheStoresAllBootstrapJar extends TestUtils {
	public static void main(String[] args) {
		runSimpleJavaProgramWithPersistentCache("TestZipCacheStoresAllBootstrapJar");
		runPrintStats("TestZipCacheStoresAllBootstrapJar",true,"zipcache");

		String bootClassPath = System.getProperty("sun.boot.class.path");
		
		// Assumimg that the sun.boot.class.path is of the following format:
		// full_path_to_file1.jar;full_path_to_file2.jar
		String[] bootClassPathSplit = bootClassPath.split(";");
		
		for (String bootClassPathEntry : bootClassPathSplit) {
			// Not all files in the "sun.boot.class.path" have to exist, check if they do
			if ((new File(bootClassPathEntry)).exists()) {
				// get the file name, split for \ or /.
				String[] bootClassPathEntryParts = bootClassPathEntry.split("[\\\\/]");
	
				String filename = bootClassPathEntryParts[bootClassPathEntryParts.length- 1];
				checkOutputContains(filename, "The shared cache printstats output must contain the ZIPCACHE data for the file "+filename+".");
			}
		}
		runDestroyAllCaches();
	}
}
