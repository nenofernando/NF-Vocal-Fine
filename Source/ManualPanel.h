#pragma once
#include "ManualModel.h"
#include "FineLook.h"
class ManualPanel final : public juce::Component,private juce::Timer {
    ManualDocument& doc;
    juce::TextButton capture{"Capturar"},fit{"Enquadrar"},zoomIn{"Zoom +"},zoomOut{"Zoom -"},down{"-1 semitom"},up{"+1 semitom"},remove{"Remover"},undoButton{"Desfazer"};
    std::vector<CapturedPitch> points;std::vector<ManualRegion> edits;
    double viewStart=0,viewEnd=8,selectionStart=0,selectionEnd=0,anchor=0;bool selecting=false,hasSelection=false,autoFit=true;
    unsigned lastRevision=~0u;
    juce::Rectangle<float> plot()const{return {48,58,float(getWidth()-65),112};}
    double timeAt(float x)const{auto r=plot();return viewStart+juce::jlimit(0.0,1.0,double((x-r.getX())/r.getWidth()))*(viewEnd-viewStart);}
    float xAt(double t)const{return plot().getX()+float((t-viewStart)/(viewEnd-viewStart))*plot().getWidth();}
    juce::String label(double t)const{int minutes=int(t)/60;return juce::String(minutes)+":"+juce::String(t-minutes*60,2).paddedLeft('0',5);}
    void refresh(){doc.drain();unsigned rev=doc.revision.load();if(rev!=lastRevision){points=doc.getPoints();edits=doc.getEdits();lastRevision=rev;if(autoFit&&points.size()>1){viewStart=std::max(0.0,points.front().time-0.1);viewEnd=std::max(viewStart+2.0,points.back().time+0.25);}}
        capture.setToggleState(doc.capturing.load(),juce::dontSendNotification);capture.setButtonText(doc.capturing.load()?"Parar captura":"Capturar");down.setEnabled(hasSelection);up.setEnabled(hasSelection);remove.setEnabled(hasSelection);repaint();}
    void move(int semitones){if(!hasSelection)return;int current=doc.offsetAt((selectionStart+selectionEnd)/2);doc.setRange(selectionStart,selectionEnd,current+semitones);refresh();}
    void zoom(double factor){double c=hasSelection?(selectionStart+selectionEnd)/2:(viewStart+viewEnd)/2;double length=juce::jlimit(0.5,1200.0,(viewEnd-viewStart)*factor);viewStart=c-length/2;viewEnd=c+length/2;autoFit=false;repaint();}
public:
    explicit ManualPanel(ManualDocument& d):doc(d){
        for(auto* b:{&capture,&fit,&zoomIn,&zoomOut,&down,&up,&remove,&undoButton})addAndMakeVisible(*b);
        capture.setClickingTogglesState(true);capture.onClick=[this]{doc.capturing=capture.getToggleState();if(doc.onEdit)doc.onEdit();if(doc.capturing.load())autoFit=true;refresh();};
        fit.onClick=[this]{autoFit=true;lastRevision=~0u;refresh();};zoomIn.onClick=[this]{zoom(0.5);};zoomOut.onClick=[this]{zoom(2);};
        down.onClick=[this]{move(-1);};up.onClick=[this]{move(1);};remove.onClick=[this]{if(hasSelection)doc.setRange(selectionStart,selectionEnd,0,true);refresh();};undoButton.onClick=[this]{doc.undo();refresh();};
        capture.setTooltip("Clique e reproduza a voz pela DAW para registrar o mapa de notas.");
        up.setTooltip("Sobe um semitom somente no intervalo selecionado, mesmo fora da escala.");down.setTooltip("Desce um semitom somente no intervalo selecionado.");
        setMouseCursor(juce::MouseCursor::CrosshairCursor);startTimerHz(20);refresh();
    }
    ~ManualPanel()override{stopTimer();doc.capturing=false;doc.drain();}
    void timerCallback()override{refresh();}
    void resized()override{
        capture.setBounds(12,9,108,28);fit.setBounds(128,9,92,28);zoomIn.setBounds(228,9,72,28);zoomOut.setBounds(308,9,72,28);
        down.setBounds(469,9,120,28);up.setBounds(597,9,120,28);remove.setBounds(725,9,104,28);undoButton.setBounds(837,9,126,28);
    }
    void mouseDown(const juce::MouseEvent& e)override{if(!plot().contains(e.position)||points.empty())return;anchor=timeAt(e.position.x);selectionStart=selectionEnd=anchor;selecting=true;hasSelection=false;repaint();}
    void mouseDrag(const juce::MouseEvent& e)override{if(!selecting)return;double t=timeAt(e.position.x);selectionStart=std::min(anchor,t);selectionEnd=std::max(anchor,t);hasSelection=selectionEnd-selectionStart>=0.025;repaint();}
    void mouseUp(const juce::MouseEvent& e)override{
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
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails& wheel)override{autoFit=false;double delta=(viewEnd-viewStart)*(-wheel.deltaY-wheel.deltaX)*0.4;viewStart+=delta;viewEnd+=delta;repaint();}
    void paint(juce::Graphics& g)override{
        auto bounds=getLocalBounds().toFloat();g.setGradientFill(juce::ColourGradient(juce::Colour(0xff182335),0,0,juce::Colour(0xff101a29),bounds.getRight(),bounds.getBottom(),false));g.fillRoundedRectangle(bounds,14);g.setColour(juce::Colour(0xff2b3950));g.drawRoundedRectangle(bounds.reduced(0.5f),14,1);
        auto r=plot();float lo=127,hi=0;for(const auto& p:points)if(p.time>=viewStart&&p.time<=viewEnd&&p.midi>=0){lo=std::min(lo,p.midi-2);hi=std::max(hi,p.midi+2);}if(hi<=lo){lo=48;hi=72;}else if(hi-lo<8){float centre=(lo+hi)*0.5f;lo=centre-4;hi=centre+4;}
        auto yAt=[&](float midi){return r.getBottom()-(midi-lo)/(hi-lo)*r.getHeight();};
        g.setColour(juce::Colour(0xff26354a));for(int i=int(std::ceil(lo));i<=int(hi);++i){g.drawHorizontalLine(int(yAt(float(i))),r.getX(),r.getRight());}
        g.setFont(juce::FontOptions(9));for(int i=int(std::ceil(lo));i<=int(hi);++i)if(i%12==0){g.setColour(fine::muted());g.drawText("C"+juce::String(i/12-1),4,int(yAt(float(i)))-6,38,12,juce::Justification::centredRight);}
        for(int i=0;i<=4;++i){float x=r.getX()+r.getWidth()*i/4;g.setColour(juce::Colour(0xff2b3c51));g.drawVerticalLine(int(x),r.getY(),r.getBottom());g.setColour(fine::muted());g.drawText(label(viewStart+(viewEnd-viewStart)*i/4),int(x)-27,int(r.getBottom()+3),54,15,juce::Justification::centred);}
        {juce::Graphics::ScopedSaveState clip(g);g.reduceClipRegion(r.toNearestInt());
            for(const auto& edit:edits){float x=xAt(edit.start),w=xAt(edit.end)-x;g.setColour(juce::Colour(0xff48e3ac).withAlpha(0.09f));g.fillRect(x,r.getY(),w,r.getHeight());g.setColour(juce::Colour(0xff48e3ac));g.drawText((edit.semitones>=0?"+":"")+juce::String(edit.semitones)+" st",int(x)+3,59,int(w)-6,18,juce::Justification::centredLeft);}
            juce::Path original,changed;bool pen=false;double prev=-100;
            for(const auto& p:points){if(p.time<viewStart||p.time>viewEnd||p.midi<0){pen=false;continue;}float x=xAt(p.time),y=yAt(p.midi),shift=0;for(const auto& e:edits)if(p.time>=e.start&&p.time<e.end){shift=float(e.semitones);break;}float corrected=yAt(p.midi+shift);
                if(pen&&p.time-prev<0.12){original.lineTo(x,y);changed.lineTo(x,corrected);}else{original.startNewSubPath(x,y);changed.startNewSubPath(x,corrected);}pen=true;prev=p.time;
            }g.setColour(fine::blue());g.strokePath(original,juce::PathStrokeType(1.8f));g.setColour(juce::Colour(0xff48e3ac));g.strokePath(changed,juce::PathStrokeType(1.4f));
            if(hasSelection){g.setColour(fine::blue().withAlpha(0.12f));g.fillRect(xAt(selectionStart),r.getY(),xAt(selectionEnd)-xAt(selectionStart),r.getHeight());g.setColour(fine::blue());g.drawRect(xAt(selectionStart),r.getY(),xAt(selectionEnd)-xAt(selectionStart),r.getHeight(),1.0f);}
            float cursor=xAt(doc.cursor.load());g.setColour(juce::Colours::white.withAlpha(0.5f));g.drawVerticalLine(int(cursor),r.getY(),r.getBottom());
        }
        g.setFont(juce::FontOptions(11));g.setColour(fine::muted());juce::String status;
        if(points.empty())status=doc.capturing.load()?(doc.timelineAvailable.load()?"Reproduza a voz na DAW para capturar as notas.":"Aguardando a posicao de reproducao da DAW."):"Capturar > reproduzir na DAW > clicar numa nota ou arrastar um trecho > -1 / +1 semitom.";
        else if(hasSelection)status=label(selectionStart)+" a "+label(selectionEnd)+"  |  "+juce::String(doc.offsetAt((selectionStart+selectionEnd)/2))+" st (previsto)  |  Ajuste localizado; reproduza para ouvir.";
        else status="Clique numa nota ou arraste um intervalo. Verde no mapa = destino manual previsto; monitore o audio ao reproduzir.";
        g.drawText(status,14,191,getWidth()-28,17,juce::Justification::centredLeft);
    }
};
