#include "taskeditor.h"

#include <QAbstractTableModel>
#include <QAction>
#include <QDebug>
#include <QHeaderView>
#include <QMenu>
#include <QSortFilterProxyModel>
#include <QTextEdit>
#include <QToolBar>
#include <QVBoxLayout>

#include "branchitem.h"
#include "mainwindow.h"
#include "task.h"
#include "taskmodel.h"
#include "vymmodel.h"

extern Main *mainWindow;
extern Settings settings;
extern QMenu* taskContextMenu;
extern TaskModel* taskModel;

TaskEditor::TaskEditor(QWidget *)
{
    // Creat Table view
    view = new QTableView; 

    QVBoxLayout* mainLayout = new QVBoxLayout;

    QToolBar *tb=new QToolBar ("TaskEditor filters");
    tb->setToolButtonStyle (Qt::ToolButtonTextBesideIcon);
    mainLayout->addWidget (tb);

    // Original icon from KDE: /usr/share/icons/oxygen/16x16/actions/view-filter.png

    QIcon icon=QIcon (":/view-filter.png");
    QAction *a = new QAction(icon,  tr( "Current map","TaskEditor" ),this );
    a->setCheckable(true);
    a->setChecked  (settings.value("/taskeditor/filterMap", false).toBool());
    tb->addAction (a);
    connect( a, SIGNAL( triggered() ), this, SLOT(toggleFilterMap() ) );
    actionToggleFilterMap = a;

    a = new QAction(icon,  tr( "Active tasks","TaskEditor" ),this );
    a->setCheckable(true);
    a->setChecked  (settings.value("/taskeditor/filterActive", false).toBool());
    tb->addAction (a);
    connect( a, SIGNAL( triggered() ), this, SLOT(toggleFilterActive() ) );
    actionToggleFilterActive = a;

    a = new QAction(icon,  tr( "New tasks","TaskEditor" ),this );
    a->setCheckable(true);
    a->setChecked  (settings.value("/taskeditor/filterNew", false).toBool());
    tb->addAction (a);
    connect( a, SIGNAL( triggered() ), this, SLOT(toggleFilterNew() ) );
    actionToggleFilterNew = a;

    a = new QAction(icon,  "Flag", this); //tr( "Flags","TaskEditor" ),this );   // FIXME-1 add translation
    a->setCheckable(true);
    a->setChecked  (settings.value("/taskeditor/filterNew", false).toBool());
    if (settings.value( "/mainwindow/showTestMenu",false).toBool())
        tb->addAction (a);
    connect( a, SIGNAL( triggered() ), this, SLOT(toggleFilterFlags() ) );
    actionToggleFilterFlags = a;

    // Forward Enter and Return to MapEditor
    a = new QAction(icon, tr( "Edit heading","TaskEditor" ), this);
    a->setShortcut ( Qt::Key_Return);		
    a->setShortcutContext (Qt::WidgetWithChildrenShortcut);
    addAction (a);
    connect( a, SIGNAL( triggered() ), mainWindow, SLOT( editHeading() ) );
    a = new QAction( tr( "Edit heading","TaskEditor" ), this);
    a->setShortcut ( Qt::Key_Enter);			
    a->setShortcutContext (Qt::WidgetWithChildrenShortcut);
    addAction (a);
    connect( a, SIGNAL( triggered() ), mainWindow, SLOT( editHeading() ) );

    // Clone actions defined in MainWindow
    foreach (QAction* qa, mainWindow->taskEditorActions)
    {
        a = new QAction( this );
        a->setShortcut( qa->shortcut() );
        a->setShortcutContext (Qt::WidgetWithChildrenShortcut);
        connect( a, SIGNAL( triggered() ), qa, SLOT( trigger() ) );
        addAction(a);
    }

    mainLayout->addWidget (view);
    setLayout (mainLayout);

    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->horizontalHeader()->setStretchLastSection(true);
    view->verticalHeader()->hide();
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);

    filterActiveModel = new TaskFilterModel;
    filterActiveModel->setSourceModel(taskModel);

    view->setModel (filterActiveModel);
    view->setSortingEnabled(true);
    view->horizontalHeader()->setSortIndicator (0,Qt::AscendingOrder);

    blockExternalSelect=false;

    connect (
	view->selectionModel(),SIGNAL (selectionChanged (QItemSelection,QItemSelection)),
	this, SLOT (selectionChanged (QItemSelection,QItemSelection)));
    
    // layout changes trigger resorting
    connect( taskModel, SIGNAL( layoutChanged() ), this, SLOT(sort() ) );

    // Enable wordwrap when data changes
    connect ( 
        taskModel, SIGNAL( dataChanged( QModelIndex, QModelIndex)),
        view, SLOT( resizeRowsToContents() ) );
    connect ( 
        view->horizontalHeader(), SIGNAL( sectionResized(int, int, int)),
        view, SLOT( resizeRowsToContents() ) );


    // Initialize view filters according to previous settings
    setFilterMap();
    setFilterActive();
    setFilterNew();
    setFilterFlags();

    // Initialize column widths
    QString s;
    for (int i=0; i<6; i++)
    {
	s=QString("/taskeditor/columnWidth/%1").arg(i);
	if (settings.contains (s) )
	    view->setColumnWidth (i, settings.value(s, 100).toInt() );
    }

    // Initialize display of parents of a task
    bool ok;
    int i=settings.value ("/taskeditor/showParentsLevel", 0).toInt(&ok);
    if (ok) taskModel->setShowParentsLevel(i);

}

