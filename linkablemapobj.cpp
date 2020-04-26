#include <iostream>
#include <math.h>
#include <cstdlib>

#include "linkablemapobj.h"
#include "branchobj.h"
#include "vymmodel.h"

extern bool debug;

/////////////////////////////////////////////////////////////////
// LinkableMapObj
/////////////////////////////////////////////////////////////////

LinkableMapObj::LinkableMapObj(QGraphicsItem* parent, TreeItem *ti) :MapObj(parent,ti)
{
    //qDebug() << "Const LinkableMapObj this="<<this<<"  ti="<<ti<<"  treeItem="<<treeItem;
    parObj=(LinkableMapObj*)parent; //FIXME-4 try to get rid of parObj and use parentItem() instead
    init ();
}

LinkableMapObj::~LinkableMapObj()
{
    //qDebug()<< "Destructor LMO  this="<<this<<" style="<<style<<" l="<<l<<"  p="<<p<<"  segment="<<segment.count();
    delLink();
}

void LinkableMapObj::init ()
{
    parObjTmpBuf=NULL;
    tmpParent=false;
    parPos=QPointF(0,0);
    childRefPos=QPointF(0,0);
    floatRefPos=QPointF(0,0);
    link2ParPos=false;
    l=NULL;
    p=NULL;
    orientation=UndefinedOrientation;
    linkwidth=20;	
    thickness_start=8;
    style=UndefinedStyle;
    linkpos=Bottom;
    arcsegs=13;
    
// TODO instead of linkcolor pen.color() could be used	all around
    pen.setWidth (1);
    pen.setColor (linkcolor);
    pen.setCapStyle ( Qt::RoundCap );

    useBottomline=false;
    bottomline=NULL;

    topPad=botPad=leftPad=rightPad=0;

    repositionRequest=false;

    // Rel Positions
    relPos=QPointF(0,0);
    useRelPos=false;
}

void LinkableMapObj::createBottomLine()
{
    bottomline=scene()->addLine(QLineF(1,1,1,1),pen);
    bottomline->setZValue(dZ_LINK);
}

void LinkableMapObj::delLink()
{
    if (bottomline)
    {
	delete (bottomline);
	bottomline=NULL;
    }	
    switch (style)
    {
	case Line:
	    delete (l);
	    break;
	case Parabel:
	    while (!segment.isEmpty()) delete segment.takeFirst();
	    break;
	case PolyLine:
	    delete (p);
	    break;
	case PolyParabel:
	    delete (p);
	    break;
	default:
	    break;
    }	    
}

void LinkableMapObj::copy (LinkableMapObj* other)
{
    MapObj::copy(other);
    bboxTotal=other->bboxTotal;
    setLinkStyle(other->style);
    setLinkColor (other->linkcolor);
    relPos=other->relPos;
    treeItem=other->treeItem;
}

void LinkableMapObj::setParObj(LinkableMapObj* o)
{
    parObj=o;
    setParentItem (parObj);
}

void LinkableMapObj::setParObjTmp(LinkableMapObj*,QPointF,int)	
{
}

void LinkableMapObj::unsetParObjTmp()
{
}

bool LinkableMapObj::hasParObjTmp()
{
    return tmpParent;
}

void LinkableMapObj::setUseRelPos (const bool &b)
{
    useRelPos=b;
}

bool LinkableMapObj::getUseRelPos ()
{
    return useRelPos;
}

void LinkableMapObj::setRelPos()
{
    if (parObj)
	setRelPos (absPos - parObj->getChildRefPos() );
    else
	qWarning()<<"LMO::setRelPos parObj==0   this="<<this;
}

void LinkableMapObj::setRelPos(const QPointF &p)
{
    if (parObj)
    {	    
	relPos=p;
	useRelPos=true;
	setOrientation();
    }	else
	qWarning()<<"LMO::setRelPos (p)  parObj==0   this="<<this;
}

QPointF LinkableMapObj::getRelPos()
{
    if (!parObj) 
    {
	qWarning()<<"LMO::getRelPos parObj==0   this="<<this;
	return QPointF();
    }
    return relPos;
}

qreal LinkableMapObj::getTopPad()
{
    return topPad;
}

