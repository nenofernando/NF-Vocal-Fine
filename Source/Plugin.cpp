#include "Engine.h"
class FineProcessor : public juce::AudioProcessor {
public:
    juce::AudioProcessorValueTreeState state;
    NaturalEngine engine;
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
    }
    const juce::String getName()const override{return "NF Vocal Fine";}
    bool acceptsMidi()const override{return false;} bool producesMidi()const override{return false;}
    double getTailLengthSeconds()const override{return engine.latency/engine.rate;}
    int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}const juce::String getProgramName(int)override{return "Natural";}void changeProgramName(int,const juce::String&)override{}
    bool isBusesLayoutSupported(const BusesLayout& l)const override{return l.getMainInputChannelSet()==l.getMainOutputChannelSet()&&(l.getMainOutputChannelSet()==juce::AudioChannelSet::mono()||l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo());}
    void prepareToPlay(double sr,int)override{engine.prepare(sr,getTotalNumOutputChannels());setLatencySamples(engine.latency);}
    void releaseResources()override{}void reset()override{engine.reset();}
    float value(int index)const{return controls[index]->load();}
    void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&)override{
        TuneSettings s;s.key=int(value(0));s.scale=int(value(1));s.amount=value(2)/100;s.speed=value(3);s.human=value(4)/100;s.formant=value(5);s.mix=value(6)/100;s.mask=0;
        for(int i=0;i<12;++i)if(value(i+7)>0.5f)s.mask|=1u<<i;
        engine.process(b,s);
    }
    void getStateInformation(juce::MemoryBlock& m)override{auto xml=state.copyState().createXml();copyXmlToBinary(*xml,m);}
    void setStateInformation(const void* data,int bytes)override{if(auto xml=getXmlFromBinary(data,bytes))if(xml->hasTagName(state.state.getType()))state.replaceState(juce::ValueTree::fromXml(*xml));}
    bool hasEditor()const override{return true;}juce::AudioProcessorEditor* createEditor()override;
};
#include "FineLook.h"
#if !NF_PREVIEW
#include <FineAssets.h>
#endif
class FineEditor final : public juce::AudioProcessorEditor,private juce::Timer {
    FineProcessor& p;
    fine::Look look;
    juce::Image logo;
    juce::ComboBox key,scale;
    juce::TooltipWindow tips{this,650};
    std::array<juce::Slider,5> knobs;
    std::array<juce::TextButton,12> notes;
    std::array<float,180> history{};
    size_t head=0;
    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<SA>> sliders;
    std::vector<std::unique_ptr<BA>> buttons;
    std::unique_ptr<CA> keyAttachment,scaleAttachment;
    void text(juce::Graphics& g,const juce::String& s,juce::Rectangle<float> r,float size,juce::Colour colour,bool bold=false,int align=juce::Justification::centredLeft){g.setColour(colour);g.setFont(bold?juce::FontOptions(size).withStyle("Bold"):juce::FontOptions(size));g.drawText(s,r,align);}
    void card(juce::Graphics& g,juce::Rectangle<float> r){g.setGradientFill(juce::ColourGradient(juce::Colour(0xff182335),r.getX(),r.getY(),juce::Colour(0xff101a29),r.getRight(),r.getBottom(),false));g.fillRoundedRectangle(r,14);g.setColour(juce::Colour(0xff2b3950));g.drawRoundedRectangle(r.reduced(0.5f),14,1);}
public:
    explicit FineEditor(FineProcessor& processor):AudioProcessorEditor(processor),p(processor){
        setLookAndFeel(&look);
#if NF_PREVIEW
        logo=juce::ImageFileFormat::loadFrom(juce::File::getCurrentWorkingDirectory().getChildFile("Assets/Logo.png"));
#else
        logo=juce::ImageCache::getFromMemory(FineAssets::Logo_png,FineAssets::Logo_pngSize);
#endif
        history.fill(std::numeric_limits<float>::quiet_NaN());
        key.addItemList({"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"},1);
        scale.addItemList({juce::String::fromUTF8("Cromática"),"Maior","Menor natural"},1);
        key.setTooltip("Tom da musica");scale.setTooltip("Escala usada para escolher as notas de destino");
        addAndMakeVisible(key);addAndMakeVisible(scale);keyAttachment=std::make_unique<CA>(p.state,"key",key);scaleAttachment=std::make_unique<CA>(p.state,"scale",scale);
        const char* ids[]={"amount","speed","human","formant","mix"};
        const char* help[]={"Intensidade da afinacao. Para comecar, use 65 a 85%.","Tempos maiores preservam a passagem natural entre as notas.","Preserva vibrato e pequenas variacoes expressivas da voz.","Zero preserva o timbre. Outros valores mudam a ressonancia da voz.","Mistura com a voz original, alinhada a latencia do efeito."};
        for(int i=0;i<5;++i){auto& k=knobs[i];k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);k.setTextBoxStyle(juce::Slider::TextBoxBelow,false,145,31);k.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);addAndMakeVisible(k);sliders.push_back(std::make_unique<SA>(p.state,ids[i],k));k.textFromValueFunction=[i](double v){return juce::String(v,i==3?1:0)+(i==1?" ms":i==3?" st":" %");};k.updateText();k.setTooltip(help[i]);}
        juce::StringArray ns{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        for(int i=0;i<12;++i){notes[i].setButtonText(ns[i]);notes[i].setClickingTogglesState(true);notes[i].setTooltip("Permitir a nota "+ns[i]+" dentro da escala selecionada");addAndMakeVisible(notes[i]);buttons.push_back(std::make_unique<BA>(p.state,"note"+juce::String(i),notes[i]));}
        setSize(1060,680);startTimerHz(30);
    }
    ~FineEditor()override{stopTimer();setLookAndFeel(nullptr);}
    void timerCallback()override{float m=p.engine.note.load();history[head]=m>=0?(m-std::round(m))*100:std::numeric_limits<float>::quiet_NaN();head=(head+1)%history.size();repaint(24,110,1012,224);}
    void resized()override{
        key.setBounds(195,373,86,35);scale.setBounds(293,373,190,35);
        for(int i=0;i<12;++i)notes[i].setBounds(506+i*42,369,36,45);
        for(int i=0;i<5;++i)knobs[i].setBounds(45+i*204,477,154,140);
    }
    void paint(juce::Graphics& g)override{
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff111b2a),0,0,juce::Colour(0xff080e18),float(getWidth()),float(getHeight()),false));g.fillAll();
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff1765aa).withAlpha(0.16f),760,0,juce::Colours::transparentBlack,760,350,true));g.fillRect(getLocalBounds());
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);g.drawImageWithin(logo,28,13,135,77,juce::RectanglePlacement::centred);
        text(g,"NF Vocal Fine",{189,26,430,40},30,fine::ink(),true);
        text(g,juce::String::fromUTF8("AFINAÇÃO VOCAL  /  EXPRESSÃO NATURAL"),{191,65,500,18},10.5f,fine::muted());
        g.setColour(juce::Colour(0xff162d46));g.fillRoundedRectangle(877,34,159,29,14);g.setColour(fine::blue());g.fillEllipse(890,45,6,6);
        text(g,juce::String::fromUTF8("PRÉVIA  0.2"),{905,35,120,26},11,fine::blue(),true);
        g.setColour(juce::Colour(0xff243247));g.drawHorizontalLine(100,24,1036);
        card(g,{24,119,1012,213});
        g.setColour(juce::Colour(0xff2c3c52));g.drawVerticalLine(220,146,306);g.drawVerticalLine(822,146,306);
        float m=p.engine.note.load(),shift=p.engine.correction.load();
        text(g,"NOTA DE ENTRADA",{47,142,165,20},10.5f,fine::muted(),true);
        juce::String caption=juce::String::fromUTF8("—");
        if(m>=0){static const char* ns[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};int n=juce::roundToInt(m);caption=juce::String(ns[(n%12+12)%12])+juce::String(n/12-1);}
        text(g,caption,{44,170,175,91},66,fine::ink(),true);
        text(g,m>=0?juce::String(440*std::pow(2.0,(m-69)/12),1)+" Hz":"Aguardando voz",{48,271,164,22},13,m>=0?fine::blue():fine::muted());
        text(g,juce::String::fromUTF8("AFINAÇÃO DE ENTRADA"),{248,140,350,20},10.5f,fine::muted(),true);
        auto plot=juce::Rectangle<float>(252,179,538,103);
        for(int i=0;i<=4;++i){float y=plot.getY()+plot.getHeight()*i/4;g.setColour(juce::Colour(0xff29394f).withAlpha(i==2?1.0f:0.55f));g.drawHorizontalLine(int(y),plot.getX(),plot.getRight());}
        for(int i=0;i<=8;++i){g.setColour(juce::Colour(0xff253449).withAlpha(0.4f));g.drawVerticalLine(int(plot.getX()+i*plot.getWidth()/8),plot.getY(),plot.getBottom());}
        juce::Path trace;bool pen=false,hasData=false;
        for(size_t i=0;i<history.size();++i){float v=history[(head+i)%history.size()];if(!std::isfinite(v)){pen=false;continue;}hasData=true;float x=plot.getX()+plot.getWidth()*float(i)/(history.size()-1),y=plot.getCentreY()-juce::jlimit(-50.0f,50.0f,v)*plot.getHeight()/100;if(pen)trace.lineTo(x,y);else trace.startNewSubPath(x,y);pen=true;}
        g.setColour(fine::blue().withAlpha(0.12f));g.strokePath(trace,juce::PathStrokeType(7));g.setColour(fine::blue());g.strokePath(trace,juce::PathStrokeType(2));
        if(!hasData)text(g,"Reproduza uma voz solo",plot,13,fine::muted(),false,juce::Justification::centred);
        text(g,"+50",{778,163,26,14},9,fine::muted());text(g,"-50",{778,285,26,14},9,fine::muted());
        text(g,"ENTRADA  /  cents",{252,295,300,16},9,fine::muted());
        text(g,juce::String::fromUTF8("CORREÇÃO APLICADA"),{846,142,175,20},10.5f,fine::muted(),true);
        text(g,(shift>=0?"+":"")+juce::String(shift*100,0),{843,181,170,65},42,fine::blue(),true);
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
juce::AudioProcessorEditor* FineProcessor::createEditor(){return new FineEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new FineProcessor();}
