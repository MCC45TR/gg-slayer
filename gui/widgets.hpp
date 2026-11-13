// SPDX-License-Identifier: MIT
#pragma once
#include "backend.hpp"
#include <QAbstractButton>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <functional>

namespace SlayerUi {
using namespace Qt::Literals::StringLiterals;

inline QColor blend(const QColor& a,const QColor& b,qreal weight) {
    return QColor::fromRgbF(a.redF()*weight+b.redF()*(1-weight),
                           a.greenF()*weight+b.greenF()*(1-weight),
                           a.blueF()*weight+b.blueF()*(1-weight));
}
inline QColor ink(const QColor& background){
    const auto linear=[](qreal c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
    const auto luminance=.2126*linear(background.redF())+.7152*linear(background.greenF())+.0722*linear(background.blueF());
    return luminance>.179?QColor(Qt::black):QColor(Qt::white);
}
inline QLabel* label(const QString& text,bool muted=false) {
    auto* item=new QLabel(text);
    item->setTextFormat(Qt::PlainText);
    item->setWordWrap(true);
    if(muted)item->setForegroundRole(QPalette::PlaceholderText);
    return item;
}
inline void heading(QLabel* item,int extra=0) {
    auto font=item->font();font.setPointSizeF(font.pointSizeF()+extra);
    font.setWeight(QFont::DemiBold);item->setFont(font);
}
class ThemeIcon final:public QWidget {
    QString name;
protected:
    void paintEvent(QPaintEvent*)override{
        QPainter painter(this);QIcon::fromTheme(name).paint(&painter,rect());
    }
public:
    ThemeIcon(const QString& value,int size):name(value){setFixedSize(size,size);}
};
inline QWidget* icon(const QString& name,int size=22) {
    return new ThemeIcon(name,size);
}

// Native controls wrap at narrow widths; no fixed-width action strips.
class FlowLayout final:public QLayout {
    QList<QLayoutItem*> items;
    int arrange(const QRect& rect,bool measure)const {
        int left,top,right,bottom;getContentsMargins(&left,&top,&right,&bottom);
        const auto area=rect.adjusted(left,top,-right,-bottom);
        int x=area.x(),y=area.y(),line=0;
        for(auto* item:items){
            if(item->isEmpty())continue;
            auto size=item->sizeHint().expandedTo(item->minimumSize());
            size.setWidth(std::min(size.width(),area.width()));
            if(x>area.x()&&x+size.width()>area.right()+1){x=area.x();y+=line+spacing();line=0;}
            if(!measure)item->setGeometry(QRect(QPoint(x,y),size));
            x+=size.width()+spacing();line=std::max(line,size.height());
        }
        return y+line-area.y()+top+bottom;
    }
public:
    explicit FlowLayout(QWidget* parent):QLayout(parent){setContentsMargins(0,0,0,0);setSpacing(8);}
    ~FlowLayout()override{while(auto* item=takeAt(0))delete item;}
    void addItem(QLayoutItem* item)override{items.append(item);}
    int count()const override{return items.size();}
    QLayoutItem* itemAt(int index)const override{return items.value(index);}
    QLayoutItem* takeAt(int index)override{return index>=0&&index<items.size()?items.takeAt(index):nullptr;}
    Qt::Orientations expandingDirections()const override{return {};}
    bool hasHeightForWidth()const override{return true;}
    int heightForWidth(int width)const override{return arrange(QRect(0,0,width,0),true);}
    QSize minimumSize()const override{
        QSize size;for(auto* item:items)size=size.expandedTo(item->minimumSize());
        return size+QSize(contentsMargins().left()+contentsMargins().right(),contentsMargins().top()+contentsMargins().bottom());
    }
    QSize sizeHint()const override{return minimumSize();}
    void setGeometry(const QRect& rect)override{QLayout::setGeometry(rect);arrange(rect,false);}
};

inline QWidget* actionRow(const QList<QWidget*>& widgets){
    auto* row=new QWidget;auto* layout=new FlowLayout(row);
    for(auto* widget:widgets)layout->addWidget(widget);
    return row;
}

class Card final:public QFrame {
public:
    QVBoxLayout* content;
    Card(const QString& title,const QString& subtitle={},const QString& symbol={}):content(new QVBoxLayout(this)) {
        setObjectName(u"card"_s);content->setContentsMargins(18,18,18,18);content->setSpacing(12);
        if(!title.isEmpty()){
            auto* header=new QHBoxLayout;header->setSpacing(10);
            if(!symbol.isEmpty())header->addWidget(icon(symbol),0,Qt::AlignTop);
            auto* words=new QVBoxLayout;words->setSpacing(4);
            auto* name=label(title);heading(name,1);words->addWidget(name);
            if(!subtitle.isEmpty())words->addWidget(label(subtitle,true));
            header->addLayout(words,1);content->addLayout(header);
        }
    }
};

class Disclosure final:public QWidget {
public:
    QVBoxLayout* content;
    Disclosure(const QString& title){
        auto* layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(12);
        auto* toggle=new QToolButton;toggle->setText(title);toggle->setCheckable(true);
        toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);toggle->setArrowType(Qt::RightArrow);
        toggle->setAutoRaise(true);toggle->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
        auto* body=new QWidget;body->hide();content=new QVBoxLayout(body);content->setContentsMargins(0,0,0,0);content->setSpacing(12);
        layout->addWidget(toggle);layout->addWidget(body);
        connect(toggle,&QToolButton::toggled,this,[toggle,body](bool shown){
            toggle->setArrowType(shown?Qt::DownArrow:Qt::RightArrow);body->setVisible(shown);
        });
    }
};

class ResponsiveCards final:public QWidget {
    QWidget *first,*second;
    QGridLayout* grid;
    bool wide=false;
protected:
    void resizeEvent(QResizeEvent*)override{
        const bool columns=width()>=760;if(columns==wide)return;wide=columns;
        grid->removeWidget(first);grid->removeWidget(second);
        grid->addWidget(first,0,0,Qt::AlignTop);grid->addWidget(second,wide?0:1,wide?1:0,Qt::AlignTop);
        grid->setColumnStretch(0,1);grid->setColumnStretch(1,wide?1:0);
    }
public:
    ResponsiveCards(QWidget* a,QWidget* b):first(a),second(b),grid(new QGridLayout(this)){
        grid->setContentsMargins(0,0,0,0);grid->setSpacing(14);
        grid->addWidget(first,0,0,Qt::AlignTop);grid->addWidget(second,1,0,Qt::AlignTop);
        grid->setColumnStretch(0,1);
    }
};

class Swatch final:public QAbstractButton {
    QColor color;
public:
    explicit Swatch(const QColor& value):color(value){
        setFixedSize(32,32);setCheckable(true);setCursor(Qt::PointingHandCursor);
        setToolTip(color.name().toUpper());setAccessibleName(u"Renk "_s+color.name());
    }
    QColor value()const{return color;}
    void paintEvent(QPaintEvent*)override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(isChecked()||hasFocus()?palette().color(QPalette::Highlight):palette().color(QPalette::Mid),isChecked()?2:1));
        p.setBrush(color);p.drawEllipse(QRectF(4,4,24,24));
        if(isChecked()){
            p.setPen(QPen(ink(color),2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            p.drawPolyline(QPolygonF{QPointF(10,16),QPointF(14,20),QPointF(22,12)});
        }
    }
};

class Keyboard final:public QWidget {
    struct Key{QString id,label,shift,altgr,shape;int led,scan;bool calibrated,hasRgb;QColor rgb;QRectF units,rect;QPainterPath path;};
    SlayerBackend& backend;
    QList<Key> keys;
    QString selected=u"W"_s;
    int hovered=-1;
    bool physicalSelection=false;
    QColor preview=QColor(u"#3daee9"_s);
    std::function<void(QString,int)> clicked;
    void geometry(){
        const qreal unit=(width()-24)/21.0;
        for(auto& key:keys){
            key.rect=QRectF(12+key.units.x()*unit+2,10+key.units.y()*37+2,key.units.width()*unit-4,key.units.height()*37-4);
            key.path={};const auto& r=key.rect;
            if(key.shape==u"iso-enter"){
                key.path.moveTo(r.topLeft());key.path.lineTo(r.topRight());key.path.lineTo(r.bottomRight());
                key.path.lineTo(r.left()+unit*.25,r.bottom());key.path.lineTo(r.left()+unit*.25,r.top()+35);
                key.path.lineTo(r.left(),r.top()+35);key.path.closeSubpath();
            }else key.path.addRoundedRect(r,4,4);
        }
    }
    void choose(int index){
        if(index<0||index>=keys.size())return;
        selected=keys[index].id;clicked(selected,keys[index].led);
        setAccessibleDescription(u"Seçili tuş: "_s+keys[index].label);update();
    }
    int hit(const QPointF& point)const{for(int i=0;i<keys.size();++i)if(keys[i].path.contains(point))return i;return -1;}
protected:
    void resizeEvent(QResizeEvent*)override{geometry();}
    void paintEvent(QPaintEvent*)override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        const auto base=palette().color(QPalette::Base),text=palette().color(QPalette::Text);
        p.setPen(Qt::NoPen);p.setBrush(blend(text,base,.035));p.drawRoundedRect(QRectF(rect()).adjusted(0,0,-1,-1),8,8);
        const int fontSize=qBound(9,(width()-20)/65,12);auto font=this->font();font.setPixelSize(fontSize);
        for(int i=0;i<keys.size();++i){
            const auto& key=keys[i];const bool active=key.id==selected;
            const auto color=active?preview:key.hasRgb?key.rgb:base;
            const auto fill=active||key.hasRgb?blend(color,base,active?.42:.28):blend(text,base,i==hovered?.10:.055);
            p.setBrush(fill);p.setPen(QPen(active?preview:blend(text,base,key.calibrated?.28:.15),active?2:1));p.drawPath(key.path);
            p.setPen(blend(text,base,.94));p.setFont(font);p.drawText(key.rect.adjusted(2,1,-2,-1),Qt::AlignCenter,key.label);
            auto small=font;small.setPixelSize(qMax(7,fontSize-3));p.setFont(small);p.setPen(blend(text,base,.65));
            if(!key.shift.isEmpty()&&key.shift.compare(key.label,Qt::CaseInsensitive)!=0)p.drawText(key.rect.adjusted(4,0,-4,-1),Qt::AlignTop|Qt::AlignLeft,key.shift);
            if(!key.altgr.isEmpty()&&key.altgr!=key.label)p.drawText(key.rect.adjusted(4,0,-4,-1),Qt::AlignBottom|Qt::AlignRight,key.altgr);
            if(key.hasRgb){p.save();p.setClipPath(key.path);p.setPen(QPen(key.rgb,2));p.drawLine(key.rect.bottomLeft()+QPointF(3,-2),key.rect.bottomRight()+QPointF(-3,-2));p.restore();}
        }
        if(hasFocus()){
            p.setPen(QPen(palette().color(QPalette::Highlight),1,Qt::DashLine));p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRectF(rect()).adjusted(1,1,-2,-2),7,7);
        }
    }
    void mousePressEvent(QMouseEvent* event)override{setFocus(Qt::MouseFocusReason);choose(hit(event->position()));}
    void mouseMoveEvent(QMouseEvent* event)override{
        const int index=hit(event->position());if(index==hovered)return;hovered=index;update();
        if(index<0){QToolTip::hideText();return;}
        const auto& key=keys[index];QString tip=key.label+u" · LED "_s+QString::number(key.led)+(key.calibrated?u" · Eşleme doğrulandı"_s:u" · LED eşlemesi doğrulanmalı"_s);
        if(!key.shift.isEmpty())tip+=u"\nShift: "_s+key.shift;
        if(!key.altgr.isEmpty())tip+=u" · AltGr: "_s+key.altgr;
        if(key.hasRgb)tip+=u"\nStatik renk: "_s+key.rgb.name();
        QToolTip::showText(event->globalPosition().toPoint(),tip,this);
    }
    void leaveEvent(QEvent*)override{hovered=-1;QToolTip::hideText();update();}
    void keyPressEvent(QKeyEvent* event)override{
        if(physicalSelection&&event->nativeScanCode()){
            for(int i=0;i<keys.size();++i)if(static_cast<quint32>(keys[i].scan)==event->nativeScanCode()){choose(i);event->accept();return;}
        }
        const auto found=std::find_if(keys.begin(),keys.end(),[this](const Key& key){return key.id==selected;});
        if(found==keys.end()){QWidget::keyPressEvent(event);return;}
        int dx=0,dy=0;
        switch(event->key()){case Qt::Key_Left:dx=-1;break;case Qt::Key_Right:dx=1;break;case Qt::Key_Up:dy=-1;break;case Qt::Key_Down:dy=1;break;default:QWidget::keyPressEvent(event);return;}
        int best=-1;qreal score=1e9;const auto center=found->rect.center();
        for(int i=0;i<keys.size();++i){
            const auto delta=keys[i].rect.center()-center;
            if((dx&&delta.x()*dx<1)||(dy&&delta.y()*dy<1))continue;
            const auto cost=dx?qAbs(delta.x())+qAbs(delta.y())*5:qAbs(delta.y())+qAbs(delta.x())*2;
            if(cost<score){score=cost;best=i;}
        }
        choose(best);event->accept();
    }
