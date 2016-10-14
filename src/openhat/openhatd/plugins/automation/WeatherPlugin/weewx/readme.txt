Integrating the WeatherPlugin with Weewx
----------------------------------------

After installing Weewx you should check that everything works ok with the standard configuration.
See the Weewx documentation for installation and setup instructions.

Important: On a Raspberry Pi, to minimize SD card wear, put output and log directories on a ramdisk.
This example assumes that /var/weewx is the target directory.

To mount this directory as a ramdisk, put the following into /etc/fstab:
weewx /var/weewx tmpfs size=10M,noexec,nosuid,nodev 0 0

Redirect the weewx log messages to a separate file on a ramdisk. Put the following into /etc/rsyslog.d/weewx.conf

if $programname == 'weewx' then /var/weewx/log/weewx.log
if $programname == 'weewx' then ~

Restart syslog so that it picks up the changes:

sudo /etc/init.d/rsyslog stop
sudo /etc/init.d/rsyslog start

Use logrotate to prevent the logfiles from getting too big. Put the following into /etc/logrotate.d/weewx:

/var/weewx/log/weewx.log {
  weekly
  missingok
  rotate 12
  compress
  delaycompress
  notifempty
  create 644 root adm
  sharedscripts
  postrotate
    /etc/init.d/weewx reload > /dev/null
  endscript
}

Recommendation for weewx.conf settings
--------------------------------------
The standard configuration file is located at /etc/weewx/weewx.conf.

1. For use with a TFA Sinus weather station:
[Station]
    # Set to type of station hardware.  There must be a corresponding stanza
    # in this file with a 'driver' parameter indicating the driver to be used.
    station_type = TE923

[TE923]
    model = TE923

    driver = weewx.drivers.te923

    # The default configuration associates the channel 1 sensor with outTemp
    # and outHumidity.  To change this, or to associate other channels with
    # specific columns in the database schema, use the following maps.
    [[sensor_map]]
        # Map the remote sensors to columns in the database schema.
        outTemp = t_1
        outHumidity = h_1
        extraTemp1 = t_2
        extraHumid1 = h_2
        extraTemp2 = t_3
        extraHumid2 = h_3
        extraTemp3 = t_4
        # WARNING: the following are not in the default schema
        extraHumid3 = h_4
        extraTemp4 = t_5
        extraHumid4 = h_5
    
    [[battery_map]]
        txBatteryStatus = batteryUV
        windBatteryStatus = batteryWind
        rainBatteryStatus = batteryRain
        outTempBatteryStatus = battery1
        # WARNING: the following are not in the default schema
        extraBatteryStatus1 = battery2
        extraBatteryStatus2 = battery3
        extraBatteryStatus3 = battery4
        extraBatteryStatus4 = battery5

2. Set target_unit to METRIC (delete or move SQLite database if it already exists)
[StdConvert]
    target_unit = METRIC    # Options are 'US', 'METRICWX', or 'METRIC'

3. Use the following StdQC settings:
[StdQC]
    # This section is for quality control checks.  If units are not specified,
    # values must be in the units defined in the StdConvert section.
    
    [[MinMax]]
        barometer = 990, 1050
        outTemp = -40, 120
        inTemp = 0, 50
        outHumidity = 0, 100
        inHumidity = 0, 100

Installing the JSON skin
------------------------

To use the WeatherPlugin with Weewx, you need to install the JSON skin.
These instructions assume the default installation folders on the Raspberry Pi.

Create the skin directory and copy the files:
> sudo mkdir /etc/weewx/skins/JSON
> sudo cp skin.conf /etc/weewx/skins/JSON/
> sudo cp current.json /etc/weewx/skins/JSON/

Edit /etc/weewx/weewx.conf:

Below [[StdReport]], add:
    [[JSON]]
        skin = JSON
		
To avoid unnecessary generation of the standard reports, disable their generation
by uncommenting all lines including and belonging to [[StdReport]].
As this takes quite a bit of resources it is probably a wise thing to do.
    
Restart weewx:
> sudo service weewx restart

Check /var/log/syslog for weewx messages. You should see the following (similar) lines:
May  2 15:53:34 raspberrypi weewx[17062]: engine: Starting up weewx version 3.1.0
May  2 15:53:34 raspberrypi weewx[17062]: engine: Starting main packet loop.

If the JSON output file has been generated, the syslog will show something like:
May  2 15:55:16 raspberrypi weewx[17062]: manager: added record 2015-05-02 15:55:00 BST (1430578500) to database '/var/lib/weewx/weewx.sdb'
May  2 15:55:16 raspberrypi weewx[17062]: manager: added record 2015-05-02 15:55:00 BST (1430578500) to daily summary in '/var/lib/weewx/weewx.sdb'
May  2 15:55:21 raspberrypi weewx[17062]: cheetahgenerator: Generated 1 files for report JSON in 1.66 seconds

Check the generated JSON output:
> cat /var/www/weewx/current.JSON
Output should be something like:
{
  "title":"Current Values",
  "location":"rpi",
  "time":"02/05/15 15:55:00",
  "lat":"47&deg; 39.68' N",
  "lon":"008&deg; 53.32' E",
  "alt":"450",
  "hardware":"TE923",
  "uptime":"0, 0, 1",
  "serverUptime":"0, 1, 51",
  "weewxVersion":"3.1.0",
  "stats": {
    "current": {
      "inTemp":"18.2",
      "inHumidity":"58",
      "outTemp":"   N/A",
      "outHumidity":"   N/A",
      "dewpoint":"   N/A",
      "heatIndex":"   N/A",
      "barometer":"1015.2",
      "windSpeed":"1",
      "windchill":"18.5",
      "windDir":"234",
      "windDirText":"SW",
      "windGust":"3",
      "windGustDir":"225",
      "rainRate":"0.0",
      "extraTemp1":"18.3",
      "extraHumid1":"68",
      "extraTemp2":"   N/A",
      "extraHumid2":"   N/A",
      "extraTemp3":"   N/A",
      "extraHumid3":"?'extraHumid3'?",
      "extraTemp4":"?'extraTemp4'?",
      "extraHumid4":"?'extraHumid4'?"
    }
  }
}

Check that sensor values are present. If a sensor is N/A, it will show as invalid on the GUI.