qreal LinkableMapObj::getLeftPad()
{
    return leftPad;
}

qreal LinkableMapObj::getRightPad()
{
    return rightPad;
}

LinkableMapObj::Style LinkableMapObj::getDefLinkStyle (TreeItem *parent)
{
    VymModel *model=treeItem->getModel();
    if (!model)
    {
	qWarning ("LMO::getDefLinkStyle   model=NULL");
	//return UndefinedStyle;
    }
    Style ls=model->getMapLinkStyle();
    int depth=1+parent->depth();
    if (depth==0) return UndefinedStyle;
    switch (ls)
    {
	case Line: 
	    return ls;
	    break;
	case Parabel:
	    return ls;
	    break;
	case PolyLine:	
	    if (depth>1)
		return Line;
	    else    
		return ls;
	    break;
	case PolyParabel:   
	    if (depth>1)
		return Parabel;
	    else    
		return ls;
	    break;
	default: 
	    break;  
    }	
    return UndefinedStyle;
}

void LinkableMapObj::setLinkStyle(Style newstyle)
{
    //qDebug()<<"LMO::setLinkStyle s="<<newstyle;	//FIXME-4 called very often?!?!
    //qDebug()<<"LMO::setLinkStyle s="<<newstyle<<" for "<<this<<" "<<treeItem->getHeading()<<"  parObj="<<parObj;
    delLink();
	
    style=newstyle;

    QGraphicsLineItem *cl;
    switch (style)
    {
	case Line: 
	    l = scene()->addLine(QLineF(1,1,1,1),pen);
	    l->setZValue(dZ_LINK);
	    if (visible)
		l->show();
	    else
		l->hide();
	    createBottomLine();
	    break;
	case Parabel:
	    for (int i=0;i<arcsegs;i++)
	    {
		cl = scene()->addLine(QLineF(i*5,0,i*10,100),pen);
		cl->setZValue(dZ_LINK);
		if (visible)
		    cl->show();
		else
		    cl->hide();
		segment.append(cl);
	    }
	    pa0.resize (arcsegs+1);
	    createBottomLine();
	    break;
	case PolyLine:  
	    p =scene()->addPolygon(QPolygonF(),pen,linkcolor);
	    p->setZValue(dZ_LINK);
	    if (visible)
		p->show();
	    else
		p->hide();
	    pa0.resize (3);
	    createBottomLine();
	    break;
	case PolyParabel:	
	    p = scene()->addPolygon(QPolygonF(),pen,linkcolor);
	    p->setZValue(dZ_LINK);
	    if (visible)
		p->show();
	    else
		p->hide();
	    pa0.resize (arcsegs*2+2);
	    pa1.resize (arcsegs+1);
	    pa2.resize (arcsegs+1);
	    createBottomLine();
	    break;
	default: 
	    break;	
    }   
}

LinkableMapObj::Style LinkableMapObj::getLinkStyle()
{
    return style;
}

void LinkableMapObj::setLinkPos(Position lp)
{
    linkpos=lp;
}

LinkableMapObj::Position LinkableMapObj::getLinkPos()
{
    return linkpos;
}

void LinkableMapObj::setLinkColor()
{
    // Overloaded in BranchObj and children
    // here only set default color
    VymModel *model=treeItem->getModel();
    if (model)
	setLinkColor (model->getMapDefLinkColor());
}

void LinkableMapObj::setLinkColor(QColor col)
{
    linkcolor=col;
    pen.setColor(col);
    if (bottomline) bottomline->setPen( pen );
    switch (style)
    {
	case Line:
	    l->setPen( pen);
	    break;  
	case Parabel:	
	    for (int i=0; i<segment.size(); ++i)
		segment.at(i)->setPen( pen);
	    break;
	case PolyLine:
	    p->setBrush( QBrush(col));
	    p->setPen( pen);
	    break;
	case PolyParabel:   
	    p->setBrush( QBrush(col));
	    p->setPen( pen);
	    break;
	default:
	    break;
    } 
}

QColor LinkableMapObj::getLinkColor()
{
    return linkcolor;
}

void LinkableMapObj::setVisibility (bool v)
{
    MapObj::setVisibility (v);
    updateVisibility();
}

