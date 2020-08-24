# 11nocturn
 A Max/Msp external using LibUSB to connect with a Novation Nocturn USB controller.
 
 As Mac OsX user, skip this.
 
 As PC/Windows user, we have to install the WinUSB driver for the Nocturn device. 
 It will replace the original driver by Novation and enable a connection with the C external.
 There are multiple ways to install a WinUSB driver.
 
 You'll find "zadig-2.5.exe" released under GPLv3 here: 
 https://zadig.akeo.ie/
 or added to the zip from 11oLsen.de.
 Alternatively, search for "libusbK-inf-wizard" which does the same. 
 
 Make sure Nocturn is connected,
 run zadig-2.5.exe,
 choose "list all devices" in the menu,
 pick Nocturn and choose WinUSB Driver,
 Install !

 The Device Manager of Windows is useful to monitor driver changes and connection state of usb devices.
 
 Maybe you have to repeat the driver installation once you changed the usb port for the Nocturn.
 
 In rare cases, the Nocturn is not noticed by Windows after a reboot: 
 Reinstalling WinUSB driver or dis- and reconnecting usb cable always helped so far.

 
