#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
namespace fine {
inline juce::Colour ink(){return juce::Colour(0xffecf3fc);}
inline juce::Colour muted(){return juce::Colour(0xff8291a8);}
inline juce::Colour blue(){return juce::Colour(0xff39aaff);}
class Look final : public juce::LookAndFeel_V4 {
public:
    Look(){
        setColour(juce::ComboBox::textColourId,ink());
        setColour(juce::PopupMenu::backgroundColourId,juce::Colour(0xff151e2d));
        setColour(juce::PopupMenu::textColourId,ink());
        setColour(juce::PopupMenu::highlightedBackgroundColourId,juce::Colour(0xff16649a));
        setColour(juce::TextEditor::textColourId,ink());
        setColour(juce::TextEditor::backgroundColourId,juce::Colour(0xff0d1521));
        setColour(juce::TextEditor::highlightColourId,blue().withAlpha(0.35f));
        setColour(juce::Slider::textBoxTextColourId,ink());
        setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxHighlightColourId,blue().withAlpha(0.3f));
        setColour(juce::TooltipWindow::backgroundColourId,juce::Colour(0xff17263a));
        setColour(juce::TooltipWindow::textColourId,ink());
        setColour(juce::TooltipWindow::outlineColourId,blue().withAlpha(0.3f));
    }
    juce::Label* createSliderTextBox(juce::Slider& s)override{auto* l=LookAndFeel_V4::createSliderTextBox(s);l->setFont(juce::FontOptions(22).withStyle("Bold"));l->setJustificationType(juce::Justification::centred);return l;}
    void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float value,float begin,float end,juce::Slider&)override{
        auto r=juce::Rectangle<float>(float(x),float(y),float(w),float(h)).reduced(10);
        float radius=std::min(r.getWidth(),r.getHeight())*0.5f;auto c=r.getCentre();
        float angle=begin+value*(end-begin);
        for(int i=0;i<=32;++i){float a=begin+(end-begin)*i/32;float outer=radius+5;float inner=outer-(i%4==0?4:2);g.setColour(i<=value*32?blue().withAlpha(0.65f):juce::Colour(0xff334057));g.drawLine(c.x+std::sin(a)*inner,c.y-std::cos(a)*inner,c.x+std::sin(a)*outer,c.y-std::cos(a)*outer,1);}
        auto arc=[&](float stop,juce::Colour colour,float thickness){juce::Path p;p.addCentredArc(c.x,c.y,radius-3,radius-3,0,begin,stop,true);g.setColour(colour);g.strokePath(p,juce::PathStrokeType(thickness,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));};
        arc(end,juce::Colour(0xff28364c),4);arc(angle,blue().withAlpha(0.1f),11);arc(angle,blue(),4);
        float body=radius-12;
        g.setColour(juce::Colours::black.withAlpha(0.45f));g.fillEllipse(c.x-body,c.y-body+4,body*2,body*2);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff34465e),c.x-body,c.y-body,juce::Colour(0xff111b29),c.x+body,c.y+body,false));g.fillEllipse(c.x-body,c.y-body,body*2,body*2);
        g.setColour(juce::Colours::white.withAlpha(0.12f));g.drawEllipse(c.x-body,c.y-body,body*2,body*2,1);
        g.setColour(juce::Colours::black.withAlpha(0.3f));g.drawEllipse(c.x-body+4,c.y-body+4,body*2-8,body*2-8,1);
        juce::Point<float> a{c.x+std::sin(angle)*(body-13),c.y-std::cos(angle)*(body-13)},b{c.x+std::sin(angle)*(body-5),c.y-std::cos(angle)*(body-5)};
        g.setColour(ink());g.drawLine({a,b},2.4f);
    }
    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour&,bool hover,bool down)override{
        auto r=b.getLocalBounds().toFloat().reduced(0.7f);bool on=b.getToggleState();
        auto top=on?juce::Colour(0xff197fbd):juce::Colour(0xff1b273a);auto bottom=on?juce::Colour(0xff15496e):juce::Colour(0xff121c2b);
        if(hover){top=top.brighter(0.12f);bottom=bottom.brighter(0.12f);}if(down)top=top.darker(0.15f);
        g.setGradientFill(juce::ColourGradient(top,0,0,bottom,0,r.getBottom(),false));g.fillRoundedRectangle(r,7);
        g.setColour(on?blue().withAlpha(0.65f):juce::Colour(0xff334156));g.drawRoundedRectangle(r,7,1);
        if(on){g.setColour(juce::Colour(0xff80d5ff));g.fillRoundedRectangle(r.getCentreX()-5,r.getBottom()-5,10,2,1);}
    }
    void drawButtonText(juce::Graphics& g,juce::TextButton& b,bool,bool)override{g.setColour(b.getToggleState()?ink():muted());g.setFont(juce::FontOptions(13).withStyle("Bold"));g.drawText(b.getButtonText(),b.getLocalBounds().reduced(1,0),juce::Justification::centred);}
    void drawComboBox(juce::Graphics& g,int w,int h,bool,int,int,int,int,juce::ComboBox&)override{
        auto r=juce::Rectangle<float>(0,0,float(w),float(h)).reduced(0.6f);g.setColour(juce::Colour(0xff101b2a));g.fillRoundedRectangle(r,7);g.setColour(juce::Colour(0xff334157));g.drawRoundedRectangle(r,7,1);
        juce::Path p;p.startNewSubPath(float(w-21),float(h/2-2));p.lineTo(float(w-16),float(h/2+3));p.lineTo(float(w-11),float(h/2-2));g.setColour(muted());g.strokePath(p,juce::PathStrokeType(1.5f));
    }
    juce::Font getComboBoxFont(juce::ComboBox&)override{return juce::Font(juce::FontOptions(14).withStyle("Bold"));}
};
}
