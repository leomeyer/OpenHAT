## openhatd latest stable release
There is no latest stable release yet. Please refer to the [release plan](release_plan.md).

## openhatd current development version
These are the current Continuous Integration builds from the master branch. They are not considered to be stable or suitable for production use. 

- Linux 32 bit: [openhatd-linux-i386-0.1.0-current.tar.gz](https://openhat.org/downloads/openhatd-linux-i386-0.1.0-current.tar.gz) ([MD5](https://openhat.org/downloads/openhatd-linux-i386-0.1.0-current.tar.gz.md5))
- Linux 64 bit: [openhatd-linux-x86_64-0.1.0-current.tar.gz](https://openhat.org/downloads/openhatd-linux-x86_64-0.1.0-current.tar.gz) ([MD5](https://openhat.org/downloads/openhatd-linux-x86_64-0.1.0-current.tar.gz.md5))
- Raspberry Pi: [openhatd-linux-armhf-0.1.0-current.tar.gz](https://openhat.org/downloads/openhatd-linux-armhf-0.1.0-current.tar.gz) ([MD5](https://openhat.org/downloads/openhatd-linux-armhf-0.1.0-current.tar.gz.md5))
- Windows: [openhatd-windows-0.1.0-current.zip](https://openhat.org/downloads/openhatd-windows-0.1.0-current.zip) ([MD5](https://openhat.org/downloads/openhatd-windows-0.1.0-current.zip.md5))

- Offline HTML documentation: [openhatd-docs-0.1.0-current.tar.gz](https://openhat.org/downloads/openhatd-docs-0.1.0-current.tar.gz)

## AndroPDI Remote Control
The stable version of the AndroPDI Remote Control can be found on the Google Play store. 

- AndroPDI Remote Control: [AndroPDI-0.1.0.apk](https://openhat.org/downloads/AndroPDI-0.1.0.apk) or from the Google Play store: [AndroPDI Remote Control](https://play.google.com/store/apps/details?id=org.openhat.androPDI)

Installing the AndroPDI Remote Control directly from the APK file requires you to temporarily allow the installation of third-party apps. Your device should ask for the permission automatically. 

## Installation
A special installation routine is not required.

1. Create a directory called `openhat`.
2. On Windows, copy the zip file contents to the directory.
3. On Linux, copy the tar.gz file to the directory and extract it, for example:  
 	`$ tar xfz openhatd-linux-x86_64-0.1.0-current.tar.gz --strip-components=1`

The folder contents are:

- `bin/`: main executable, shared libraries and `hello-world.ini`
- `plugins/`: plugin shared object folders
- `testconfigs/`: a number of test configurations that can be used as templates
- `doc/`: HTML documentation

To run openhat, go to the `bin` folder and execute it:

	$ openhatd -c hello-world.ini

Point your browser to [http://localhost:8080](http://localhost:8080) or use the AndroPDI Remote Control at port 13110.

The Raspberry Pi package may also work on other ARM devices such as the NanoPi. Plugins may be specific to the Raspberry Pi, however. openhatd has been tested and confirmed to work on the following systems:

- Raspberry Pi models B and B+ (Raspbian)
- Raspberry Pi 2 (Raspbian)
- NanoPi NEO (FriendlyARM's image based on UbuntuCore)

## Building from source
See [Development](development.md).

## Previous releases
There are currently no previous releases.
