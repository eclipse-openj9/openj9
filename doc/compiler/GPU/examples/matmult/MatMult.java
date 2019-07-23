import java.util.Random;

import java.util.stream.IntStream;

public class MatMult {
    static float[] A;
    static float[] B;
    static float[] C;
    final static int T = 1000;
    final static int ROWS = 512;

    public static void main (String[] args) {
        A = new float [ROWS*ROWS];
        B = new float [ROWS*ROWS];
        C = new float [ROWS*ROWS];

        for (int i = 0; i < ROWS*ROWS; i++) {
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
        IntStream.range(0, ROWS*ROWS).parallel().forEach( idx -> {
                int i = idx / ROWS;
                int j = idx % ROWS;
                float sum = 0;
                for (int k = 0; k < ROWS; k++) {
                    sum += A[i*ROWS + k] * B[k*ROWS + j];
                }
                C[idx] = sum;
            } );        
    }
}