void LinkableMapObj::setOrientation()
{
    if (!parObj)
    {
        orientation = UndefinedOrientation;
        return;
    }

    // calc orientation depending on position rel to parent
    if (useRelPos)
    {
        if (relPos.x() < 0)
            orientation = LeftOfCenter;
        else
            orientation = RightOfCenter;
    }  else
        // use the orientation of the parent:
        orientation = parObj->getOrientation();
}

void LinkableMapObj::updateVisibility()
{
    bool visnow=visible;

    // Hide links of unselected objects (if wanted)
    if (((MapItem*)treeItem)->getHideLinkUnselected()
	&& !treeItem->getModel()->isSelected(treeItem) )
	visnow=false;

    if (visnow) 
    {
	if (bottomline)
	{
	    if (useBottomline)
		bottomline->show();
	    else	
		bottomline->hide();
	}

	switch (style)
	{
	    case Line:
		if (l) l->show();
		break;
	    case Parabel:   
		for (int i=0; i<segment.size(); ++i)
		    segment.at(i)->show();
		break;	
	    case PolyLine:
		if (p) 
		    p->show();
		else 
		    qDebug()<< "LMO::updateVis p==0 (PolyLine)"; //FIXME-4
		break;
	    case PolyParabel:	
		if (p) 
		    p->show();
		else
            qDebug()<< "LMO::updateVis p==0 (PolyParabel) "<<treeItem->getHeadingPlain(); //FIXME-4
		break;
	    default:
		break;
	}
    } else 
    {
	if (bottomline) bottomline->hide();
	switch (style)
	{
	    case Line:
		if (l) l->hide();
		break;
	    case Parabel:   
		for (int i=0; i<segment.size(); ++i)
		    segment.at(i)->hide();
		break;	
	    case PolyLine:
		if (p) p->hide();
		break;
	    case PolyParabel:	
		if (p) p->hide();
		break;
	    default:
		break;
	}
    }	
}

void LinkableMapObj::updateLinkGeometry()
{
    // needs:
    //	childRefPos of parent
    //	orient   of parent
    //	style
    //
    // sets:
    //	orientation
    //	childRefPos    (by calling setDockPos())
    //	parPos	    (by calling setDockPos())
    //  bottomlineY
    //	drawing of the link itself

    // updateLinkGeometry is called from move, but called from constructor we don't
    // have parents yet...

    if (style == UndefinedStyle)
    {
        setDockPos();
        return;
    }

    switch (linkpos)
    {
        case Middle:
            bottomlineY = bbox.top() + bbox.height()/2;	// draw link to middle (of frame)
            break;
        case Bottom:
            //bottomlineY = bbox.bottom()-1;  // draw link to bottom of box
            bottomlineY = bbox.bottom()  - botPad;  
            break;
    }
    
    double p2x, p2y;				// Set P2 Before setting
    if (!link2ParPos)
    {
        p2x = QPointF( parObj->getChildRefPos() ).x();   // P1, we have to look at
        p2y = QPointF( parObj->getChildRefPos() ).y();   // orientation
    } else
    {
        p2x = QPointF( parObj->getParPos() ).x();
        p2y = QPointF( parObj->getParPos() ).y();
    }

    setOrientation();
    setDockPos(); // Call overloaded method

    double p1x = parPos.x();  // Link is drawn from P1 to P2
    double p1y = parPos.y();

    double vx = p2x - p1x;    // V=P2-P1
    double vy = p2y - p1y;

    int z;
    // Hack to z-move links to MapCenter (d==1) below MCOs frame (d==0) //FIXME-4 no longer used?
    if (treeItem->depth()<2)
        //z=(treeItem->depth() -2)*dZ_DEPTH + dZ_LINK;
        z =- dZ_LINK;
    else
        z = dZ_LINK;

    //qDebug()<<"LMO::updateGeo d="<<treeItem->depth()<<"  this="<<this<<"  "<<treeItem->getHeading();

    // Draw the horizontal line below heading (from childRefPos to ParPos)
    if (bottomline) 
    {
        bottomline->setLine (QLineF (childRefPos.x(), childRefPos.y(), p1x, p1y) );
        bottomline->setZValue (z);
    }

    double a;	// angle
    if (vx > -0.000001 && vx < 0.000001)
        a = M_PI_2;
    else
        a = atan( vy / vx );
    // "turning point" for drawing polygonal links
    QPointF tp ( -qRound(sin (a) * thickness_start), qRound(cos (a) * thickness_start));
    
    // Draw the link
    switch (style)
    {
    case Line:
        l->setLine( QLine(qRound (parPos.x()),
                          qRound(parPos.y()),
                          qRound(p2x),
                          qRound(p2y) ));
        l->setZValue (z);
        break;
    case Parabel:
        parabel (pa0, p1x,p1y,p2x,p2y);
        for (int i=0; i<segment.size(); ++i)
        {
            segment.at(i)->setLine(QLineF( pa0.at(i).x(), pa0.at(i).y(), pa0.at(i + 1).x(),pa0.at(i + 1).y()));
            segment.at(i)->setZValue (z);
        }
        break;
    case PolyLine:
        pa0.clear();
        pa0 << QPointF (qRound(p2x + tp.x()), qRound(p2y + tp.y()));
        pa0 << QPointF (qRound(p2x - tp.x()), qRound(p2y - tp.y()));
        pa0 << QPointF (qRound (parPos.x()), qRound(parPos.y()) );
        p->setPolygon(QPolygonF (pa0));
        p->setZValue (z);
        break;
    case PolyParabel:
        parabel (pa1, p1x,p1y,p2x+tp.x(),p2y+tp.y());
        parabel (pa2, p1x,p1y,p2x-tp.x(),p2y-tp.y());
        pa0.clear();
        for (int i = 0; i <= arcsegs; i++)
            pa0 << QPointF (pa1.at(i));
        for (int i = 0; i<= arcsegs; i++)
            pa0 << QPointF (pa2.at(arcsegs-i));
        p->setPolygon(QPolygonF (pa0));
        p->setZValue (z);
        break;
    default:
        break;
    }
}
    
