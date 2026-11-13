// SPDX-License-Identifier: MIT
#pragma once
#include "widgets.hpp"
#include <QImageReader>
#include <QFileInfo>
#include <QPainterPath>
#include <malloc.h>
namespace SlayerUi {
class LaptopIllustration final:public QWidget {
    QImage wallpaper;
    bool dark=false,loaded=false;
    void loadWallpaper(){
        dark=palette().color(QPalette::Window).lightness()<128;loaded=true;wallpaper={};
        const auto dir=QStringLiteral("/usr/share/wallpapers/Default/contents/")+(dark?QStringLiteral("images_dark/"):QStringLiteral("images/"));
        for(const auto& name:{QStringLiteral("1280x800.jxl"),QStringLiteral("1280x800.png"),QStringLiteral("1920x1200.jpg"),QStringLiteral("1920x1080.png")}){
            const QFileInfo file(dir+name);if(!file.isFile()||file.size()>32*1024*1024)continue;
            {
                QImageReader reader(file.absoluteFilePath());reader.setFormat(file.suffix().toLatin1());
                const auto size=reader.size();
                if(size.isEmpty()||size.width()>8192||size.height()>8192)continue;
                reader.setScaledSize(QSize(640,400));
                // ScaledSize is only a decoder hint; enforce the cache bound
                // even for plugins that return the full-resolution image.
                wallpaper=reader.read().scaled(640,400,Qt::KeepAspectRatio,Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB32);
                setProperty("wallpaperSize",wallpaper.size());
            }
            // JPEG XL can leave large, freed work buffers in glibc arenas.
            // Return those unused pages once the decoder has been destroyed.
            malloc_trim(0);
            if(!wallpaper.isNull()){setProperty("wallpaperLoaded",true);return;}

        }
        setProperty("wallpaperLoaded",false);
    }
protected:
    void paintEvent(QPaintEvent*)override{
        if(!loaded||dark!=(palette().color(QPalette::Window).lightness()<128))loadWallpaper();
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::SmoothPixmapTransform);
        const qreal scale=qMin(width()/500.0,height()/310.0);p.translate((width()-500*scale)/2,(height()-310*scale)/2);p.scale(scale,scale);
        const QColor body(64,69,75),edge(100,106,113),keyboard(30,34,39);
        p.setPen(QPen(edge,1.5));p.setBrush(body);p.drawRoundedRect(QRectF(84,9,332,221),8,8);
        const QRectF screen(92,20,316,197.5);p.setBrush(palette().color(QPalette::Base));p.setPen(Qt::NoPen);p.drawRect(screen);
        if(!wallpaper.isNull())p.drawImage(screen,wallpaper);
        p.setBrush(QColor(17,21,25));p.drawEllipse(QPointF(250,14),2,2);
        p.setPen(QColor(185,190,196));auto font=p.font();font.setPixelSize(5);font.setLetterSpacing(QFont::AbsoluteSpacing,1.5);p.setFont(font);p.drawText(QRectF(200,218,100,8),Qt::AlignCenter,QStringLiteral("SLAYER"));
        p.setPen(QPen(edge,1));p.setBrush(body);p.drawPolygon(QPolygonF{{84,231},{416,231},{472,294},{28,294}});
        p.setPen(Qt::NoPen);p.setBrush(keyboard);
        for(int row=0;row<5;++row)for(int key=0;key<19;++key){
            const qreal offset=(row==2?2:row==3?4:0);const qreal x=91+key*16.7-row*3+offset;
            p.drawRoundedRect(QRectF(x,238+row*6,13.7,4.1),.8,.8);
        }
        p.setPen(QPen(edge,1));p.setBrush(Qt::NoBrush);p.drawRoundedRect(QRectF(188,273,124,15),2,2);
        p.setBrush(QColor(39,43,48));p.setPen(Qt::NoPen);p.drawPolygon(QPolygonF{{28,294},{472,294},{467,299},{33,299}});
        p.setBrush(edge);p.drawRoundedRect(QRectF(220,294,60,2),1,1);
    }
public:
    LaptopIllustration(){setObjectName(QStringLiteral("laptopIllustration"));setMinimumHeight(210);setMaximumHeight(240);setAccessibleName(QStringLiteral("Açık Slayer laptop; ekranda Fedora'nın varsayılan duvar kâğıdı"));}
};
}
