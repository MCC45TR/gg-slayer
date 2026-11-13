// SPDX-License-Identifier: MIT
#include "session.hpp"
#include <QCoreApplication>
int main(int argc,char** argv){QCoreApplication app(argc,argv);auto args=app.arguments();args.removeFirst();return desktopPower(args);}
