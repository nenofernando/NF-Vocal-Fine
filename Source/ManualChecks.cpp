#include "Plugin.cpp"
#include <iostream>
struct TestTransport final:juce::AudioPlayHead {
 double time=0;bool running=true;
 juce::Optional<PositionInfo> getPosition()const override{PositionInfo p;p.setTimeInSeconds(time);p.setIsPlaying(running);return p;}
};
static juce::MouseEvent manualEvent(juce::Component& c,juce::Point<float> p,juce::Point<float> down,bool drag=false){return {juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&c,&c,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,drag};}
int main(int argc,char** argv){
 juce::ScopedJuceInitialiser_GUI gui;int failures=0;auto check=[&](bool ok,const char* s){std::cout<<(ok?"PASS ":"FAIL ")<<s<<"\n";if(!ok)++failures;};
 ManualDocument document;check(document.setRange(1,2,1),"create a local +1 semitone edit");document.setRange(1.4,1.6,-1);auto edits=document.getEdits();check(edits.size()==3&&edits[0].semitones==1&&edits[1].semitones==-1&&edits[2].semitones==1,"overlap changes only selected interval");document.undo();check(document.getEdits().size()==1,"undo restores previous manual edit");auto saved=document.save();ManualDocument restored;restored.restore(saved);check(restored.getEdits().size()==1&&restored.offsetAt(1.5)==1,"manual intervals persist");restored.setRange(1.2,1.8,0,true);check(restored.getEdits().size()==2&&restored.offsetAt(1.5)==0,"remove returns just the selected range to automatic");
 for(double sr:{44100.,48000.})for(int shift:{-1,1}){
  NaturalEngine engine;engine.prepare(sr,1);ManualSnapshot snapshot;snapshot.count=1;snapshot.regions[0]={1,2,shift};TuneSettings settings;settings.key=0;settings.scale=1;settings.amount=1;settings.human=0;settings.speed=20;
  int count=int(sr*3.2),block=257;juce::AudioBuffer<float> result(1,count+engine.latency+512);result.clear();
  for(int offset=0;offset<count+engine.latency;offset+=block){juce::AudioBuffer<float> b(1,block);for(int i=0;i<block;++i)b.setSample(0,i,float(0.2*std::sin(2*juce::MathConstants<double>::pi*261.625565*(offset+i)/sr)));engine.process(b,settings,&snapshot,offset/sr);result.copyFrom(0,offset,b,0,0,block);}
  auto pitchAt=[&](double start){VoiceDetector detector;detector.prepare(sr);for(int i=int((start+engine.latency/sr)*sr);i<int((start+0.35+engine.latency/sr)*sr);++i)detector.push(result.getSample(0,i));detector.analyse();return detector.midi;};
  float before=pitchAt(0.35),inside=pitchAt(1.3),after=pitchAt(2.4);std::cout<<"rate "<<sr<<" shift "<<shift<<" before "<<before<<" inside "<<inside<<" after "<<after<<"\n";
  check(std::abs(before-60)<0.08&&std::abs(after-60)<0.08,"other occurrences of the same note stay unchanged");check(std::abs(inside-(60+shift))<0.08,"selected note moves by one semitone, including C sharp outside C major");
 }
 FineProcessor processor;processor.setPlayConfigDetails(1,1,48000,256);processor.prepareToPlay(48000,256);TestTransport transport;processor.setPlayHead(&transport);processor.manual.capturing=true;
 for(int offset=0;offset<144000;offset+=256){transport.time=offset/48000.;juce::AudioBuffer<float> b(1,256);juce::MidiBuffer midi;for(int i=0;i<256;++i)b.setSample(0,i,float(0.2*std::sin(2*juce::MathConstants<double>::pi*261.625565*(offset+i)/48000)));processor.processBlock(b,midi);}
 processor.manual.capturing=false;processor.manual.drain();auto points=processor.manual.getPoints();check(points.size()>60&&processor.manual.timelineAvailable.load(),"capture records pitch against host session time");
 std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());editor->setVisible(true);FinePanel* content=nullptr;for(auto* c:editor->getChildren())if(auto* f=dynamic_cast<FinePanel*>(c))content=f;
 ManualPanel* panel=nullptr;if(content)for(auto* c:content->getChildren()){if(auto* m=dynamic_cast<ManualPanel*>(c))panel=m;if(auto* button=dynamic_cast<juce::TextButton*>(c))if(button->getButtonText()=="Manual por trecho"){button->setToggleState(true,juce::dontSendNotification);button->onClick();}}
 check(panel&&panel->isVisible(),"manual mode opens from the header button");
 if(panel){panel->timerCallback();double begin=std::max(0.0,points.front().time-0.1),end=std::max(begin+2.0,points.back().time+0.25);auto x=[&](double t){return 48+float((t-begin)/(end-begin))*(panel->getWidth()-65);};juce::Point<float> a{x(0.7),100},b{x(1.2),100};panel->mouseDown(manualEvent(*panel,a,a));panel->mouseDrag(manualEvent(*panel,b,a,true));panel->mouseUp(manualEvent(*panel,b,a,true));for(auto* c:panel->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(c))if(button->getButtonText()=="+1 semitom")button->onClick();
  auto regions=processor.manual.getEdits();check(regions.size()==1&&std::abs(regions[0].start-0.7)<0.01&&std::abs(regions[0].end-1.2)<0.01&&regions[0].semitones==1,"drag a specific interval and press +1");
  if(argc>1){auto file=juce::File(argv[1]);file.deleteFile();juce::FileOutputStream stream(file);juce::PNGImageFormat().writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds(),true,1),stream);}
 }
 juce::MemoryBlock state;processor.getStateInformation(state);FineProcessor recall;recall.setStateInformation(state.getData(),int(state.getSize()));check(recall.manual.getEdits().size()==1&&recall.manual.getPoints().size()==points.size(),"host session state recalls manual edit and pitch map");
 TestTransport recalledHead;recall.setPlayConfigDetails(1,1,48000,256);recall.prepareToPlay(48000,256);recall.setPlayHead(&recalledHead);
 auto play=[&](double start){for(int n=0;n<48;++n){recalledHead.time=start+n*256.0/48000;juce::AudioBuffer<float> b(1,256);juce::MidiBuffer midi;for(int i=0;i<256;++i)b.setSample(0,i,float(0.2*std::sin(2*juce::MathConstants<double>::pi*261.625565*(n*256+i)/48000)));recall.processBlock(b,midi);}return recall.engine.correction.load();};
 check(std::abs(play(0.78)-1)<0.01,"recalled edit applies at its session position");
 check(std::abs(play(2.0))<0.02,"seek outside selection returns to automatic correction");
 check(std::abs(play(0.78)-1)<0.01,"seek back repeats the same localized edit");
 recalledHead.running=false;check(std::abs(play(0.78))<0.02,"manual changes do not affect live input while transport is stopped");
 recall.manual.restore({});check(recall.manual.getEdits().empty(),"old sessions without manual data have no manual edits");
 transport.running=false;juce::AudioBuffer<float> stop(1,256);stop.clear();juce::MidiBuffer midi;processor.processBlock(stop,midi);check(!processor.manual.playing.load(),"stopped transport is identified");editor.reset();
 return failures?1:0;
}
