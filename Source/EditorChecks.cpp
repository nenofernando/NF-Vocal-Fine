#include "Plugin.cpp"
#include <iostream>
static juce::MouseEvent eventFor(juce::Component& c,juce::Point<float> position,juce::Point<float> down,int clicks=1,bool dragged=false){
    return {juce::Desktop::getInstance().getMainMouseSource(),position,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&c,&c,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),clicks,dragged};
}
int main(int argc,char** argv){
    juce::ScopedJuceInitialiser_GUI gui;int failures=0;
    auto check=[&](bool ok,const char* description){std::cout<<(ok?"PASS ":"FAIL ")<<description<<"\n";if(!ok)++failures;};
    FineProcessor processor;processor.prepareToPlay(48000,256);std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());editor->setVisible(true);
    FinePanel* panel=nullptr;juce::ResizableCornerComponent* grip=nullptr;
    for(auto* c:editor->getChildren()){if(auto* p=dynamic_cast<FinePanel*>(c))panel=p;if(auto* g=dynamic_cast<juce::ResizableCornerComponent*>(c))grip=g;}
    check(editor->getWidth()==848&&editor->getHeight()==544,"smaller default size 848 x 544");
    check(editor->isResizable()&&grip&&grip->isVisible(),"visible corner grip and host resizing enabled");
    check(panel!=nullptr,"scaled content panel exists");
    if(!panel||!grip)return 2;
    auto drag=[&](int dx,int dy){auto down=eventFor(*grip,{15,15},{15,15});static_cast<juce::Component*>(grip)->mouseDown(down);auto moved=eventFor(*grip,{float(15+dx),float(15+dy)},{15,15},1,true);static_cast<juce::Component*>(grip)->mouseDrag(moved);static_cast<juce::Component*>(grip)->mouseUp(moved);};
    auto aligned=[&]{auto r=editor->getLocalArea(panel,panel->getLocalBounds().toFloat());return std::abs(r.getX())<0.01f&&std::abs(r.getY())<0.01f&&std::abs(r.getWidth()-editor->getWidth())<0.01f&&std::abs(r.getHeight()-editor->getHeight())<0.01f;};
    check(aligned(),"content fills the compact editor without clipping");
    drag(212,136);check(editor->getWidth()==1060&&editor->getHeight()==680&&aligned(),"drag grip to enlarge proportionally");
    drag(-212,-136);check(editor->getWidth()==848&&editor->getHeight()==544&&aligned(),"drag grip to shrink proportionally");
    drag(-10000,-10000);std::cout<<"minimum dimensions "<<editor->getWidth()<<" x "<<editor->getHeight()<<"\n";check(editor->getWidth()==636&&editor->getHeight()==408&&aligned(),"minimum size and layout bounds");
    drag(10000,10000);check(editor->getWidth()==1590&&editor->getHeight()==1020&&aligned(),"maximum size and layout bounds");
    auto logoDouble=eventFor(*panel,{70,50},{70,50},2);panel->mouseDoubleClick(logoDouble);
    check(editor->getWidth()==848&&editor->getHeight()==544,"logo double click restores compact default");
    editor->setSize(1060,680);auto outside=eventFor(*panel,{300,50},{300,50},2);panel->mouseDoubleClick(outside);
    check(editor->getWidth()==1060&&editor->getHeight()==680,"double click outside logo keeps chosen size");
    panel->mouseDown(eventFor(*panel,{70,50},{70,50}));check(editor->getWidth()==1060,"single logo click keeps chosen size");
    editor->setScaleFactor(1.5f);panel->mouseDoubleClick(logoDouble);
    check(std::abs(editor->getTransform().mat00-1.5f)<0.001f&&editor->getWidth()==848&&aligned(),"host DPI scaling survives reset and content resize");
    editor->setScaleFactor(1.0f);
    auto snapshot=[&](const char* name){if(argc<2)return;auto path=juce::File(argv[1]).getChildFile(name);path.getParentDirectory().createDirectory();path.deleteFile();juce::FileOutputStream out(path);juce::PNGImageFormat().writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds(),true,1),out);};
    snapshot("Interface.png");
    for(int frame=0;frame<180;++frame){juce::AudioBuffer<float> b(2,1600);juce::MidiBuffer midi;for(int i=0;i<1600;++i){double t=(frame*1600+i)/48000.0,phase=2*juce::MathConstants<double>::pi*225*t+0.12*std::sin(2*juce::MathConstants<double>::pi*3*t);float v=float(0.18*std::sin(phase)+0.05*std::sin(2*phase));b.setSample(0,i,v);b.setSample(1,i,v);}processor.processBlock(b,midi);panel->timerCallback();}
    check(processor.engine.originalMeasured.load()>0&&processor.engine.outputMeasured.load()>0,"original and corrected curves receive measured pitch");
    snapshot("Interface-em-uso.png");editor->setSize(636,408);snapshot("Interface-minima.png");editor->setSize(1590,1020);snapshot("Interface-maxima.png");
    editor.reset();check(!processor.engine.meteringEnabled.load(),"closing editor stops extra pitch monitoring");
    return failures?1:0;
}
