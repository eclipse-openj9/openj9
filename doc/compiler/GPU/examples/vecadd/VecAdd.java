import java.util.Random;

import java.util.stream.IntStream;

public class VecAdd {
    static float[] A;
    static float[] B;
    static float[] C;
    final static int T = 100;
    final static int ROWS = 4*1024*1024;

    public static void main (String[] args) {
        A = new float [ROWS];
        B = new float [ROWS];
        C = new float [ROWS];

        for (int i = 0; i < ROWS; i++) {
            A[i] = (float)i;
            B[i] = (float)i*2;
        }

        for (int iter  = 0; iter < T; iter++) {
            runGPULambda();
        }
        for (int i = 0; i < 10; i++) {
            System.out.println(C[i]);
        }
    }
    
    public static void runGPULambda() {
        IntStream.range(0, ROWS).parallel().forEach( i -> {
                C[i] = A[i] + B[i];
            } );        
    }
}
