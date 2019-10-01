package tests.sharedclasses.options;

import java.io.*;

import tests.sharedclasses.RunCommand;
import tests.sharedclasses.TestUtils;

/*
 * Create a cache, discover the semaphore - wipe it with ipcrm, rerun and check a new semaphore is
 * created for the existing cache.
 */
public class TestSemaphoreMeddling01 extends TestUtils {
  public static void main(String[] args) throws Exception {
	if (!isLinux()) {
		System.err.println("Test 'TestSemaphoreMeddling01' not supported on os: "+getOS());
		System.err.println("test skipped");
		return;
	}
	runDestroyAllCaches();    
    createNonPersistentCache("Oranges");
    
    // take the semaphore file to pieces
    String semaphoreFile = getCacheFileLocationForNonPersistentCache("Oranges").replace("memory","semaphore");
    SemaphoreFileContents semFile = new SemaphoreFileContents(semaphoreFile);
    
    // Delete the semaphore
    // System.out.println("Semaphore id = "+semFile.semaphoreId);
    int semIdBefore = semFile.semaphoreId;
    RunCommand.execute("ipcrm -s "+semIdBefore);
    checkOutputDoesNotContain("invalid id", 
    		"Did not expect error about invalid semaphore id when calling ipcrm for semaphore "+semIdBefore);
    
    runSimpleJavaProgramWithNonPersistentCache("Oranges", "disablecorruptcachedumps");
    checkOutputContains("JVMSHRC159I.*Oranges","Did not get expected message about attaching to cache 'Oranges'");

    semFile = new SemaphoreFileContents(semaphoreFile);
    // System.out.println("New semaphore id = "+semFile.semaphoreId);
    
    // chances of it being the same semaphore, 10zillion to one??
    if (semFile.semaphoreId==semIdBefore) {
    	fail("Really did not expect the new semaphore "+semFile.semaphoreId+" to be the same as the deleted one "+semIdBefore);
    }
   
    runDestroyAllCaches();
  }
  
  /**
   * From j9shsem.h
  typedef struct j9shsem_baseFileFormat {
 	I_32 version;
 	I_32 modlevel;
 	I_32 timeout;
 	I_32 proj_id;
 	key_t ftok_key;
 	I_32 semid;
 	I_32 creator_pid;
 	I_32 semsetSize;
  } j9shsem_baseFileFormat;
  */
  static class SemaphoreFileContents {
	  public int semaphoreId;
	  
	  public SemaphoreFileContents(String filename) throws Exception {

		    File f = new File(filename);
		    FileInputStream fis;
			fis = new FileInputStream(f);
		    DataInputStream dis = new DataInputStream(fis);
		    int version = dis.readInt();
		    int modlevel = dis.readInt();
		    int timeout = dis.readInt();
		    int projId = dis.readInt();
		    int ftokKey = dis.readInt(); // vary in size on platforms?
		    int b1 = dis.readUnsignedByte();
		    int b2 = dis.readUnsignedByte();
		    int b3 = dis.readUnsignedByte();
		    int b4 = dis.readUnsignedByte();
//		    System.out.println(b1+" "+b2+" "+b3+" "+b4);
		    semaphoreId = b4<<24 | b3<<16 |b2<<8 |b1;
//		    semaphoreId = dis.readInt();
		    int creatorPid = dis.readInt();
		    int semsetSize = dis.readInt();
//		    System.out.println("SemaphoreFile[version="+version+",modlevel="+modlevel+",timeout="+timeout+
//		    				   ",projId="+projId+",ftokKey="+ftokKey+",semaphoreId="+semaphoreId+
//		    				   ",creatorPid="+creatorPid+",semsetSize="+semsetSize);
		    dis.close();
		    fis.close();
	  }
  }
}
