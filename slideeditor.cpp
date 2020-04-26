#include <QVBoxLayout>
#include <QTreeView>

#include "mainwindow.h"
#include "slidecontrolwidget.h"
#include "slidemodel.h"
#include "slideitem.h"
#include "vymmodel.h"

#include "slideeditor.h"

extern Main *mainWindow;
extern SlideEditor *slideEditor;

SlideEditor::SlideEditor(VymModel *m)
{
    vymModel=m;

    // Create slides model
    slideModel=vymModel->getSlideModel();

    // Create TreeView
    view = new QTreeView (this);
    view->setModel (slideModel);
    
    slideModel->setSelectionModel ( view->selectionModel() );

    // Create ControlWidget
    slideControl= new SlideControlWidget (this);
    connect (
	slideControl, SIGNAL (takeSnapshot() ), 
	this, SLOT (addSlide() ) );
    connect (
	slideControl, SIGNAL (editButtonPressed() ), 
	mainWindow, SLOT (windowToggleScriptEditor() ) );
    connect (
	slideControl, SIGNAL (deleteButtonPressed() ), 
	this, SLOT (deleteSlide() ) );
    connect (
	slideControl, SIGNAL (previousButtonPressed() ), 
	this, SLOT (previousSlide() ) );
    connect (
	slideControl, SIGNAL (nextButtonPressed() ), 
	this, SLOT (nextSlide() ) );
    connect (
	slideControl, SIGNAL (upButtonPressed() ), 
	this, SLOT (moveSlideUp() ) );
    connect (
	slideControl, SIGNAL (downButtonPressed() ), 
	this, SLOT (moveSlideDown() ) );

    QVBoxLayout* mainLayout = new QVBoxLayout;

    mainLayout->addWidget(view);
    mainLayout->addWidget(slideControl);

    setLayout (mainLayout);

    // Selection
    connect (view->selectionModel(),SIGNAL (selectionChanged (QItemSelection,QItemSelection)),
	vymModel, SLOT (updateSlideSelection (QItemSelection,QItemSelection)));
    connect (view->selectionModel(),SIGNAL (selectionChanged (QItemSelection,QItemSelection)),
	this, SLOT (updateSelection (QItemSelection,QItemSelection)));

//    connect (resultsModel, SIGNAL(layoutChanged() ), view, SLOT (expandAll() ));    
}

void SlideEditor::previousSlide()
{
    QModelIndex ix=slideModel->getSelectedIndex();
    if (ix.isValid())
        ix=view->indexAbove (ix);

    if (ix.isValid())
        view->selectionModel()->select (ix,QItemSelectionModel::ClearAndSelect );
}

void SlideEditor::nextSlide()
{
    QModelIndex ix=slideModel->getSelectedIndex();
    if (ix.isValid())
        ix=view->indexBelow (ix);
    if (ix.isValid())
        view->selectionModel()->select (ix,QItemSelectionModel::ClearAndSelect );
}

void SlideEditor::addSlide()
{
    vymModel->addSlide();
}

void SlideEditor::editSlide() // FIXME-4 not used yet
{
}

void SlideEditor::deleteSlide() 
{
    SlideItem *si=slideModel->getSelectedItem();
    vymModel->deleteSlide(si);
}

void SlideEditor::moveSlideUp() 
{
    vymModel->moveSlideUp ();	
}

void SlideEditor::moveSlideDown() 
{
    vymModel->moveSlideDown ();	
}

void SlideEditor::updateSelection(QItemSelection, QItemSelection)
{
    // FIXME-3 updateActions missing, e.g. state for moveUp/down
}

