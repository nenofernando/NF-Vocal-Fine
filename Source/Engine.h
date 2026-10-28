#pragma once
#include "ManualModel.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include "../ThirdParty/signalsmith-stretch/signalsmith-stretch.h"

struct VoiceDetector {
    std::array<float,1024> ring{};
    std::array<double,512> difference{};
    int position=0, count=0, decimation=4, phase=0;
    double rate=48000, filtered=0, pole=0;
    float midi=-1, confidence=0;
    void prepare(double sr) {rate=sr; decimation=std::max(1,int(sr/12000)); pole=std::exp(-2*juce::MathConstants<double>::pi*2200/sr); reset();}
    void reset(){ring.fill(0);position=count=phase=0;filtered=0;midi=-1;confidence=0;}
    void push(float x){filtered=pole*filtered+(1-pole)*x;if(++phase<decimation)return;phase=0;ring[position]=float(filtered);position=(position+1)%1024;count=std::min(count+1,1024);}
    void analyse(){
        midi=-1;confidence=0;if(count<1024)return;
        std::array<float,1024> x{};double power=0;
        for(int i=0;i<1024;++i){x[i]=ring[(position+i)%1024];power+=x[i]*x[i];}
        if(power/1024<0.000004)return;
        double analysisRate=rate/decimation;
        int lo=std::max(2,int(analysisRate/1100)), hi=std::min(510,int(analysisRate/65));
        double sum=0;difference[0]=1;
        for(int lag=1;lag<=hi+1;++lag){double d=0;for(int i=0;i<512;++i){double v=x[i]-x[i+lag];d+=v*v;}sum+=d;difference[lag]=sum>1e-20?d*lag/sum:1;}
        int best=0;
        for(int lag=lo;lag<hi;++lag)if(difference[lag]<0.15){while(lag+1<=hi&&difference[lag+1]<difference[lag])++lag;best=lag;break;}
        if(best==0)return;
        confidence=float(1-difference[best]);
        double a=difference[best-1],b=difference[best],c=difference[best+1],denom=a-2*b+c;
        double fine=best+(std::abs(denom)>1e-12?juce::jlimit(-0.5,0.5,0.5*(a-c)/denom):0);
        midi=float(69+12*std::log2(analysisRate/fine/440.0));
    }
};

