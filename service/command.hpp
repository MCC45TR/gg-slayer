// SPDX-License-Identifier: MIT
#pragma once
#include <QStringList>
#include <QVariantMap>
#include <functional>
struct CommandResult {bool ok;QString output;};
CommandResult ctlCommand(const QStringList& args,bool privileged=false);
bool allowedCommand(const QStringList& args);
using CommandExecutor=std::function<CommandResult(const QStringList&,bool)>;
CommandResult applyCommand(const QStringList& command,const QStringList& remember,const CommandExecutor& execute=ctlCommand);
