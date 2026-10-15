#include "actuators/ServoModule.h"
#include "core/CoreProtocol.h"
#include "core/LiftCycle.h"
#include <ArduinoJson.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#define CHECK(c) do{if(!(c))throw std::runtime_error(#c);}while(false)
static bool decode(const char* wire,CoreCommand& c){return CoreProtocol::decodeCommand(reinterpret_cast<const uint8_t*>(wire),std::strlen(wire),c);}

static int selfTest(){
    ServoModule servo;servo.begin();CHECK(servo.liftConfigured()==Config::SERVO_SINGLE_TEST);
    CoreCommand up,down,bad;
    const char* upWire=R"({"cmd":"LIFT_UP","cmd_id":100,"node":"K5"})";
    CHECK(decode(upWire,up)==Config::SERVO_SINGLE_TEST);
    if(!Config::SERVO_SINGLE_TEST){CHECK(ServoIo::pins.empty());CHECK(!servo.liftUp());std::cout<<"PASS profile02 servo disabled\n";return 0;}
    CHECK(decode(R"({"cmd":"LIFT_DOWN","cmd_id":101,"node":"K5"})",down));
    for(const auto* wire:{R"({"cmd":"LIFT_UP","cmd_id":102,"node":"S1"})",R"({"cmd":"LIFT_UP","cmd_id":102})",R"({"cmd":"LIFT_UP","cmd_id":102,"node":"K5\u0000X"})"})CHECK(!decode(wire,bad));
    bad=up;CHECK(CoreProtocol::sameCommand(up,bad));bad.type=CoreCommandType::LiftDown;CHECK(!CoreProtocol::sameCommand(up,bad));
    CHECK(ServoIo::pins.size()==1&&ServoIo::pins.at(17)==5);CHECK(ServoIo::pwm.at(5)==0);
    CHECK(ServoIo::hz.at(5)==50&&ServoIo::bits.at(5)==16);CHECK(servo.getAngle(1)==-1);
    LiftCycle lift;ServoIo::timeMs=100;CHECK(servo.liftUp());CHECK(lift.start(up,millis()));
    CHECK(servo.getAngle(1)==45&&ServoIo::pwm.at(5)==4095);CHECK(!servo.liftDown());CHECK(!lift.start(down,millis()));
    ServoIo::timeMs=5099;servo.update();CHECK(servo.busy());CHECK(!lift.update(millis(),true));CHECK(!lift.cargoKnown());
    ServoIo::timeMs=5100;servo.update();CHECK(!servo.busy());CHECK(lift.update(millis(),true));
    CHECK(lift.cargoKnown()&&lift.cargo()&&!lift.busy()&&std::strcmp(lift.state(),"DONE_UP")==0);
    CHECK(!lift.update(20000,true));CHECK(servo.liftDown());CHECK(lift.start(down,millis()));
    CHECK(servo.getAngle(1)==0&&ServoIo::pwm.at(5)==3276);CHECK(lift.cargo());
    ServoIo::timeMs+=5000;servo.update();CHECK(lift.update(millis(),true)&&!lift.cargo()&&lift.cargoKnown());
    CHECK(servo.liftUp());CHECK(lift.start(up,millis()));CHECK(lift.update(++ServoIo::timeMs,false));
    CHECK(!lift.busy()&&!lift.cargoKnown()&&std::strcmp(lift.state(),"INTERRUPTED")==0);
    CHECK(!lift.update(ServoIo::timeMs+5000,true)); // No automatic completion/replay after STOP.
    ServoIo::timeMs=UINT32_MAX-1000;servo.update();CHECK(servo.liftDown());CHECK(lift.start(down,millis()));
    ServoIo::timeMs+=5000;servo.update();CHECK(lift.update(millis(),true)&&!lift.cargo());
    std::cout<<"PASS GPIO17 one-channel 50Hz PWM/down0/up45, parser/dedup identity, 5s, interruption, wrap, no boot motion\n";return 0;
}

// Line-oriented adapter for isolated Core/MQTT/gateway/browser integration.
// Runs real ServoModule, LiftCycle and CoreProtocol; GPIO/time are host stubs.
static int receiver(){
    ServoModule servo;servo.begin();LiftCycle lift;std::vector<CoreCommand> seen;
    std::string line;
    while(std::getline(std::cin,line)){
        StaticJsonDocument<4096> request;CHECK(!deserializeJson(request,line));
        ServoIo::timeMs=request["now"].as<uint32_t>();servo.update();
        bool accepted=true;
        if(request.containsKey("command")){
            const std::string wire=request["command"].as<std::string>();CoreCommand command;
            accepted=decode(wire.c_str(),command);bool duplicate=false;
            if(accepted){
                for(const auto& prior:seen)if(prior.cmdId==command.cmdId){duplicate=true;accepted=CoreProtocol::sameCommand(prior,command);break;}
                if(!duplicate){
                    if(command.type==CoreCommandType::Stop||command.type==CoreCommandType::Wait)lift.interrupt();
                    else if(command.type==CoreCommandType::LiftUp||command.type==CoreCommandType::LiftDown){
                        accepted=!lift.busy()&&(command.type==CoreCommandType::LiftUp?servo.liftUp():servo.liftDown());
                        if(accepted)accepted=lift.start(command,millis());
                    }
                    if(accepted)seen.push_back(command);
                }
            }
        }
        lift.update(millis(),request["safe"]|true);
        StaticJsonDocument<512> output;output["accepted"]=accepted;output["liftBusy"]=lift.busy();
        output["liftCommandId"]=lift.cmdId();output["liftState"]=lift.state();output["liftElapsedMs"]=lift.elapsed(millis());
        output["angle"]=servo.getAngle(1);output["pwm"]=ServoIo::pwm[5];output["writes"]=ServoIo::writes[5];
        if(lift.cargoKnown())output["virtualCargo"]=lift.cargo();else output["virtualCargo"]=nullptr;
        serializeJson(output,std::cout);std::cout<<'\n'<<std::flush;
    }
    return 0;
}
int main(int argc,char** argv){try{return argc>1&&std::string(argv[1])=="--receiver"?receiver():selfTest();}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
