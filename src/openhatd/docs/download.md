## openhatd latest stable release

There is no latest stable release yet. Please refer to the [release plan](release_plan).

## openhatd current development version

- Linux: [openhatd-linux-x86_64-0.1.0-current.tar.gz](https://openhat.org/downloads/openhatd-linux-x86_64-0.1.0-current.tar.gz)
- Raspberry Pi: [openhatd-linux-armhf-0.1.0-current.tar.gz](https://openhat.org/downloads/openhatd-linux-armhf-0.1.0-current.tar.gz)
- Windows: [openhatd-windows-0.1.0-current.zip](https://openhat.org/downloads/openhatd-windows-0.1.0-current.zip)

- Offline HTML documentation: [openhatd-docs-0.1.0-current.tar.gz](https://openhat.org/downloads/openhatd-docs-0.1.0-current.tar.gz)

- AndroPDI Remote Control: [AndroPDI-0.1.0.apk](https://openhat.org/downloads/AndroPDI-0.1-0.apk)

## Installation

A special installation routine is not required.

1. Create a directory called `openhat`.
2. On Windows, copy the zip file contents to the directory.
3. On Linux, copy the tar.gz file to the directory and extract it:  
 	`$ tar xfz openhatd-linux-x86_64-0.1.0-current.tar.gz --strip-components=1`


The folder contents are:

- `bin/`: main executable, shared libraries (on Windows and Linux), and hello-world.ini
- `plugins/`: plugin shared object folders
- `testconfigs/`: a number of test configurations that can be used as templates
- `docs/`: offline HTML documentation

To run openhat, go to the `bin` folder and execute it:

	$ openhatd -c hello-world.ini

Point your browser to http://localhost:8080 or use the AndroPDI Remote Control at port 13110.

Installing the AndroPDI Remote Control requires you to temporarily allow the installation of third-party apps. Your device should ask for the permission automatically. 

The Raspberry Pi package may also work on other ARM devices. Plugins may be specific to the Raspberry Pi, however. openhatd has been tested and confirmed to work on the following systems:

- Raspberry Pi models B and B+ (Raspbian)
- Raspberry Pi (Raspbian)
- NanoPi NEO (FriendlyARM's image based on UbuntuCore)

## Building from source

See [Development](development).

## Previous releases

There are currently no previous releases.