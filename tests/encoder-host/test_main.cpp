#include "sensors/EncoderModule.h"
#include "core/CoreProtocol.h"
#include "core/StoppedPosition.h"
#include "core/EncoderOdometry.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#define CHECK(c) do {if (!(c)) throw std::runtime_error(#c);} while(false)
void near(double a,double b) {CHECK(std::fabs(a-b) < 0.02);}
int main() {
    try {
#if defined(AGV_PROFILE_ID)
        static_assert(Config::ENCODER_ENABLED, "Both robot profiles need encoder support");
#endif
        EncoderModule sensor; sensor.begin();
        const auto sampleAt = [&](uint32_t now) {EncoderIo::timeUs = now*1000;sensor.update(now);};
        if (!Config::ENCODER_ENABLED) {
            CHECK(EncoderIo::interrupts.empty());
            std::cout << "PASS encoder disabled on robot02\n";return 0;
        }
        CHECK(Config::ENCODER_LEFT_PIN == 16 && Config::ENCODER_RIGHT_PIN == 4);
        CHECK(EncoderIo::pinModes.at(16) == INPUT_PULLUP && EncoderIo::interrupts.at(4).mode == RISING);
        EncoderIo::fire(16,1000);EncoderIo::fire(16,1200);EncoderIo::fire(16,1300);EncoderIo::fire(4,1400);
        sampleAt(2);auto raw = sensor.snapshot();CHECK(raw.leftTicks == 1 && raw.rightTicks == 1);
        sensor.context("C4","M4",42,0,1,true,true,false);
        for (unsigned i=0;i<161;++i) {
            EncoderIo::fire(16,2000+i*1000);EncoderIo::fire(4,2400+i*1000);
        }
        sampleAt(202);auto forward = sensor.snapshot();near(forward.distanceCm,100);near(forward.speedCmS,500);
        auto translated = sensor.translationTicks();CHECK(translated.left == 161 && translated.right == 161);
        sensor.context("C4","M4",42,0,1,true,false,false); // STOP/pivot: preserve edge, no progress.
        for(unsigned i=0;i<20;++i) {EncoderIo::fire(16,203000+i*1000);EncoderIo::fire(4,203400+i*1000);}
        sampleAt(402);auto pivot = sensor.snapshot();near(pivot.distanceCm,100);near(pivot.speedCmS,0);
        translated = sensor.translationTicks();CHECK(translated.left == 161 && translated.right == 161);
        sensor.context("M4","GS2",42,1,1,true,true,false); // new RFID resets the segment exactly once.
        auto next = sensor.snapshot();near(next.distanceCm,0);
        for(unsigned i=0;i<16;++i) {EncoderIo::fire(16,403000+i*1000);EncoderIo::fire(4,403400+i*1000);}
        sampleAt(602);auto segment = sensor.snapshot();near(segment.distanceCm,1600.0/161);near(segment.speedCmS,8000.0/161);
        translated = sensor.translationTicks();CHECK(translated.left == 177 && translated.right == 177);
        sensor.context("M4","GS2",42,1,1,true,true,false);near(sensor.snapshot().distanceCm,segment.distanceCm);
        sensor.context("K5","G5",43,0,1,true,true,true); // reverse progresses K5 -> G5, not backward along the prior edge.
        for(unsigned i=0;i<8;++i) {EncoderIo::fire(16,603000+i*1000);EncoderIo::fire(4,603400+i*1000);}
        sampleAt(802);auto reverse = sensor.snapshot();CHECK(reverse.reverse);near(reverse.distanceCm,800.0/161);
        char wire[1536];CHECK(CoreProtocol::odometry(reverse,1234,"fixture-core",wire,sizeof(wire)));
        auto parsed = EncoderSample::parse(nlohmann::json::parse(wire));near(parsed.distance,reverse.distanceCm);CHECK(parsed.reverse);
        char small[32];CHECK(!CoreProtocol::odometry(reverse,1234,"fixture-core",small,sizeof(small)));
        CHECK(!CoreProtocol::odometry(reverse,0,"fixture-core",wire,sizeof(wire)));
        sensor.context("K5","G5",43,0,2,true,true,true);near(sensor.snapshot().distanceCm,0); // reconnect resets origin.
        sensor.context("K5","",0,0,2,false,false,false);CHECK(!sensor.snapshot().tracking);
        OdometryTracker wrap;wrap.begin(UINT32_MAX-2,UINT32_MAX-2,UINT32_MAX-100);
        wrap.context("C4","M4",42,0,1,true,true,false);wrap.update(2,2,100);
        near(wrap.snapshot(1).distanceCm,500.0/161);near(wrap.snapshot(1).speedCmS,500000.0/161/201);
        StoppedPosition stopped;
        auto segmentProof=reverse;segmentProof.uptimeMs=1000;
        stopped.update(segmentProof,"K5",1,true,false,false,false,UINT32_MAX);
        stopped.update(segmentProof,"K5",1,true,true,true,false,UINT32_MAX);
        auto faultSample=segmentProof;faultSample.tracking=faultSample.translating=faultSample.reverse=false;
        faultSample.routeCmdId=0;faultSample.toNode[0]=0;faultSample.distanceCm=faultSample.speedCmS=0;
        faultSample.uptimeMs=1599;stopped.attach(faultSample);CHECK(!faultSample.stopPositionValid);
        faultSample.uptimeMs=1600;stopped.attach(faultSample);CHECK(faultSample.stopPositionValid);
        CHECK(std::string(faultSample.stopFrom)=="K5" && std::string(faultSample.stopTo)=="G5");
        CHECK(CoreProtocol::odometry(faultSample,1234,"fixture-core",wire,sizeof(wire)));
        const auto stoppedWire=EncoderSample::parse(nlohmann::json::parse(wire));CHECK(stoppedWire.stoppedPosition);
        CHECK(stoppedWire.stopFrom=="K5" && stoppedWire.stopTo=="G5");
        faultSample.stopPositionValid=false;++faultSample.leftTicks;
        stopped.update(faultSample,"K5",1,true,true,true,false,UINT32_MAX);
        stopped.attach(faultSample);CHECK(!faultSample.stopPositionValid); // one wheel/coasting resets stationarity.
        faultSample.uptimeMs=2200;stopped.update(faultSample,"K5",1,true,true,true,false,0);
        stopped.attach(faultSample);CHECK(faultSample.stopPositionValid && !faultSample.stopTo[0]);
        faultSample.stopPositionValid=false;stopped.update(faultSample,"K5",2,false,true,true,false,0);
        stopped.attach(faultSample);CHECK(!faultSample.stopPositionValid); // no proof after session loss.
        StoppedPosition pivotFault;
        pivotFault.update(segmentProof,"K5",1,true,false,false,true,UINT32_MAX);
        auto afterPivot=segmentProof;afterPivot.uptimeMs=3000;
        pivotFault.update(afterPivot,"K5",1,true,true,true,false,UINT32_MAX);
        afterPivot.uptimeMs=3700;pivotFault.attach(afterPivot);CHECK(!afterPivot.stopPositionValid);
        std::cout << "PASS encoder profile: GPIO16/4, 300us noise filter, 161 pulses/100cm, translation-only lifetime ticks, speed, pivot/STOP, RFID rebase, reverse, session/count/time wrap, wire codec\n";
    } catch (const std::exception& e) {std::cerr << e.what() << '\n';return 1;}
}
