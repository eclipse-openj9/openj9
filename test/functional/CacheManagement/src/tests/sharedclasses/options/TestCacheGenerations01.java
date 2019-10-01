package tests.sharedclasses.options;

import java.util.Iterator;
import java.util.List;

import tests.sharedclasses.TestUtils;

/*
 * Create cache generations (we will have generations 1 and 3 around), then listAllCaches - 
 * compatible previous generations should not show up.
 */
public class TestCacheGenerations01 extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();    
    
    if (isMVS())
	  {
		  //this test checks persistent and non persistent cahces ... zos only has nonpersistent ... so we assume other tests cover this
		  return;
	  }
    
    createGenerations01();
    listAllCaches();
    List /*String*/ compatibleCaches = processOutputForCompatibleCacheList();
    // Should be at most two entries for Foo and Bar (persistent and non-persistent variants of each)
    // If there are 4 of each then generations are not being hidden
    int fooCount = 0;
    int barCount = 0;
    for (Iterator iterator = compatibleCaches.iterator(); iterator.hasNext();) {
		String name = (String) iterator.next();
		if (name.equals("Foo")) fooCount++;
		if (name.equals("Bar")) barCount++;
	}

    if (fooCount==4) fail("The generations are not being hidden, there are 4 entries for Foo in the list");
    if (barCount==4) fail("The generations are not being hidden, there are 4 entries for Bar in the list");
    if (fooCount!=2) fail("Expected to find two entries for cache 'Foo' (one persistent, one non-persistent)");
    if (barCount!=2) fail("Expected to find two entries for cache 'Bar' (one persistent, one non-persistent)");

    runDestroyAllCaches();   
  }
}
