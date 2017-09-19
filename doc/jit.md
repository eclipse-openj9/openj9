# Testarossa JIT (tr.jit) compiler

Testarossa JIT compiler is a highly performant dynamic compiler that powers OpenJ9 - an enterprise-grade Java Virtual Machine (JVM). It's characteristic features are to compress the application code for improved runtime performance by exploiting the exact host CPU model and apply profiling on the runtime behavior of the generated code, feed it back to the optimizer to further refine it. The compiler provides a wide range of command line options to (i) trace the actions of the compilation and the compiled code, and (ii) control the compiler behavior.

While care is taken at the design itself to derive optimal performance under default configurations for the most common workloads, these command line options help to derive insight into the efficiency of the compiler for a given workload, and fine tune the compilation behavior for maximizing the runtime performance / throughput.


This page contains the full list of compiler options that are classified into: (i) optimization control knob options (which generally takes the form of enable | disable syntax), (ii) trace options (which generally take the form of traceXXX syntax), and (iii) debug options (which generally helps to manually control the execution on a specific compilation / compiled code event) and (iv) Environment variables that influences the behavior of JVMs that are spawned within that environment.

If you are looking for troubleshooting a specific problem at hand that has a perceived scope within the JIT compiler or the compiled code, refer to the [Compiler Problem Determination guide] under Eclipse OMR project.

For most part of the documentation, this below code will be used.

```java
public class Foo {
  public static void main(String args[]) {
    new Foo().run(Integer.parseInt(args[0]));
  }
  public void run(int count) {
    for(int i=0; i<count; i++)
      System.out.println("hello jit world!");
  }
}
```

## Control options

### Disable JIT
Usage: -Xint
Side effect: Performance will be impacted severely, so use only for problem determination, not recommended in production.

Example usage:
Run with method trace ON to see that the method gets compiled
```trace
bash$ java -Xtrace:print=mt,methods={Foo.run*} Foo 300000 > /dev/null
10:36:02.432*0x2118200              mt.0        > Foo.run(I)V bytecode method, this = 0xfff90558
10:36:03.109 0x2118200              mt.7        < Foo.run(I)V compiled method
```

With -Xint in place, see that the method is always interpreted. Please note that a single method is used for demonstration purpose only, this flag disables the JIT compiler globally for all the methods.
```trace
bash$ java -Xtrace:print=mt,methods={Foo.run*} -Xint Foo 300000 > /dev/null
10:36:18.404*0x145a200              mt.0        > Foo.run(I)V bytecode method, this = 0xfff81bd0
10:36:32.084 0x145a200              mt.6        < Foo.run(I)V bytecode method
```
[Compiler Problem Determination guide]: https://github.com/eclipse/omr/blob/master/doc/compiler/ProblemDetermination.md