public:
    Keyboard(SlayerBackend& value,std::function<void(QString,int)> callback):backend(value),clicked(std::move(callback)){
        setObjectName(u"keyboardPreview"_s);setMinimumSize(460,255);setFocusPolicy(Qt::StrongFocus);setMouseTracking(true);
        setAccessibleName(u"Sistem düzeninde fiziksel klavye"_s);
        connect(&backend,&SlayerBackend::dataChanged,this,[this]{
            keys.clear();for(const auto& entry:backend.keys()){
                const auto k=entry.toMap();if(k.contains(u"present"_s)&&!k.value(u"present"_s).toBool())continue;
                const auto rgb=k.value(u"rgb"_s).toList();const bool hasColor=rgb.size()==3;
                const auto color=hasColor?QColor(rgb[0].toInt(),rgb[1].toInt(),rgb[2].toInt()):QColor();
                keys.append({k.value(u"id"_s).toString(),k.value(u"label"_s).toString(),k.value(u"shift"_s).toString(),k.value(u"altgr"_s).toString(),k.value(u"shape"_s).toString(),k.value(u"led"_s).toInt(),k.value(u"scan"_s).toInt(),k.value(u"calibrated"_s).toBool(),hasColor,color,QRectF(k.value(u"x"_s).toDouble(),k.value(u"y"_s).toDouble(),k.value(u"w"_s).toDouble(),k.value(u"h"_s).toDouble()),{}, {}});
            }
            geometry();update();
        });
    }
    void color(const QColor& value){preview=value;update();}
    void capturePhysical(bool enabled){physicalSelection=enabled;if(enabled)setFocus(Qt::OtherFocusReason);}
};

