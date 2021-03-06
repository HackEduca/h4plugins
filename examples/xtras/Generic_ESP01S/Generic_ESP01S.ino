#include<H4Plugins.h>
H4_USE_PLUGINS(0,20,true) // Serial baud rate, Q size, SerialCmd autostop
H4P_GPIOManager h4gm;
H4P_FlasherController h4fc;
H4P_WiFi h4wifi("XXXXXXXX","XXXXXXXX"); // device defaults to "H4-<chip id>"
H4P_AsyncMQTT h4mqtt("192.168.1.4",1883);
H4P_BinarySwitch h4onof(LED_BUILTIN,ACTIVE_LOW,OFF);
H4P_UPNPServer h4upnp; // friendly name defaults to "upnp <chip id>"
H4P_RemoteUpdate h4ru("192.168.1.4:1880/update",__FILE__);