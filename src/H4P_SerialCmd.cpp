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
#include<H4P_SerialCmd.h>

extern void h4FactoryReset(const string& src);

H4P_SerialCmd::H4P_SerialCmd(bool autoStop): H4Plugin(H4PID_CMD){
    _addLocals({
        {h4Tag(),      { 0,         H4PC_H4, nullptr}},
        {"help",       { 0,         0, CMD(help) }},
#if SANITY
        {"sanity",     { 0,         0, CMD(h4psanitycheck) }},
#endif
        {"event",      { H4PC_H4,   0, CMDVS(_event) }}, // dangerous!!!
        {"reboot",     { H4PC_H4,   0, CMD(h4reboot) }},
        {"factory",    { H4PC_H4,   0, ([this](vector<string>){ h4FactoryReset(_cb[srcTag()]); return H4_CMD_OK; }) }},
        {"dump",       { H4PC_H4,   0, CMDVS(_dump)}},
        {"svc",        { H4PC_H4,   H4PC_SVC, nullptr}},
        {"info",       { H4PC_SVC,  0, CMDVS(_svcInfo) }},
        {"restart",    { H4PC_SVC,  0, CMDVS(_svcRestart) }},
        {"start",      { H4PC_SVC,  0, CMDVS(_svcStart) }},
        {"stop",       { H4PC_SVC,  0, CMDVS(_svcStop) }},
        {"show",       { H4PC_H4,   H4PC_SHOW, nullptr}},
        {"all",        { H4PC_SHOW, 0, CMD(all) }},
        {"config",     { H4PC_SHOW, 0, CMD(config) }},
        {"q",          { H4PC_SHOW, 0, CMD(showQ) }},
        {"plugins",    { H4PC_SHOW, 0, CMD(plugins) }},
        {"heap",       { H4PC_SHOW, 0, CMD(heap) }},
        {"fs",         { H4PC_SHOW, 0, CMD(showFS)}}
            });
    if(autoStop) QTHIS(stop);
}

H4_CMD_MAP_I H4P_SerialCmd::__exactMatch(const string& cmd,uint32_t owner){
    auto any=_commands.equal_range(cmd);
    for(auto i=any.first;i!=any.second;i++) if(i->second.owner==owner) return i;
    return _commands.end();
}

void H4P_SerialCmd::__flatten(function<void(string)> fn){
    H4_CMD_MAP_I ptr;
    for(ptr=_commands.begin();ptr!=_commands.end(); ptr++){
        if(!(ptr->second.owner)){
            if(ptr->second.levID) _flattenCmds(fn,ptr->first,ptr->first,ptr->second.levID);    
            else fn(ptr->first);
        } 
    }
}

uint32_t H4P_SerialCmd::_dispatch(vector<string> vs,uint32_t owner=0){
    if(vs.size()){
        H4_CMD_MAP_I i;
        string cmd=vs[0];
        i=__exactMatch(cmd,owner);
        if(i!=_commands.end()){
            if(i->second.fn) return [=](){ return i->second.fn(CHOP_FRONT(vs)); }();
            else return _dispatch(CHOP_FRONT(vs),i->second.levID);
        } else return H4_CMD_UNKNOWN;
    } else return H4_CMD_UNKNOWN;
}

uint32_t H4P_SerialCmd::_executeCmd(string topic, string pload){
	vector<string> vs=split(CSTR(topic),"/");
    _cb[srcTag()]=vs[0];
	vs.push_back(pload);
    vector<string> cmd(vs.begin()+2,vs.end());
    #if H4P_LOG_EVENTS
        PEVENT(H4P_EVENT_CMD,"%s %s",CSTR(vs[0]),CSTR(join(cmd,"/")));
    #endif	
    uint32_t rv=_dispatch(vector<string>(cmd)); // optimise?
    #if H4P_LOG_EVENTS
        PEVENT(H4P_EVENT_CMDREPLY,"%s",CSTR(h4pgetErrorMessage(rv)));
    #endif
    return rv;
}

