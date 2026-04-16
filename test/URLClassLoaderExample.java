import java.net.URL;
import java.net.URLClassLoader;
import java.nio.file.Paths;
import java.lang.reflect.Method;

/**
 * Sample application demonstrating URLClassLoader usage.
 *
 * This example shows how to:
 * 1. Create a URLClassLoader with a specific classpath
 * 2. Load a class dynamically from a JAR or directory
 * 3. Instantiate and invoke methods on the loaded class
 */
public class URLClassLoaderExample {

    public static void main(String[] args) {
        try {
            // Example 1: Load from a JAR file
            loadFromJar();

            // Example 2: Load from a directory
            loadFromDirectory();

            // Example 3: Load from multiple URLs
            loadFromMultipleURLs();

        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    /**
     * Example 1: Load a class from a JAR file
     */
    private static void loadFromJar() throws Exception {
        System.out.println("=== Example 1: Loading from JAR ===");

        // Create URL pointing to a JAR file
        URL jarUrl = new URL("file:///path/to/mylib.jar");

        // Create URLClassLoader with the JAR in its classpath
        try (URLClassLoader classLoader = new URLClassLoader(new URL[] { jarUrl })) {

            // Load a class from the JAR
            Class<?> clazz = classLoader.loadClass("com.example.MyClass");

            // Create an instance
            Object instance = clazz.getDeclaredConstructor().newInstance();

            // Invoke a method
            Method method = clazz.getMethod("sayHello");
            method.invoke(instance);

            System.out.println("Successfully loaded and invoked class from JAR");
        }
    }

    /**
     * Example 2: Load a class from a directory
     */
    private static void loadFromDirectory() throws Exception {
        System.out.println("\n=== Example 2: Loading from Directory ===");

        // Create URL pointing to a directory containing .class files
        URL dirUrl = Paths.get("/path/to/classes").toUri().toURL();

        // Create URLClassLoader
        try (URLClassLoader classLoader = new URLClassLoader(new URL[] { dirUrl })) {

            // Load class (e.g., if MyPlugin.class is in /path/to/classes/com/example/)
            Class<?> clazz = classLoader.loadClass("com.example.MyPlugin");

            // Check if it implements an interface
            if (Plugin.class.isAssignableFrom(clazz)) {
                Plugin plugin = (Plugin) clazz.getDeclaredConstructor().newInstance();
                plugin.execute();
            }

            System.out.println("Successfully loaded class from directory");
        }
    }

    /**
     * Example 3: Load from multiple URLs (classpath with multiple entries)
     */
    private static void loadFromMultipleURLs() throws Exception {
        System.out.println("\n=== Example 3: Loading from Multiple URLs ===");

        // Create multiple URLs
        URL[] urls = new URL[] {
            new URL("file:///path/to/lib1.jar"),
            new URL("file:///path/to/lib2.jar"),
            Paths.get("/path/to/classes").toUri().toURL()
        };

        // Create URLClassLoader with multiple classpath entries
        try (URLClassLoader classLoader = new URLClassLoader(urls)) {

            // Load class that might depend on classes from multiple URLs
            Class<?> clazz = classLoader.loadClass("com.example.ComplexClass");

            // Use the class
            Object instance = clazz.getDeclaredConstructor().newInstance();

            System.out.println("Loaded class: " + clazz.getName());
            System.out.println("ClassLoader: " + clazz.getClassLoader());

            // Print the classpath
            System.out.println("\nClassLoader URLs:");
            for (URL url : classLoader.getURLs()) {
                System.out.println("  - " + url);
            }
        }
    }

    /**
     * Example 4: Practical use case - Plugin system
     */
    public static class PluginLoader {

        public static Plugin loadPlugin(String jarPath, String className) throws Exception {
            URL pluginUrl = Paths.get(jarPath).toUri().toURL();
            URLClassLoader loader = new URLClassLoader(new URL[] { pluginUrl });

            Class<?> pluginClass = loader.loadClass(className);
            return (Plugin) pluginClass.getDeclaredConstructor().newInstance();
        }

        public static void main(String[] args) throws Exception {
            // Load and execute a plugin
            Plugin plugin = loadPlugin("/plugins/myplugin.jar", "com.example.MyPlugin");
            plugin.execute();
        }
    }

    /**
     * Example 5: Loading with parent classloader delegation
     */
    public static void loadWithParentDelegation() throws Exception {
        System.out.println("\n=== Example 5: Parent Delegation ===");

        URL url = new URL("file:///path/to/mylib.jar");

        // Create URLClassLoader with parent classloader
        // Parent will be the system classloader by default
        URLClassLoader classLoader = new URLClassLoader(
            new URL[] { url },
            ClassLoader.getSystemClassLoader()  // Explicit parent
        );

        // This will first try to load from parent, then from the URL
        Class<?> clazz = classLoader.loadClass("com.example.MyClass");

        System.out.println("Class loaded by: " + clazz.getClassLoader());

        classLoader.close();
    }

    /**
     * Example 6: Loading resources (not just classes)
     */
    public static void loadResources() throws Exception {
        System.out.println("\n=== Example 6: Loading Resources ===");

        URL url = new URL("file:///path/to/resources.jar");

        try (URLClassLoader classLoader = new URLClassLoader(new URL[] { url })) {

            // Load a resource file
            URL resourceUrl = classLoader.getResource("config/application.properties");
            if (resourceUrl != null) {
                System.out.println("Found resource: " + resourceUrl);
            }

            // Load as stream
            var stream = classLoader.getResourceAsStream("data/sample.txt");
            if (stream != null) {
                System.out.println("Loaded resource stream");
                stream.close();
            }
        }
    }

    // Sample interface for plugin example
    interface Plugin {
        void execute();
    }
}

// Made with Bob
