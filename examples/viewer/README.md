# Viewer example

This example contains a small model viewer using [Sokol](https://github.com/floooh/sokol).

## Building

To build the example you need to compile `../../ufbx.c`, `external.c`, and `viewer.c` and link
with the necessary platform libraries.

### Linux

```bash
# Install dependencies if missing (Debian/Ubuntu specific here)
sudo apt install -y libgl1-mesa-dev libx11-dev libxi-dev libxcursor-dev

# Compile and link system libraries
clang -lm -ldl -lGL -lX11 -lXi -lXcursor ../../ufbx.c external.c viewer.c -o viewer

# Run the executable
./viewer /path/to/my/model.fbx
```

### Windows

Create a new Visual Studio solution and add `../../ufbx.c`, `external.c`, and `viewer.c` as source files.
Either build and run from the command line giving the desired model as an argument or
set the command line arguments from the project "Debugging" settings.