void H4P_SerialCmd::_flattenCmds(function<void(string)> fn,string cmd,string prefix,uint32_t lev){
    H4_CMD_MAP_I i=_commands.find(cmd);
    for(i=_commands.begin();i!=_commands.end();i++){
        if(i->second.owner==lev){
			string trim = prefix+"/"+i->first;
			if(i->second.levID) _flattenCmds(fn,i->first,trim,i->second.levID);
			else fn(trim);
        }
    }
}

void H4P_SerialCmd::_hookIn(){ HAL_FS.begin(); }

void H4P_SerialCmd::_run(){
    static string cmd="";
	static int	c;
    if((c=Serial.read()) != -1){
        if (c == '\n') {
            h4.queueFunction([=](){
                uint32_t err=_simulatePayload(cmd);
                if(err) reply("%s\n",CSTR(h4pgetErrorMessage(err)));
                cmd="";
            },nullptr,H4P_TRID_SCMD);
        } else cmd+=c;
    }
}

uint32_t H4P_SerialCmd::_simulatePayload(string flat,const char* src){ // refac
    vector<string> vs=split(flat,"/");
    if(vs.size()){
		string pload=CSTR(H4PAYLOAD);
		vs.pop_back();
		string topic=join(vs,"/");
		return invokeCmd(topic,pload,src); // _invoke
	} else return H4_CMD_TOO_FEW_PARAMS;
}
//
uint32_t H4P_SerialCmd::_svcControl(H4P_SVC_CONTROL svc,vector<string> vs){
    return _guard1(vs,[this,svc](vector<string> vs){
        return ([this,svc](string s){
            auto p=h4pptrfromtxt(s);
            if(p) {
                switch(svc){
                    case H4PSVC_START:
                        p->start();
                        break;
                    case H4PSVC_STATE:
                        p->show();
                        break;
                    case H4PSVC_STOP:
                        p->stop();
                        break;
                    case H4PSVC_RESTART:
                        p->restart();
                        break;
                }
                return H4_CMD_OK;
            }
            return H4_CMD_NAME_UNKNOWN;
        })(H4PAYLOAD);
    });
}

uint32_t H4P_SerialCmd::_event(vector<string> vs){ 
    return _guard1(vs,[this](vector<string> vs){
        auto vg=split(H4PAYLOAD,",");
        if(vg.size()!=2) return H4_CMD_PAYLOAD_FORMAT;
        if(!stringIsNumeric(vg[0])) return H4_CMD_NOT_NUMERIC;
        PEVENT(static_cast<H4P_EVENT_TYPE>(1 << STOI(vg[0])),"%s",CSTR(vg[1]));
        return H4_CMD_OK;
    });
}

uint32_t H4P_SerialCmd::_svcRestart(vector<string> vs){ return _svcControl(H4PSVC_RESTART,vs); }

uint32_t H4P_SerialCmd::_svcStart(vector<string> vs){ return _svcControl(H4PSVC_START,vs); }

uint32_t H4P_SerialCmd::_svcInfo(vector<string> vs){ return _svcControl(H4PSVC_STATE,vs); }

uint32_t H4P_SerialCmd::_svcStop(vector<string> vs){ return _svcControl(H4PSVC_STOP,vs); }
//
//
//
void H4P_SerialCmd::addCmd(const string& name,uint32_t owner, uint32_t levID,H4_FN_MSG f){
    if(__exactMatch(name,owner)==_commands.end()) _commands.insert(make_pair(name,command {owner,levID,f}));
}

void H4P_SerialCmd::help(){ 
    vector<string> unsorted={};
    __flatten([&unsorted](string s){ unsorted.push_back(s); });
    sort(unsorted.begin(),unsorted.end());
    for(auto const& s:unsorted) { reply(CSTR(s)); }
}

uint32_t H4P_SerialCmd::invokeCmd(string topic,string payload,const char* src){ 
    return _executeCmd(string(src)+"/h4/"+CSTR(topic),string(CSTR(payload)));
}

uint32_t H4P_SerialCmd::invokeCmd(string topic,uint32_t payload,const char* src){ 
    return invokeCmd(topic,stringFromInt(payload),src);
}

void H4P_SerialCmd::removeCmd(const string& s,uint32_t _pid){ if(__exactMatch(s,_pid)!=_commands.end()) _commands.erase(s); }

