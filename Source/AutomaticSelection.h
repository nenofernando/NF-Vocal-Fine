#pragma once
#include "ManualModel.h"
namespace automaticSelection {
struct Scale {int key=0;bool minor=false,valid=false,uncertain=true;double coverage=0,margin=0,voiced=0;};
inline bool allowed(int note,const Scale& scale){int degree=((note-scale.key)%12+12)%12;return ((scale.minor?0x5ad:0xab5)>>degree)&1;}
inline Scale estimate(const std::vector<CapturedPitch>& map){
    std::array<double,12> histogram{};double total=0;int distinct=0;
    for(size_t i=0;i<map.size();++i){if(map[i].midi<0||!std::isfinite(map[i].midi)||!std::isfinite(map[i].time))continue;double dt=i+1<map.size()?juce::jlimit(0.005,0.1,map[i+1].time-map[i].time):1.0/30;
        int note=juce::roundToInt(map[i].midi),pc=(note%12+12)%12;double weight=dt*(1.0-0.5*std::min(0.5,std::abs(double(map[i].midi-note))));histogram[size_t(pc)]+=weight;total+=dt;}
    double sum=0;for(double v:histogram)sum+=v;for(double v:histogram)if(v>sum*0.025)++distinct;
    Scale best;best.voiced=total;if(total<0.6||distinct<3||sum<=0)return best;
    double top=-1,second=-1;
    for(int root=0;root<12;++root)for(int minor=0;minor<2;++minor){Scale candidate;candidate.key=root;candidate.minor=minor!=0;double coverage=0;
        for(int pc=0;pc<12;++pc)if(allowed(pc,candidate))coverage+=histogram[size_t(pc)];coverage/=sum;
        double triad=(0.5*histogram[size_t(root)]+0.3*histogram[size_t((root+(minor?3:4))%12)]+0.2*histogram[size_t((root+7)%12)])/sum;
        double score=coverage+0.45*triad;
        if(score>top){second=top;top=score;best=candidate;best.coverage=coverage;}else second=std::max(second,score);
    }
    best.valid=best.coverage>=0.72;best.voiced=total;best.margin=top-second;best.uncertain=best.margin<0.035||distinct<5||best.coverage<0.85;return best;
}
inline float nearest(float midi,const Scale& scale,float previous){int centre=juce::roundToInt(midi);float best=midi,distance=100;for(int n=centre-12;n<=centre+12;++n)if(allowed(n,scale)&&std::abs(n-midi)<distance){best=float(n);distance=std::abs(n-midi);}if(previous>=0&&allowed(juce::roundToInt(previous),scale)&&std::abs(previous-midi)<=distance+0.12f)return previous;return best;}
inline std::vector<CurvePoint> simplify(const std::vector<CurvePoint>& input){
    if(input.size()<3)return input;std::vector<bool> keep(input.size(),false);keep.front()=keep.back()=true;std::vector<std::pair<size_t,size_t>> pending{{0,input.size()-1}};
    while(!pending.empty()){auto range=pending.back();pending.pop_back();size_t left=range.first,right=range.second,index=0;float error=0;
        for(size_t i=left+1;i<right;++i){float expected=input[left].midi+float((input[i].time-input[left].time)/(input[right].time-input[left].time))*(input[right].midi-input[left].midi);float difference=std::abs(input[i].midi-expected);if(difference>error){error=difference;index=i;}}
        if(error>0.025f){keep[index]=true;pending.push_back({left,index});pending.push_back({index,right});}
    }std::vector<CurvePoint> result;for(size_t i=0;i<input.size();++i)if(keep[i])result.push_back(input[i]);return result;
}
inline std::vector<PitchCurve> build(const std::vector<CapturedPitch>& map,double start,double end,const Scale& scale,float amount,float human,float speed){
    std::vector<PitchCurve> result;std::vector<CurvePoint> group;float previous=-1,correction=0;double last=-100;
    auto flush=[&]{if(group.size()>1)result.push_back({0,simplify(group)});group.clear();previous=-1;correction=0;};
    for(size_t i=0;i<map.size();++i){const auto& p=map[i];if(p.time<start||p.time>end||p.midi<0||!std::isfinite(p.midi)||!std::isfinite(p.time))continue;if(p.time-last>0.35)flush();
        std::array<float,5> local{};int count=0;for(int offset=-2;offset<=2;++offset){long k=long(i)+offset;if(k>=0&&k<long(map.size())&&map[size_t(k)].midi>=0&&std::abs(map[size_t(k)].time-p.time)<0.12)local[size_t(count++)]=map[size_t(k)].midi;}
        std::sort(local.begin(),local.begin()+count);float centre=count?local[size_t(count/2)]:p.midi;
        float target=nearest(centre,scale,previous);previous=target;float vibrato=juce::jlimit(-0.35f,0.35f,p.midi-centre)*human;
        float wanted=(target+vibrato-p.midi)*amount;double dt=last<0?1.0/30:juce::jlimit(0.005,0.1,p.time-last);float alpha=float(1-std::exp(-dt/std::max(0.005f,speed*0.001f)));correction+=alpha*(wanted-correction);
        group.push_back({p.time,juce::jlimit(0.0f,127.0f,p.midi+correction)});last=p.time;
    }flush();return result;
}
inline juce::String name(const Scale& scale){static const char* names[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};return juce::String(names[scale.key])+(scale.minor?" menor":" maior");}
}
