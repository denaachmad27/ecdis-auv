#ifndef IPLUGININTERFACE_H
#define IPLUGININTERFACE_H

#include <QObject>
#include <QString>

class IPluginInterface {
public:
    virtual ~IPluginInterface() {}
    virtual QString pluginName() const = 0;
    virtual void showWindow() = 0;  // 👈 Tambahan
};

#define IPluginInterface_iid "org.example.PluginInterface"
Q_DECLARE_INTERFACE(IPluginInterface, IPluginInterface_iid)

#endif // IPLUGININTERFACE_H
