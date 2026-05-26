import java.net.URL;
import java.net.URLClassLoader;
import java.nio.file.Paths;
import java.lang.reflect.Method;

/**
 * Demonstrates loading a plugin class dynamically using URLClassLoader.
 *
 * Setup:
 * 1. Compile the plugin: javac DynamicPlugin.java
 * 2. Create a JAR: jar cf plugin.jar DynamicPlugin.class
 * 3. Create a plugins directory: mkdir plugins
 * 4. Move the JAR: mv plugin.jar plugins/
 * 5. Remove the .class file: rm DynamicPlugin.class
 * 6. Compile this loader: javac PluginLoaderDemo.java
 * 7. Run: java PluginLoaderDemo plugins/plugin.jar
 */
public class PluginLoaderDemo {

    public static void main(String[] args) {
        if (args.length < 1) {
            System.out.println("Usage: java PluginLoaderDemo <path-to-plugin.jar>");
            System.out.println("Example: java PluginLoaderDemo plugins/plugin.jar");
            return;
        }

        String jarPath = args[0];

        try {
            System.out.println("=== Dynamic Plugin Loading Demo ===\n");

            // Load and execute the plugin
            loadAndExecutePlugin(jarPath);

        } catch (Exception e) {
            System.err.println("Error loading plugin: " + e.getMessage());
            e.printStackTrace();
        }
    }

    private static void loadAndExecutePlugin(String jarPath) throws Exception {
        System.out.println("Loading plugin from: " + jarPath);

        // Convert JAR path to URL
        URL pluginUrl = Paths.get(jarPath).toUri().toURL();
        System.out.println("Plugin URL: " + pluginUrl);

        // Create URLClassLoader with the plugin JAR
        // Note: We use null as parent to prevent delegation to system classloader
        // This ensures the class is loaded by our URLClassLoader
        URLClassLoader pluginLoader = new URLClassLoader(
            new URL[] { pluginUrl },
            ClassLoader.getSystemClassLoader()  // Use system classloader as parent
        );

        System.out.println("Created URLClassLoader: " + pluginLoader);
        System.out.println();

        // Load the plugin class
        String className = "DynamicPlugin";
        System.out.println("Loading class: " + className);
        Class<?> pluginClass = pluginLoader.loadClass(className);
        System.out.println("Class loaded: " + pluginClass.getName());
        System.out.println("Loaded by: " + pluginClass.getClassLoader());
        System.out.println();

        // Create an instance of the plugin
        System.out.println("Creating plugin instance...");
        Object plugin = pluginClass.getDeclaredConstructor().newInstance();
        System.out.println();

        // Call initialize method
        System.out.println("Calling initialize()...");
        Method initMethod = pluginClass.getMethod("initialize");
        initMethod.invoke(plugin);
        System.out.println();

        // Call execute method
        System.out.println("Calling execute()...");
        Method execMethod = pluginClass.getMethod("execute");
        execMethod.invoke(plugin);
        System.out.println();

        // Call getName method
        Method getNameMethod = pluginClass.getMethod("getName");
        String name = (String) getNameMethod.invoke(plugin);
        System.out.println("Plugin name: " + name);
        System.out.println();

        // Demonstrate that we can load multiple instances
        System.out.println("Creating second instance...");
        Object plugin2 = pluginClass.getDeclaredConstructor().newInstance();

        // Set a different name using reflection
        Method setNameMethod = pluginClass.getMethod("setName", String.class);
        setNameMethod.invoke(plugin2, "SecondPlugin");

        String name2 = (String) getNameMethod.invoke(plugin2);
        System.out.println("Second plugin name: " + name2);
        System.out.println();

        // Show that both instances are from the same class
        System.out.println("Both instances from same class: " +
            (plugin.getClass() == plugin2.getClass()));
        System.out.println("Both loaded by same classloader: " +
            (plugin.getClass().getClassLoader() == plugin2.getClass().getClassLoader()));

        // Close the classloader
        pluginLoader.close();
        System.out.println("\nClassLoader closed successfully");
    }
}

// Made with Bob
