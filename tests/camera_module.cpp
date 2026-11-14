// SPDX-License-Identifier: MIT
#include <QCoreApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QDebug>
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    if(argc!=2)return 1;
    QQmlEngine engine;engine.addImportPath(QString::fromLocal8Bit(argv[1]));
    QQmlComponent component(&engine);
    component.setData("import QtQml\nimport org.gamegaraj.slayerctl.camera\nQtObject { property int cameraState: CameraControl.state }",QUrl());
    if(!component.isReady()){qWarning()<<component.errors();return 2;}
    // Load the actual binary QML plugin without instantiating the singleton:
    // this test neither reads hardware nor creates a privileged request.
    return 0;
}
