package tests.sharedclasses.options;

import java.util.Iterator;
import java.util.List;

import tests.sharedclasses.TestUtils;

/*
 * Create cache generations (we will have generations 2 and 3 around), run destroyAll and check all generations of all caches disappear
 */
public class TestCacheGenerations06 extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();    
    createGenerations02();
    runDestroyAllCaches();
    listAllCaches();
    List compatibleCaches = processOutputForCompatibleCacheList();
    // Should be no entries for Foo or Bar in that list!
    for (Iterator iterator = compatibleCaches.iterator(); iterator.hasNext();) {
		String name = (String) iterator.next();
		if (name.equals("Foo")) fail("Did not expect to find an entry for 'Foo' in the compatible cache list");
		if (name.equals("Bar")) fail("Did not expect to find an entry for 'Bar' in the compatible cache list");
	}
  }
}
