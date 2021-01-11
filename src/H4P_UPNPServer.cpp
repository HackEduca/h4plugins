/*
 MIT License

Copyright (c) 2020 Phil Bowles <h4plugins@gmail.com>
   github     https://github.com/philbowles/esparto
   blog       https://8266iot.blogspot.com     
   groups     https://www.facebook.com/groups/esp8266questions/
              https://www.facebook.com/Esparto-Esp8266-Firmware-Support-2338535503093896/
                			  

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include<H4P_UPNPServer.h>

//constexpr char* xh4dTag(){ return "X-H4-DEVICE"; }

void H4P_UPNPServer::_hookIn(){
    DEPEND(asws);
    REQUIREBT;
    string dn=uppercase(h4Tag())+" "+deviceTag()+" ";
    if(!h4wifi._getPersistentValue(nameTag(),dn)) if(_name!="") _cb[nameTag()]=_name;
    H4EVENT("UPNP name %s",CSTR(_cb[nameTag()]));
    H4EVENT("***** DIAG N of _detect items=%d",_detect.size());
/*    // sniff neighbours
    _listenTag("NT",_pups.back(),[this](uint32_t mx,H4P_CONFIG_BLOCK blok,bool dora){ 
        if(dora) _otherH4s.insert(blok[xh4dTag()]);
        else _otherH4s.erase(blok[xh4dTag()]);
    });
*/
}

void  H4P_UPNPServer::friendlyName(const string& name){ h4wifi._setPersistentValue(nameTag(),name,true); }

uint32_t H4P_UPNPServer::_friendly(vector<string> vs){
    return guard1(vs,[this](vector<string> vs){
        h4wifi._setPersistentValue(nameTag(),H4PAYLOAD,true);
        return H4_CMD_OK;
    });
}

void H4P_UPNPServer::__upnpSend(uint32_t mx,const string s,IPAddress ip,uint16_t port){
	h4.nTimesRandom(H4P_UDP_REPEAT,0,mx,bind([this](IPAddress ip,uint16_t port,string s) {
		_udp.writeTo((uint8_t *)CSTR(s), s.size(), ip, port);
	},ip,port,s),nullptr,H4P_TRID_UDPS); // name this!!
}

void H4P_UPNPServer::_handlePacket(string p,IPAddress ip,uint16_t port){
    static bool cursed=false;
    if(cursed) Serial.printf("I BEEN RECURSED!!!!!!!!!!!!!!!!!!!*********************\n");
    cursed=true;
//    Serial.printf("IN  pkt sz=%d\n", p.size());
    if(p.size() > 50){
        H4P_CONFIG_BLOCK uhdrs;
        vector<string> hdrs = split(p, "\r\n");
//        Serial.printf("nlines=%d\n",hdrs.size());
        while (hdrs.back() == "") hdrs.pop_back();
        if(hdrs.size() > 4){
            for (auto const &h :vector<string>(++hdrs.begin(), hdrs.end())) {
                vector<string> parts=split(h,":");
                if(parts.size()){
                    string key=uppercase(parts[0]);
                    uhdrs[key]=join(vector<string>(++parts.begin(),parts.end()),":");
                } else Serial.printf("NO COLONS %s\n",CSTR(h)); 
            }
        } else Serial.printf("TiNY BLOCK Nh=%d\n",hdrs.size()); 

        if(_detect.size()) Serial.printf("DETECT CORRUPTION 2 sz=%d\n",_detect.size());
        uint32_t mx=1000 * atoi(CSTR(replaceAll(uhdrs["CACHE-CONTROL"],"max-age=","")));
        if (p[0] == 'M') { //-SEARCH
            string ST = uhdrs["ST"];
            if (ST==_pups[1]) { // make tag
                string tail=((ST==_pups[1]) ? ST:"");
                __upnpSend(mx, "HTTP/1.1 200 OK\r\nST:" + ST +"\r\n" +__upnpCommon(tail), ip,port);
            }
        }
        else {
            if(_detect.size()) Serial.printf("DETECT CORRUPTION 3 sz=%d\n",_detect.size());
            if (p[0] == 'N') { //OTIFY
                for(auto const& d:_detect){
                    Serial.printf(" WILL NEVER SEE!!! %s sz=%d [DET sz=%d]\n",CSTR(p),p.size(),_detect.size());
                    for (auto const &h :uhdrs) Serial.printf("UH: %s=%s\n",CSTR(h.first),CSTR(h.second));
                    string tag=d.first;
                    if(uhdrs.count(tag) && uhdrs[tag].find(d.second.first)!=string::npos) d.second.second(mx,uhdrs,uhdrs["NTS"].find(aliveTag())!=string::npos);
                }
            }
        }
    } else Serial.printf("WTF SHORT SHIT %s\n",CSTR(p));
    cursed=false;
//    Serial.printf("OUT pkt sz=%d\n", p.size());
}

