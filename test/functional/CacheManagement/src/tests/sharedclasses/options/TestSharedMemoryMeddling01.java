package tests.sharedclasses.options;

import java.io.*;

import tests.sharedclasses.RunCommand;
import tests.sharedclasses.TestUtils;

/*
 * Create a cache, discover the shared memory id - wipe it with ipcrm, rerun and we should get a new cache
 */
public class TestSharedMemoryMeddling01 extends TestUtils {
  public static void main(String[] args) throws Exception {
	if (!isLinux()) {
		System.err.println("Test 'TestSemaphoreMeddling01' not supported on os: "+getOS());
		System.err.println("test skipped");
		return;
	}
	runDestroyAllCaches();    
    createNonPersistentCache("Oranges");
    
    // take the semaphore file to pieces
    String semaphoreFile = getCacheFileLocationForNonPersistentCache("Oranges");
    CacheControlFile ctrlFile = new CacheControlFile(semaphoreFile);
    
    // Delete the shared memory
    int shmIdBefore = ctrlFile.shmId;
    // System.out.println("Shm id = "+shmIdBefore);
    RunCommand.execute("ipcs");
    
    RunCommand.execute("ipcrm -m "+shmIdBefore);
    checkOutputDoesNotContain("invalid id", 
    		"Did not expect error about invalid shm id when calling ipcrm for shmid "+shmIdBefore);
    RunCommand.execute("ipcs");
    
    runSimpleJavaProgramWithNonPersistentCache("Oranges");
    checkOutputContains("JVMSHRC158I.*Oranges","Did not get expected message about creating cache 'Oranges'");

    ctrlFile = new CacheControlFile(semaphoreFile);
    // System.out.println("New shm id = "+ctrlFile.shmId);
    
    // chances of it being the same semaphore, 10zillion to one??
    if (ctrlFile.shmId==shmIdBefore) {
    	fail("Really did not expect the new shmid "+ctrlFile.shmId+" to be the same as the deleted one "+shmIdBefore);
    }
   
    runDestroyAllCaches();
  }
  
  /**
   typedef struct j9shmem_controlFileFormat {
	I_32 version;
	I_32 modlevel;
	key_t ftok_key;
	I_32 proj_id;
	I_32 shmid;
	I_64 size;
	I_32 uid;
	I_32 gid;
   } j9shmem_controlFileFormat;
  */
  static class CacheControlFile {
	  public int shmId;
	  
	  public CacheControlFile(String filename) throws Exception {
		    File f = new File(filename);
		    FileInputStream fis;
			fis = new FileInputStream(f);
		    DataInputStream dis = new DataInputStream(fis);
		    int version = dis.readInt();
		    int modlevel = dis.readInt();
		    int ftokKey = dis.readInt(); // vary in size on platforms?
		    int projId = dis.readInt();
		    int b1 = dis.readUnsignedByte();
		    int b2 = dis.readUnsignedByte();
		    int b3 = dis.readUnsignedByte();
		    int b4 = dis.readUnsignedByte();
//		    System.out.println(b1+" "+b2+" "+b3+" "+b4);
		    shmId = b4<<24 | b3<<16 |b2<<8 |b1;
//		    shmId = dis.readInt();
		    long size = dis.readLong();
		    int uid = dis.readInt();
		    int gid = dis.readInt();
		    System.out.println("ControlFile[version="+version+",modlevel="+modlevel+",ftokKey="+ftokKey+
		    				   ",projId="+projId+",shmId="+shmId+",size="+size+
		    				   ",uid="+uid+",gid="+gid);
		    dis.close();
		    fis.close();
	  }
  }
}
