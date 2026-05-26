#!/bin/bash
# Script to compile and run the URLClassLoader demo

set -e  # Exit on error

echo "=== URLClassLoader Demo Setup ==="
echo

# Set Java home if provided as argument
if [ -n "$1" ]; then
    JAVA_HOME="$1"
    echo "Using Java from: $JAVA_HOME"
else
    JAVA_HOME="${HOME}/openj9-openjdk-jdk11/build/linux-x86_64-normal-server-release/images/jdk"
    echo "Using default Java from: $JAVA_HOME"
fi

JAVA="${JAVA_HOME}/bin/java"
JAVAC="${JAVA_HOME}/bin/javac"

# Check if Java exists
if [ ! -f "$JAVA" ]; then
    echo "Error: Java not found at $JAVA"
    exit 1
fi

echo "Java version:"
"$JAVA" -version
echo

# Step 1: Compile DynamicPlugin
echo "Step 1: Compiling DynamicPlugin.java..."
"$JAVAC" DynamicPlugin.java
echo "✓ DynamicPlugin compiled"
echo

# Step 2: Create JAR
echo "Step 2: Creating plugin.jar..."
jar cf plugin.jar DynamicPlugin.class
echo "✓ plugin.jar created"
echo

# Step 3: Create plugins directory
echo "Step 3: Creating plugins directory..."
mkdir -p plugins
echo "✓ plugins directory created"
echo

# Step 4: Move JAR to plugins
echo "Step 4: Moving plugin.jar to plugins/..."
mv plugin.jar plugins/
echo "✓ plugin.jar moved to plugins/"
echo

# Step 5: Remove .class file
echo "Step 5: Removing DynamicPlugin.class from current directory..."
rm -f DynamicPlugin.class
echo "✓ DynamicPlugin.class removed"
echo

# Step 6: Compile PluginLoaderDemo
echo "Step 6: Compiling PluginLoaderDemo.java..."
"$JAVAC" PluginLoaderDemo.java
echo "✓ PluginLoaderDemo compiled"
echo

# Step 7: Run the demo
echo "Step 7: Running PluginLoaderDemo..."
echo "========================================"
echo
"$JAVA" PluginLoaderDemo plugins/plugin.jar
echo
echo "========================================"
echo "✓ Demo completed successfully!"
echo

# Cleanup option
echo
read -p "Clean up generated files? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Cleaning up..."
    rm -f PluginLoaderDemo.class
    rm -rf plugins
    echo "✓ Cleanup complete"
fi

# Made with Bob
