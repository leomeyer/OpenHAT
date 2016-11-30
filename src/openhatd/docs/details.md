## Details

### Versioning and compatibility

Releases of openhatd follow the versioning scheme MAJOR.MINOR.PATCH:

The MAJOR component increases with very major changes in program architecture and/or release status.

The MINOR component is increased when additional features are added that may break backwards compatibility with existing plugins. A different MINOR number likely means that the Application Binary Interface (ABI), probably the memory layout of the important classes, has changed. It is not safe to use versions of plugins which have been compiled against a different MINOR number.

The PATCH component increases when bugs are fixed or new functions are added without changing the ABI. Plugins with a different PATCH number are safe. 

### Development and release branches

New development takes place on the master branch of the Git repository. Whenever a new version is released a Git branch is made with this version number and the version number on the master branch is advanced. Fixes on the version branches require the creation of a patch tag with an increased patch number.


### Operating system compatibility

OpenHAT is designed to be platform-independent and portable between C++ compilers. It works with recent MSVC and GCC that support the C++11 standard (gcc 4.8 and newer). There are currently some problems with clang that might eventually be worked out.

Home automation server hardware needs to provide lots of I/O ports and ideally consumes little power. This rules out most consumer grade PCs or laptops. Instead, home automation with OpenHAT is best done with small and efficient headless computers like the Raspberry Pi.

Compiling OpenHAT on a Raspberry Pi unfortunately is not feasible, so it requires the use of a cross-compiler. The recommended system for cross-compiling OpenHAT for Raspberry Pi is an Ubuntu-based Linux system (you can also use a virtual machine on Windows). Different operating systems provide different advantages when developing or compiling OpenHAT:


- Windows: Good tooling (Visual Studio 2015 Community Edition provides an excellent debugger). Well suited for development and testing.
- Linux: Good toolchains, but debugger support is somewhat basic. Good for tests, static analysis, and cross-compiling, or trying different compilers. Linux specific development, obviously, must be done here.
- Raspberry Pi: Suitable for production use, but less for development. OK for developing specific plugins which compile reasonably well on the Pi.

Only the binaries for the Raspberry Pi are compiled in "release mode", i. e. without debug symbols and with optimizations for speed and size. In general, OpenHAT binaries should be statically linked to avoid problems with shared libraries. Plugins, however, are provided as dynamic libraries that are linked at runtime.



### Environment

A home automation server like OpenHAT should start running at system startup. This can be done in a variety of ways, depending on OS and personal preference. An example script for Upstart on Linux is provided (for more information about Upstart see http://upstart.ubuntu.com).

OpenHAT is typically started by the root account. In fact, root permissions must be provided to gain access to certain resources if required; for example, on the Raspberry Pi, to have fast access to the GPIO pins root is required as these pins are memory-mapped to a restricted system memory area. However, it it is not desirable to keep OpenHAT running as root, especially if you expose network access which OpenHAT naturally must do. OpenHAT allows you to specify a SwitchToUser setting which is applied after initializing (and acquiring restricted resources). The process will then continue to run with the permission of this user making your setup more secure.

It is recommended to create a user "openhat" on your system, with home directory being for example /home/openhat. Within this directory you should create a sub-directory for each OpenHAT instance configuration. The default instance which does the main work might be called "openhat"; there may also an instance like "openhat-test" or a restricted system administration instance. The SwitchToUser setting should be set accordingly.

All necessary code (application, plugins, and support files) plus the configuration should be put into such a folder. This folder should also keep scene files, and serve as the permanent storage location for the persistent config file.

During operation, OpenHAT needs (optionally) to write a few useful files, these are:


	- A log file which is most useful for testing and inspecting the current state of the file (log rotation is being used, with two log files of 1 MB max),
	- A heartbeat file used in determining server state,
	- A persistent configuration file for user settings that should not be lost on a restart,
	- A number of (possibly timestamped) protocol files that contain the state of ports; these are useful for creating statistics or graphs.

Writing these files puts quite a lot of stress on systems that use an SD card as primary storage. Therefore it is recommended to reserve a ram disk with mount point /var/openhat and have OpenHAT's configuration use this directory as base for the file locations. During server startup and shutdown scripts can be run to create this folder and copy necessary files to or from there. Care should be taken to get the folder and file permissions correct, especially if using the SwitchToUser setting with a user with less privileges.

SD card or harddisk failure should be expected at any time. It would therefore be wise to make regular backups of the relevant configuration files or to keep a reserve medium at hand.

### Heartbeat file