QPointF LinkableMapObj::getChildRefPos()
{
    return childRefPos;
}

QPointF LinkableMapObj::getFloatRefPos()
{
    return floatRefPos;
}

QPointF LinkableMapObj::getParPos()
{
    return parPos;
}

LinkableMapObj::Orientation LinkableMapObj::getOrientation()
{
    return orientation;
}

void LinkableMapObj::reposition()  // virtual
{
}

void LinkableMapObj::requestReposition()   
{
    if (!repositionRequest)
    {
	// Pass on the request to parental objects, if this hasn't
	// been done yet
	repositionRequest=true;
	if (parObj) parObj->requestReposition();
    }
}

void LinkableMapObj::forceReposition()
{
    // Sometimes a reposition has to be done immediatly: For example
    // if the note editor flag changes, there is no user event in mapeditor
    // which could collect requests for a reposition.
    // Then we have to call forceReposition()
    // But no rule without exception: While loading a map or undoing it,
    // we want to block expensive repositioning, but just do it once at
    // the end, thus check first:

    VymModel *model=treeItem->getModel();
    if (model->isRepositionBlocked()) return;	
    
    // Pass on the request to parent objects, if this hasn't been done yet
    if (parObj) 
	parObj->forceReposition(); 
    else 
	reposition(); 
}

bool LinkableMapObj::repositionRequested()
{
    return repositionRequest;
}

void LinkableMapObj::parabel (QPolygonF &ya, qreal p1x, qreal p1y, qreal p2x, qreal p2y)

{
    qreal vx = p2x - p1x;    // V=P2-P1
    qreal vy = p2y - p1y;

    qreal dx;		    // delta x during calculation of parabel
    
    qreal pnx;		    // next point
    qreal pny;
    qreal m;

    if (vx > -0.0001 && vx < 0.0001)
	m = 0;
    else    
	m = (vy / (vx*vx));
    dx = vx/(arcsegs);
    ya.clear();
    ya << QPointF (p1x,p1y);
    for (int i=1; i <= arcsegs; i++)
    {	
	pnx = p1x + dx;
	pny = m * (pnx - parPos.x()) * (pnx - parPos.x()) + parPos.y();
	ya << QPointF (pnx, pny);
	p1x = pnx;
	p1y = pny;
    }	
}

