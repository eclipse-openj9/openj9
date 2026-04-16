/**
 * A simple plugin class to be loaded dynamically.
 *
 * To use:
 * 1. Compile: javac DynamicPlugin.java
 * 2. Create a JAR: jar cf plugin.jar DynamicPlugin.class
 * 3. Move JAR to a different directory
 * 4. Run the loader demo pointing to that JAR
 */
public class DynamicPlugin {

    private String name;

    public DynamicPlugin() {
        this.name = "DynamicPlugin";
        System.out.println("DynamicPlugin constructor called");
    }

    public void initialize() {
        System.out.println("Initializing " + name);
    }

    public void execute() {
        System.out.println("Executing " + name);
        System.out.println("ClassLoader: " + this.getClass().getClassLoader());
    }

    public String getName() {
        return name;
    }

    public void setName(String name) {
        this.name = name;
    }
}

// Made with Bob
