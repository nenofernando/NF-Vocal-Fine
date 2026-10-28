#include "Engine.h"
class FineProcessor : public juce::AudioProcessor {
public:
    juce::AudioProcessorValueTreeState state;
    NaturalEngine engine;
    ManualDocument manual;ManualSnapshot manualAudio;
    double expectedTime=0;int captureSamples=0;bool hadTransport=false;
    std::array<std::atomic<float>*,19> controls{};
    static juce::AudioProcessorValueTreeState::ParameterLayout layout(){
        juce::AudioProcessorValueTreeState::ParameterLayout p;
        auto add=[&](const char* id,const char* name,float lo,float hi,float value){p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name,juce::NormalisableRange<float>(lo,hi),value));};
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"key",1},"Tom",juce::StringArray{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"},0));
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"scale",1},"Escala",juce::StringArray{"Cromatica","Maior","Menor natural"},0));
        add("amount","Correcao",0,100,80);add("speed","Transicao (ms)",5,350,110);add("human","Naturalidade",0,100,70);add("formant","Formantes (st)",-6,6,0);add("mix","Mix",0,100,100);
        for(int i=0;i<12;++i)p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"note"+juce::String(i),1},"Nota "+juce::String(i),true));return p;
    }
    FineProcessor():AudioProcessor(BusesProperties().withInput("Entrada",juce::AudioChannelSet::stereo(),true).withOutput("Saida",juce::AudioChannelSet::stereo(),true)),state(*this,nullptr,"VocalNatural",layout()){
        const char* ids[]={"key","scale","amount","speed","human","formant","mix"};for(int i=0;i<7;++i)controls[i]=state.getRawParameterValue(ids[i]);for(int i=0;i<12;++i)controls[i+7]=state.getRawParameterValue("note"+juce::String(i));
        manual.onEdit=[this]{updateHostDisplay(juce::AudioProcessorListener::ChangeDetails{}.withNonParameterStateChanged(true));};
    }
    const juce::String getName()const override{return "NF Vocal Fine";}
    bool acceptsMidi()const override{return false;} bool producesMidi()const override{return false;}
    double getTailLengthSeconds()const override{return engine.latency/engine.rate;}
    int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}const juce::String getProgramName(int)override{return "Natural";}void changeProgramName(int,const juce::String&)override{}
    bool isBusesLayoutSupported(const BusesLayout& l)const override{return l.getMainInputChannelSet()==l.getMainOutputChannelSet()&&(l.getMainOutputChannelSet()==juce::AudioChannelSet::mono()||l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo());}
    void prepareToPlay(double sr,int)override{engine.prepare(sr,getTotalNumOutputChannels());hadTransport=false;captureSamples=0;setLatencySamples(engine.latency);}
    void releaseResources()override{}void reset()override{engine.reset();hadTransport=false;captureSamples=0;}
    float value(int index)const{return controls[index]->load();}
    void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&)override{
        TuneSettings s;s.key=int(value(0));s.scale=int(value(1));s.amount=value(2)/100;s.speed=value(3);s.human=value(4)/100;s.formant=value(5);s.mix=value(6)/100;s.mask=0;
        for(int i=0;i<12;++i)if(value(i+7)>0.5f)s.mask|=1u<<i;
        auto* playHead=getPlayHead();auto position=playHead?playHead->getPosition():juce::Optional<juce::AudioPlayHead::PositionInfo>{};
        bool valid=false,running=false;double time=0;
        if(position){if(auto seconds=position->getTimeInSeconds()){time=*seconds;valid=std::isfinite(time);}else if(auto samples=position->getTimeInSamples()){time=double(*samples)/getSampleRate();valid=true;}running=position->getIsPlaying()||position->getIsRecording();}
        manual.timelineAvailable=valid;manual.playing=running;manual.cursor=time;
        if(valid&&running){if(!hadTransport||std::abs(time-expectedTime)>std::max(0.010,2.0*b.getNumSamples()/getSampleRate())){engine.reset();captureSamples=0;}expectedTime=time+b.getNumSamples()/getSampleRate();hadTransport=true;}else hadTransport=false;
        manual.readForAudio(manualAudio);
        engine.process(b,s,valid&&running?&manualAudio:nullptr,time);
        if(valid&&running&&manual.capturing.load()&&!isNonRealtime()){
            captureSamples+=b.getNumSamples();if(captureSamples>=getSampleRate()/30){captureSamples=0;double centreTime=time+b.getNumSamples()/getSampleRate()-512.0*engine.detector.decimation/getSampleRate();manual.push({centreTime,engine.note.load()});}
        }
    }
    void getStateInformation(juce::MemoryBlock& m)override{auto snapshot=state.copyState();snapshot.addChild(manual.save(),-1,nullptr);auto xml=snapshot.createXml();copyXmlToBinary(*xml,m);}
    void setStateInformation(const void* data,int bytes)override{if(auto xml=getXmlFromBinary(data,bytes))if(xml->hasTagName(state.state.getType())){auto restored=juce::ValueTree::fromXml(*xml);manual.restore(restored.getChildWithName("ManualV1"));restored.removeChild(restored.getChildWithName("ManualV1"),nullptr);state.replaceState(restored);}}
    bool hasEditor()const override{return true;}juce::AudioProcessorEditor* createEditor()override;
};
#include "FineLook.h"
#include "ManualPanel.h"
#if !NF_PREVIEW
#include <FineAssets.h>
#endif
class FinePanel final : public juce::Component,private juce::Timer {
    FineProcessor& p;
    fine::Look look;
    juce::Image logo;
    juce::ComboBox key,scale;
    ManualPanel manualPanel;juce::TextButton manualButton{"Manual por trecho"};
    juce::TooltipWindow tips{this,650};
    std::array<juce::Slider,5> knobs;
    std::array<juce::TextButton,12> notes;
    std::array<float,180> history{},correctedHistory{};
    size_t head=0;
    float neonOriginal=0,neonCorrected=0,originalGlow=0,correctedGlow=0;
    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<SA>> sliders;
    std::vector<std::unique_ptr<BA>> buttons;
    std::unique_ptr<CA> keyAttachment,scaleAttachment;
    void text(juce::Graphics& g,const juce::String& s,juce::Rectangle<float> r,float size,juce::Colour colour,bool bold=false,int align=juce::Justification::centredLeft){g.setColour(colour);g.setFont(bold?juce::FontOptions(size).withStyle("Bold"):juce::FontOptions(size));g.drawText(s,r,align);}
    void card(juce::Graphics& g,juce::Rectangle<float> r){g.setGradientFill(juce::ColourGradient(juce::Colour(0xff182335),r.getX(),r.getY(),juce::Colour(0xff101a29),r.getRight(),r.getBottom(),false));g.fillRoundedRectangle(r,14);g.setColour(juce::Colour(0xff2b3950));g.drawRoundedRectangle(r.reduced(0.5f),14,1);}
public:
    explicit FinePanel(FineProcessor& processor):p(processor),manualPanel(processor.manual){
        setLookAndFeel(&look);
        addAndMakeVisible(manualPanel);manualPanel.setVisible(false);
        addAndMakeVisible(manualButton);manualButton.setClickingTogglesState(true);
        manualButton.onClick=[this]{bool show=manualButton.getToggleState();manualPanel.setVisible(show);manualButton.setButtonText(show?"Voltar ao monitor":"Manual por trecho");if(!show&&p.manual.capturing.exchange(false)){p.manual.drain();if(p.manual.onEdit)p.manual.onEdit();}};
#if NF_PREVIEW
        logo=juce::ImageFileFormat::loadFrom(juce::File::getCurrentWorkingDirectory().getChildFile("Assets/Logo.png"));
#else
        logo=juce::ImageCache::getFromMemory(FineAssets::Logo_png,FineAssets::Logo_pngSize);
#endif
        history.fill(std::numeric_limits<float>::quiet_NaN());correctedHistory.fill(std::numeric_limits<float>::quiet_NaN());
        p.engine.meteringEnabled=true;
        key.addItemList({"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"},1);
        scale.addItemList({juce::String::fromUTF8("Cromática"),"Maior","Menor natural"},1);
        key.setTooltip("Tom da musica");scale.setTooltip("Escala usada para escolher as notas de destino");
        addAndMakeVisible(key);addAndMakeVisible(scale);keyAttachment=std::make_unique<CA>(p.state,"key",key);scaleAttachment=std::make_unique<CA>(p.state,"scale",scale);
        const char* ids[]={"amount","speed","human","formant","mix"};
        const char* help[]={"Intensidade da afinacao. Para comecar, use 65 a 85%.","Tempos maiores preservam a passagem natural entre as notas.","Preserva vibrato e pequenas variacoes expressivas da voz.","Zero preserva o timbre. Outros valores mudam a ressonancia da voz.","Mistura com a voz original, alinhada a latencia do efeito."};
        for(int i=0;i<5;++i){auto& k=knobs[i];k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);k.setTextBoxStyle(juce::Slider::TextBoxBelow,false,145,31);k.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);addAndMakeVisible(k);sliders.push_back(std::make_unique<SA>(p.state,ids[i],k));k.textFromValueFunction=[i](double v){return (i==3?juce::String(v,1):juce::String(juce::roundToInt(v)))+(i==1?" ms":i==3?" st":" %");};k.updateText();k.setTooltip(help[i]);}
        juce::StringArray ns{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        for(int i=0;i<12;++i){notes[i].setButtonText(ns[i]);notes[i].setClickingTogglesState(true);notes[i].setTooltip("Permitir a nota "+ns[i]+" dentro da escala selecionada");addAndMakeVisible(notes[i]);buttons.push_back(std::make_unique<BA>(p.state,"note"+juce::String(i),notes[i]));}
        setSize(1060,680);startTimerHz(30);
    }
    ~FinePanel()override{stopTimer();p.engine.meteringEnabled=false;setLookAndFeel(nullptr);}
    std::function<void()> onLogoDoubleClick;
    void mouseDoubleClick(const juce::MouseEvent& e)override{if(juce::Rectangle<int>(28,13,135,77).contains(e.getPosition())&&onLogoDoubleClick)onLogoDoubleClick();}
    void mouseMove(const juce::MouseEvent& e)override{setMouseCursor(juce::Rectangle<int>(28,13,135,77).contains(e.getPosition())?juce::MouseCursor::PointingHandCursor:juce::MouseCursor::NormalCursor);}
    void timerCallback()override{
        if(!manualButton.getToggleState()){int count=int(p.manual.getEdits().size());manualButton.setButtonText(count?"Manual: "+juce::String(count)+" ajuste(s)":"Manual por trecho");}
        float original=p.engine.originalMeasured.load(),corrected=p.engine.outputMeasured.load();
        float missing=std::numeric_limits<float>::quiet_NaN();
        float reference=original>=0?std::round(original):corrected>=0?std::round(corrected):0;
        history[head]=original>=0?(original-reference)*100:missing;
        correctedHistory[head]=corrected>=0?(corrected-reference)*100:missing;
        // Visual smoothing only: audio and measured pitch are untouched.
        if(original>=0)neonOriginal+=0.28f*(juce::jlimit(-50.0f,50.0f,(original-reference)*100)-neonOriginal);
        if(corrected>=0)neonCorrected+=0.28f*(juce::jlimit(-50.0f,50.0f,(corrected-reference)*100)-neonCorrected);
        originalGlow+=0.22f*((original>=0?1.0f:0.0f)-originalGlow);
        correctedGlow+=0.22f*((corrected>=0?1.0f:0.0f)-correctedGlow);
        head=(head+1)%history.size();repaint(24,110,1012,224);
    }
    void resized()override{
        manualPanel.setBounds(24,119,1012,213);manualButton.setBounds(690,68,160,24);
        key.setBounds(195,373,86,35);scale.setBounds(293,373,190,35);
        for(int i=0;i<12;++i)notes[i].setBounds(506+i*42,369,36,45);
        for(int i=0;i<5;++i)knobs[i].setBounds(45+i*204,477,154,140);
    }
    void paint(juce::Graphics& g)override{
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff111b2a),0,0,juce::Colour(0xff080e18),float(getWidth()),float(getHeight()),false));g.fillAll();
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff1765aa).withAlpha(0.16f),760,0,juce::Colours::transparentBlack,760,350,true));g.fillRect(getLocalBounds());
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);g.drawImageWithin(logo,28,13,135,77,juce::RectanglePlacement::centred);
        text(g,"NF Vocal Fine",{260,26,540,40},30,fine::ink(),true,juce::Justification::centred);
        text(g,juce::String::fromUTF8("AFINAÇÃO VOCAL  /  EXPRESSÃO NATURAL"),{260,65,540,18},10.5f,fine::muted(),false,juce::Justification::centred);
        g.setColour(juce::Colour(0xff162d46));g.fillRoundedRectangle(877,34,159,29,14);g.setColour(fine::blue());g.fillEllipse(890,45,6,6);
        text(g,juce::String::fromUTF8("PRÉVIA  0.3.0"),{905,35,120,26},11,fine::blue(),true);
        g.setColour(juce::Colour(0xff243247));g.drawHorizontalLine(100,24,1036);
        card(g,{24,119,1012,213});
        g.setColour(juce::Colour(0xff2c3c52));g.drawVerticalLine(220,146,306);g.drawVerticalLine(822,146,306);
        float m=p.engine.originalMeasured.load(),corrected=p.engine.outputMeasured.load();
        float shift=m>=0&&corrected>=0?corrected-m:0;
        text(g,"NOTA DE ENTRADA",{47,142,165,20},10.5f,fine::muted(),true);
        juce::String caption=juce::String::fromUTF8("—");
        if(m>=0){static const char* ns[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};int n=juce::roundToInt(m);caption=juce::String(ns[(n%12+12)%12])+juce::String(n/12-1);}
        text(g,caption,{44,170,175,91},66,fine::ink(),true);
        text(g,m>=0?juce::String(440*std::pow(2.0,(m-69)/12),1)+" Hz":"Aguardando voz",{48,271,164,22},13,m>=0?fine::blue():fine::muted());
        text(g,juce::String::fromUTF8("AFINAÇÃO DA VOZ"),{248,140,310,20},10.5f,fine::muted(),true);
        g.setColour(fine::blue());g.fillEllipse(599,147,5,5);text(g,"Original",{611,140,85,20},10,fine::blue());
        auto green=juce::Colour(0xff48e3ac);g.setColour(green);g.fillEllipse(700,147,5,5);text(g,"Corrigida",{712,140,88,20},10,green);
        auto plot=juce::Rectangle<float>(252,177,538,77);
        for(int i=0;i<=4;++i){float y=plot.getY()+plot.getHeight()*i/4;g.setColour(juce::Colour(0xff29394f).withAlpha(i==2?1.0f:0.55f));g.drawHorizontalLine(int(y),plot.getX(),plot.getRight());}
        for(int i=0;i<=8;++i){g.setColour(juce::Colour(0xff253449).withAlpha(0.4f));g.drawVerticalLine(int(plot.getX()+i*plot.getWidth()/8),plot.getY(),plot.getBottom());}
        auto drawTrace=[&](const std::array<float,180>& data,juce::Colour colour){
            juce::Path trace;bool pen=false,hasData=false;
            for(size_t i=0;i<data.size();++i){float v=data[(head+i)%data.size()];if(!std::isfinite(v)){pen=false;continue;}hasData=true;
                float x=plot.getX()+plot.getWidth()*float(i)/(data.size()-1),y=plot.getCentreY()-juce::jlimit(-100.0f,100.0f,v)*plot.getHeight()/200;
                if(pen)trace.lineTo(x,y);else trace.startNewSubPath(x,y);pen=true;
            }
            g.setColour(colour.withAlpha(0.10f));g.strokePath(trace,juce::PathStrokeType(6));g.setColour(colour);g.strokePath(trace,juce::PathStrokeType(1.8f));return hasData;
        };
        bool hasOriginal=drawTrace(history,fine::blue()),hasCorrected=drawTrace(correctedHistory,green);
        if(!hasOriginal&&!hasCorrected)text(g,"Reproduza uma voz solo",plot,13,fine::muted(),false,juce::Justification::centred);
        text(g,"+100",{776,162,34,12},8,fine::muted());text(g,"-100",{776,255,34,12},8,fine::muted());
        auto bar=juce::Rectangle<float>(252,278,538,10);
        g.setColour(juce::Colour(0xff26364b));g.fillRoundedRectangle(bar,4);
        g.setColour(green.withAlpha(0.15f));g.fillRoundedRectangle(bar.getCentreX()-bar.getWidth()*0.05f,bar.getY(),bar.getWidth()*0.1f,bar.getHeight(),2);
        g.setColour(green.withAlpha(0.32f));g.fillRect(bar.getCentreX()-0.5f,274.0f,1.0f,18.0f);
        auto neon=[&](float cents,juce::Colour colour,float intensity){
            if(intensity<0.01f)return;
            const float x=bar.getCentreX()+cents*bar.getWidth()/100,y=bar.getCentreY();
            juce::Graphics::ScopedSaveState clip(g);g.reduceClipRegion(245,263,552,35);
            {
                juce::Graphics::ScopedSaveState halo(g);
                g.addTransform(juce::AffineTransform::scale(1.0f,0.27f).translated(0,y*0.73f));
                juce::ColourGradient glow(colour.withAlpha(intensity*0.55f),x,y,colour.withAlpha(0.0f),x+48,y,true);
                glow.addColour(0.35,colour.withAlpha(intensity*0.28f));glow.addColour(0.7,colour.withAlpha(intensity*0.07f));
                g.setGradientFill(glow);g.fillEllipse(x-48,y-48,96,96);
            }
            g.setColour(colour.withAlpha(intensity*0.16f));g.fillRoundedRectangle(x-28,y-5,56,10,5);
            g.setColour(colour.withAlpha(intensity*0.95f));g.fillRoundedRectangle(x-20,y-2.5f,40,5,2.5f);
            g.setColour(juce::Colour(0xffe9fff7).withAlpha(intensity*0.85f));g.fillRoundedRectangle(x-8,y-0.65f,16,1.3f,0.65f);
        };
        neon(neonOriginal,fine::blue(),originalGlow);neon(neonCorrected,green,correctedGlow);
        text(g,"-50",{249,299,50,15},9,fine::muted());text(g,"-25",{365,299,50,15},9,fine::muted(),false,juce::Justification::centred);
        text(g,"0",{496,299,50,15},9,green,true,juce::Justification::centred);text(g,"+25",{631,299,50,15},9,fine::muted(),false,juce::Justification::centred);text(g,"+50 cents",{728,299,65,15},9,fine::muted(),false,juce::Justification::centredRight);
        text(g,juce::String::fromUTF8("CORREÇÃO MEDIDA"),{846,142,175,20},10.5f,fine::muted(),true);
        text(g,(shift>=0?"+":"")+juce::String(juce::roundToInt(shift*100)),{843,181,170,65},42,fine::blue(),true);
        text(g,"cents",{848,246,150,21},12,fine::muted());
        g.setColour(m>=0?fine::blue():juce::Colour(0xff48556b));g.fillEllipse(848,288,5,5);
        text(g,m>=0?"Monitorando":"Sem sinal vocal",{860,278,160,24},11,fine::muted());
        card(g,{24,350,1012,86});
        text(g,"ESCALA & NOTAS",{45,368,145,20},11,fine::ink(),true);
        text(g,"Escolha a melodia",{45,392,145,18},10.5f,fine::muted());
        const char* names[]={"CORREÇÃO","TRANSIÇÃO","NATURALIDADE","FORMANTES","MIX"};
        const char* hints[]={"Aproxima a nota alvo","Preserva as passagens","Mantém o vibrato","Preserva o timbre em 0","Original + afinado"};
        for(int i=0;i<5;++i){float x=24.0f+i*204;card(g,{x,452,196,184});text(g,juce::String::fromUTF8(names[i]),{x+8,465,180,18},10.5f,i==0?fine::blue():fine::ink(),true,juce::Justification::centred);text(g,juce::String::fromUTF8(hints[i]),{x+8,619,180,14},10,fine::muted(),false,juce::Justification::centred);}
        text(g,juce::String::fromUTF8("VOZ SOLO  •  PRESERVAÇÃO DE FORMANTES"),{28,650,620,18},10,fine::muted());
        text(g,juce::String::fromUTF8("Latência: ")+juce::String(p.getLatencySamples()*1000.0/p.engine.rate,0)+" ms",{800,650,232,18},10,fine::muted(),false,juce::Justification::centredRight);
    }
};
class FineEditor final : public juce::AudioProcessorEditor {
    fine::Look look;
    FinePanel panel;
public:
    static constexpr int defaultWidth=848,defaultHeight=544;
    explicit FineEditor(FineProcessor& processor):AudioProcessorEditor(processor),panel(processor){
        setLookAndFeel(&look);
        addAndMakeVisible(panel);
        setResizable(true,true);
        setResizeLimits(636,408,1590,1020);
        getConstrainer()->setFixedAspectRatio(1060.0/680.0);
        panel.onLogoDoubleClick=[this]{setSize(defaultWidth,defaultHeight);};
        setSize(defaultWidth,defaultHeight);
    }
    ~FineEditor()override{setLookAndFeel(nullptr);}
    void resized()override{
        // Scale only the content; the host retains control of editor DPI scaling.
        panel.setTransform(juce::AffineTransform::scale(float(getWidth())/1060.0f,float(getHeight())/680.0f));
    }
};
juce::AudioProcessorEditor* FineProcessor::createEditor(){return new FineEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new FineProcessor();}