// A small palette-aware diagram; repaint only when a selection changes.
class GraphicsDiagram final:public QWidget {
    QString mode=u"hybrid"_s;
protected:
    void paintEvent(QPaintEvent*)override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        const auto text=palette().color(QPalette::Text),base=palette().color(QPalette::Base),accent=palette().color(QPalette::Highlight);
        const qreal middle=width()/2.0;const QRectF display(middle-43,8,86,45);
        p.setPen(QPen(blend(text,base,.45),2));p.setBrush(blend(accent,base,.08));p.drawRoundedRect(display,4,4);
        p.drawLine(QPointF(middle,53),QPointF(middle,61));p.drawLine(QPointF(middle-16,61),QPointF(middle+16,61));
        for(int i=0;i<2;++i){
            const bool active=mode==u"hybrid"||(i==0?mode==u"integrated":mode==u"dedicated");
            const qreal x=middle+(i?112:-112);const QRectF chip(x-64,105,128,50);
            p.setPen(QPen(active?accent:blend(text,base,.16),2,active?Qt::SolidLine:Qt::DashLine));
            p.drawLine(QPointF(middle,69),QPointF(x,95));p.setBrush(active?blend(accent,base,.10):base);p.drawRoundedRect(chip,6,6);
            p.setPen(active?text:blend(text,base,.5));p.drawText(chip,Qt::AlignCenter,i?u"NVIDIA"_s:u"AMD iGPU"_s);
        }
    }
public:
    GraphicsDiagram(){setObjectName(u"graphicsDiagram"_s);setMinimumSize(400,170);setAccessibleName(u"Grafik modu önizlemesi"_s);}
    void setMode(const QString& value){if(mode!=value){mode=value;update();}}
};
}
