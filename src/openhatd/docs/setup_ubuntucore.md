Setting up an OpenHAT server

Based on Ubuntu Core

Install the operating system on a Flash card. How to do this depends on your host system; please see the instructions on the Ubuntu Core web site.

Start the system and log in via SSH as root. The first thing to do is to change your root password:

	# passwd
 
Increase the file system size to the maximum of your Flash card:

	# fs_resize

If you plan to run your device 24/7 you should now give some consideration to Flash card wear. Flash cards degrade when written, and after some time they fail which can result in all kinds of weird errors. For this reason it is recommended to make your operating system partition read-only. This is however not practical for an OpenHAT system because OpenHAT sometimes needs write access to store (for example, for user preferences), and it is especially inconvenient for testing during the development of an automation system.