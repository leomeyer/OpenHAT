## Hardware

Home automation server hardware needs to provide lots of I/O ports and ideally consumes little power. This rules out most consumer grade PCs or laptops. Instead, home automation with openhatd is best done with small and efficient headless computers like the Raspberry Pi.

Tested hardware:

- Raspberry Pi models B and B+ (Raspbian)
- Raspberry Pi (Raspbian)
- NanoPi NEO (FriendlyARM's image based on UbuntuCore)

## Environment

A home automation server like openhatd should start running at system startup. This can be done in a variety of ways, depending on OS and personal preference. An example script for Upstart on Linux is provided (for more information about Upstart see http://upstart.ubuntu.com).

openhatd is typically started by the root account. In fact, root permissions must be provided to gain access to certain resources if required; for example, on the Raspberry Pi, to have fast access to the GPIO pins root is required as these pins are memory-mapped to a restricted system memory area. However, it it is not desirable to keep openhatd running as root, especially if you expose network access which openhatd naturally must do. openhatd allows you to specify a SwitchToUser setting which is applied after initializing (and acquiring restricted resources). The process will then continue to run with the permission of this user making your setup more secure.

It is recommended to create a user "openhat" on your system, with home directory being for example /home/openhat. Within this directory you should create a sub-directory for each openhatd instance configuration. The default instance which does the main work might be called "openhat"; there may also an instance like "openhat-test" or a restricted system administration instance. The SwitchToUser setting should be set accordingly.

All necessary code (application, plugins, and support files) plus the configuration should be put into such a folder. This folder should also keep scene files, and serve as the permanent storage location for the persistent config file.

During operation, openhatd needs (optionally) to write a few useful files, these are:


	- A log file which is most useful for testing and inspecting the current state of the file (log rotation is being used, with two log files of 1 MB max),
	- A heartbeat file used in determining server state,
	- A persistent configuration file for user settings that should not be lost on a restart,
	- A number of (possibly timestamped) protocol files that contain the state of ports; these are useful for creating statistics or graphs.

Writing these files puts quite a lot of stress on systems that use an SD card as primary storage. Therefore it is recommended to reserve a ram disk with mount point /var/openhat and have openhatd's configuration use this directory as base for the file locations. During server startup and shutdown scripts can be run to create this folder and copy necessary files to or from there. Care should be taken to get the folder and file permissions correct, especially if using the SwitchToUser setting with a user with less privileges.

SD card or harddisk failure should be expected at any time. It would therefore be wise to make regular backups of the relevant configuration files or to keep a reserve medium at hand.

