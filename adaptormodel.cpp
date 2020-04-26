#include "adaptormodel.h"
#include <QtCore/QMetaObject>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include "branchitem.h"
#include "mainwindow.h"
#include "vymmodel.h"

extern QString vymInstanceName;
extern Main *mainWindow;

AdaptorModel::AdaptorModel(QObject *obj)
         : QDBusAbstractAdaptor(obj)
{
    model=static_cast <VymModel*> (obj);
    setAutoRelaySignals (true);
}

void AdaptorModel::setModel(VymModel *vm)
{
    model=vm;
}

QString AdaptorModel::caption()
{
    return m_caption;
}

void AdaptorModel::setCaption (const QString &newCaption)
{
    m_caption=newCaption;
}

QDBusVariant AdaptorModel::getCurrentModelID()
{
    return QDBusVariant (mainWindow->currentModelID());
}

QDBusVariant AdaptorModel::branchCount()
{
    BranchItem *selbi=model->getSelectedBranch();
    if (selbi) 
	return QDBusVariant (selbi->branchCount() );
    else	
	return QDBusVariant (-1 );
}

QDBusVariant AdaptorModel::execute (const QString &s)
{
    return QDBusVariant (model->execute (s));
}

QDBusVariant AdaptorModel::errorLevel()
{
    return QDBusVariant (model->parser.errorLevel() );
}

QDBusVariant AdaptorModel::errorDescription()
{
    return QDBusVariant (model->parser.errorDescription() );
}

QDBusVariant AdaptorModel::listCommands ()
{
    return QDBusVariant (model->parser.getCommands().join(",") );
}

