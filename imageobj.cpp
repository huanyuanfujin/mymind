#include "imageobj.h"
#include "mapobj.h"

/////////////////////////////////////////////////////////////////
// ImageObj	
/////////////////////////////////////////////////////////////////
ImageObj::ImageObj( QGraphicsItem *parent) : QGraphicsPixmapItem (parent )
{
//  qDebug() << "Const ImageObj (scene)";

    setShapeMode (QGraphicsPixmapItem::BoundingRectShape);
    setZValue(dZ_FLOATIMG);	
    hide();
}

ImageObj::~ImageObj()
{
 //  qDebug() << "Destr ImageObj";
}

void ImageObj::copy(ImageObj* other)
{
    prepareGeometryChange();
    setVisibility (other->isVisible() );
    setPixmap (other->QGraphicsPixmapItem::pixmap());	
    setPos (other->pos());
}

void ImageObj::setVisibility (bool v)
{
    if (v)
        show();
    else
        hide();
}

void ImageObj::save(const QString &fn, const char *format)
{
    pixmap().save (fn,format,100);
}

bool ImageObj::load (const QString &fn)
{
    QPixmap pixmap;
    if (pixmap.load (fn))
    {
        prepareGeometryChange();
        setPixmap (pixmap);
        return true;
    }
    return false;
}

bool ImageObj::load (const QPixmap &pm)
{
    prepareGeometryChange();
    setPixmap (pm);
    return true;
}