struct TuneSettings {int key=0,scale=0;unsigned mask=4095;float amount=0.8f,speed=110,human=0.7f,formant=0,mix=1;};
struct NaturalEngine {
    signalsmith::stretch::SignalsmithStretch<float> stretch{42};
    VoiceDetector detector, meterOriginal, meterOutput;
    std::atomic<bool> meteringEnabled{false};
    std::atomic<float> originalMeasured{-1}, outputMeasured{-1};
    bool wasMetering=false;
    int meterHop=0;
    juce::AudioBuffer<float> scratch,dry;
    double rate=48000;
    int channels=2,latency=0,dryPosition=0,hop=0;
    float shift=0,centre=-1,target=-1;
    std::atomic<float> note{-1}, correction{0}, certainty{0};
    void prepare(double sr,int ch){rate=sr;channels=ch;detector.prepare(sr);meterOriginal.prepare(sr);meterOutput.prepare(sr);stretch.presetDefault(ch,float(sr));latency=stretch.inputLatency()+stretch.outputLatency();scratch.setSize(ch,128);dry.setSize(ch,latency+1);reset();}
    void reset(){stretch.reset();detector.reset();meterOriginal.reset();meterOutput.reset();originalMeasured=outputMeasured=-1;meterHop=0;wasMetering=false;dry.clear();dryPosition=hop=0;shift=0;centre=target=-1;note=-1;correction=certainty=0;}
    static bool allowed(int pc,const TuneSettings& s){pc=(pc%12+12)%12;if(!(s.mask&(1u<<pc)))return false;int rel=(pc-s.key+12)%12;return s.scale==0||(s.scale==1?((0xab5>>rel)&1):((0x5ad>>rel)&1));}
    static float nearest(float m,const TuneSettings& s){float best=m,dist=100;int n=int(std::round(m));for(int i=n-12;i<=n+12;++i)if(allowed(i,s)&&std::abs(i-m)<dist){best=float(i);dist=std::abs(i-m);}return best;}
    void process(juce::AudioBuffer<float>& audio,const TuneSettings& s,const ManualSnapshot* manual=nullptr,double blockStart=0){
        juce::ScopedNoDenormals guard;
        const bool measuring=meteringEnabled.load(std::memory_order_relaxed);
        if(measuring!=wasMetering){meterOriginal.reset();meterOutput.reset();originalMeasured=outputMeasured=-1;meterHop=0;wasMetering=measuring;}
        for(int offset=0;offset<audio.getNumSamples();){
            int n=std::min({128,audio.getNumSamples()-offset,256-hop});
            for(int i=0;i<n;++i){float mono=0;for(int c=0;c<channels;++c){float v=audio.getSample(c,offset+i);mono+=std::isfinite(v)?v:0;}detector.push(mono/channels);}
            hop+=n;
            if(hop==256){hop=0;detector.analyse();float m=detector.midi;note=m;certainty=detector.confidence;
                bool active=false;for(int pc=0;pc<12;++pc)active|=allowed(pc,s);
                if(m>=0&&active){if(centre<0||std::abs(m-centre)>1.2f)centre=m;else centre+=float(1-std::exp(-256/(rate*0.16)))*(m-centre);
                    float candidate=nearest(centre,s);
                    if(target<0||!allowed(int(target),s)||std::abs(centre-candidate)+0.12f<std::abs(centre-target))target=candidate;
                }else{centre=target=-1;}
            }
            float wanted=0;
            if(detector.midi>=0&&target>=0){float reference=detector.midi*(1-s.human)+centre*s.human;float error=target-reference;
                float dead=0.035f*s.human;wanted=(std::abs(error)>dead?error-std::copysign(dead,error):0)*s.amount;wanted=juce::jlimit(-3.0f,3.0f,wanted);}
            shift+=float(1-std::exp(-n/(rate*std::max(0.005f,s.speed*0.001f))))*(wanted-shift);correction=shift;
            float appliedShift=shift;
            if(manual){float manualShift=0;double sourceTime=blockStart+(offset+n*0.5-stretch.inputLatency())/rate;
                if(manual->valueAt(sourceTime,manualShift))appliedShift+=(manualShift-appliedShift)*manual->weight;}
            correction=appliedShift;
            stretch.setTransposeSemitones(appliedShift);stretch.setFormantSemitones(s.formant,true);
            std::array<const float*,2> in{};std::array<float*,2> out{};
            for(int c=0;c<channels;++c){in[c]=audio.getReadPointer(c,offset);out[c]=scratch.getWritePointer(c);}
            stretch.process(in.data(),n,out.data(),n);
            for(int i=0;i<n;++i){
                float originalMono=0,outputMono=0;
                for(int c=0;c<channels;++c){
                    float original=audio.getSample(c,offset+i),delayed=dry.getSample(c,dryPosition);
                    dry.setSample(c,dryPosition,original);
                    float wet=scratch.getSample(c,i),v=delayed*(1-s.mix)+wet*s.mix;
                    if(!std::isfinite(v))v=0;
                    audio.setSample(c,offset+i,v);
                    if(measuring){originalMono+=delayed;outputMono+=v;}
                }
                if(measuring){
                    // Both detectors see the same audio time: the original is delayed by the DSP latency.
                    meterOriginal.push(originalMono/channels);meterOutput.push(outputMono/channels);
                    if(++meterHop==256){meterHop=0;meterOriginal.analyse();meterOutput.analyse();originalMeasured=meterOriginal.midi;outputMeasured=meterOutput.midi;}
                }
                dryPosition=(dryPosition+1)%latency;
            }
            offset+=n;
        }
    }
};
