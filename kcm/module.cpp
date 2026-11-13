// SPDX-License-Identifier: MIT
#include "module.hpp"
#include <KPluginFactory>
K_PLUGIN_CLASS_WITH_JSON(SlayerModule, "kcm_slayerctl.json")
SlayerModule::SlayerModule(QObject*parent,const KPluginMetaData&data,const QVariantList&)
    :KQuickConfigModule(parent,data),m_backend(this){
    setButtons(NoAdditionalButton);
    connect(&m_backend,&SlayerBackend::updated,this,&SlayerModule::updated);
    connect(&m_backend,&SlayerBackend::dataChanged,this,&SlayerModule::dataChanged);
}
void SlayerModule::refresh(){m_backend.refresh();}
void SlayerModule::run(const QStringList&a,bool p){m_backend.run(a,p);}
void SlayerModule::openSettings(const QString&m){m_backend.openSettings(m);}
bool SlayerModule::ready(const QString&i)const{return m_backend.ready(i);}
QString SlayerModule::reason(const QString&i)const{return m_backend.reason(i);}
#include "module.moc"
