#pragma once
#include "ManualModel.h"
#include "FineLook.h"
#include "AutomaticSelection.h"
class ManualPanel final : public juce::Component,private juce::Timer {
    ManualDocument& doc;
    juce::TextButton capture{"Capturar"},fit{"Enquadrar"},zoomIn{"Zoom +"},zoomOut{"Zoom -"},down{"-1 semitom"},up{"+1 semitom"},remove{"Remover"},undoButton{"Desfazer"},newLine{"Nova linha"},deletePoint{"Excluir ponto"},selectAll{"Selecionar tudo"},automatic{"AUTOMATIC"};
    juce::ComboBox tool,scaleChoice;juce::ToggleButton snap{"Semitons"};
    std::vector<CapturedPitch> points;std::vector<ManualRegion> edits;std::vector<PitchCurve> curves;
    std::vector<CurvePoint> pending;
    double viewStart=0,viewEnd=8,selectionStart=0,selectionEnd=0,anchor=0,lastDrawTime=0;
    bool selecting=false,hasSelection=false,autoFit=true,gesturing=false;
    int activeCurve=0,selectedNode=-1,segment=-1;float gestureLow=48,gestureHigh=72,segmentMouseY=0,segmentLeft=0,segmentRight=0;
    juce::String error,automaticSummary;unsigned lastRevision=~0u;
    juce::Rectangle<float> plot()const{return {48,80,float(getWidth()-65),float(getHeight()-125)};}
    double timeAt(float x)const{auto r=plot();return viewStart+juce::jlimit(0.0,1.0,double((x-r.getX())/r.getWidth()))*(viewEnd-viewStart);}
    float xAt(double t)const{return plot().getX()+float((t-viewStart)/(viewEnd-viewStart))*plot().getWidth();}
    std::pair<float,float> pitchBounds()const{
        float lo=127,hi=0;for(const auto& p:points)if(p.time>=viewStart&&p.time<=viewEnd&&p.midi>=0){lo=std::min(lo,p.midi-2);hi=std::max(hi,p.midi+2);}
        for(const auto& c:curves)for(const auto& p:c.nodes)if(p.time>=viewStart&&p.time<=viewEnd){lo=std::min(lo,p.midi-2);hi=std::max(hi,p.midi+2);}
        if(hi<=lo)return {48,72};if(hi-lo<8){float centre=(lo+hi)*0.5f;lo=centre-4;hi=centre+4;}return {lo,hi};
    }
    float pitchAt(float y,bool free)const{float midi=gestureLow+(plot().getBottom()-juce::jlimit(plot().getY(),plot().getBottom(),y))*(gestureHigh-gestureLow)/plot().getHeight();return juce::jlimit(0.0f,127.0f,snap.getToggleState()&&!free?std::round(midi):midi);}
    juce::String label(double t)const{bool negative=t<0;t=std::abs(t);int minutes=int(t)/60;return (negative?"-":"")+juce::String(minutes)+":"+juce::String(t-minutes*60,2).paddedLeft('0',5);}
    void refresh(){doc.drain();unsigned rev=doc.revision.load();if(rev!=lastRevision){points=doc.getPoints();edits=doc.getEdits();curves=doc.getCurves();lastRevision=rev;if(autoFit&&!gesturing&&points.size()>1){viewStart=std::max(0.0,points.front().time-0.1);viewEnd=std::max(viewStart+2.0,points.back().time+0.25);}}
        bool autoSelected=scaleChoice.getSelectedId()<=1;int guessed=doc.lastAutoKey.load();if(guessed>=0){automaticSelection::Scale suggested;suggested.key=guessed;suggested.minor=doc.lastAutoMinor.load()!=0;scaleChoice.changeItemText(1,"Auto: "+automaticSelection::name(suggested)+(doc.lastAutoUncertain.load()?" ?":""));}else scaleChoice.changeItemText(1,"Auto: detectar");if(autoSelected)scaleChoice.setSelectedId(1,juce::dontSendNotification);
        automatic.setEnabled(hasSelection&&!gesturing&&!points.empty());selectAll.setEnabled(!points.empty());
        capture.setToggleState(doc.capturing.load(),juce::dontSendNotification);capture.setButtonText(doc.capturing.load()?"Parar captura":"Capturar");down.setEnabled(hasSelection&&!gesturing);up.setEnabled(hasSelection&&!gesturing);remove.setEnabled(hasSelection||activeCurve>0);deletePoint.setEnabled(selectedNode>=0&&activeCurve>0);repaint();}
    void eraseSelectedPoint(){
        gesturing=false;selecting=false;pending.clear();segment=-1;automaticSummary.clear();
        for(const auto& c:curves)if(c.id==activeCurve&&selectedNode>=0&&selectedNode<int(c.nodes.size())){auto nodes=c.nodes;nodes.erase(nodes.begin()+selectedNode);if(nodes.empty()){doc.eraseCurve(c.id);activeCurve=0;}else doc.setCurve(c.id,std::move(nodes));break;}
        selectedNode=-1;setMouseCursor(tool.getSelectedId()==2?juce::MouseCursor::PointingHandCursor:juce::MouseCursor::CrosshairCursor);refresh();
    }
    void move(int semitones){if(!hasSelection)return;automaticSummary.clear();int current=doc.offsetAt((selectionStart+selectionEnd)/2);if(!doc.setRange(selectionStart,selectionEnd,current+semitones))error="Limite de ajustes atingido.";activeCurve=0;selectedNode=-1;refresh();}
    void zoom(double factor){double c=hasSelection?(selectionStart+selectionEnd)/2:(viewStart+viewEnd)/2;double length=juce::jlimit(0.5,1200.0,(viewEnd-viewStart)*factor);viewStart=c-length/2;viewEnd=c+length/2;autoFit=false;repaint();}
    void commit(){automaticSummary.clear();int id=doc.setCurve(activeCurve,pending);if(id)activeCurve=id;else error="Nao foi possivel salvar: limite de 64 linhas / 4096 pontos.";gesturing=false;pending.clear();lastRevision=~0u;refresh();}
    std::pair<int,int> hitNode(juce::Point<float> pos)const{auto bounds=pitchBounds();float best=100;std::pair<int,int> found{0,-1};
        for(const auto& c:curves)for(size_t i=0;i<c.nodes.size();++i){const auto& p=c.nodes[i];float y=plot().getBottom()-(p.midi-bounds.first)/(bounds.second-bounds.first)*plot().getHeight();float distance=pos.getDistanceFrom({xAt(p.time),y});if(distance<10&&distance<=best){best=distance;found={c.id,int(i)};}}return found;
    }
    std::pair<int,int> hitSegment(juce::Point<float> pos)const{auto bounds=pitchBounds();float best=7;std::pair<int,int> found{0,-1};
        for(const auto& c:curves)for(size_t i=1;i<c.nodes.size();++i){float a=xAt(c.nodes[i-1].time),b=xAt(c.nodes[i].time);if(pos.x<=a||pos.x>=b)continue;float midi=c.nodes[i-1].midi+(pos.x-a)/(b-a)*(c.nodes[i].midi-c.nodes[i-1].midi);float y=plot().getBottom()-(midi-bounds.first)/(bounds.second-bounds.first)*plot().getHeight();float distance=std::abs(y-pos.y);if(distance<=best){best=distance;found={c.id,int(i)-1};}}return found;
    }
    void putDrawn(CurvePoint point){
        double lo=std::min(lastDrawTime,point.time),hi=std::max(lastDrawTime,point.time);
        pending.erase(std::remove_if(pending.begin(),pending.end(),[&](const auto& p){return p.time>lo+0.001&&p.time<hi-0.001;}),pending.end());
        auto it=std::lower_bound(pending.begin(),pending.end(),point.time,[](const auto& p,double t){return p.time<t;});
        if(it!=pending.end()&&std::abs(it->time-point.time)<0.003)*it=point;else if(it!=pending.begin()&&std::abs((it-1)->time-point.time)<0.003)*(it-1)=point;else if(pending.size()<4096)pending.insert(it,point);
        lastDrawTime=point.time;
    }
public:
    float automaticAmount=0.8f,automaticHuman=0.7f,automaticSpeed=110;
    explicit ManualPanel(ManualDocument& d):doc(d){
        for(auto* b:{&capture,&fit,&zoomIn,&zoomOut,&down,&up,&remove,&undoButton,&newLine,&deletePoint,&selectAll,&automatic})addAndMakeVisible(*b);
        tool.addItem("Selecionar",1);tool.addItem(juce::String::fromUTF8("Mão / Pontos"),2);tool.addItem(juce::String::fromUTF8("Lápis"),3);tool.setSelectedId(1);addAndMakeVisible(tool);addAndMakeVisible(snap);snap.setToggleState(true,juce::dontSendNotification);snap.setColour(juce::ToggleButton::textColourId,fine::ink());
        scaleChoice.addItem("Auto: detectar",1);for(int key=0;key<12;++key)for(int minor=0;minor<2;++minor){automaticSelection::Scale scale;scale.key=key;scale.minor=minor!=0;scaleChoice.addItem(automaticSelection::name(scale),2+key*2+minor);}scaleChoice.setSelectedId(1);addAndMakeVisible(scaleChoice);
        selectAll.onClick=[this]{if(points.empty())return;tool.setSelectedId(1,juce::dontSendNotification);if(tool.onChange)tool.onChange();selectionStart=points.front().time;selectionEnd=points.back().time;hasSelection=selectionEnd-selectionStart>=0.025;refresh();};
        automatic.onClick=[this]{if(!hasSelection)return;doc.capturing=false;doc.drain();points=doc.getPoints();automaticSelection::Scale scale;
            int chosen=scaleChoice.getSelectedId();if(chosen<=1){scale=automaticSelection::estimate(points);if(!scale.valid){error="Nao ha base suficiente para sugerir uma escala. Capture mais voz ou escolha um tom na lista.";repaint();return;}}
            else{scale.key=(chosen-2)/2;scale.minor=(chosen-2)%2!=0;scale.valid=true;scale.uncertain=false;}
            auto generated=automaticSelection::build(points,selectionStart,selectionEnd,scale,automaticAmount,automaticHuman,automaticSpeed);
            if(!doc.replaceSelection(selectionStart,selectionEnd,std::move(generated),scale.key,scale.minor?1:0,scale.uncertain?1:0)){error="Selecao sem notas suficientes ou limite atingido. Selecione um trecho menor.";repaint();return;}
            error.clear();automaticSummary="AUTOMATIC: "+automaticSelection::name(scale)+(scale.uncertain?" (sugestao incerta)":"")+" aplicado somente a selecao. Desfazer reverte.";activeCurve=0;selectedNode=-1;refresh();};
        automatic.setTooltip("Estima o tom a partir do mapa e afina apenas a selecao. Confira a sugestao na lista; a voz isolada pode ser ambigua.");
        scaleChoice.setTooltip("Auto estima o tom; voce tambem pode escolher maior/menor manualmente. Nao muda o tom global fora da selecao.");
        tool.onChange=[this]{automaticSummary.clear();gesturing=false;pending.clear();hasSelection=false;selectedNode=-1;snap.setToggleState(tool.getSelectedId()==2,juce::dontSendNotification);setMouseCursor(tool.getSelectedId()==2?juce::MouseCursor::PointingHandCursor:juce::MouseCursor::CrosshairCursor);error.clear();refresh();};
        capture.setClickingTogglesState(true);capture.onClick=[this]{doc.capturing=capture.getToggleState();if(doc.onEdit)doc.onEdit();if(doc.capturing.load())autoFit=true;refresh();};
        fit.onClick=[this]{autoFit=true;lastRevision=~0u;refresh();};zoomIn.onClick=[this]{zoom(0.5);};zoomOut.onClick=[this]{zoom(2);};
        down.onClick=[this]{move(-1);};up.onClick=[this]{move(1);};remove.onClick=[this]{automaticSummary.clear();if(hasSelection)doc.setRange(selectionStart,selectionEnd,0,true);else if(activeCurve)doc.eraseCurve(activeCurve);activeCurve=0;selectedNode=-1;refresh();};
        undoButton.onClick=[this]{automaticSummary.clear();error.clear();gesturing=false;pending.clear();doc.undo();activeCurve=0;selectedNode=-1;refresh();};newLine.onClick=[this]{activeCurve=0;selectedNode=-1;pending.clear();error.clear();repaint();};
        deletePoint.onClick=[this]{eraseSelectedPoint();};
        capture.setTooltip("Clique e reproduza a voz pela DAW para registrar o mapa de notas.");newLine.setTooltip("Comeca uma linha independente. Os proximos pontos serao ligados entre si.");snap.setTooltip("Encaixa os pontos em semitons. Segure Shift para mover livremente.");
        up.setTooltip("Sobe um semitom apenas na selecao. Substitui curvas desenhadas dentro desse intervalo.");down.setTooltip("Desce um semitom apenas na selecao.");
        setMouseCursor(juce::MouseCursor::CrosshairCursor);startTimerHz(20);refresh();
    }
    ~ManualPanel()override{stopTimer();doc.capturing=false;doc.drain();}
    void timerCallback()override{refresh();}
    void resized()override{
        capture.setBounds(12,9,108,28);fit.setBounds(128,9,92,28);zoomIn.setBounds(228,9,72,28);zoomOut.setBounds(308,9,72,28);
        down.setBounds(469,9,120,28);up.setBounds(597,9,120,28);remove.setBounds(725,9,104,28);undoButton.setBounds(837,9,126,28);
        tool.setBounds(12,43,164,25);snap.setBounds(188,43,106,25);newLine.setBounds(308,43,100,25);deletePoint.setBounds(420,43,122,25);selectAll.setBounds(554,43,130,25);automatic.setBounds(696,43,128,25);scaleChoice.setBounds(836,43,160,25);
    }
    void mouseDown(const juce::MouseEvent& e)override{
        if(tool.getSelectedId()==2&&e.getNumberOfClicks()>1){gesturing=false;selecting=false;pending.clear();return;}
        if(!plot().contains(e.position)||points.empty())return;error.clear();automaticSummary.clear();segment=-1;anchor=timeAt(e.position.x);auto bounds=pitchBounds();gestureLow=bounds.first;gestureHigh=bounds.second;double time=timeAt(e.position.x);
        if(tool.getSelectedId()==1){anchor=time;selectionStart=selectionEnd=anchor;selecting=true;hasSelection=false;repaint();return;}
        hasSelection=false;autoFit=false;gesturing=true;selecting=false;
        if(tool.getSelectedId()==3){activeCurve=0;selectedNode=-1;pending={{time,pitchAt(e.position.y,e.mods.isShiftDown())}};lastDrawTime=time;}
        else{auto hit=hitNode(e.position);if(hit.first){activeCurve=hit.first;selectedNode=hit.second;}
            else {auto line=hitSegment(e.position);if(line.first){activeCurve=line.first;segment=line.second;selectedNode=-1;segmentMouseY=e.position.y;}}
            pending.clear();for(const auto& c:curves)if(c.id==activeCurve)pending=c.nodes;
            if(segment>=0){segmentLeft=pending[size_t(segment)].midi;segmentRight=pending[size_t(segment+1)].midi;}
            else if(!hit.first){CurvePoint point{time,pitchAt(e.position.y,e.mods.isShiftDown())};auto it=std::lower_bound(pending.begin(),pending.end(),time,[](const auto& p,double t){return p.time<t;});if(it!=pending.end()&&std::abs(it->time-time)<0.005){selectedNode=int(it-pending.begin());*it=point;}else if(it!=pending.begin()&&std::abs((it-1)->time-time)<0.005){selectedNode=int(it-pending.begin())-1;*(it-1)=point;}else{selectedNode=int(it-pending.begin());pending.insert(it,point);}}
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        }repaint();
    }
    void mouseDrag(const juce::MouseEvent& e)override{
        if(selecting){double t=timeAt(e.position.x);selectionStart=std::min(anchor,t);selectionEnd=std::max(anchor,t);hasSelection=selectionEnd-selectionStart>=0.025;repaint();return;}
        if(!gesturing)return;double time=timeAt(e.position.x);float pitch=pitchAt(e.position.y,e.mods.isShiftDown());
        if(tool.getSelectedId()==3)putDrawn({time,pitch});
        else if(segment>=0&&segment+1<int(pending.size())){float delta=(segmentMouseY-e.position.y)*(gestureHigh-gestureLow)/plot().getHeight();if(snap.getToggleState()&&!e.mods.isShiftDown())delta=std::round(delta);delta=juce::jlimit(-std::min(segmentLeft,segmentRight),127-std::max(segmentLeft,segmentRight),delta);pending[size_t(segment)].midi=segmentLeft+delta;pending[size_t(segment+1)].midi=segmentRight+delta;}
        else if(selectedNode>=0&&selectedNode<int(pending.size())){double lo=selectedNode?pending[size_t(selectedNode-1)].time+0.001:viewStart,hi=selectedNode+1<int(pending.size())?pending[size_t(selectedNode+1)].time-0.001:viewEnd;pending[size_t(selectedNode)]={juce::jlimit(lo,hi,time),pitch};}
        repaint();
    }
    void mouseUp(const juce::MouseEvent& e)override{
        if(gesturing){if(segment>=0&&e.getDistanceFromDragStart()<4){CurvePoint point{anchor,pitchAt(e.position.y,e.mods.isShiftDown())};auto it=std::lower_bound(pending.begin(),pending.end(),anchor,[](const auto& p,double t){return p.time<t;});if(it!=pending.end()&&std::abs(it->time-anchor)<0.005){selectedNode=int(it-pending.begin());*it=point;}else if(it!=pending.begin()&&std::abs((it-1)->time-anchor)<0.005){selectedNode=int(it-pending.begin())-1;*(it-1)=point;}else{selectedNode=int(it-pending.begin());pending.insert(it,point);}}segment=-1;commit();setMouseCursor(tool.getSelectedId()==2?juce::MouseCursor::PointingHandCursor:juce::MouseCursor::CrosshairCursor);return;}
        if(!selecting)return;selecting=false;
        if(e.getDistanceFromDragStart()<4){double t=timeAt(e.position.x);auto it=std::lower_bound(points.begin(),points.end(),t,[](const auto& p,double v){return p.time<v;});
            if(it==points.end()&&it!=points.begin())--it;else if(it!=points.begin()&&std::abs((it-1)->time-t)<std::abs(it->time-t))--it;
            if(it!=points.end()&&it->midi>=0&&std::abs(it->time-t)<0.18){int note=juce::roundToInt(it->midi);auto first=it,last=it;
                while(first!=points.begin()&&(first-1)->midi>=0&&std::round((first-1)->midi)==note&&first->time-(first-1)->time<0.12)--first;
                while(last+1!=points.end()&&(last+1)->midi>=0&&std::round((last+1)->midi)==note&&(last+1)->time-last->time<0.12)++last;
                selectionStart=first->time-0.017;selectionEnd=last->time+0.017;hasSelection=true;
            }
        }refresh();
    }
    void mouseDoubleClick(const juce::MouseEvent& e)override{
        if(e.getNumberOfClicks()!=2||tool.getSelectedId()!=2||!plot().contains(e.position))return;
        auto hit=hitNode(e.position);if(!hit.first)return;
        activeCurve=hit.first;selectedNode=hit.second;eraseSelectedPoint();
    }
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails& wheel)override{if(gesturing)return;autoFit=false;double delta=(viewEnd-viewStart)*(-wheel.deltaY-wheel.deltaX)*0.4;viewStart+=delta;viewEnd+=delta;repaint();}
    void paint(juce::Graphics& g)override{
        auto bounds=getLocalBounds().toFloat();g.setGradientFill(juce::ColourGradient(juce::Colour(0xff182335),0,0,juce::Colour(0xff101a29),bounds.getRight(),bounds.getBottom(),false));g.fillRoundedRectangle(bounds,14);g.setColour(juce::Colour(0xff2b3950));g.drawRoundedRectangle(bounds.reduced(0.5f),14,1);
        auto r=plot();auto range=gesturing?std::make_pair(gestureLow,gestureHigh):pitchBounds();float lo=range.first,hi=range.second;
        auto yAt=[&](float midi){return r.getBottom()-(midi-lo)/(hi-lo)*r.getHeight();};
        g.setColour(juce::Colour(0xff26354a));for(int i=int(std::ceil(lo));i<=int(hi);++i)g.drawHorizontalLine(int(yAt(float(i))),r.getX(),r.getRight());
        static const char* names[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};g.setFont(juce::FontOptions(10));for(int i=int(std::ceil(lo));i<=int(hi);++i)if(hi-lo<=16||i%12==0){g.setColour(fine::muted());g.drawText(juce::String(names[(i%12+12)%12])+juce::String(i/12-1),3,int(yAt(float(i)))-6,38,12,juce::Justification::centredRight);}
        for(int i=0;i<=4;++i){float x=r.getX()+r.getWidth()*i/4;g.setColour(juce::Colour(0xff2b3c51));g.drawVerticalLine(int(x),r.getY(),r.getBottom());g.setColour(fine::muted());g.setFont(juce::FontOptions(9));g.drawText(label(viewStart+(viewEnd-viewStart)*i/4),int(x)-27,int(r.getBottom()+3),54,15,juce::Justification::centred);}
        {juce::Graphics::ScopedSaveState clip(g);g.reduceClipRegion(r.toNearestInt());
            for(const auto& edit:edits){float x=xAt(edit.start),w=xAt(edit.end)-x;g.setColour(juce::Colour(0xff48e3ac).withAlpha(0.07f));g.fillRect(x,r.getY(),w,r.getHeight());}
            juce::Path original,changed;bool pen=false;double prev=-100;
            for(const auto& p:points){if(p.time<viewStart||p.time>viewEnd||p.midi<0){pen=false;continue;}float x=xAt(p.time),y=yAt(p.midi),shift=0;for(const auto& e:edits)if(p.time>=e.start&&p.time<e.end){shift=float(e.semitones);break;}float corrected=yAt(p.midi+shift);
                if(pen&&p.time-prev<0.12){original.lineTo(x,y);changed.lineTo(x,corrected);}else{original.startNewSubPath(x,y);changed.startNewSubPath(x,corrected);}pen=true;prev=p.time;
            }g.setColour(fine::blue());g.strokePath(original,juce::PathStrokeType(1.6f));g.setColour(juce::Colour(0xff48e3ac).withAlpha(0.55f));g.strokePath(changed,juce::PathStrokeType(1.2f));
            auto drawCurve=[&](const std::vector<CurvePoint>& nodes,bool selected){juce::Path line;for(size_t i=0;i<nodes.size();++i){float x=xAt(nodes[i].time),y=yAt(nodes[i].midi);if(i)line.lineTo(x,y);else line.startNewSubPath(x,y);}g.setColour(juce::Colour(0xff48e3ac));g.strokePath(line,juce::PathStrokeType(selected?2.2f:1.8f));
                if(tool.getSelectedId()==2)for(size_t i=0;i<nodes.size();++i){float x=xAt(nodes[i].time),y=yAt(nodes[i].midi);g.setColour(juce::Colour(0xff101a29));g.fillEllipse(x-4.5f,y-4.5f,9,9);g.setColour(juce::Colour(0xff48e3ac));g.drawEllipse(x-4.5f,y-4.5f,9,9,1.5f);if(selected&&int(i)==selectedNode)g.fillEllipse(x-2,y-2,4,4);}};
            for(const auto& c:curves)if(!gesturing||c.id!=activeCurve)drawCurve(c.nodes,c.id==activeCurve);
            if(gesturing)drawCurve(pending,true);
            if(hasSelection){g.setColour(fine::blue().withAlpha(0.10f));g.fillRect(xAt(selectionStart),r.getY(),xAt(selectionEnd)-xAt(selectionStart),r.getHeight());g.setColour(fine::blue());g.drawRect(xAt(selectionStart),r.getY(),xAt(selectionEnd)-xAt(selectionStart),r.getHeight(),1.0f);}
            g.setColour(juce::Colours::white.withAlpha(0.5f));g.drawVerticalLine(int(xAt(doc.cursor.load())),r.getY(),r.getBottom());
        }
        g.setFont(juce::FontOptions(11));g.setColour(fine::muted());juce::String status;
        if(error.isNotEmpty())status=error;
        else if(points.empty())status=doc.capturing.load()?"Reproduza a voz na DAW para capturar o mapa.":"Capturar > reproduzir na DAW > escolher Selecionar, Mao / Pontos ou Lapis.";
        else if(automaticSummary.isNotEmpty())status=automaticSummary;
        else if(gesturing)status="Solte para aplicar a curva desenhada. Verde = altura alvo prevista; azul = voz capturada.";
        else if(tool.getSelectedId()==2){bool single=false;for(const auto& c:curves)if(c.id==activeCurve&&c.nodes.size()==1)single=true;status=single?"Crie um segundo ponto para ativar a linha de afinacao. Arraste os pontos para ajustar a altura.":"Clique cria pontos; dois cliques num ponto excluem. Arraste pontos ou a linha para subir/descer.";}
        else if(tool.getSelectedId()==3)status="Arraste para desenhar a afinacao alvo. Semitons encaixa nas notas; Shift permite desenho livre.";
        else if(hasSelection)status=label(selectionStart)+" a "+label(selectionEnd)+"  |  "+juce::String(doc.offsetAt((selectionStart+selectionEnd)/2))+" st  |  Verde = previsao. Reproduza para ouvir.";
        else status="Selecione uma nota ou intervalo para +/-1. As curvas verdes definem a altura alvo entre seus pontos.";
        g.drawText(status,14,getHeight()-22,getWidth()-28,17,juce::Justification::centredLeft);
    }
};
