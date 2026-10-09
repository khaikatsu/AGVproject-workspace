#pragma once
#include <string>
#include <unordered_map>
inline std::unordered_map<std::string,std::string> hostNvs;
inline bool hostNvsAvailable=true,hostNvsWriteSucceeds=true;
class Preferences {
    std::string prefix_;bool opened_=false;
public:
    bool begin(const char* name,bool) {prefix_=std::string(name)+"/";opened_=hostNvsAvailable;return opened_;}
    void end() {opened_=false;}
    std::string getString(const char* key,const char* fallback) {
        const auto it=hostNvs.find(prefix_+key);return opened_ && it!=hostNvs.end() ? it->second : fallback;
    }
    size_t putString(const char* key,const char* value) {
        if (!opened_ || !hostNvsWriteSucceeds) return 0;
        hostNvs[prefix_+key]=value;return std::string(value).size();
    }
    bool remove(const char* key) {return opened_ && hostNvsWriteSucceeds && hostNvs.erase(prefix_+key);}
};
