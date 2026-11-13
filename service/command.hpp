// SPDX-License-Identifier: MIT
#pragma once
#include <QStringList>
#include <QVariantMap>
struct CommandResult {bool ok;QString output;};
CommandResult ctlCommand(const QStringList& args,bool privileged=false);
bool allowedCommand(const QStringList& args);
