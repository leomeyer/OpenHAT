##Xiaomi Mi Robot Vacuum Cleaner Integration

The Xiaomi Mi robot can be controlled using a Python command line tool that can be found at [https://github.com/rytilahti/python-mirobo](https://github.com/rytilahti/python-mirobo). This program requires Python 3.4 or newer. On a Raspberry Pi with a Raspbian distribution older than Jessie you have to setup Python 3 by following the instructions at [http://depado.markdownblog.com/2015-03-12-short-tutorial-raspbian-python3-4-rpi-gpio](http://depado.markdownblog.com/2015-03-12-short-tutorial-raspbian-python3-4-rpi-gpio). (You don't need to install the Python GPIO libraries, however.) You can also download and install a version of Python that is more recent; have a look at [https://www.python.org/ftp/python/](https://www.python.org/ftp/python/) to see which versions are available. This integration has been verified to work with Python 3.4.6.

Once the command line tool is available and working, the Xiaomi robot can be controlled using commands issued by `Exec` ports. You need the robot's IP address and the encryption token generated before registering the robot with its control app.

It is recommended to use `virtualenv`. To do so, follow the instructions at [http://docs.python-guide.org/en/latest/dev/virtualenvs/](http://docs.python-guide.org/en/latest/dev/virtualenvs/). 

Make sure that you have Python installed. Install `virtualenv` via `pip`:

	$ pip install virtualenv

In your `openhat` directory, create a folder called `mirobo` and initialize it for `virtualenv`: 

	$ mkdir mirobo
	$ virtualenv -p /usr/bin/python3 mirobo

Use your mirobo folder as Python environment:

	$ cd mirobo
	$ source bin/activate

Install mirobo:

	$ sudo apt-get install libffi-dev libssl-dev
	$ pip3 install python-mirobo

See [https://github.com/rytilahti/python-mirobo](https://github.com/rytilahti/python-mirobo) for how to get started and retrieve the encryption token.

The `testconfigs` folder contains an include file that can be used to setup the control ports for a Xiaomi robot (`xiaomi_mi_robovac.ini`). Example:

	[Root]
	...
	IncludeXiaomi = 100

	...
	[IncludeXiaomi]
	Type = Include
	Filename = testconfigs/xiaomi_mi_robovac.ini

	[IncludeXiaomi.Parameters]
	ROBOVAC_ID = Xiaomi
	ROBOVAC_NAME = Xiaomi
	ROBOVAC_GROUP = ... (optional)
	ROBOVAC_IP = <IP address as reported by 'mirobo discover'>
	ROBOVAC_TOKEN = <encryption token as reported by 'mirobo discover'>

See the include file `testconfigs/xiaomi_mi_robovac.ini` for more documentation.
