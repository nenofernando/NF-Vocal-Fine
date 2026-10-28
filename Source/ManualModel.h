#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

struct CurvePoint {double time=0;float midi=60;};
struct PitchCurve {int id=0;std::vector<CurvePoint> nodes;};
struct ManualRegion { double start=0,end=0; int semitones=0; };
struct ManualSnapshot {
    std::array<ManualRegion,128> regions{};int count=0;
    struct CurveIndex {int first=0,count=0;};
    std::array<CurvePoint,4096> nodes{};std::array<CurveIndex,64> curves{};int nodeCount=0,curveCount=0;
    bool valueAt(double time,float& shift,float originalMidi=-1)const {
        if(originalMidi>=0)for(int c=curveCount-1;c>=0;--c){auto curve=curves[size_t(c)];if(curve.count<2)continue;
            const auto* begin=nodes.data()+curve.first;const auto* end=begin+curve.count;
            if(time<begin->time||time>=(end-1)->time)continue;
            const auto* right=std::upper_bound(begin,end,time,[](double t,const CurvePoint& p){return t<p.time;});const auto* left=right-1;
            float target=left->midi+float((time-left->time)/(right->time-left->time))*(right->midi-left->midi);
            double fade=std::min(0.010,((end-1)->time-begin->time)*0.2),blend=juce::jlimit(0.0,1.0,std::min((time-begin->time)/fade,((end-1)->time-time)/fade));
            shift=juce::jlimit(-12.0f,12.0f,target-originalMidi);weight=float(blend*blend*(3-2*blend));return true;
        }
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
    std::vector<PitchCurve> curves;
    struct History {std::vector<ManualRegion> edits;std::vector<PitchCurve> curves;int autoKey=-1,autoMinor=0,autoUncertain=1;};
    std::vector<History> undoHistory;
    int nextCurveId=1;
    void remember(){if(undoHistory.size()==64)undoHistory.erase(undoHistory.begin());undoHistory.push_back({edits,curves,lastAutoKey.load(),lastAutoMinor.load(),lastAutoUncertain.load()});}
    static float curveAt(const PitchCurve& c,double t){auto it=std::lower_bound(c.nodes.begin(),c.nodes.end(),t,[](const CurvePoint& p,double time){return p.time<time;});if(it==c.nodes.begin())return it->midi;if(it==c.nodes.end())return c.nodes.back().midi;const auto& a=*(it-1);return a.midi+float((t-a.time)/(it->time-a.time))*(it->midi-a.midi);}
    std::vector<PitchCurve> withoutRange(double a,double b){std::vector<PitchCurve> out;
        for(const auto& c:curves){if(c.nodes.empty())continue;if(c.nodes.back().time<a||c.nodes.front().time>b){out.push_back(c);continue;}
            if(c.nodes.front().time<a){PitchCurve left{c.id,{}};for(const auto& p:c.nodes)if(p.time<a)left.nodes.push_back(p);left.nodes.push_back({a,curveAt(c,a)});if(left.nodes.size()>1)out.push_back(std::move(left));}
            if(c.nodes.back().time>b){PitchCurve right{nextCurveId++,{{b,curveAt(c,b)}}};for(const auto& p:c.nodes)if(p.time>b)right.nodes.push_back(p);if(right.nodes.size()>1)out.push_back(std::move(right));}
        }return out;
    }
    std::atomic<unsigned> captureGeneration{0};
    juce::AbstractFifo fifo{4096};std::array<CapturedPitch,4096> incoming{};
    void changed(){revision.fetch_add(1);if(onEdit)onEdit();}
public:
    std::atomic<int> lastAutoKey{-1},lastAutoMinor{0},lastAutoUncertain{1};
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
    std::vector<PitchCurve> getCurves(){const juce::SpinLock::ScopedLockType g(lock);return curves;}
    int editCount(){const juce::SpinLock::ScopedLockType g(lock);return int(edits.size()+curves.size());}
    int setCurve(int id,std::vector<CurvePoint> curve){
        if(curve.empty()||curve.size()>4096)return 0;
        std::sort(curve.begin(),curve.end(),[](const auto& a,const auto& b){return a.time<b.time;});
        for(size_t i=0;i<curve.size();++i)if(!std::isfinite(curve[i].time)||!std::isfinite(curve[i].midi)||curve[i].time< -3600||curve[i].time>864000||curve[i].midi<0||curve[i].midi>127||(i&&curve[i].time-curve[i-1].time<0.0005))return 0;
        {const juce::SpinLock::ScopedLockType g(lock);auto it=std::find_if(curves.begin(),curves.end(),[&](const auto& c){return c.id==id;});if(it!=curves.end()&&it->nodes.size()==curve.size()&&std::equal(curve.begin(),curve.end(),it->nodes.begin(),[](const auto& a,const auto& b){return a.time==b.time&&a.midi==b.midi;}))return id;size_t total=curve.size();for(const auto& c:curves)if(c.id!=id)total+=c.nodes.size();if(total>4096||(it==curves.end()&&curves.size()==64))return 0;
            remember();if(it==curves.end()){id=nextCurveId++;curves.push_back({id,std::move(curve)});}else it->nodes=std::move(curve);
        }changed();return id;
    }
    bool replaceSelection(double start,double end,std::vector<PitchCurve> generated,int key,int minor,int uncertain){
        if(!std::isfinite(start)||!std::isfinite(end)||end-start<0.025||generated.empty()||key<0||key>11)return false;
        for(const auto& c:generated){if(c.nodes.size()<2)return false;for(size_t i=0;i<c.nodes.size();++i){const auto& p=c.nodes[i];if(!std::isfinite(p.time)||!std::isfinite(p.midi)||p.time<start||p.time>end||p.midi<0||p.midi>127||(i&&p.time-c.nodes[i-1].time<0.0005))return false;}}
        {const juce::SpinLock::ScopedLockType g(lock);auto remaining=withoutRange(start,end);for(auto& c:generated){c.id=nextCurveId++;remaining.push_back(std::move(c));}size_t total=0;for(const auto& c:remaining)total+=c.nodes.size();if(remaining.size()>64||total>4096)return false;
            std::vector<ManualRegion> ranges;for(const auto& r:edits){if(r.end<=start||r.start>=end)ranges.push_back(r);else{if(r.start<start)ranges.push_back({r.start,start,r.semitones});if(r.end>end)ranges.push_back({end,r.end,r.semitones});}}if(ranges.size()>128)return false;
            remember();edits=std::move(ranges);curves=std::move(remaining);lastAutoKey=key;lastAutoMinor=minor;lastAutoUncertain=uncertain;
        }changed();return true;
    }
    bool eraseCurve(int id){ {const juce::SpinLock::ScopedLockType g(lock);auto it=std::find_if(curves.begin(),curves.end(),[&](const auto& c){return c.id==id;});if(it==curves.end())return false;remember();curves.erase(it);}changed();return true;}
    // Never waits for the UI and never allocates on the audio thread.
    void readForAudio(ManualSnapshot& cache){if(!lock.tryEnter())return;cache.count=int(edits.size());for(int i=0;i<cache.count;++i)cache.regions[size_t(i)]=edits[size_t(i)];cache.curveCount=int(curves.size());cache.nodeCount=0;for(size_t c=0;c<curves.size();++c){cache.curves[c]={cache.nodeCount,int(curves[c].nodes.size())};for(const auto& p:curves[c].nodes)cache.nodes[size_t(cache.nodeCount++)]=p;}lock.exit();}
    bool setRange(double start,double end,int semitones,bool remove=false){
        if(!std::isfinite(start)||!std::isfinite(end)||end-start<0.025)return false;
        {const juce::SpinLock::ScopedLockType guard(lock);std::vector<ManualRegion> next;
            for(const auto& r:edits){if(r.end<=start||r.start>=end)next.push_back(r);else{if(r.start<start)next.push_back({r.start,start,r.semitones});if(r.end>end)next.push_back({end,r.end,r.semitones});}}
            if(!remove)next.push_back({start,end,juce::jlimit(-12,12,semitones)});
            std::sort(next.begin(),next.end(),[](const auto& a,const auto& b){return a.start<b.start;});
            if(next.size()>128)return false;
            auto remaining=withoutRange(start,end);size_t total=0;for(const auto& c:remaining)total+=c.nodes.size();if(remaining.size()>64||total>4096)return false;
            remember();edits=std::move(next);curves=std::move(remaining);
        }changed();return true;
    }
    int offsetAt(double t){const juce::SpinLock::ScopedLockType g(lock);for(const auto& r:edits)if(t>=r.start&&t<r.end)return r.semitones;return 0;}
    bool undo(){ {const juce::SpinLock::ScopedLockType g(lock);if(undoHistory.empty())return false;edits=std::move(undoHistory.back().edits);curves=std::move(undoHistory.back().curves);lastAutoKey=undoHistory.back().autoKey;lastAutoMinor=undoHistory.back().autoMinor;lastAutoUncertain=undoHistory.back().autoUncertain;undoHistory.pop_back();}changed();return true;}
    void clearMap(){capturing=false;captureGeneration.fetch_add(1);drain();{const juce::SpinLock::ScopedLockType g(lock);points.clear();}revision.fetch_add(1);}
    juce::ValueTree save(){drain();const juce::SpinLock::ScopedLockType g(lock);juce::ValueTree tree("ManualV1");tree.setProperty("autoKey",lastAutoKey.load(),nullptr);tree.setProperty("autoMinor",lastAutoMinor.load(),nullptr);tree.setProperty("autoUncertain",lastAutoUncertain.load(),nullptr);
        for(const auto& r:edits){juce::ValueTree item("Edit");item.setProperty("start",r.start,nullptr);item.setProperty("end",r.end,nullptr);item.setProperty("shift",r.semitones,nullptr);tree.addChild(item,-1,nullptr);}
        for(const auto& c:curves){juce::ValueTree curve("Curve");curve.setProperty("id",c.id,nullptr);for(const auto& n:c.nodes){juce::ValueTree node("Node");node.setProperty("t",n.time,nullptr);node.setProperty("m",n.midi,nullptr);curve.addChild(node,-1,nullptr);}tree.addChild(curve,-1,nullptr);}
        for(const auto& p:points){juce::ValueTree item("Pitch");item.setProperty("t",p.time,nullptr);item.setProperty("m",p.midi,nullptr);tree.addChild(item,-1,nullptr);}return tree;
    }
    void restore(const juce::ValueTree& tree){capturing=false;captureGeneration.fetch_add(1);const juce::SpinLock::ScopedLockType g(lock);edits.clear();points.clear();curves.clear();nextCurveId=1;undoHistory.clear();lastAutoKey=juce::jlimit(-1,11,int(tree.getProperty("autoKey",-1)));lastAutoMinor=int(tree.getProperty("autoMinor",0))!=0;lastAutoUncertain=int(tree.getProperty("autoUncertain",1))!=0;
        for(const auto& c:tree){if(c.hasType("Edit")&&edits.size()<128){double a=c["start"],b=c["end"];int s=c["shift"];if(std::isfinite(a)&&std::isfinite(b)&&b-a>=0.025&&a>=-3600&&b<=864000&&s>=-12&&s<=12)edits.push_back({a,b,s});}
            else if(c.hasType("Pitch")&&points.size()<36000){double t=c["t"];float m=c["m"];if(std::isfinite(t)&&std::isfinite(m)&&t>=-3600&&t<=864000&&m>=-1&&m<=127)points.push_back({t,m});}}
        size_t totalNodes=0;for(const auto& c:tree)if(c.hasType("Curve")&&curves.size()<64){PitchCurve curve{nextCurveId++,{}};
            for(const auto& n:c)if(n.hasType("Node")&&totalNodes+curve.nodes.size()<4096){double t=n["t"];float m=n["m"];if(std::isfinite(t)&&std::isfinite(m)&&t>=-3600&&t<=864000&&m>=0&&m<=127)curve.nodes.push_back({t,m});}
            std::sort(curve.nodes.begin(),curve.nodes.end(),[](const auto&a,const auto&b){return a.time<b.time;});curve.nodes.erase(std::unique(curve.nodes.begin(),curve.nodes.end(),[](const auto&a,const auto&b){return b.time-a.time<0.0005;}),curve.nodes.end());
            if(!curve.nodes.empty()){totalNodes+=curve.nodes.size();curves.push_back(std::move(curve));}}
        std::sort(edits.begin(),edits.end(),[](const auto&a,const auto&b){return a.start<b.start;});std::sort(points.begin(),points.end(),[](const auto&a,const auto&b){return a.time<b.time;});revision.fetch_add(1);
    }
};