TaskEditor::~TaskEditor()
{
    settings.setValue ("/taskeditor/filterMap",actionToggleFilterMap->isChecked());
    settings.setValue ("/taskeditor/filterActive",actionToggleFilterActive->isChecked());
    settings.setValue ("/taskeditor/filterNew",actionToggleFilterNew->isChecked());
    settings.setValue ("/taskeditor/showParentsLevel",taskModel->getShowParentsLevel() );
    for (int i=0; i<7; i++)
	settings.setValue (QString("/taskeditor/columnWidth/%1").arg(i),view->columnWidth(i) );
}

void TaskEditor::setMapName (const QString &n)
{
    currentMapName=n;
    setFilterMap();
}

bool TaskEditor::isUsedFilterMap()
{
    return actionToggleFilterMap->isChecked();
}

void TaskEditor::setFilterMap () 
{
    if (isUsedFilterMap() )
        filterActiveModel->setMapFilter(currentMapName);
    else
        filterActiveModel->setMapFilter(QString() );
    sort();
}

bool TaskEditor::isUsedFilterActive()
{
    return actionToggleFilterActive->isChecked();
}

void TaskEditor::setFilterActive () 
{
    filterActiveModel->setFilter (actionToggleFilterActive->isChecked() );   
    sort();	
}

void TaskEditor::setFilterNew ()
{
    filterActiveModel->setFilterNew (actionToggleFilterNew->isChecked() );
    sort();
}

void TaskEditor::setFilterFlags ()  // FIXME-1 experimental
{
    filterActiveModel->setFilterFlags (actionToggleFilterFlags->isChecked() );
    sort();
}

void TaskEditor::showSelection()
{
    QModelIndexList list=view->selectionModel()->selectedIndexes();
    if (list.count()>0)
	// Usually whole row is selected, so just go for first cell
	view->scrollTo(taskModel->index(taskModel->getTask(list.first())), QAbstractItemView::EnsureVisible);
}

bool TaskEditor::select (Task *task)	
{
    if (task)
    {
	blockExternalSelect=true;
	QModelIndex i0b=taskModel->index (task); 
	QModelIndex i0e=taskModel->indexRowEnd (task); 

	QModelIndex i1b=filterActiveModel->mapFromSource(i0b ); 
	QModelIndex i1e=filterActiveModel->mapFromSource(i0e ); 

	QItemSelection sel (i1b, i1e);

	view->selectionModel()->select (sel, QItemSelectionModel::ClearAndSelect  );
	blockExternalSelect=false;
	return true;
    }
    return false;
}

void TaskEditor::clearSelection()
{
    view->selectionModel()->clearSelection();
}

void TaskEditor::selectionChanged ( const QItemSelection & selected, const QItemSelection & )
{// FIXME-3 what, if multiple selection in MapEditor?
    // Avoid segfault on quit, when selected is empty
    if (selected.indexes().isEmpty() ) return;

    QItemSelection sel0=filterActiveModel->mapSelectionToSource (selected);
    QModelIndex ix=sel0.indexes().first();
    Task *t=taskModel->getTask (ix);
    if (t) 
    {
	BranchItem *bi=t->getBranch();
	if (bi) 
	{
	    VymModel *m=bi->getModel();
	    if (!blockExternalSelect) m->select (bi);
	    if (m!=mainWindow->currentModel() )
		mainWindow->gotoModel (m);
	    view->setStyleSheet( 
	    QString ("selection-color: %1;" 
		     "selection-background-color: %2;").arg(bi->getHeadingColor().name() ).arg(m->getSelectionColor().name() ) );
	    view->scrollTo (selected.indexes().first() );   
	}
    }
}

void TaskEditor::contextMenuEvent ( QContextMenuEvent * e )
{
    taskContextMenu->popup (e->globalPos() );
}

void TaskEditor::sort()
{
    QHeaderView *hv=view->horizontalHeader();
    view->sortByColumn( hv->sortIndicatorSection(), hv->sortIndicatorOrder() );
    filterActiveModel->invalidate();	

}

void TaskEditor::toggleFilterMap ()
{
    setFilterMap ();
}

void TaskEditor::toggleFilterActive ()
{
    setFilterActive();

}
void TaskEditor::toggleFilterNew ()
{
    setFilterNew();
}

void TaskEditor::toggleFilterFlags ()
{
    setFilterFlags();
}