void H4P_UPNPServer::_listenUDP(){ 
    if(!_udp.listenMulticast(_ubIP, 1900)) return; // some kinda error?
    _udp.onPacket([this](AsyncUDPPacket packet){
//        Serial.printf("pkt sz=%d\n",packet.length());
        string pkt=stringFromBuff(packet.data(),packet.length());
        IPAddress ip=packet.remoteIP();
        uint16_t port=packet.remotePort();
        h4.queueFunction([this,pkt,ip,port](){ _handlePacket(pkt,ip,port); });
    }); 
}

string H4P_UPNPServer::__makeUSN(const string& s){
	string full=_uuid+_cb["udn"];
	return s.size() ? full+="::"+s:full;
}

string H4P_UPNPServer::__upnpCommon(const string& usn){
	_cb["usn"]=__makeUSN(usn);
	string rv=replaceParams(_ucom);
	return rv+"\r\n";
}

void H4P_UPNPServer::_start(){
    _cb["age"]=stringFromInt(H4P_UDP_REFRESH/1000); // fix
    _cb["udn"]="Socket-1_0-upnp"+_cb[chipTag()];
    _cb["updt"]=_pups[2];
    _cb["umfr"]="Belkin International Inc.";
    _cb["usvc"]=_pups[3];
    _cb["usid"]=_urn+"serviceId:basicevent1";

    _xml=replaceParamsFile("/up.xml");
    _ucom=replaceParamsFile("/ucom.txt");
    _soap=H4P_SerialCmd::read("/soap.xml");
// erase redundant _cb?
    _cb.erase("age");
    _cb.erase("updt");
    _cb.erase("umfr");
    _cb.erase("usvc");
    _cb.erase("usid");
//
    h4asws.on("/we",HTTP_GET, [this](AsyncWebServerRequest *request){ request->send(200,"text/xml",CSTR(_xml)); });
    h4asws.on("/upnp", HTTP_POST,[this](AsyncWebServerRequest *request){ _upnp(request); },
        NULL,
        [](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total){
            if(!index) request->_tempObject = malloc(total+1);
            memcpy((uint8_t*) request->_tempObject+index,data,len);
            if(index + len == total) *((uint8_t*) request->_tempObject+total)='\0';
        }
    );
    _listenUDP();
    _notify(aliveTag()); // TAG
    h4.every(H4P_UDP_REFRESH / 2,[this](){ _notify(aliveTag()); },nullptr,H4P_TRID_NTFY,true); // TAG
    _upHooks();
}

void H4P_UPNPServer::_upnp(AsyncWebServerRequest *request){ // redo
  h4.queueFunction(bind([this](AsyncWebServerRequest *request) {
        string soap=stringFromBuff((const byte*) request->_tempObject,strlen((const char*) request->_tempObject));
        _cb["gs"]=(soap.find("Get")==string::npos) ? "Set":"Get";
        uint32_t _set=soap.find(">1<")==string::npos ? 0:1;
#ifdef H4P_LOG_EVENTS
        if(_cb["gs"]=="Set") _btp->_turn(_set,_pName);
#else
        if(_cb["gs"]=="Set") _btp->turn(_set);
#endif
//        _cb[stateTag()]=stringFromInt(_btp->state());
        request->send(200, "text/xml", CSTR(replaceParams(_soap))); // refac
    },request),nullptr, H4P_TRID_SOAP);
}

void H4P_UPNPServer::_stop(){
    _notify("byebye");
    h4.cancelSingleton(H4P_TRID_NTFY);
    _udp.close();
    _downHooks();
}

void H4P_UPNPServer::_notify(const string& phase){ // h4Chunker it up
    h4Chunker<vector<string>>(_pups,[this,phase](vector<string>::const_iterator i){ 
        string NT=(*i).size() ? (*i):__makeUSN("");
        string nfy="NOTIFY * HTTP/1.1\r\nHOST:"+string(_ubIP.toString().c_str())+":1900\r\nNTS:ssdp:"+phase+"\r\nNT:"+NT+"\r\n"+__upnpCommon((*i));
        broadcast(H4P_UDP_JITTER,CSTR(nfy));
    });
}

string H4P_UPNPServer::replaceParams(const string& s){ // oh for a working regex!
	int i=0;
	int j=0;
	string rv(s);

	while((i=rv.find("%",i))!=string::npos){
        if(j){
            string var=rv.substr(j+1,i-j-1);
            if(_cb.count(var)) {
                rv.replace(j,i-j+1,_cb[var]); // FIX!!
                rv.shrink_to_fit();
            }
            j=0;
        } else j=i;    
        ++i;
	}
	return rv.c_str();	
}