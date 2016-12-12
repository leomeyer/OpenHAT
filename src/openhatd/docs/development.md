This information is for developers who want to get involved with openhatd.

## Building openhatd

It is recommended to build openhatd on Linux. Any modern Linux will probably be fine. openhatd builds at least on Debian 8 (Jessie), Linux Mint 17, and Ubuntu 16. If you develop platform independent code, if you need the convenience of the Visual Studio debugger, or if you simply prefer Windows, you can also develop for openhatd using Visual Studio 2015 or newer.

Cloning the OpenHAT repository will pull in most dependencies via submodules.
Obtain the code from Github using the following command:

	$ git clone https://github.com/openhat-org/OpenHAT --recursive

 The submodule `opdi_core` in turn includes the submodule `POCO` which you have to build first. For instructions please see the file [opdi\_core/code/c/libraries/POCO\_patches/readme.txt](https://github.com/leomeyer/opdi_core/blob/master/code/c/libraries/POCO_patches/readme.txt).

### Linux

On Linux, you need to install [mkdocs](http://www.mkdocs.org) as the documentation build depends on it:

	$ sudo apt-get install python-pip
	$ sudo pip install mkdocs

To build openhatd, go to the folder `src/openhatd/openhatd` and issue

	$ make

First, plugins will be built. This will also create a tarball with the current binaries. To run a number of tests, issue

	$ make tests

### Windows

Open the Visual Studio solution file `src/openhatd/openhatd.sln`. Build the solution. To run the program from within Visual Studio, go to the openhatd project's properties and enter `-c hello-world.ini` as command line parameter for debugging. Press F5 or Ctrl+F5 to run the program.

## Versioning and compatibility

Releases of openhatd follow the versioning scheme MAJOR.MINOR.PATCH:

The MAJOR component increases with very major changes in program architecture and/or release status.

The MINOR component is increased when additional features are added that may break backwards compatibility with existing plugins. A different MINOR number likely means that the Application Binary Interface (ABI), probably the memory layout of the important classes, has changed. It is not safe to use versions of plugins which have been compiled against a different MINOR number.

The PATCH component increases when bugs are fixed or new functions are added without changing the ABI. Plugins with a different PATCH number are safe. 

## Development and release branches

New development takes place on the master branch of the Git repository. Whenever a new version is released a Git branch is made with this version number and the version number on the master branch is advanced. Fixes on the version branches require the creation of a patch tag with an increased patch number. New features are not to be implemented in already released versions.

## API Contracts