string H4P_SerialCmd::read(const string& fn){
	string rv="";
        File f=HAL_FS.open(CSTR(fn), "r");
        if(f && f.size()) {
            int n=f.size();
            uint8_t* buff=(uint8_t *) malloc(n+1);
            f.readBytes((char*) buff,n);
            rv=stringFromBuff(buff,n);
            free(buff);
        }
        f.close();
	return rv;	
}

uint32_t H4P_SerialCmd::write(const string& fn,const string& data,const char* mode){
    File b=HAL_FS.open(CSTR(fn), mode);
    b.print(CSTR(data));
    uint32_t rv=b.size(); // ESP32 pain
    b.close();
    return rv; 
}

void H4P_SerialCmd::all(){
    __flatten([this](string s){ 
        vector<string> candidates=split(s,"/");
        if(candidates.size()>1 && candidates[1]=="show" && candidates[2]!="all") {
            reply(CSTR(s));
            invokeCmd(s,"",CSTR(_cb[srcTag()])); // snake / tail etc
            reply("");
        }
    });
}
// ifdef log events?
const char* __attribute__((weak)) giveTaskName(uint32_t id){ return "ANON"; }

string H4P_SerialCmd::_dumpTask(task* t){
    char buf[128];
    uint32_t type=t->uid/100;
    uint32_t id=t->uid%100;
    sprintf(buf,"%09lu %s/%s %s %9d %9d %9d",
        t->at,
        CSTR(h4pgetTaskType(type)),
        CSTR(h4pgetTaskName(id)),
        t->singleton ? "S":" ",
        t->rmin,
        t->rmax,
        t->nrq);
    return string(buf);
}

void H4P_SerialCmd::showQ(){
	reply("Due @tick Type              Min       Max       nRQ");  
    vector<task*> tlist=h4._copyQ();
    sort(tlist.begin(),tlist.end(),[](const task* a, const task* b){ return a->at < b->at; });
    for(auto const& t:tlist) reply(CSTR(_dumpTask(t)));
}

void  H4P_SerialCmd::plugins(){
    for(auto const& pp:h4pmap){
        auto p=pp.second;
        reply("h4/svc/info/%s %s ID=%d",CSTR(p->_pName),p->_state() ? "UP":"DN",p->_pid);
        p->show();
        reply("");
    }
}

uint32_t H4P_SerialCmd::_dump(vector<string> vs){
    return _guard1(vs,[this](vector<string> vs){
        return ([this](string h){ 
            reply("DUMP FILE %s",CSTR(h));
            reply("%s",CSTR(read("/"+h)));
            return H4_CMD_OK;
        })(H4PAYLOAD);
    });
}

#ifdef ARDUINO_ARCH_ESP8266
void H4P_SerialCmd::showFS(){
    uint32_t sigma=0;
    uint32_t n=0;

    FSInfo info;
    HAL_FS.info(info);
    reply("totalBytes %d",info.totalBytes);
    reply("usedBytes %d",info.usedBytes);
    reply("blockSize %d",info.blockSize);
    reply("pageSize %d",info.pageSize);
    reply("maxOpenFiles %d",info.maxOpenFiles);
    reply("maxPathLength %d",info.maxPathLength);
    
    Dir dir = HAL_FS.openDir("/");

    while (dir.next()) {
        n++;
        reply("%s (%u)",CSTR(dir.fileName()),dir.fileSize());
        sigma+=dir.fileSize();
    }
    reply("%d file(s) %u bytes",n,sigma);
}
#else
void H4P_SerialCmd::showFS(){
    uint32_t sigma=0;
    uint32_t n=0;

    reply("totalBytes %d",HAL_FS.totalBytes());
    reply("usedBytes %d",HAL_FS.usedBytes());

    File root = HAL_FS.open("/");
 
    File file = root.openNextFile();
 
    while(file){
        n++;
        reply("%s (%u)",file.name(),file.size());
        sigma+=file.size();
        file = root.openNextFile();
    }
    reply("%d file(s) %u bytes",n,sigma);
}
#endif // 8266/32 spiffs