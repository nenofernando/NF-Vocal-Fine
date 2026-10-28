#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

struct ManualRegion { double start=0,end=0; int semitones=0; };
struct ManualSnapshot {
    std::array<ManualRegion,128> regions{};int count=0;
    bool valueAt(double time,float& shift)const {
        for(int i=0;i<count;++i){const auto& r=regions[size_t(i)];if(time>=r.start&&time<r.end){
            double fade=std::min(0.010,(r.end-r.start)*0.2);
            double blend=juce::jlimit(0.0,1.0,std::min((time-r.start)/fade,(r.end-time)/fade));
            shift=float(r.semitones);weight=float(blend*blend*(3-2*blend));return true;
        }}weight=0;return false;
    }
    // Audio-thread-local scratch, never shared with the UI.
    mutable float weight=0;
};
struct CapturedPitch {double time=0;float midi=-1;unsigned generation=0;};
class ManualDocument {
    juce::SpinLock lock;
    std::vector<ManualRegion> edits;
    std::vector<CapturedPitch> points;
    std::vector<std::vector<ManualRegion>> undoHistory;
    std::atomic<unsigned> captureGeneration{0};
    juce::AbstractFifo fifo{4096};std::array<CapturedPitch,4096> incoming{};
    void changed(){revision.fetch_add(1);if(onEdit)onEdit();}
public:
    std::atomic<bool> capturing{false},timelineAvailable{false},playing{false};
    std::atomic<double> cursor{0};std::atomic<unsigned> revision{0};
    std::function<void()> onEdit;
    void push(CapturedPitch frame){frame.generation=captureGeneration.load();if(!capturing.load())return;int a,n,b,m;fifo.prepareToWrite(1,a,n,b,m);if(n){incoming[size_t(a)]=frame;fifo.finishedWrite(1);}}
    void drain(){
        const juce::SpinLock::ScopedLockType guard(lock);
        int a,n,b,m;fifo.prepareToRead(fifo.getNumReady(),a,n,b,m);if(n+m==0)return;
        auto put=[&](CapturedPitch frame){if(frame.generation!=captureGeneration.load())return;auto it=std::lower_bound(points.begin(),points.end(),frame.time,[](const auto& p,double t){return p.time<t;});
            if(it!=points.end()&&std::abs(it->time-frame.time)<0.018)*it=frame;
            else if(it!=points.begin()&&std::abs((it-1)->time-frame.time)<0.018)*(it-1)=frame;
            else if(points.size()<36000)points.insert(it,frame);
        };
        for(int i=0;i<n;++i)put(incoming[size_t(a+i)]);for(int i=0;i<m;++i)put(incoming[size_t(b+i)]);fifo.finishedRead(n+m);revision.fetch_add(1);
    }
    std::vector<CapturedPitch> getPoints(){const juce::SpinLock::ScopedLockType g(lock);return points;}
    std::vector<ManualRegion> getEdits(){const juce::SpinLock::ScopedLockType g(lock);return edits;}
    // Never waits for the UI and never allocates on the audio thread.
    void readForAudio(ManualSnapshot& cache){if(!lock.tryEnter())return;cache.count=int(edits.size());for(int i=0;i<cache.count;++i)cache.regions[size_t(i)]=edits[size_t(i)];lock.exit();}
    bool setRange(double start,double end,int semitones,bool remove=false){
        if(!std::isfinite(start)||!std::isfinite(end)||end-start<0.025)return false;
        {const juce::SpinLock::ScopedLockType guard(lock);std::vector<ManualRegion> next;
            for(const auto& r:edits){if(r.end<=start||r.start>=end)next.push_back(r);else{if(r.start<start)next.push_back({r.start,start,r.semitones});if(r.end>end)next.push_back({end,r.end,r.semitones});}}
            if(!remove)next.push_back({start,end,juce::jlimit(-12,12,semitones)});
            std::sort(next.begin(),next.end(),[](const auto& a,const auto& b){return a.start<b.start;});
            if(next.size()>128)return false;
            if(undoHistory.size()==64)undoHistory.erase(undoHistory.begin());undoHistory.push_back(edits);edits=std::move(next);
        }changed();return true;
    }
    int offsetAt(double t){const juce::SpinLock::ScopedLockType g(lock);for(const auto& r:edits)if(t>=r.start&&t<r.end)return r.semitones;return 0;}
    bool undo(){ {const juce::SpinLock::ScopedLockType g(lock);if(undoHistory.empty())return false;edits=std::move(undoHistory.back());undoHistory.pop_back();}changed();return true;}
    void clearMap(){capturing=false;captureGeneration.fetch_add(1);drain();{const juce::SpinLock::ScopedLockType g(lock);points.clear();}revision.fetch_add(1);}
    juce::ValueTree save(){drain();const juce::SpinLock::ScopedLockType g(lock);juce::ValueTree tree("ManualV1");
        for(const auto& r:edits){juce::ValueTree item("Edit");item.setProperty("start",r.start,nullptr);item.setProperty("end",r.end,nullptr);item.setProperty("shift",r.semitones,nullptr);tree.addChild(item,-1,nullptr);}
        for(const auto& p:points){juce::ValueTree item("Pitch");item.setProperty("t",p.time,nullptr);item.setProperty("m",p.midi,nullptr);tree.addChild(item,-1,nullptr);}return tree;
    }
    void restore(const juce::ValueTree& tree){capturing=false;captureGeneration.fetch_add(1);const juce::SpinLock::ScopedLockType g(lock);edits.clear();points.clear();undoHistory.clear();
        for(const auto& c:tree){if(c.hasType("Edit")&&edits.size()<128){double a=c["start"],b=c["end"];int s=c["shift"];if(std::isfinite(a)&&std::isfinite(b)&&b-a>=0.025&&a>=-3600&&b<=864000&&s>=-12&&s<=12)edits.push_back({a,b,s});}
            else if(c.hasType("Pitch")&&points.size()<36000){double t=c["t"];float m=c["m"];if(std::isfinite(t)&&std::isfinite(m)&&t>=-3600&&t<=864000&&m>=-1&&m<=127)points.push_back({t,m});}}
        std::sort(edits.begin(),edits.end(),[](const auto&a,const auto&b){return a.start<b.start;});std::sort(points.begin(),points.end(),[](const auto&a,const auto&b){return a.time<b.time;});revision.fetch_add(1);
    }
};
