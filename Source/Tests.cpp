#include "Engine.h"
#include <iostream>
int main(int argc,char** argv){
 juce::ScopedJuceInitialiser_GUI gui;
 int failures=0;auto check=[&](bool ok,const char* msg){std::cout<<(ok?"PASS ":"FAIL ")<<msg<<"\n";if(!ok)++failures;};
 for(double sr:{44100.,48000.,96000.}){
  VoiceDetector d;d.prepare(sr);for(int i=0;i<int(sr*0.15);++i)d.push(float(0.2*std::sin(2*juce::MathConstants<double>::pi*225*i/sr)));d.analyse();check(std::abs(d.midi-(69+12*std::log2(225./440)))<0.04,"YIN accuracy at 44.1/48/96k");
  for(double hz:{82.41,110.,440.,880.}){d.reset();for(int i=0;i<int(sr*0.2);++i){double phase=2*juce::MathConstants<double>::pi*hz*i/sr;d.push(float(0.12*std::sin(phase)+0.08*std::sin(2*phase)+0.04*std::sin(3*phase)));}d.analyse();check(std::abs(d.midi-(69+12*std::log2(hz/440)))<0.08,"harmonic voice-range detection");}
  NaturalEngine e;e.prepare(sr,2);TuneSettings s;s.speed=60;s.human=0;s.amount=1;
  double energy=0,stereoError=0;int crossings=0;float prev=0;const int block=257;double start=sr*0.8;int measured=0;
  for(int offset=0;offset<int(sr*1.6);offset+=block){juce::AudioBuffer<float> b(2,block);for(int i=0;i<block;++i){float x=float(0.2*std::sin(2*juce::MathConstants<double>::pi*225*(offset+i)/sr));b.setSample(0,i,x);b.setSample(1,i,x);}e.process(b,s);for(int i=0;i<block;++i){float v=b.getSample(0,i);if(!std::isfinite(v))++failures;if(offset+i>start){if(prev<=0&&v>0)++crossings;prev=v;energy+=v*v;stereoError+=std::abs(v-b.getSample(1,i));++measured;}}}
  double hz=crossings*sr/measured;std::cout<<"output Hz "<<hz<<" latency "<<e.latency<<"\n";check(std::abs(hz-220)<2,"actual audio converges to A3");std::cout<<"rms "<<std::sqrt(energy/measured)<<" stereo mean error "<<stereoError/measured<<"\n";check(energy/measured>0.002&&stereoError/measured<0.0001,"finite audible output and stereo coherence");
  e.reset();s.amount=0;s.mix=1;juce::AudioBuffer<float> wetImpulse(2,e.latency+4096);wetImpulse.clear();wetImpulse.setSample(0,0,0.5f);wetImpulse.setSample(1,0,0.5f);e.process(wetImpulse,s);int peak=0;for(int i=1;i<wetImpulse.getNumSamples();++i)if(std::abs(wetImpulse.getSample(0,i))>std::abs(wetImpulse.getSample(0,peak)))peak=i;std::cout<<"wet impulse peak "<<peak<<"\n";check(std::abs(peak-e.latency)<=2,"wet impulse matches host latency");
  e.reset();s.mix=0;juce::AudioBuffer<float> impulse(2,e.latency+1000);impulse.clear();impulse.setSample(0,0,1);impulse.setSample(1,0,1);e.process(impulse,s);check(impulse.getSample(0,e.latency)==1&&impulse.getSample(0,0)==0,"dry impulse matches reported latency");
  e.reset();impulse.clear();s.mix=1;e.process(impulse,s);check(impulse.getMagnitude(0,impulse.getNumSamples())<1e-6,"silence after reset");
 }
 {
  NaturalEngine straight,natural;straight.prepare(48000,1);natural.prepare(48000,1);TuneSettings a;a.amount=1;a.human=0;a.speed=5;TuneSettings b=a;b.human=1;
  double energyA=0,energyB=0;int count=0;
  for(int offset=0;offset<96000;offset+=128){juce::AudioBuffer<float> x(1,128),y(1,128);for(int i=0;i<128;++i){double t=(offset+i)/48000.;double phase=2*juce::MathConstants<double>::pi*220*t+0.32*std::sin(2*juce::MathConstants<double>::pi*5*t);x.setSample(0,i,float(0.2*std::sin(phase)));}y.makeCopyOf(x);straight.process(x,a);natural.process(y,b);if(offset>48000){energyA+=std::pow(straight.correction.load(),2);energyB+=std::pow(natural.correction.load(),2);++count;}}
  check(energyB<energyA*0.6,"natural control preserves vibrato with less corrective modulation");
 }
 TuneSettings s;s.scale=1;s.key=0;check(NaturalEngine::nearest(61.1f,s)==62,"major scale excludes C sharp");s.mask=0;check(NaturalEngine::nearest(61.1f,s)==61.1f,"all notes disabled leaves pitch alone");
 if(argc>1){
 juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> types;format.findAllTypesForFile(types,argv[1]);check(types.size()==1,"VST3 bundle scan");
 if(types.size()){juce::String error;auto plugin=format.createInstanceFromDescription(*types[0],48000,257,error);check(plugin!=nullptr,"VST3 host instantiation");
 if(plugin){check(plugin->getName()=="NF Vocal Fine","VST3 product name");plugin->prepareToPlay(48000,257);juce::AudioBuffer<float> b(2,257);juce::MidiBuffer midi;b.clear();plugin->processBlock(b,midi);check(plugin->getLatencySamples()>0,"VST3 latency exposed to host");
 auto& params=plugin->getParameters();std::cout<<"host parameters "<<params.size()<<"\n";for(auto* control:params)std::cout<<control->getName(64)<<" | ";std::cout<<"\n";check(params.size()==20,"all controls exposed for automation");auto* param=params[2];param->setValueNotifyingHost(0.43f);juce::MemoryBlock saved;plugin->getStateInformation(saved);param->setValueNotifyingHost(0.9f);plugin->setStateInformation(saved.getData(),int(saved.getSize()));check(std::abs(param->getValue()-0.43f)<0.001f,"session state recall");
 std::unique_ptr<juce::AudioProcessorEditor> editor(plugin->createEditorAndMakeActive());check(editor!=nullptr,"VST3 editor opens");if(editor){editor->setVisible(true);editor->setVisible(false);}editor.reset();plugin->releaseResources();}
 }
 }
 return failures?1:0;
}
