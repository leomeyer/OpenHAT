FritzBox automation plugin for OpenHAT

This plugin provides the functionality to set and read the state of actors connected to a Fritz!Box.
Refer to: http://avm.de/service/schnittstellen/

At the moment only the Fritz!DECT200 is supported. This power socket can be switched on and off.
Additionally, it provides a power meter (measurements are taken in mW) plus a total energy meter
(measured in Wh).

The readings are not always read when the OPDI master requests the current state. This would make no sense
because the FritzBox also periodically queries the sensors; so, in any case, the OPDI slave's readings
reflect the FritzBox state and not the actual sensor state.
In order to periodically query the FritzBox you should setup periodic refreshs for the sensors.

The FritzBox is controlled via its HTTP API. HTTPS is not (yet) supported by this plugin.

Configure the FritzBoxPlugin using the following node sections:

[FritzBox]
Driver = ..\plugins\automation\FritzBoxPlugin\Debug\FritzBoxPlugin.dll

; Host name or IP address of the FritzBox
Host = 192.168.0.113

; Port number. Usually this will be 80.
;Port = 90

; User name. It's recommended to use a special user that has only permissions for the SmartHome functions.
User = FBSmartHome
Password = FBSmartHome123

; Timeout in seconds. Use a short timeout (usually the FritzBox will be in a local network, anyway).
Timeout = 3

; Nodes of the FritzBox are defined in its own section named <node>.Nodes
[FritzBox.Nodes]

; FritzBox actor definitions
FritzBoxSwitch1 = 0

[FritzBoxSwitch1]
; Required. Defines a DECT200 power socket.
Type = FritzDECT200
; Actor Identification Number as displayed by the FritzBox interface (without blanks).
AIN = 087610154758

; General port settings (standard)
Label = DECTDose

; The FritzDECT200 node type creates three ports; in this example:
; ID = FritzBoxSwitch1: A digital output port that can be used to switch the socket on and off
; ID = FritzBoxSwitch1Power: A read-only dial port that reflects the current power consumption (mW)
; ID = FritzBoxSwitch1Energy: A read-only dial port that reflects the total energy consumption (Wh)
; Make sure that these internally created port IDs do not conflict with other nodes.
; You can hide the energy and power ports separately. If you specify Hidden = true, all three ports
; will be hidden.
Hidden = false

; Hides the power display port, if true
PowerHidden = false
; Label for the power display
PowerLabel = DECTDose Aktuelle Leistung
; Refresh mode and time for the power meter port. It's recommended to use Periodic with a not too short timespan.
PowerRefreshMode = Periodic
PowerRefreshTime = 10000

; Hides the energy display port, if true
EnergyHidden = false
; Label for the energy display
EnergyLabel = DECTDose Gesamt-Verbrauch
; Refresh mode and time for the energy meter port. It's recommended to use Periodic with a not too short timespan.
EnergyRefreshMode = Periodic
EnergyRefreshTime = 10000
