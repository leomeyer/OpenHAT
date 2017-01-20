This information is for developers who want to get involved with openhatd.

## Operating system compatibility
openhatd is designed to be platform-independent and portable between C++ compilers. It works with recent MSVCs and GCCs that support the C++11 standard (g++ 4.9 and newer). There are currently some problems with clang that might eventually be worked out.

Different operating systems provide different advantages when developing or compiling openhatd:

- Windows: Good tooling (Visual Studio 2015 Community Edition provides an excellent debugger). Well suited for development and testing.
- Linux: Good toolchains, but debugger support is somewhat basic. Good for tests, static analysis, and cross-compiling, or trying different compilers. Linux specific development, obviously, must be done here.
- Raspberry Pi: Suitable for production use, but less for development. OK for developing specific plugins which compile reasonably well on the Pi.

openhatd uses continuous integration platforms [Travis CI](https://travis-ci.org) for Linux releases and [AppVeyor](https://appveyor.com) for Windows releases. Windows and Raspberry Pi binaries are standalone, statically linked. Linux binaries cannot be statically linked due to problems with glibc; therefore, they are dynamically linked and releases contain necessary libraries.

When pushing commits or merging pull requests continuous integration will automatically build the project, run tests, build documentation and upload everything to the openhat.org server.

Releases do not contain debug information. For debugging on Windows, set the configuration to "Debug". On Linux, the environment variable `DEBUG` must be set to 1 when invoking `make`, such as

	$ DEBUG=1 make 

## Building openhatd
It is recommended to build openhatd on Linux. Any modern Linux will probably be fine. openhatd builds at least on Debian 8 (Jessie), Linux Mint 17, and Ubuntu 16. If you develop platform independent code, if you need the convenience of the Visual Studio debugger, or if you simply prefer Windows, you can also develop for openhatd using Visual Studio 2015 or newer.

Cloning the OpenHAT repository will pull in most dependencies via submodules.
Obtain the code from Github using the following command:

	$ git clone https://github.com/openhat-org/OpenHAT --recursive

To manually download the submodules use:

	$ git submodule update --init --recursive

The submodule `opdi_core` in turn includes the submodule `POCO` which you have to build first. For instructions please see the file [opdi\_core/code/c/libraries/POCO\_patches/readme.txt](https://github.com/leomeyer/opdi_core/blob/master/code/c/libraries/POCO_patches/readme.txt).

You have to copy the content of the `opdi_core/code/c/libraries/POCO_patches` folder over to the `opdi_core/code/c/libraries/POCO` folder.  

### Building on Linux
On Linux, you need to install the prerequisites for the documentation first (see below: Building the documentation).

To build openhatd, go to the folder `src/openhatd/openhatd` and issue

	$ make

First, plugins will be built. This will also create a current release tarball. To run a number of tests, issue

	$ make tests

### Cross-compiling on Linux
openhatd provides makefiles for cross-compiling to armhf and i386.

#### armhf
To setup the armhf cross-compiling environment (assuming a 64 bit machine), go to your home directory and issue:

	git clone https://github.com/raspberrypi/tools.git --depth=1
	export PATH=$(pwd)/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin:$PATH

From the OpenHAT directory, do:

	./build_poco_rpi.sh

This will cross-compile the POCO libraries for armhf.

To build openhatd for armhf, go to `openhatd/src` and issue:

	make -f makefile-rpicc

#### i386
Building openhatd for 32 bit Linux required additional packages:

	sudo apt-get install gcc-4.9-multilib g++-4.9-multilib linux-libc-dev:i386 lib32stdc++6

From the OpenHAT directory, do:

	./build_poco_i386.sh

This will cross-compile the POCO libraries for i386.

To build openhatd for i386, go to `openhatd/src` and issue:

	make -f makefile-i386

### Building on Windows
Open the Visual Studio solution file `src/openhatd/openhatd.sln`. Build the solution. To run the program from within Visual Studio, go to the openhatd project's properties and enter `-c hello-world.ini` as command line parameter for debugging. Press F5 (debug) or Ctrl+F5 to run the program.

## Building the documentation
Building the documentation requires Python, the Python package installer `pip` and Doxygen with the `graphviz `package.

On Linux:

	$ sudo apt-get install python python-pip doxygen graphviz

Additionally required Python modules are `mkdocs` and `markdown_include`:

	$ sudo pip install mkdocs 
	$ sudo pip install markdown_include

From within the openhatd folder, first build the Doxygen documentation:

	$ doxygen Doxyfile

Then, build the mkdocs documentation:

	$ mkdocs build

The configuration files for these documentations contain placeholders for version and timestamp. The `common.mk ` makefile within the `src` folder contains logic to replace these placeholders when the `docs` target is invoked. This way is intended for releases only. If documentation generation fails the replaced versions of the files will remain in the openhatd folder; they then need to be reset with `git checkout`. For this reason it is recommended to invoke `make release` only if a previous documentation generation has completed successfully.   

## Testing
Ideally, testing is done on the target hardware with external components connected. Unfortunately, this is not possible with continuous integration.

openhatd supports a test mode switched on by the command line flag `-t`. If this flag is set openhatd goes through the port initialization and configuration phase, then exits with code 0. Any error during these phases will fail the continuous integration tests (all configuration files supplied with an openhatd release should be at least checked with `-t` to ensure that they are free of superficial errors). This method only tests configuration file integrity and the correct operation of the startup phases. It does, however, not support functional testing and is therefore not a reliable indicator whether a build is free of certain bugs or not.

To test port state integrity and check for regressions you can use [Test ports](ports/test_port.md). Test ports are Digital ports that, when switched to High, perform regular tests on port properties. You can define an arbitrary number of Test ports, however, for automated tests there should be at least one test that terminates the program after it has been executed. Automatic tests are stored in the `testconfigs/automatic` directory and are not intended to be included in a release. If you provide new features or bug fixes please add a test case if possible.

## Versioning and compatibility
Releases of openhatd follow the versioning scheme `MAJOR.MINOR.PATCH`:

The `MAJOR` component increases with very major changes in program architecture and/or release status.

The `MINOR` component is increased when additional features are added that may break backwards compatibility with existing plugins. A different `MINOR` number likely means that the Application Binary Interface (ABI), probably the memory layout of the important classes, has changed. It is not safe to use versions of plugins which have been compiled against a different `MINOR` number. This situation may result in inexplicable errors or strange behavior.

The `PATCH` component increases when bugs are fixed or new functions are added without changing the ABI. Plugins with a different `PATCH` number are safe.

Currently there is a limitation that the versioning components may be one-digit numbers only. (This may change when switching to C++14.)

## Development and release branches
Development of new features takes place on the master branch of the Git repository. Whenever a new version is released a Git branch is made with this version number and the version number on the master branch is advanced. Fixes on the version branches require the creation of a patch tag with an increased patch number. New features are not to be implemented in already released versions.
