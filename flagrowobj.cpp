#include <QDebug>
#include <QToolBar>

#include "flag.h"
#include "flagrowobj.h"

#include "geometry.h"

/////////////////////////////////////////////////////////////////
// FlagRowObj
/////////////////////////////////////////////////////////////////
FlagRowObj::FlagRowObj()
{
//    qDebug() << "Const FlagRowObj ()";
    init ();
}

FlagRowObj::FlagRowObj(QGraphicsItem *parent):MapObj(parent) 
{
//    qDebug() << "Const FlagRowObj (p)";
    init ();
}

FlagRowObj::~FlagRowObj()
{
    //qDebug() << "Destr FlagRowObj";
    while (!flag.isEmpty())
	delete (flag.takeFirst() );
}

void FlagRowObj::init ()
{
    showFlags=true;
}

void FlagRowObj::copy (FlagRowObj* other)
{
    MapObj::copy(other);
    flag.clear();
    for (int i=0; i<flag.size(); ++i)
	addFlag (flag.at(i));
}

void FlagRowObj::move(double x, double y)
{
    MapObj::move(x,y);
    qreal dx=0;
    for (int i=0; i<flag.size(); ++i)
    {
	flag.at(i)->move(x+dx,y);
	dx+=QSizeF(flag.at(i)->getSize() ).width();
    }
}

void FlagRowObj::moveBy(double x, double y)
{
    move (x+absPos.x(),y+absPos.y() );
}

void FlagRowObj::setZValue (double z)
{
    for (int i=0; i<flag.size(); ++i)
	flag.at(i)->setZValue (z);
}

void FlagRowObj::setVisibility (bool v)
{
    MapObj::setVisibility(v);
    for (int i=0; i<flag.size(); ++i)
	flag.at(i)->setVisibility (v);
}

FlagObj* FlagRowObj::addFlag (FlagObj *fo)
{
    FlagObj *newfo=new FlagObj (parentItem() );
    newfo->copy (fo);	// create a deep copy of fo
    newfo->move (absPos.x() + bbox.width(), absPos.y() );
    flag.append(newfo);
    calcBBoxSize();
    positionBBox();
    return newfo;
}

QStringList FlagRowObj::activeFlagNames()
{
    QStringList list;
    for (int i=0; i<flag.size(); ++i)
	list.append (flag.at(i)->getName());
    return list;
}

void FlagRowObj::positionBBox()
{
    bbox.moveTopLeft(absPos );
    clickPoly=QPolygonF (bbox);
}

void FlagRowObj::calcBBoxSize()
{
    QSizeF size(0,0);
    QSizeF boxsize(0,0);
    for (int i=0; i<flag.size(); ++i)
    {
	size=flag.at(i)->getSize();
	// add widths
	boxsize.setWidth(boxsize.width() + size.width() );
	// maximize height
	if (size.height() > boxsize.height() ) 
	    boxsize.setHeight(size.height() );
    }
    bbox.setSize (boxsize);
    clickPoly=QPolygonF (bbox);
}

QString FlagRowObj::getFlagName (const QPointF &p)
{
    if (!isInBox (p,clickPoly.boundingRect() )) return "";
    for (int i=0; i<flag.size(); ++i)
	if (isInBox (p,flag.at(i)->getClickPoly().boundingRect() )) return flag.at(i)->getName();
    return "";	
}

bool FlagRowObj::isActive (const QString &foname)
{
    FlagObj *fo=findFlag (foname);
    if (fo) 
	return true;
    else
	return false;
}

void FlagRowObj::activate (Flag *flag)	
{
    if (flag) 
    {
	FlagObj *fo=addFlag (new FlagObj (this));
	fo->load (flag->getPixmap() );
	fo->setName (flag->getName() );
	fo->activate();
	if (showFlags)	// necessary? only called from FIO::init
	    fo->setVisibility (visible);
	else
	    fo->setVisibility (false);
	calcBBoxSize();
    }
}

void FlagRowObj::deactivate (const QString &foname)
{
    FlagObj *fo=findFlag (foname);
    if (fo) 
    {
	flag.removeAll(fo);
	delete (fo);
    }	
    calcBBoxSize();
    positionBBox();
}

void FlagRowObj::setShowFlags (bool b)
{
    showFlags=b;
}

FlagObj* FlagRowObj::findFlag (const QString &name)
{
    for (int i=0; i<flag.size(); ++i)
	if (flag.at(i)->getName()==name) return flag.at(i);
    return NULL;
}

