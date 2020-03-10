#include<H4Plugins.h>
H4_USE_PLUGINS
//
//  Function is called by
//  h4/off
//  h4/on
//  h4/toggle
//  h4/switch/n where n=0 or 1
//  And reports current state with h4/state
//  If MQTT is used, publishes current state 
//
H4 h4(115200);
H4P_SerialCmd h4sc;
H4P_SerialLogger h4sl;
H4P_BinaryThing h4onof([](bool b){ Serial.print("I am now ");Serial.println(b ? "ON":"OFF"); },OFF,10000);

void h4setup(){
  h4onof.turnOn(); // will turn off automatically after 10 seconds
}