import java.net.URL;
import java.net.URLClassLoader;
import java.nio.file.Paths;

/**
 * Simple standalone demo of URLClassLoader.
 *
 * To run this example:
 * 1. Compile: javac SimpleURLClassLoaderDemo.java
 * 2. Run: java SimpleURLClassLoaderDemo
 */
public class SimpleURLClassLoaderDemo {

    public static void main(String[] args) {
        System.out.println("URLClassLoader Demo\n");

        try {
            // Demo 1: Basic URLClassLoader creation
            basicExample();

            // Demo 2: Loading from current directory
            loadFromCurrentDirectory();

        } catch (Exception e) {
            System.err.println("Error: " + e.getMessage());
            e.printStackTrace();
        }
    }

    /**
     * Basic example showing URLClassLoader structure
     */
    private static void basicExample() throws Exception {
        System.out.println("=== Basic URLClassLoader Example ===");

        // Create URLs for the classpath
        URL url1 = new URL("file:///opt/myapp/lib/library.jar");
        URL url2 = Paths.get("/opt/myapp/classes").toUri().toURL();

        // Create URLClassLoader with these URLs
        URLClassLoader loader = new URLClassLoader(new URL[] { url1, url2 });

        System.out.println("Created URLClassLoader with classpath:");
        for (URL url : loader.getURLs()) {
            System.out.println("  - " + url);
        }

        // Attempt to load a class (will fail if class doesn't exist)
        try {
            Class<?> clazz = loader.loadClass("com.example.MyClass");
            System.out.println("Loaded class: " + clazz.getName());
        } catch (ClassNotFoundException e) {
            System.out.println("Class not found (expected for demo): " + e.getMessage());
        }

        loader.close();
        System.out.println();
    }

    /**
     * Example loading from current directory
     */
    private static void loadFromCurrentDirectory() throws Exception {
        System.out.println("=== Loading from Current Directory ===");

        // Get current directory as URL
        URL currentDir = Paths.get(".").toUri().toURL();

        // Create loader
        URLClassLoader loader = new URLClassLoader(new URL[] { currentDir });

        System.out.println("ClassLoader created for: " + currentDir);

        // Try to load this class itself
        try {
            Class<?> thisClass = loader.loadClass("SimpleURLClassLoaderDemo");
            System.out.println("Successfully loaded: " + thisClass.getName());
            System.out.println("Loaded by: " + thisClass.getClassLoader().getClass().getName());
        } catch (ClassNotFoundException e) {
            System.out.println("Could not load class: " + e.getMessage());
        }

        loader.close();
        System.out.println();
    }

    /**
     * Practical example: Dynamic plugin loading pattern
     */
    public static class PluginSystem {

        public static void loadPlugin(String jarPath) throws Exception {
            // Convert path to URL
            URL pluginUrl = Paths.get(jarPath).toUri().toURL();

            // Create isolated classloader for the plugin
            URLClassLoader pluginLoader = new URLClassLoader(
                new URL[] { pluginUrl },
                PluginSystem.class.getClassLoader()  // Parent classloader
            );

            // Load the plugin class
            Class<?> pluginClass = pluginLoader.loadClass("com.example.Plugin");

            // Create instance and use it
            Object plugin = pluginClass.getDeclaredConstructor().newInstance();

            // Invoke plugin methods via reflection
            pluginClass.getMethod("initialize").invoke(plugin);
            pluginClass.getMethod("execute").invoke(plugin);

            // Cleanup
            pluginLoader.close();
        }
    }
}

// Made with Bob
