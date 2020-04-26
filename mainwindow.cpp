#include "mainwindow.h"

#include <iostream>
#include <typeinfo>

#ifndef Q_OS_WIN
#include <unistd.h>
#endif

#if defined(VYM_DBUS)
#include "adaptorvym.h"
#endif

#include <QColorDialog>
#include <QDockWidget>
#include <QFontDialog>
#include <QInputDialog>
#include <QMenuBar>
#include <QPrinter>
#include <QStatusBar>
#include <QTextStream>

#include "aboutdialog.h"
#include "branchpropeditor.h"
#include "branchitem.h"
#include "command.h"
#include "downloadagent.h"
#include "exportoofiledialog.h"
#include "exports.h"
#include "file.h"
#include "findresultwidget.h"
#include "findresultmodel.h"
#include "flagrow.h"
#include "headingeditor.h"
#include "historywindow.h"
#include "imports.h"
#include "lineeditdialog.h"
#include "macros.h"
#include "mapeditor.h"
#include "misc.h"
#include "options.h"
#include "vymprocess.h"
#include "scripteditor.h"
#include "settings.h"
#include "shortcuts.h"
#include "noteeditor.h"
#include "task.h"
#include "taskeditor.h"
#include "taskmodel.h"
#include "treeeditor.h"
#include "version.h"
#include "warningdialog.h"
#include "xlinkitem.h"

QPrinter *printer;

//#include <modeltest.h>    

#if defined(VYM_DBUS)
#include <QDBusConnection>
#endif

extern NoteEditor    *noteEditor;
extern HeadingEditor *headingEditor;
extern ScriptEditor  *scriptEditor;
extern Main *mainWindow;
extern FindResultWidget *findResultWidget;  
extern TaskEditor *taskEditor;
extern TaskModel *taskModel;
extern Macros macros;
extern QString tmpVymDir;
extern QString clipboardDir;
extern QString clipboardFile;
extern bool clipboardEmpty;
extern int statusbarTime;
extern FlagRow *standardFlagsMaster;	
extern FlagRow *systemFlagsMaster;
extern QString vymName;
extern QString vymVersion;
extern QString vymPlatform;
extern QString vymBuildDate;
extern QString localeName;
extern bool debug;
extern bool testmode;
extern QTextStream vout;
extern bool bugzillaClientAvailable;
extern Switchboard switchboard;


extern QList <Command*> modelCommands;

QMenu* branchAddContextMenu;
QMenu* branchContextMenu;
QMenu* branchLinksContextMenu;
QMenu* branchRemoveContextMenu;
QMenu* branchXLinksContextMenuEdit;
QMenu* branchXLinksContextMenuFollow;
QMenu* canvasContextMenu;
QMenu* floatimageContextMenu;
QMenu* targetsContextMenu;
QMenu* taskContextMenu;
QMenu* fileLastMapsMenu;
QMenu* fileImportMenu;
QMenu* fileExportMenu;

extern Settings settings;
extern Options options;
extern ImageIO imageIO;

extern QDir vymBaseDir;
extern QDir lastImageDir;
extern QDir lastMapDir;
#if defined(Q_OS_WIN32)
extern QDir vymInstallDir;
#endif
extern QString zipToolPath;

Main::Main(QWidget* parent, Qt::WindowFlags f) : QMainWindow(parent,f)
{
    mainWindow = this;

    setWindowTitle ("VYM - View Your Mind");

    shortcutScope = tr("Main window","Shortcut scope");

    // Load window settings
    #if defined(Q_OS_WIN32)
    if (settings.value("/mainwindow/geometry/maximized", false).toBool())
    {
	setWindowState(Qt::WindowMaximized);
    }
    else
    #endif
    {
	resize (settings.value("/mainwindow/geometry/size", QSize (1024,900)).toSize());
	move   (settings.value("/mainwindow/geometry/pos",  QPoint(50,50)).toPoint());
    }

    // Sometimes we may need to remember old selections
    prevSelection="";

    // Default color
    currentColor=Qt::black;

    // Create unique temporary directory
    bool ok;
    tmpVymDir=makeTmpDir (ok,"vym");
    if (!ok)
    {
	qWarning ("Mainwindow: Could not create temporary directory, failed to start vym");
	exit (1);
    }
    if (debug) qDebug ()<<"tmpDir="<<tmpVymDir;

    // Create direcctory for clipboard
    clipboardDir=tmpVymDir+"/clipboard";
    clipboardFile="map.xml";
    QDir d(clipboardDir);
    d.mkdir (clipboardDir);
    makeSubDirs (clipboardDir);
    clipboardEmpty=true;

    // Remember PID of our friendly webbrowser
    browserPID=new qint64;
    *browserPID=0;

    // Define commands in API (used globally)
    setupAPI();

    // Initialize some settings, which are platform dependant
    QString p,s;

	// application to open URLs
	p="/system/readerURL";
	#if defined(Q_OS_WIN)
	    // Assume that system has been set up so that
	    // Explorer automagically opens up the URL
	    // in the user's preferred browser.
	    s=settings.value (p,"explorer").toString();
	#elif defined(Q_OS_MACX)
	    s=settings.value (p,"/usr/bin/open").toString();
	#else
	    s=settings.value (p,"xdg-open").toString();
	#endif
	settings.setValue( p,s);

	// application to open PDFs
	p="/system/readerPDF";
	#if defined(Q_OS_WIN)
	    s=settings.value (p,"explorer").toString();
	#elif defined(Q_OS_MACX)
	    s=settings.value (p,"/usr/bin/open").toString();
	#else
	    s=settings.value (p,"xdg-open").toString();
	#endif
	settings.setValue( p,s);

    // width of xLinksMenu
    xLinkMenuWidth=60;

    // Create Layout
    QWidget* centralWidget = new QWidget (this);
    QVBoxLayout *layout=new QVBoxLayout (centralWidget);
    setCentralWidget(centralWidget);	

    // Create tab widget which holds the maps
    tabWidget= new QTabWidget (centralWidget);
    connect(tabWidget, SIGNAL( currentChanged( int  ) ), 
            this, SLOT( editorChanged( ) ) );

    // Allow closing of tabs (introduced in Qt 4.5)
    tabWidget->setTabsClosable( true ); 
    connect(tabWidget, SIGNAL(tabCloseRequested(int)), 
            this, SLOT( fileCloseMap(int) ));

    layout->addWidget (tabWidget);

    switchboard.addGroup("MainWindow",tr("Main window","Shortcut group"));
    switchboard.addGroup("MapEditor",tr("Map Editors","Shortcut group"));
    switchboard.addGroup("TextEditor",tr("Text Editors","Shortcut group"));

    // Setup actions
    setupFileActions();
    setupEditActions();
    setupSelectActions();
    setupFormatActions();
    setupViewActions();
    setupModeActions();
    setupNetworkActions();
    setupSettingsActions();
    setupContextMenus();
    setupMacros();
    setupToolbars();
    setupFlagActions();

    // Dock widgets ///////////////////////////////////////////////
    QDockWidget *dw;
    dw = new QDockWidget ();
    dw->setWidget (noteEditor);
    dw->setObjectName ("NoteEditor");
    dw->setWindowTitle(noteEditor->getEditorTitle() );
    dw->hide();
    noteEditorDW=dw;
    addDockWidget (Qt::LeftDockWidgetArea,dw);

    dw = new QDockWidget ();
    dw->setWidget (headingEditor);
    dw->setObjectName ("HeadingEditor");
    dw->setWindowTitle(headingEditor->getEditorTitle() );
    dw->hide();
    headingEditorDW=dw;
    addDockWidget (Qt::BottomDockWidgetArea,dw);

    dw = new QDockWidget (tr("Script Editor"));
    dw->setWidget (scriptEditor);
    dw->setObjectName ("ScriptEditor");
    dw->hide();
    scriptEditorDW=dw;
    addDockWidget (Qt::LeftDockWidgetArea,dw);

    findResultWidget=new FindResultWidget ();
    dw= new QDockWidget (tr ("Search results list","FindResultWidget"));
    dw->setWidget (findResultWidget);
    dw->setObjectName ("FindResultWidget");
    dw->hide();	
    addDockWidget (Qt::RightDockWidgetArea,dw);
    connect (
	findResultWidget, SIGNAL (noteSelected (QString, int)),
	this, SLOT (selectInNoteEditor (QString, int)));
    connect (
	findResultWidget, SIGNAL (findPressed (QString) ), 
	this, SLOT (editFindNext(QString) ) );

    scriptEditor = new ScriptEditor(this);
    dw= new QDockWidget (tr ("Script Editor","ScriptEditor"));
    dw->setWidget (scriptEditor);
    dw->setObjectName ("ScriptEditor");
    dw->hide();	
    addDockWidget (Qt::LeftDockWidgetArea,dw);

    branchPropertyEditor = new BranchPropertyEditor();
    dw = new QDockWidget (tr("Property Editor","PropertyEditor"));
    dw->setWidget (branchPropertyEditor);
    dw->setObjectName ("PropertyEditor");
    dw->hide();
    addDockWidget (Qt::LeftDockWidgetArea,dw);

    historyWindow=new HistoryWindow();
    dw = new QDockWidget (tr("History window","HistoryWidget"));
    dw->setWidget (historyWindow);
    dw->setObjectName ("HistoryWidget");
    dw->hide();
    addDockWidget (Qt::RightDockWidgetArea,dw);
    connect (dw, SIGNAL (visibilityChanged(bool ) ), this, SLOT (updateActions()));

    // Connect NoteEditor, so that we can update flags if text changes
    connect (noteEditor, SIGNAL (textHasChanged() ), this, SLOT (updateNoteFlag()));
    connect (noteEditor, SIGNAL (windowClosed() ), this, SLOT (updateActions()));

    // Connect heading editor
    connect (headingEditor, SIGNAL (textHasChanged() ), this, SLOT (updateHeading()));

    connect( scriptEditor, SIGNAL( runScript ( QString ) ),  this, SLOT( execute( QString ) ) );

    // Switch back  to MapEditor using Esc 
    QAction* a = new QAction(this);
    a->setShortcut (Qt::Key_Escape);
    a->setShortcutContext (Qt::ApplicationShortcut);
    a->setCheckable(false);
    a->setEnabled (true);
    addAction(a);
    connect (a , SIGNAL (triggered() ), this, SLOT (setFocusMapEditor()));
    
    // Create TaskEditor after setting up above actions, allow cloning 
    taskEditor = new TaskEditor ();
    dw= new QDockWidget (tr ("Task list","TaskEditor"));
    dw->setWidget (taskEditor);
    dw->setObjectName ("TaskEditor");
    dw->hide();
    addDockWidget (Qt::TopDockWidgetArea,dw);
    connect (dw, SIGNAL (visibilityChanged(bool ) ), this, SLOT (updateActions()));
    //FIXME -0 connect (taskEditor, SIGNAL (focusReleased() ), this, SLOT (setFocusMapEditor()));

    if (options.isOn("shortcutsLaTeX")) switchboard.printLaTeX();

    if (settings.value( "/mainwindow/showTestMenu",false).toBool()) setupTestActions();
    setupHelpActions();

    // Status bar and progress bar there
    statusBar();
    progressMax=0;
    progressCounter=0;
    progressCounterTotal=0;

    progressDialog.setAutoReset(false);
    progressDialog.setAutoClose(false);
    progressDialog.setMinimumWidth (600);
    //progressDialog.setWindowModality (Qt::WindowModal);   // That forces mainwindo to update and slows down
    progressDialog.setCancelButton (NULL);

    restoreState (settings.value("/mainwindow/state",0).toByteArray());

    // Global Printer
    printer=new QPrinter (QPrinter::HighResolution );	

    // Enable testmenu
    //settings.setValue( "mainwindow/showTestMenu", true);
    updateGeometry();

#if defined(VYM_DBUS)
    // Announce myself on DBUS
    new AdaptorVym (this);    // Created and not deleted as documented in Qt
    if (!QDBusConnection::sessionBus().registerObject ("/vym",this))
	qWarning ("MainWindow: Couldn't register DBUS object!");
#endif    

}

Main::~Main()
{
    // qDebug()<<"Destr Mainwindow"<<flush;

    // Save Settings

    if (!testmode) 
    {
	#if defined(Q_OS_WIN32)
	settings.setValue ("/mainwindow/geometry/maximized", isMaximized());
	#endif
	settings.setValue ("/mainwindow/geometry/size", size());
	settings.setValue ("/mainwindow/geometry/pos", pos());
	settings.setValue ("/mainwindow/state",saveState(0));

	settings.setValue ("/mainwindow/view/AntiAlias",actionViewToggleAntiAlias->isChecked());
	settings.setValue ("/mainwindow/view/SmoothPixmapTransform",actionViewToggleSmoothPixmapTransform->isChecked());
	settings.setValue( "/system/autosave/use",actionSettingsToggleAutosave->isChecked() );
	settings.setValue ("/system/autosave/ms", settings.value("/system/autosave/ms",60000)); 
	settings.setValue ("/mainwindow/autoLayout/use",actionSettingsToggleAutoLayout->isChecked() );
	settings.setValue( "/mapeditor/editmode/autoSelectNewBranch",actionSettingsAutoSelectNewBranch->isChecked() );
	settings.setValue( "/system/writeBackupFile",actionSettingsWriteBackupFile->isChecked() );

	settings.setValue("/system/printerName",printer->printerName());
	settings.setValue("/system/printerFormat",printer->outputFormat());
	settings.setValue("/system/printerFileName",printer->outputFileName());
	settings.setValue( "/mapeditor/editmode/autoSelectText",actionSettingsAutoSelectText->isChecked() );
	settings.setValue( "/mapeditor/editmode/autoEditNewBranch",actionSettingsAutoEditNewBranch->isChecked() );
	settings.setValue( "/mapeditor/editmode/useFlagGroups",actionSettingsUseFlagGroups->isChecked() );
	settings.setValue( "/export/useHideExport",actionSettingsUseHideExport->isChecked() );
	settings.setValue( "/system/version", vymVersion );
	settings.setValue( "/system/builddate", vymBuildDate );
    }
    //
    // call the destructors
    delete noteEditorDW;
    delete historyWindow;
    delete branchPropertyEditor;

    // Remove temporary directory
    removeDir (QDir(tmpVymDir));
}

void Main::loadCmdLine()
{
    QStringList flist=options.getFileList();
    QStringList::Iterator it=flist.begin();

    initProgressCounter (flist.count());
    while (it !=flist.end() )
    {
	FileType type=getMapType (*it);
	fileLoad (*it, NewMap,type);
	*it++;
    }	
    removeProgressCounter();
}


void Main::statusMessage(const QString &s)
{
    // Surpress messages while progressdialog during 
    // load is active
    statusBar()->showMessage( s,statusbarTime);
}

void Main::setProgressMaximum (int max)	{
    if (progressCounter==0)
    {
	// Init range only on first time, when progressCounter still 0
	// Normalize range to 1000
	progressDialog.setRange (0,1000);
	progressDialog.setValue (1);
    }
    progressCounter++;	// Another map is loaded

    progressMax=max*1000;
    QApplication::processEvents();
}

void Main::addProgressValue (float v) 

{
    int progress_value= (v + progressCounter -1)*1000/progressCounterTotal;
/*
    qDebug() << "addVal v="<<v
	 <<"  cur="<<progressDialog.value()
	 <<"  pCounter="<<progressCounter
	 <<"  pCounterTotal="<<progressCounterTotal
         <<"  newv="<< progress_value
	 ;
	 */

    // Make sure the progress dialog shows, even if value == 0
    if (progress_value < 1) progress_value = 1; 
    progressDialog.setValue ( progress_value );
    if (progress_value == 1) QApplication::processEvents();
}

void Main::initProgressCounter(uint n)
{
    progressCounterTotal=n;
}

void Main::removeProgressCounter()
{
    // Hide dialog again
    progressCounter=0;
    progressCounterTotal=0;
    progressDialog.reset();
    progressDialog.hide();
}

void Main::closeEvent (QCloseEvent* event)
{
    if (fileExitVYM())
	event->ignore();
    else	
	event->accept();
}

// Define commands for models
void Main::setupAPI()
{
    Command *c = new Command ("addBranch",Command::Branch);
    c->addPar (Command::Int, true, "Index of new branch");
    modelCommands.append(c);

    c=new Command ("addBranchBefore",Command::Branch);
    modelCommands.append(c);

    c=new Command ("addMapCenter",Command::Any);
    c->addPar (Command::Double,false, "Position x");
    c->addPar (Command::Double,false, "Position y");
    modelCommands.append(c);

    c=new Command ("addMapInsert",Command::Any);
    c->addPar (Command::String,false, "Filename of map to load");
    c->addPar (Command::Int,true, "Index where map is inserted");
    c->addPar (Command::Int,true, "Content filter");
    modelCommands.append(c);

    c=new Command ("addMapReplace",Command::Branch);
    c->addPar (Command::String,false, "Filename of map to load");
    modelCommands.append(c);

    c=new Command ("addSlide",Command::Branch);
    modelCommands.append(c);

    c=new Command ("addXLink",Command::BranchLike);
    c->addPar (Command::String, false, "Begin of XLink");
    c->addPar (Command::String, false, "End of XLink");
    c->addPar (Command::Int,    true, "Width of XLink");
    c->addPar (Command::Color,  true, "Color of XLink");
    c->addPar (Command::String, true, "Penstyle of XLink");
    modelCommands.append(c);

    c=new Command ("branchCount",Command::Any);
    modelCommands.append(c);

    c=new Command ("centerCount",Command::BranchLike);
    modelCommands.append(c);

    c=new Command ("centerOnID",Command::Any);
    c->addPar (Command::String,false, "UUID of object to center on");
    modelCommands.append(c);

    c=new Command ("clearFlags",Command::BranchLike);
    modelCommands.append(c);

    c=new Command ("colorBranch",Command::Branch);
    c->addPar (Command::Color,true, "New color");
    modelCommands.append(c);

    c=new Command ("colorSubtree",Command::Branch);
    c->addPar (Command::Color,true, "New color");
    modelCommands.append(c);

    c=new Command ("copy",Command::BranchOrImage);
    modelCommands.append(c);

    c=new Command ("cut",Command::BranchOrImage);
    modelCommands.append(c);

    c=new Command ("cycleTask",Command::BranchOrImage);
    c->addPar (Command::Bool,true, "True, if cycling in reverse order");
    modelCommands.append(c);

    c=new Command ("delete",Command::TreeItem);
    modelCommands.append(c);

    c=new Command ("deleteChildren",Command::Branch);
    modelCommands.append(c);

    c=new Command ("deleteKeepChildren",Command::Branch);
    modelCommands.append(c);

    c=new Command ("deleteSlide",Command::Any);
    c->addPar (Command::Int,false,"Index of slide to delete");
    modelCommands.append(c);

    c=new Command ("exportAO",Command::Any);
    c->addPar (Command::String,false,"Filename for export");
    modelCommands.append(c);

    c=new Command ("exportASCII",Command::Any);
    c->addPar (Command::String,false,"Filename for export");
    c->addPar (Command::Bool,false,"Flag, if tasks should be appended");
    modelCommands.append(c);

    c=new Command ("exportCSV",Command::Any);
    c->addPar (Command::String,false,"Filename for export");
    modelCommands.append(c);

    c=new Command ("exportHTML",Command::Any);
    c->addPar (Command::String,false,"Path used for export");
    c->addPar (Command::String,false,"Filename for export");
    modelCommands.append(c);

    c=new Command ("exportImage",Command::Any);
    c->addPar (Command::String,false,"Filename for export");
    c->addPar (Command::String,true,"Image format");
    modelCommands.append(c);

    c=new Command ("exportImpress",Command::Any);
    c->addPar (Command::String,false,"Filename for export");
    c->addPar (Command::String,false,"Configuration file for export");
    modelCommands.append(c);

    c=new Command ("exportLast",Command::Any);
    modelCommands.append(c);

    c=new Command ("exportLaTeX",Command::Any);
    c->addPar (Command::String,false,"Filename for export");
    modelCommands.append(c);

    c=new Command ("exportOrgMode",Command::Any);
    c->addPar (Command::String,false,"Filename for export");
    modelCommands.append(c);

    modelCommands.append(c);

    c=new Command ("exportPDF",Command::Any);
    c->addPar (Command::String,false,"Filename for export");
    modelCommands.append(c);

    c=new Command ("exportPDF",Command::Any);
    c->addPar (Command::String,false,"Filename for export");
    modelCommands.append(c);

    c=new Command ("exportSVG",Command::Any);
    c->addPar (Command::String,false,"Filename for export");
    modelCommands.append(c);

    c=new Command ("exportXML",Command::Any);
    c->addPar (Command::String,false,"Path used for export");
    c->addPar (Command::String,false,"Filename for export");
    modelCommands.append(c);

    c=new Command ("getDestPath",Command::Any);
    modelCommands.append(c);

    c=new Command ("getFileDir",Command::Any);
    modelCommands.append(c);

    c=new Command ("getFrameType",Command::Branch);
    modelCommands.append(c);

    c=new Command ("getHeadingPlainText",Command::TreeItem);
    modelCommands.append(c);

    c=new Command ("getHeadingXML",Command::TreeItem);
    modelCommands.append(c);

    c=new Command ("getMapAuthor",Command::Any);
    modelCommands.append(c);

    c=new Command ("getMapComment",Command::Any);
    modelCommands.append(c);

    c=new Command ("getMapTitle",Command::Any);
    modelCommands.append(c);

    c=new Command ("getNotePlainText",Command::TreeItem);
    modelCommands.append(c);

    c=new Command ("getNoteXML",Command::TreeItem);
    modelCommands.append(c);

    c=new Command ("getSelectString",Command::TreeItem);
    modelCommands.append(c);

    c=new Command ("getTaskSleepDays",Command::Branch);
    modelCommands.append(c);

    c=new Command ("getURL",Command::TreeItem); 
    modelCommands.append(c);

    c=new Command ("getVymLink",Command::Branch); 
    modelCommands.append(c);

    c=new Command ("getXLinkColor",Command::XLink);
    modelCommands.append(c);

    c=new Command ("getXLinkWidth",Command::XLink);
    modelCommands.append(c);

    c=new Command ("getXLinkPenStyle",Command::XLink);
    modelCommands.append(c);

    c=new Command ("getXLinkStyleBegin",Command::XLink);
    modelCommands.append(c);

    c=new Command ("getXLinkStyleEnd",Command::XLink);
    modelCommands.append(c);

    c=new Command ("hasActiveFlag",Command::TreeItem);
    c->addPar (Command::String,false,"Name of flag");
    modelCommands.append(c);

    c=new Command ("hasNote",Command::Branch); 
    modelCommands.append(c);

    c=new Command ("hasRichTextNote",Command::Branch); 
    modelCommands.append(c);

    c=new Command ("hasTask",Command::Branch); 
    modelCommands.append(c);

    c=new Command ("importDir",Command::Branch);
    c->addPar (Command::String,false,"Directory name to import");
    modelCommands.append(c);

    c=new Command ("isScrolled",Command::Branch); 
    modelCommands.append(c);

    c=new Command ("loadImage",Command::Branch); 
    c->addPar (Command::String,false,"Filename of image");
    modelCommands.append(c);

    c=new Command ("loadNote",Command::Branch); 
    c->addPar (Command::String,false,"Filename of note");
    modelCommands.append(c);

    c=new Command ("moveDown",Command::Branch); 
    modelCommands.append(c);

    c=new Command ("moveUp",Command::Branch); 
    modelCommands.append(c);

    c=new Command ("moveSlideDown",Command::Any); 
    modelCommands.append(c);

    c=new Command ("moveSlideUp",Command::Any); 
    modelCommands.append(c);

    c=new Command ("move",Command::BranchOrImage); 
    c->addPar (Command::Double,false,"Position x");
    c->addPar (Command::Double,false,"Position y");
    modelCommands.append(c);

    c=new Command ("moveRel",Command::BranchOrImage); 
    c->addPar (Command::Double,false,"Position x");
    c->addPar (Command::Double,false,"Position y");
    modelCommands.append(c);

    c=new Command ("nop",Command::Any); 
    modelCommands.append(c);

    c=new Command ("note2URLs",Command::Branch); 
    modelCommands.append(c);

    //internally required for undo/redo of changing VymText:
    c=new Command ("parseVymText",Command::Branch);
    c->addPar (Command::String,false,"parse XML of VymText, e.g for Heading or VymNote");
    modelCommands.append(c);

    c=new Command ("paste",Command::Branch);
    modelCommands.append(c);

    c=new Command ("redo",Command::Any); 
    modelCommands.append(c);

    c=new Command ("relinkTo",Command::TreeItem);   // FIXME different number of parameters for Image or Branch
    c->addPar (Command::String,false,"Selection string of parent");
    c->addPar (Command::Int,false,"Index position");
    c->addPar (Command::Double,true,"Position x");
    c->addPar (Command::Double,true,"Position y");
    modelCommands.append(c);

    c=new Command ("saveImage",Command::Image); 
    c->addPar (Command::String,false,"Filename of image to save");
    c->addPar (Command::String,false,"Format of image to save");
    modelCommands.append(c);

    c=new Command ("saveNote",Command::Branch); 
    c->addPar (Command::String,false,"Filename of note to save");
    modelCommands.append(c);

    c=new Command ("scroll",Command::Branch); 
    modelCommands.append(c);

    c=new Command ("select",Command::Any); 
    c->addPar (Command::String,false,"Selection string");
    modelCommands.append(c);

    c=new Command ("selectID",Command::Any); 
    c->addPar (Command::String,false,"Unique ID");
    modelCommands.append(c);

    c=new Command ("selectLastBranch",Command::Branch); 
    modelCommands.append(c);

    c=new Command ("selectLastImage",Command::Branch); 
    modelCommands.append(c);

    c=new Command ("selectLatestAdded",Command::Any); 
    modelCommands.append(c);

    c=new Command ("selectParent",Command::Branch); 
    modelCommands.append(c);

    c=new Command ("setFlag",Command::TreeItem); 
    c->addPar (Command::String,false,"Name of flag");
    modelCommands.append(c);

    c=new Command ("setTaskSleep",Command::Branch); 
    c->addPar (Command::String,false,"Days to sleep");
    modelCommands.append(c);

    c=new Command ("setFrameIncludeChildren",Command::BranchOrImage); 
    c->addPar (Command::Bool,false,"Include or don't include children in frame");
    modelCommands.append(c);

    c=new Command ("setFrameType",Command::BranchOrImage); 
    c->addPar (Command::String,false,"Type of frame");
    modelCommands.append(c);

    c=new Command ("setFramePenColor",Command::BranchOrImage); 
    c->addPar (Command::Color,false,"Color of frame border line");
    modelCommands.append(c);

    c=new Command ("setFrameBrushColor",Command::BranchOrImage); 
    c->addPar (Command::Color,false,"Color of frame background");
    modelCommands.append(c);

    c=new Command ("setFramePadding",Command::BranchOrImage); 
    c->addPar (Command::Int,false,"Padding around frame");
    modelCommands.append(c);

    c=new Command ("setFrameBorderWidth",Command::BranchOrImage); 
    c->addPar (Command::Int,false,"Width of frame borderline");
    modelCommands.append(c);

    c=new Command ("setHeadingPlainText",Command::TreeItem); 
    c->addPar (Command::String,false,"New heading");
    modelCommands.append(c);

    c=new Command ("setHideExport",Command::BranchOrImage); 
    c->addPar (Command::Bool,false,"Set if item should be visible in export");
    modelCommands.append(c);

    c=new Command ("setIncludeImagesHorizontally",Command::Branch); 
    c->addPar (Command::Bool,false,"Set if images should be included horizontally in parent branch");
    modelCommands.append(c);

    c=new Command ("setIncludeImagesVertically",Command::Branch); 
    c->addPar (Command::Bool,false,"Set if images should be included vertically in parent branch");
    modelCommands.append(c);

    c=new Command ("setHideLinksUnselected",Command::BranchOrImage); 
    c->addPar (Command::Bool,false,"Set if links of items should be visible for unselected items");
    modelCommands.append(c);

    c=new Command ("setMapAnimCurve",Command::Any); 
    c->addPar (Command::Int,false,"EasingCurve used in animation in MapEditor");
    modelCommands.append(c);

    c=new Command ("setMapAuthor",Command::Any); 
    c->addPar (Command::String,false,"");
    modelCommands.append(c);

    c=new Command ("setMapAnimDuration",Command::Any); 
    c->addPar (Command::Int,false,"Duration of animation in MapEditor in milliseconds");
    modelCommands.append(c);

    c=new Command ("setMapBackgroundColor",Command::Any); 
    c->addPar (Command::Color,false,"Color of map background");
    modelCommands.append(c);

    c=new Command ("setMapComment",Command::Any); 
    c->addPar (Command::String,false,"");
    modelCommands.append(c);

    c=new Command ("setMapTitle",Command::Any); 
    c->addPar (Command::String,false,"");
    modelCommands.append(c);

    c=new Command ("setMapDefLinkColor",Command::Any); 
    c->addPar (Command::Color,false,"Default color of links");
    modelCommands.append(c);

    c=new Command ("setMapLinkStyle",Command::Any); 
    c->addPar (Command::String,false,"Link style in map");
    modelCommands.append(c);

    c=new Command ("setMapRotation",Command::Any); 
    c->addPar (Command::Double,false,"Rotation of map");
    modelCommands.append(c);

    c=new Command ("setMapTitle",Command::Any); 
    c->addPar (Command::String,false,"");
    modelCommands.append(c);

    c=new Command ("setMapZoom",Command::Any); 
    c->addPar (Command::Double,false,"Zoomfactor of map");
    modelCommands.append(c);

    c=new Command ("setNotePlainText",Command::Branch); 
    c->addPar (Command::String,false,"Note of branch");
    modelCommands.append(c);

    c=new Command ("setScale",Command::Image); 
    c->addPar (Command::Double,false,"Scale image x");
    c->addPar (Command::Double,false,"Scale image y");
    modelCommands.append(c);

    c=new Command ("setSelectionColor",Command::Any); 
    c->addPar (Command::Color,false,"Color of selection box");
    modelCommands.append(c);

    c=new Command ("setURL",Command::TreeItem); 
    c->addPar (Command::String,false,"URL of TreeItem");
    modelCommands.append(c);

    c=new Command ("setVymLink",Command::Branch); 
    c->addPar (Command::String,false,"Vymlink of branch");
    modelCommands.append(c);

    c=new Command ("setXLinkColor", Command::XLink); 
    c->addPar (Command::String,false,"Color of xlink");
    modelCommands.append(c);

    c=new Command ("setXLinkLineStyle", Command::XLink); 
    c->addPar (Command::String,false,"Style of xlink");
    modelCommands.append(c);

    c=new Command ("setXLinkStyleBegin", Command::XLink); 
    c->addPar (Command::String,false,"Style of xlink begin");
    modelCommands.append(c);

    c=new Command ("setXLinkStyleEnd", Command::XLink); 
    c->addPar (Command::String,false,"Style of xlink end");
    modelCommands.append(c);

    c=new Command ("setXLinkWidth", Command::XLink); 
    c->addPar (Command::Int,false,"Width of xlink");
    modelCommands.append(c);

    c=new Command ("sleep",Command::Any); 
    c->addPar (Command::Int,false,"Sleep (seconds)");
    modelCommands.append(c);

    c=new Command ("sortChildren",Command::Branch); 
    c->addPar (Command::Bool,true,"Sort children of branch in revers order if set");
    modelCommands.append(c);

    c=new Command ("toggleFlag",Command::Branch); 
    c->addPar (Command::String,false,"Name of flag to toggle");
    modelCommands.append(c);

    c=new Command ("toggleFrameIncludeChildren",Command::Branch); 
    modelCommands.append(c);

    c=new Command ("toggleScroll",Command::Branch); 
    modelCommands.append(c);

    c=new Command ("toggleTarget",Command::Branch); 
    modelCommands.append(c);

    c=new Command ("toggleTask",Command::Branch); 
    modelCommands.append(c);

    c=new Command ("undo",Command::Any); 
    modelCommands.append(c);

    c=new Command ("unscroll",Command::Branch); 
    modelCommands.append(c);

    c=new Command ("unscrollChildren",Command::Branch); 
    modelCommands.append(c);

    c=new Command ("unselectAll",Command::Any); 
    modelCommands.append(c);

    c=new Command ("unsetFlag",Command::Branch); 
    c->addPar (Command::String,false,"Name of flag to unset");
    modelCommands.append(c);
}

void Main::cloneActionMapEditor( QAction *a, QKeySequence ks)
{
    a->setShortcut ( ks );
    a->setShortcutContext ( Qt::WidgetShortcut );
    mapEditorActions.append ( a );
}

// File Actions
void Main::setupFileActions()
{
    QString tag = tr ("&Map","Menu for file actions");
    QMenu *fileMenu = menuBar()->addMenu ( tag );

    QAction *a;
    a = new QAction(QPixmap( ":/filenew.png"), tr( "&New map","File menu" ),this);
    switchboard.addSwitch ("fileMapNew", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( fileNew() ) );
    cloneActionMapEditor( a, Qt::CTRL + Qt::Key_N);
    fileMenu->addAction(a);
    actionFileNew=a;

    a = new QAction(QPixmap( ":/filenewcopy.png"), tr( "&Copy to new map","File menu" ),this);
    switchboard.addSwitch ("fileMapNewCopy", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( fileNewCopy() ) );
    cloneActionMapEditor( a, Qt::CTRL + Qt::SHIFT + Qt::Key_C);
    fileMenu->addAction(a);
    actionFileNewCopy=a;

    a = new QAction( QPixmap( ":/fileopen.png"), tr( "&Open..." ,"File menu"),this);
    switchboard.addSwitch ("fileMapOpen", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( fileLoad() ) );
    cloneActionMapEditor( a, Qt::CTRL + Qt::Key_L);
    fileMenu->addAction(a);
    actionFileOpen=a;

    a = new QAction(tr( "&Restore last session","Edit menu" ), this);
    a->setShortcut (Qt::ALT + Qt::Key_R );
    switchboard.addSwitch ("fileMapRestore", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( fileRestoreSession() ) );
    fileMenu->addAction(a);
    actionListFiles.append(a);   
    actionCopy=a;

    fileLastMapsMenu = fileMenu->addMenu (tr("Open Recent","File menu"));
    fileMenu->addSeparator();

    a = new QAction( QPixmap( ":/filesave.png"), tr( "&Save...","File menu" ), this);
    switchboard.addSwitch ("fileMapSave", shortcutScope, a, tag);
    cloneActionMapEditor( a, Qt::CTRL + Qt::Key_S);
    fileMenu->addAction(a);
    restrictedMapActions.append( a );
    connect( a, SIGNAL( triggered() ), this, SLOT( fileSave() ) );
    actionFileSave=a;

    a = new QAction( QPixmap(":/filesaveas.png"), tr( "Save &As...","File menu" ), this);
    fileMenu->addAction(a);
    connect( a, SIGNAL( triggered() ), this, SLOT( fileSaveAs() ) );

    fileMenu->addSeparator();

    fileImportMenu = fileMenu->addMenu (tr("Import","File menu"));

    if (settings.value( "/mainwindow/showTestMenu",false).toBool()) 
    {
        a = new QAction( QPixmap(), tr("Firefox Bookmarks","Import filters"),this);
        connect( a, SIGNAL( triggered() ), this, SLOT( fileImportFirefoxBookmarks() ) );
        fileImportMenu->addAction(a);
    }

    a = new QAction("Freemind...",this);
    connect( a, SIGNAL( triggered() ), this, SLOT( fileImportFreemind() ) );
    fileImportMenu->addAction(a);

    a = new QAction("Mind Manager...",this);
    connect( a, SIGNAL( triggered() ), this, SLOT( fileImportMM() ) );
    fileImportMenu->addAction(a);

    a = new QAction( tr( "Import Dir%1","Import Filters").arg("..." + tr("(still experimental)") ), this);
    connect( a, SIGNAL( triggered() ), this, SLOT( fileImportDir() ) );
    fileImportMenu->addAction(a);

    fileExportMenu = fileMenu->addMenu (tr("Export","File menu"));

    a = new QAction( QPixmap(":/file-document-export.png"),tr("Repeat last export (%1)").arg("-"), this);
    switchboard.addSwitch ("fileExportLast", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( fileExportLast() ) );
    cloneActionMapEditor( a, Qt::ALT + Qt::Key_E );
    fileExportMenu->addAction(a);
    actionFileExportLast=a;

    a = new QAction(  tr("Webpage (HTML)...","File export menu"),this );
    connect( a, SIGNAL( triggered() ), this, SLOT( fileExportHTML() ) );
    fileExportMenu->addAction(a);

    a = new QAction( tr("Text (ASCII)...","File export menu"), this);
    connect( a, SIGNAL( triggered() ), this, SLOT( fileExportASCII() ) );
    fileExportMenu->addAction(a);

    a = new QAction( tr("Text with tasks","File export menu") + " (ASCII)... "  + tr("(still experimental)"), this);
    connect( a, SIGNAL( triggered() ), this, SLOT( fileExportASCIITasks() ) );
    fileExportMenu->addAction(a);

    a = new QAction( tr("Text (A&O report)...","Export format"), this);
    connect( a, SIGNAL( triggered() ), this, SLOT( fileExportAO() ) );
    fileExportMenu->addAction(a);

    a = new QAction( tr("Image%1","File export menu").arg("..."), this);
    connect( a, SIGNAL( triggered() ), this, SLOT( fileExportImage() ) );
    fileExportMenu->addAction(a);

    a = new QAction( tr("PDF%1","File export menu").arg("..."), this);
    connect( a, SIGNAL( triggered() ), this, SLOT( fileExportPDF() ) );
    fileExportMenu->addAction(a);

    a = new QAction( tr("SVG%1","File export menu").arg("..."), this);
    connect( a, SIGNAL( triggered() ), this, SLOT( fileExportSVG() ) );
    fileExportMenu->addAction(a);

    a = new QAction( "LibreOffice...", this);
    connect( a, SIGNAL( triggered() ), this, SLOT( fileExportImpress() ) );
    fileExportMenu->addAction(a);

    a = new QAction( "XML..." , this );
    connect( a, SIGNAL( triggered() ), this, SLOT( fileExportXML() ) );
    fileExportMenu->addAction(a);

    a = new QAction( tr("Spreadsheet") + " (CSV)... " + tr("(still experimental)"), this);
    connect( a, SIGNAL( triggered() ), this, SLOT( fileExportCSV() ) );
    fileExportMenu->addAction(a);

    a = new QAction( "Taskjuggler... " + tr("(still experimental)"), this );
    connect( a, SIGNAL( triggered() ), this, SLOT( fileExportTaskjuggler() ) );
    fileExportMenu->addAction(a);

    a = new QAction( "OrgMode... " + tr("(still experimental)"), this);
    connect( a, SIGNAL( triggered() ), this, SLOT( fileExportOrgMode() ) );
    fileExportMenu->addAction(a);

    a = new QAction( "LaTeX... " + tr("(still experimental)"), this);
    connect( a, SIGNAL( triggered() ), this, SLOT( fileExportLaTeX() ) );
    fileExportMenu->addAction(a);

    fileMenu->addSeparator();

    a = new QAction( tr( "Properties"), this);
    switchboard.addSwitch ("editMapProperties", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editMapProperties() ) );
    fileMenu->addAction(a);
    actionListFiles.append (a);   
    actionMapProperties = a;

    fileMenu->addSeparator();

    a = new QAction(QPixmap( ":/fileprint.png"), tr( "&Print")+QString("..."), this);
    a->setShortcut ( Qt::CTRL + Qt::Key_P);
    switchboard.addSwitch ("fileMapPrint", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( filePrint() ) );
    fileMenu->addAction(a);
    unrestrictedMapActions.append (a);
    actionFilePrint=a;

    a = new QAction( QPixmap(":/fileclose.png"), tr( "&Close Map","File menu" ), this);
    a->setShortcut (Qt::CTRL + Qt::Key_W );	 
    switchboard.addSwitch ("fileMapClose", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( fileCloseMap() ) );
    fileMenu->addAction(a);

    a = new QAction(QPixmap(":/exit.png"), tr( "E&xit","File menu"), this);
    a->setShortcut (Qt::CTRL + Qt::Key_Q );	  
    switchboard.addSwitch ("fileExit", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( fileExitVYM() ) );
    fileMenu->addAction(a);
}


//Edit Actions
void Main::setupEditActions()
{
    QString tag = tr("E&dit","Edit menu") ;
    QMenu *editMenu = menuBar()->addMenu( tag );

    QAction *a;
    a = new QAction( QPixmap( ":/undo.png"), tr( "&Undo","Edit menu" ),this);
    a->setShortcut (Qt::CTRL + Qt::Key_Z);
    a->setShortcutContext (Qt::WidgetShortcut);
    a->setEnabled (false);
    editMenu->addAction(a);
    mapEditorActions.append( a );
    restrictedMapActions.append( a );
    switchboard.addSwitch ("mapUndo", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editUndo() ) );
    actionUndo=a;

    a = new QAction( QPixmap( ":/redo.png"), tr( "&Redo","Edit menu" ), this); 
    a->setShortcut (Qt::CTRL + Qt::Key_Y);
    a->setShortcutContext (Qt::WidgetShortcut);
    editMenu->addAction(a);
    restrictedMapActions.append( a );
    mapEditorActions.append( a );
    switchboard.addSwitch ("mapRedo", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editRedo() ) );
    actionRedo=a;

    editMenu->addSeparator();
    a = new QAction(QPixmap( ":/editcopy.png"), tr( "&Copy","Edit menu" ), this);
    a->setShortcut (Qt::CTRL + Qt::Key_C );
    a->setShortcutContext (Qt::WidgetShortcut);
    a->setEnabled (false);
    editMenu->addAction(a);
    unrestrictedMapActions.append ( a );
    mapEditorActions.append( a );
    switchboard.addSwitch ("mapCopy", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editCopy() ) );
    actionCopy=a;

    a = new QAction(QPixmap( ":/editcut.png" ), tr( "Cu&t","Edit menu" ), this);
    a->setShortcut (Qt::CTRL + Qt::Key_X );
    a->setEnabled (false);
    a->setShortcutContext (Qt::WidgetShortcut);
    editMenu->addAction(a);
    restrictedMapActions.append( a );
    mapEditorActions.append( a );
    restrictedMapActions.append( a );
    switchboard.addSwitch ("mapCut", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editCut() ) );
    addAction (a);
    actionCut=a;

    a = new QAction(QPixmap( ":/editpaste.png"), tr( "&Paste","Edit menu" ),this);
    connect( a, SIGNAL( triggered() ), this, SLOT( editPaste() ) );
    a->setShortcut (Qt::CTRL + Qt::Key_V );
    a->setShortcutContext (Qt::WidgetShortcut);
    a->setEnabled (false);
    editMenu->addAction(a);
    restrictedMapActions.append( a );
    mapEditorActions.append( a );
    switchboard.addSwitch ("mapPaste", shortcutScope, a, tag);
    actionPaste=a;

    // Shortcut to delete selection
    a = new QAction( tr( "Delete Selection","Edit menu" ),this);
    a->setShortcut ( Qt::Key_Delete);		 
    a->setShortcutContext (Qt::WindowShortcut);
    switchboard.addSwitch ("mapDelete", shortcutScope, a, tag);
    addAction (a);
    connect( a, SIGNAL( triggered() ), this, SLOT( editDeleteSelection() ) );
    editMenu->addAction(a);
    actionListItems.append (a);
    actionDelete=a;

    // Shortcut to add attribute
    a= new QAction(tr( "Add attribute" ), this);
    if (settings.value( "/mainwindow/showTestMenu",false).toBool() )
    {
	//a->setShortcut ( Qt::Key_Q);	
	a->setShortcutContext (Qt::WindowShortcut);
        switchboard.addSwitch ("mapAddAttribute", shortcutScope, a, tag);
    }
    connect( a, SIGNAL( triggered() ), this, SLOT( editAddAttribute() ) );
    editMenu->addAction(a);
    actionAddAttribute= a;


    // Shortcut to add mapcenter
    a= new QAction(QPixmap(":/newmapcenter.png"),tr( "Add mapcenter","Canvas context menu" ), this);
    a->setShortcut ( Qt::Key_C);    
    a->setShortcutContext (Qt::WindowShortcut);
    switchboard.addSwitch ("mapAddCenter", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editAddMapCenter() ) );
    editMenu->addAction(a);
    actionListFiles.append (a);
    actionAddMapCenter = a;


    // Shortcut to add branch
    a = new QAction(QPixmap(":/newbranch.png"), tr( "Add branch as child","Edit menu" ), this);
    switchboard.addSwitch ("mapEditNewBranch", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editNewBranch() ) );
    cloneActionMapEditor( a, Qt::Key_A );
    taskEditorActions.append( a );
    actionListBranches.append(a);
    actionAddBranch=a;


    // Add branch by inserting it at selection
    a = new QAction(tr( "Add branch (insert)","Edit menu" ),this);
    a->setShortcut ( Qt::ALT + Qt::Key_A );	 
    a->setShortcutContext (Qt::WindowShortcut);
    switchboard.addSwitch ("mapEditAddBranchBefore", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editNewBranchBefore() ) );
    editMenu->addAction(a);
    actionListBranches.append(a);
    actionAddBranchBefore=a;

    // Add branch above
    a = new QAction(tr( "Add branch above","Edit menu" ), this);
    a->setShortcut (Qt::SHIFT+Qt::Key_Insert );	  
    a->setShortcutContext (Qt::WindowShortcut);
    switchboard.addSwitch ("mapEditAddBranchAbove", shortcutScope, a, tag);
    addAction (a);
    connect( a, SIGNAL( triggered() ), this, SLOT( editNewBranchAbove() ) );
    a->setEnabled (false);
    actionListBranches.append(a);
    actionAddBranchAbove=a;

    a = new QAction(tr( "Add branch above","Edit menu" ), this);
    a->setShortcut (Qt::SHIFT+Qt::Key_A );	 
    a->setShortcutContext (Qt::WindowShortcut);
    switchboard.addSwitch ("mapEditAddBranchAboveAlt", shortcutScope, a, tag);
    addAction (a);
    connect( a, SIGNAL( triggered() ), this, SLOT( editNewBranchAbove() ) );
    actionListBranches.append(a);
    editMenu->addAction(a);

    // Add branch below 
    a = new QAction(tr( "Add branch below","Edit menu" ), this);
    a->setShortcut (Qt::CTRL +Qt::Key_Insert );	  
    a->setShortcutContext (Qt::WindowShortcut);
    switchboard.addSwitch ("mapEditAddBranchBelow", shortcutScope, a, tag);
    addAction (a);
    connect( a, SIGNAL( triggered() ), this, SLOT( editNewBranchBelow() ) );
    a->setEnabled (false);
    actionListBranches.append(a);

    a = new QAction(tr( "Add branch below","Edit menu" ), this);
    a->setShortcut (Qt::CTRL +Qt::Key_A );	 
    a->setShortcutContext (Qt::WindowShortcut);
    switchboard.addSwitch ("mapEditAddBranchBelowAlt", shortcutScope, a, tag);
    addAction (a);
    connect( a, SIGNAL( triggered() ), this, SLOT( editNewBranchBelow() ) );
    actionListBranches.append(a);
    actionAddBranchBelow=a; 

    a = new QAction(QPixmap(":/up.png" ), tr( "Move branch up","Edit menu" ), this);
    a->setShortcut (Qt::Key_PageUp );		
    a->setShortcutContext (Qt::WidgetShortcut);
    //a->setEnabled (false);
    mapEditorActions.append( a );
    taskEditorActions.append( a );
    restrictedMapActions.append( a );
    actionListBranches.append (a);
    editMenu->addAction(a);
    switchboard.addSwitch ("mapEditMoveBranchUp", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editMoveUp() ) );
    actionMoveUp=a;

    a = new QAction( QPixmap( ":/down.png"), tr( "Move branch down","Edit menu" ),this);
    a->setShortcut ( Qt::Key_PageDown );	  
    a->setShortcutContext (Qt::WidgetShortcut);
    //a->setEnabled (false);
    mapEditorActions.append( a );
    taskEditorActions.append( a );
    restrictedMapActions.append( a );
    actionListBranches.append (a);
    editMenu->addAction(a);
    switchboard.addSwitch ("mapEditMoveBranchDown", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editMoveDown() ) );
    actionMoveDown=a;

    a = new QAction(QPixmap(), tr( "&Detach","Context menu" ),this);
    a->setStatusTip ( tr( "Detach branch and use as mapcenter","Context menu" ) );
    a->setShortcut ( Qt::Key_D );		 
    switchboard.addSwitch ("mapDetachBranch", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editDetach() ) );
    editMenu->addAction(a);
    actionListBranches.append (a);
    actionDetach=a;

    a = new QAction( QPixmap(":/editsort.png" ), tr( "Sort children","Edit menu" ), this );
    a->setEnabled (true);
    a->setShortcut ( Qt::Key_O );		  
    switchboard.addSwitch ("mapSortBranches", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editSortChildren() ) );
    editMenu->addAction(a);
    actionListBranches.append (a);
    actionSortChildren=a;

    a = new QAction( QPixmap(":/editsortback.png" ), tr( "Sort children backwards","Edit menu" ), this );
    a->setEnabled (true);
    a->setShortcut ( Qt::SHIFT + Qt::Key_O );		
    switchboard.addSwitch ("mapSortBranchesReverse", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editSortBackChildren() ) );
    editMenu->addAction(a);
    actionListBranches.append (a);
    actionSortBackChildren=a;	

    a = new QAction( QPixmap(":/flag-scrolled-right.png"), tr( "Scroll branch","Edit menu" ), this);
    a->setShortcut ( Qt::Key_S );		  
    switchboard.addSwitch ("mapToggleScroll", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editToggleScroll() ) );
    editMenu->addAction(a);
    actionListBranches.append (a);
    a->setEnabled (false);
    a->setCheckable(true);
    addAction (a);
    actionListBranches.append(a);
    actionToggleScroll=a;

    a = new QAction( tr( "Unscroll children","Edit menu" ), this);
    editMenu->addAction(a);
    connect( a, SIGNAL( triggered() ), this, SLOT( editUnscrollChildren() ) );
    actionListBranches.append (a);

    a = new QAction( tr( "Grow selection","Edit menu" ), this);
    a->setShortcut ( Qt::CTRL + Qt::Key_Plus);	  
    switchboard.addSwitch ("mapGrowSelection", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editGrowSelectionSize() ) );
    editMenu->addAction(a);
    actionListBranches.append (a);
    actionListItems.append (a);
    actionGrowSelectionSize=a;

    a = new QAction( tr( "Shrink selection","Edit menu" ), this);
    a->setShortcut ( Qt::CTRL + Qt::Key_Minus);	   
    switchboard.addSwitch ("mapShrinkSelection", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editShrinkSelectionSize() ) );
    editMenu->addAction(a);
    actionListBranches.append (a);
    actionListItems.append (a);
    actionShrinkSelectionSize=a;

    a = new QAction( tr( "Reset selection size","Edit menu" ), this);
    a->setShortcut ( Qt::CTRL + Qt::Key_0);	    
    switchboard.addSwitch ("mapResetSelectionSize", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editResetSelectionSize() ) );
    editMenu->addAction(a);
    actionListBranches.append (a);
    actionListItems.append (a);
    actionResetSelectionSize=a;

    editMenu->addSeparator();

    a = new QAction( QPixmap(), "TE: " + tr( "Collapse one level","Edit menu" ), this);
    a->setShortcut ( Qt::Key_Less + Qt::CTRL);	
    switchboard.addSwitch ("mapCollapseOneLevel", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editCollapseOneLevel() ) );
    editMenu->addAction(a);
    a->setEnabled (false);
    a->setCheckable(false);
    actionListBranches.append(a);
    addAction (a);
    actionCollapseOneLevel=a;  

    a = new QAction( QPixmap(), "TE: " + tr( "Collapse unselected levels","Edit menu" ), this);
    a->setShortcut ( Qt::Key_Less);	  
    switchboard.addSwitch ("mapCollapseUnselectedLevels", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editCollapseUnselected() ) );
    editMenu->addAction(a);
    a->setEnabled (false);
    a->setCheckable(false);
    actionListBranches.append(a);
    addAction (a);
    actionCollapseUnselected=a;

    a = new QAction( QPixmap(), tr( "Expand all branches","Edit menu" ), this);
    connect( a, SIGNAL( triggered() ), this, SLOT( editExpandAll() ) );
    actionExpandAll=a;
    actionExpandAll->setEnabled (false);
    actionExpandAll->setCheckable(false);
    actionListBranches.append(actionExpandAll);
    addAction (a);

    a = new QAction( QPixmap(), tr( "Expand one level","Edit menu" ), this);
    a->setShortcut ( Qt::Key_Greater );	    
    switchboard.addSwitch ("mapExpandOneLevel", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editExpandOneLevel() ) );
    a->setEnabled (false);
    a->setCheckable(false);
    addAction (a);
    actionListBranches.append(a);
    actionExpandOneLevel=a;

    tag = tr("References Context menu","Shortcuts");
    a = new QAction( QPixmap(":/flag-url.png"), tr( "Open URL","Edit menu" ), this);
    a->setShortcut (Qt::SHIFT + Qt::Key_U );
    switchboard.addSwitch ("mapOpenUrl", shortcutScope, a, tag);
    addAction(a);
    connect( a, SIGNAL( triggered() ), this, SLOT( editOpenURL() ) );
    actionListBranches.append (a);
    actionOpenURL=a;

    a = new QAction( tr( "Open URL in new tab","Edit menu" ), this);
    //a->setShortcut (Qt::CTRL+Qt::Key_U );
    switchboard.addSwitch ("mapOpenUrlTab", shortcutScope, a, tag);
    addAction(a);
    connect( a, SIGNAL( triggered() ), this, SLOT( editOpenURLTab() ) );
    actionListBranches.append (a);
    actionOpenURLTab=a;

    a = new QAction( tr( "Open all URLs in subtree (including scrolled branches)","Edit menu" ), this);
    a->setShortcut ( Qt::CTRL + Qt::Key_U );
    switchboard.addSwitch ("mapOpenUrlsSubTree", shortcutScope, a, tag);
    addAction(a);
    connect( a, SIGNAL( triggered() ), this, SLOT( editOpenMultipleVisURLTabs() ) );
    actionListBranches.append(a);
    actionOpenMultipleVisURLTabs=a;

    a = new QAction( tr( "Open all URLs in subtree","Edit menu" ), this);
    switchboard.addSwitch ("mapOpenMultipleUrlTabs", shortcutScope, a, tag);
    addAction(a);
    connect( a, SIGNAL( triggered() ), this, SLOT( editOpenMultipleURLTabs() ) );
    actionListBranches.append(a);
    actionOpenMultipleURLTabs=a;

    a = new QAction(QPixmap(), tr( "Extract URLs from note","Edit menu"), this);
    a->setShortcut ( Qt::SHIFT + Qt::Key_N );
    a->setShortcutContext (Qt::WindowShortcut);
    switchboard.addSwitch ("mapUrlsFromNote", shortcutScope, a, tag);
    addAction(a);
    connect( a, SIGNAL( triggered() ), this, SLOT( editNote2URLs() ) );
    actionListBranches.append(a);
    actionGetURLsFromNote=a;

    a = new QAction(QPixmap(":/flag-urlnew.png"), tr( "Edit URL...","Edit menu"), this);
    a->setShortcut ( Qt::Key_U );
    a->setShortcutContext (Qt::WindowShortcut);
    switchboard.addSwitch ("mapEditURL", shortcutScope, a, tag);
    addAction(a);
    connect( a, SIGNAL( triggered() ), this, SLOT( editURL() ) );
    actionListBranches.append(a);
    actionURLNew=a;

    a = new QAction(QPixmap(), tr( "Edit local URL...","Edit menu"), this);
    //a->setShortcut (Qt::SHIFT +  Qt::Key_U );
    a->setShortcutContext (Qt::WindowShortcut);
    switchboard.addSwitch ("mapEditLocalURL", shortcutScope, a, tag);
    addAction(a);
    connect( a, SIGNAL( triggered() ), this, SLOT( editLocalURL() ) );
    actionListBranches.append(a);
    actionLocalURL=a;

    a = new QAction( tr( "Use heading for URL","Edit menu" ), this);
    a->setShortcut ( Qt::ALT + Qt::Key_U );
    a->setShortcutContext (Qt::ApplicationShortcut);
    a->setEnabled (false);
    switchboard.addSwitch ("mapHeading2URL", shortcutScope, a, tag);
    addAction(a);
    connect( a, SIGNAL( triggered() ), this, SLOT( editHeading2URL() ) );
    actionListBranches.append(a);
    actionHeading2URL=a;

    tag = tr("Bugzilla handling","Shortcuts");
    a = new QAction(tr( "Create URL to SUSE Bugzilla","Edit menu" ), this);
    a->setEnabled (false);
    actionListBranches.append(a);
    a->setShortcut ( Qt::Key_B );
    a->setShortcutContext (Qt::WindowShortcut);
    switchboard.addSwitch ("mapUseHeadingForURL", shortcutScope, a, tag);
    addAction(a);
    connect( a, SIGNAL( triggered() ), this, SLOT( editBugzilla2URL() ) );
    actionListBranches.append(a);
    actionBugzilla2URL=a;

    a = new QAction(tr( "Get data from SUSE Bugzilla","Edit menu" ), this);
    a->setShortcut ( Qt::Key_B + Qt::SHIFT);
    a->setShortcutContext (Qt::WindowShortcut);
    switchboard.addSwitch ("mapUpdateFromBugzilla", shortcutScope, a, tag);
    addAction(a);
    connect( a, SIGNAL( triggered() ), this, SLOT( getBugzillaData() ) );
    actionListBranches.append(a);
    actionGetBugzillaData=a;

    a = new QAction(tr( "Get data from SUSE Bugzilla for subtree","Edit menu" ), this);
    a->setShortcut ( Qt::Key_B + Qt::CTRL);
    a->setShortcutContext (Qt::WindowShortcut);
    switchboard.addSwitch ("mapUpdateSubTreeFromBugzilla", shortcutScope, a, tag);
    addAction(a);
    connect( a, SIGNAL( triggered() ), this, SLOT( getBugzillaDataSubtree() ) );
    actionListBranches.append(a);
    actionGetBugzillaDataSubtree=a;

    tag = tr("SUSE Fate tool handling","Shortcuts");
    a = new QAction(tr( "Create URL to SUSE FATE tool","Edit menu" ), this);
    a->setEnabled (false);
    switchboard.addSwitch ("mapFate2URL", shortcutScope, a, tag);
    actionListBranches.append(a);
    connect( a, SIGNAL( triggered() ), this, SLOT( editFATE2URL() ) );
    actionListBranches.append(a);
    actionFATE2URL=a;

    tag = tr("vymlinks - linking maps","Shortcuts");
    a = new QAction(QPixmap(":/flag-vymlink.png"), tr( "Open linked map","Edit menu" ), this);
    a->setEnabled (false);
    switchboard.addSwitch ("mapOpenVymLink", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editOpenVymLink() ) );
    actionListBranches.append (a);
    actionOpenVymLink=a;

    a = new QAction(QPixmap(":/flag-vymlink.png"), tr( "Open linked map in background tab","Edit menu" ), this);
    a->setEnabled (false);
    switchboard.addSwitch ("mapOpenVymLink", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editOpenVymLinkBackground() ) );
    actionListBranches.append (a);
    actionOpenVymLinkBackground=a;

    a = new QAction(QPixmap(), tr( "Open all vym links in subtree","Edit menu" ), this);
    a->setEnabled (false);
    switchboard.addSwitch ("mapOpenMultipleVymLinks", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editOpenMultipleVymLinks() ) );
    actionListBranches.append(a);
    actionOpenMultipleVymLinks=a;


    a = new QAction(QPixmap(":/flag-vymlinknew.png"), tr( "Edit vym link...","Edit menu" ), this);
    a->setEnabled (false);
    switchboard.addSwitch ("mapEditVymLink", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editVymLink() ) );
    actionListBranches.append(a);
    actionEditVymLink=a;

    a = new QAction(tr( "Delete vym link","Edit menu" ),this);
    a->setEnabled (false);
    switchboard.addSwitch ("mapDeleteVymLink", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editDeleteVymLink() ) );
    actionListBranches.append(a);
    actionDeleteVymLink=a;

    tag = tr("Exports","Shortcuts");
    a = new QAction(QPixmap(":/flag-hideexport.png"), tr( "Hide in exports","Edit menu" ), this);
    a->setShortcut (Qt::Key_H );
    a->setShortcutContext (Qt::WindowShortcut);
    a->setCheckable(true);
    a->setEnabled (false);
    addAction(a);
    switchboard.addSwitch ("mapToggleHideExport", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editToggleHideExport() ) );
    actionListItems.append (a);
    actionToggleHideExport=a;

    tag = tr("Tasks","Shortcuts");
    a = new QAction(QPixmap(":/flag-task.png"), tr( "Toggle task","Edit menu" ), this);
    a->setShortcut (Qt::Key_W + Qt::SHIFT);
    a->setShortcutContext (Qt::WindowShortcut);
    a->setCheckable(true);
    a->setEnabled (false);
    addAction(a);
    switchboard.addSwitch ("mapToggleTask", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editToggleTask() ) );
    actionListBranches.append (a);
    actionToggleTask=a;

    a = new QAction(QPixmap(), tr( "Cycle task status","Edit menu" ), this);
    a->setShortcut (Qt::Key_W );
    a->setShortcutContext (Qt::WindowShortcut);
    a->setCheckable(false);
    a->setEnabled (false);
    addAction(a);
    switchboard.addSwitch ("mapCycleTaskStatus", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editCycleTaskStatus() ) );
    actionListBranches.append (a);
    actionCycleTaskStatus=a;

    a = new QAction(QPixmap(), tr( "Reset sleep","Task sleep" ), this);
    a->setShortcutContext (Qt::WindowShortcut);
    a->setCheckable(false);
    a->setEnabled (false);
    a->setData (0);
    addAction(a);
    switchboard.addSwitch ("mapResetSleep", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editTaskSleepN() ) );
    actionListBranches.append (a);
    actionTaskSleep0=a;

    a = new QAction(QPixmap(), tr( "Sleep %1 days","Task sleep" ).arg("n")+"...", this);
    a->setShortcutContext (Qt::WindowShortcut);
    a->setShortcut (Qt::Key_Q + Qt::SHIFT);
    a->setCheckable(false);
    a->setEnabled (false);
    a->setData (-1);
    addAction(a);
    switchboard.addSwitch ("mapTaskSleepN", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editTaskSleepN() ) );
    actionListBranches.append (a);
    actionTaskSleepN=a;

    a = new QAction(QPixmap(), tr( "Sleep %1 day","Task sleep" ).arg(1), this);
    a->setShortcutContext (Qt::WindowShortcut);
    a->setCheckable(false);
    a->setEnabled (false);
    a->setData (1);
    addAction(a);
    switchboard.addSwitch ("mapTaskSleep1", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editTaskSleepN() ) );
    actionListBranches.append (a);
    actionTaskSleep1=a;

    a = new QAction(QPixmap(), tr( "Sleep %1 days","Task sleep" ).arg(2), this);
    a->setShortcutContext (Qt::WindowShortcut);
    a->setCheckable(false);
    a->setEnabled (false);
    a->setData (2);
    addAction(a);
    switchboard.addSwitch ("mapTaskSleep2", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editTaskSleepN() ) );
    actionListBranches.append (a);
    actionTaskSleep2=a;

    a = new QAction(QPixmap(), tr( "Sleep %1 days","Task sleep" ).arg(3), this);
    a->setShortcutContext (Qt::WindowShortcut);
    a->setCheckable(false);
    a->setEnabled (false);
    a->setData (3);
    addAction(a);
    switchboard.addSwitch ("mapTaskSleep3", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editTaskSleepN() ) );
    actionListBranches.append (a);
    actionTaskSleep3=a;

    a = new QAction(QPixmap(), tr( "Sleep %1 days","Task sleep" ).arg(5), this); 
    a->setShortcutContext (Qt::WindowShortcut);
    a->setCheckable(false);
    a->setEnabled (false);
    a->setData (5);
    addAction(a);
    switchboard.addSwitch ("mapTaskSleep5", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editTaskSleepN() ) );
    actionListBranches.append (a);
    actionTaskSleep5=a;

    a = new QAction(QPixmap(), tr( "Sleep %1 days","Task sleep" ).arg(7), this);
    a->setShortcutContext (Qt::WindowShortcut);
    a->setCheckable(false);
    a->setEnabled (false);
    a->setData (7);
    addAction(a);
    switchboard.addSwitch ("mapTaskSleep7", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editTaskSleepN() ) );
    actionListBranches.append (a);
    actionTaskSleep7=a;

    a = new QAction(QPixmap(), tr( "Sleep %1 weeks","Task sleep" ).arg(2), this);
    a->setShortcutContext (Qt::WindowShortcut);
    a->setCheckable(false);
    a->setEnabled (false);
    a->setData (14);
    addAction(a);
    switchboard.addSwitch ("mapTaskSleep14", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editTaskSleepN() ) );
    actionListBranches.append (a);
    actionTaskSleep14=a;

    a = new QAction(QPixmap(), tr( "Sleep %1 weeks","Task sleep" ).arg(4), this);
    a->setShortcutContext (Qt::WindowShortcut);
    a->setCheckable(false);
    a->setEnabled (false);
    a->setData (28);
    addAction(a);
    switchboard.addSwitch ("mapTaskSleep28", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editTaskSleepN() ) );
    actionListBranches.append (a);
    actionTaskSleep28=a;

    // Import at selection (adding to selection)
    a = new QAction( tr( "Add map (insert)","Edit menu" ),this);
    connect( a, SIGNAL( triggered() ), this, SLOT( editImportAdd() ) );
    a->setEnabled (false);
    actionListBranches.append(a);
    actionImportAdd=a;

    // Import at selection (replacing selection)
    a = new QAction( tr( "Add map (replace)","Edit menu" ), this);
    connect( a, SIGNAL( triggered() ), this, SLOT( editImportReplace() ) );
    a->setEnabled (false);
    actionListBranches.append(a);
    actionImportReplace=a;

    // Save selection 
    a = new QAction( tr( "Save selection","Edit menu" ), this);
    connect( a, SIGNAL( triggered() ), this, SLOT( editSaveBranch() ) );
    a->setEnabled (false);
    actionListBranches.append(a);
    actionSaveBranch=a;

    tag = tr("Removing parts of a map","Shortcuts");

    // Only remove branch, not its children
    a = new QAction(tr( "Remove only branch and keep its children ","Edit menu" ), this);
    a->setShortcut (Qt::ALT + Qt::Key_X );
    connect( a, SIGNAL( triggered() ), this, SLOT( editDeleteKeepChildren() ) );
    a->setEnabled (false);
    addAction(a);
    switchboard.addSwitch ("mapDeleteKeepChildren", shortcutScope, a, tag);
    actionListBranches.append(a);
    actionDeleteKeepChildren=a;

    // Only remove children of a branch
    a = new QAction( tr( "Remove children","Edit menu" ), this);
    a->setShortcut (Qt::SHIFT + Qt::Key_X );
    addAction(a);
    switchboard.addSwitch ("mapDeleteChildren", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editDeleteChildren() ) );
    a->setEnabled (false);
    addAction (a);
    actionListBranches.append(a);
    actionDeleteChildren=a;

    tag = tr("Various","Shortcuts");
    a = new QAction(tr( "Add timestamp","Edit menu" ), this);
    a->setEnabled (false);
    actionListBranches.append(a);
    a->setShortcut ( Qt::Key_T ); 
    a->setShortcutContext (Qt::WindowShortcut);
    addAction(a);
    switchboard.addSwitch ("mapAddTimestamp", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editAddTimestamp() ) );
    actionListBranches.append (a);
    actionAddTimestamp=a;

    a = new QAction(tr( "Map properties...","Edit menu" ),this);
    a->setEnabled (true);
    connect( a, SIGNAL( triggered() ), this, SLOT( editMapProperties() ) );
    actionListFiles.append (a);
    actionMapInfo=a;

    a = new QAction( tr( "Add   ...","Edit menu" ), this);
    a->setShortcutContext (Qt::WindowShortcut);
    a->setShortcut (Qt::Key_I + Qt::SHIFT);    
    addAction(a);
    switchboard.addSwitch ("mapLoadImage", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editLoadImage() ) );
    actionLoadImage=a;

    a = new QAction( tr( "Property window","Dialog to edit properties of selection" )+QString ("..."), this);
    a->setShortcut ( Qt::Key_P );	 
    a->setShortcutContext (Qt::WindowShortcut);
    a->setCheckable (true);
    addAction(a);
    switchboard.addSwitch ("mapTogglePropertEditor", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( windowToggleProperty() ) );
    actionViewTogglePropertyEditor=a;
}

// Select Actions
void Main::setupSelectActions()
{
    QString tag = tr("Selections","Shortcuts");
    QMenu *selectMenu = menuBar()->addMenu( tr("Select","Select menu") );
    QAction *a;
    a = new QAction( QPixmap(":/flag-target.png"), tr( "Toggle target...","Edit menu"), this);
    a->setShortcut (Qt::SHIFT + Qt::Key_T );		
    a->setCheckable(true);
    selectMenu->addAction(a);
    switchboard.addSwitch ("mapToggleTarget", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editToggleTarget() ) );
    actionListBranches.append (a);
    actionToggleTarget=a;

    a = new QAction( QPixmap(":/flag-target.png"), tr( "Goto target...","Edit menu"), this);
    a->setShortcut (Qt::Key_G );		
    selectMenu->addAction(a);
    switchboard.addSwitch ("mapGotoTarget", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editGoToTarget() ) );
    actionListBranches.append (a);
    actionGoToTarget=a;

    a = new QAction( QPixmap(":/flag-target.png"), tr( "Move to target...","Edit menu"), this);
    a->setShortcut (Qt::Key_M );		
    selectMenu->addAction(a);
    switchboard.addSwitch ("mapMoveToTarget", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editMoveToTarget() ) );
    actionListBranches.append (a);
    actionMoveToTarget=a;

    a = new QAction( QPixmap(":/selectprevious.png"), tr( "Select previous","Edit menu"), this);
    a->setShortcut (Qt::CTRL+ Qt::Key_O );	
    a->setShortcutContext (Qt::WidgetShortcut);
    selectMenu->addAction(a);
    actionListFiles.append (a);
    mapEditorActions.append ( a );
    switchboard.addSwitch ("mapSelectPrevious", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editSelectPrevious() ) );
    actionSelectPrevious=a;

    a = new QAction( QPixmap(":/selectnext.png"), tr( "Select next","Edit menu"), this);
    a->setShortcut (Qt::CTRL + Qt::Key_I );
    a->setShortcutContext (Qt::WidgetShortcut);
    selectMenu->addAction(a);
    actionListFiles.append (a);
    mapEditorActions.append ( a );
    switchboard.addSwitch ("mapSelectNext", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editSelectNext() ) );
    actionSelectNext=a;

    a = new QAction( tr( "Unselect all","Edit menu"), this);
    //a->setShortcut (Qt::CTRL + Qt::Key_I );
    selectMenu->addAction(a);
    switchboard.addSwitch ("mapSelectNothing", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editSelectNothing() ) );
    actionListFiles.append (a);
    actionSelectNothing=a;

    tag = tr("Search functions","Shortcuts");
    a = new QAction( QPixmap(":/find.png"), tr( "Find...","Edit menu"), this);
    a->setShortcut (Qt::CTRL + Qt::Key_F );	
    selectMenu->addAction(a);
    switchboard.addSwitch ("mapFind", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editOpenFindResultWidget() ) );
    actionListFiles.append(a);
    actionFind=a;

    a = new QAction( QPixmap(":/find.png"), tr( "Find...","Edit menu"), this);
    a->setShortcut (Qt::Key_Slash );	
    selectMenu->addAction(a);
    switchboard.addSwitch ("mapFindAlt", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( editOpenFindResultWidget() ) );
    actionListFiles.append(a);

    a = new QAction( tr( "Find duplicate URLs","Edit menu"), this);
    a->setShortcut (Qt::SHIFT + Qt::Key_F);	
    switchboard.addSwitch ("mapFindDuplicates", shortcutScope, a, tag);
    if (settings.value( "/mainwindow/showTestMenu",false).toBool() ) 
	selectMenu->addAction (a);
    connect( a, SIGNAL( triggered() ), this, SLOT( editFindDuplicateURLs() ) );

}

// Format Actions
void Main::setupFormatActions()
{
    QMenu *formatMenu = menuBar()->addMenu (tr ("F&ormat","Format menu"));

    QString tag = tr("Formatting","Shortcuts");

    QAction *a;
    QPixmap pix( 16,16);
    pix.fill (Qt::black);
    a= new QAction(pix, tr( "Set &Color" )+QString("..."), this);
    formatMenu->addAction(a);
    switchboard.addSwitch ("mapFormatColor", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( formatSelectColor() ) );
    actionFormatColor=a;

    a= new QAction( QPixmap(":/formatcolorpicker.png"), tr( "Pic&k color","Edit menu" ), this);
    a->setShortcut (Qt::CTRL + Qt::Key_K );
    formatMenu->addAction(a);
    switchboard.addSwitch ("mapFormatColorPicker", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( formatPickColor() ) );
    a->setEnabled (false);
    actionListBranches.append(a);
    actionFormatPickColor=a;

    a= new QAction(QPixmap(":/formatcolorbranch.png"), tr( "Color &branch","Edit menu" ), this);
    //a->setShortcut (Qt::CTRL + Qt::Key_B + Qt::SHIFT);
    formatMenu->addAction(a);
    switchboard.addSwitch ("mapFormatColorBranch", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( formatColorBranch() ) );
    a->setEnabled (false);
    actionListBranches.append(a);
    actionFormatColorBranch=a;

    a= new QAction(QPixmap(":/formatcolorsubtree.png"), tr( "Color sub&tree","Edit menu" ), this);
    //a->setShortcut (Qt::CTRL + Qt::Key_B);	// Color subtree
    formatMenu->addAction(a);
    switchboard.addSwitch ("mapFormatColorSubtree", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( formatColorSubtree() ) );
    a->setEnabled (false);
    actionListBranches.append(a);
    actionFormatColorSubtree=a;

    formatMenu->addSeparator();

    a= new QAction( tr( "Select default font","Branch attribute" )+"...",  this);
    a->setCheckable(false);
    connect( a, SIGNAL( triggered() ), this, SLOT( formatSelectFont() ) );
    formatMenu->addAction (a);
    actionFormatFont=a;

    formatMenu->addSeparator();

    actionGroupFormatLinkStyles=new QActionGroup ( this);
    actionGroupFormatLinkStyles->setExclusive (true);
    a= new QAction( tr( "Linkstyle Line" ), actionGroupFormatLinkStyles);
    a->setCheckable(true);
    restrictedMapActions.append( a );
    formatMenu->addAction (a);
    connect( a, SIGNAL( triggered() ), this, SLOT( formatLinkStyleLine() ) );
    actionFormatLinkStyleLine=a;

    a= new QAction( tr( "Linkstyle Curve" ), actionGroupFormatLinkStyles);
    a->setCheckable(true);
    restrictedMapActions.append( a );
    formatMenu->addAction (a);
    connect( a, SIGNAL( triggered() ), this, SLOT( formatLinkStyleParabel() ) );
    actionFormatLinkStyleParabel=a;

    a= new QAction( tr( "Linkstyle Thick Line" ), actionGroupFormatLinkStyles );
    a->setCheckable(true);
    restrictedMapActions.append( a );
    formatMenu->addAction (a);
    connect( a, SIGNAL( triggered() ), this, SLOT( formatLinkStylePolyLine() ) );
    actionFormatLinkStylePolyLine=a;

    a= new QAction( tr( "Linkstyle Thick Curve" ), actionGroupFormatLinkStyles);
    a->setCheckable(true);
    a->setChecked (true);
    restrictedMapActions.append( a );
    formatMenu->addAction (a);
    formatMenu->addSeparator();
    connect( a, SIGNAL( triggered() ), this, SLOT( formatLinkStylePolyParabel() ) );
    actionFormatLinkStylePolyParabel=a;

    a = new QAction( tr( "Hide link if object is not selected","Branch attribute" ), this);
    a->setCheckable(true);
    connect( a, SIGNAL( triggered() ), this, SLOT( formatHideLinkUnselected() ) );
    actionListBranches.append (a);
    actionFormatHideLinkUnselected=a;

    a= new QAction( tr( "&Use color of heading for link","Branch attribute" ),  this);
    a->setCheckable(true);
    connect( a, SIGNAL( triggered() ), this, SLOT( formatToggleLinkColorHint() ) );
    formatMenu->addAction (a);
    actionFormatLinkColorHint=a;

    pix.fill (Qt::white);
    a= new QAction( pix, tr( "Set &Link Color")+"..." , this  );
    formatMenu->addAction (a);
    connect( a, SIGNAL( triggered() ), this, SLOT( formatSelectLinkColor() ) );
    actionFormatLinkColor=a;

    a= new QAction( pix, tr( "Set &Selection Color")+"...", this  );
    formatMenu->addAction (a);
    connect( a, SIGNAL( triggered() ), this, SLOT( formatSelectSelectionColor() ) );
    actionFormatSelectionColor=a;

    a= new QAction( pix, tr( "Set &Background Color" )+"...", this );
    formatMenu->addAction (a);
    connect( a, SIGNAL( triggered() ), this, SLOT( formatSelectBackColor() ) );
    actionFormatBackColor=a;

    a= new QAction( pix, tr( "Set &Background image" )+"...", this );
    formatMenu->addAction (a);
    connect( a, SIGNAL( triggered() ), this, SLOT( formatSelectBackImage() ) );
    actionFormatBackImage=a;
}

// View Actions
void Main::setupViewActions()
{
    QMenu *viewMenu = menuBar()->addMenu ( tr( "&View" ));
    toolbarsMenu=viewMenu->addMenu (tr("Toolbars","Toolbars overview in view menu") );
    QString tag = tr("Views","Shortcuts");

    viewMenu->addSeparator();	

    QAction *a;
    a = new QAction( QPixmap(":/viewmag+.png"), tr( "Zoom in","View action" ), this);
    a->setShortcut(Qt::Key_Plus);
    viewMenu->addAction (a);
    switchboard.addSwitch ("mapZoomIn", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT(viewZoomIn() ) );
    actionListFiles.append (a);
    actionZoomIn=a;

    a = new QAction( QPixmap(":/viewmag-.png"), tr( "Zoom out","View action" ), this);
    a->setShortcut(Qt::Key_Minus);
    viewMenu->addAction (a);
    switchboard.addSwitch ("mapZoomOut", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( viewZoomOut() ) );
    actionListFiles.append (a);
    actionZoomOut=a;

    a = new QAction( QPixmap(":/rotate-ccw.png"), tr( "Rotate counterclockwise","View action" ), this);
    a->setShortcut( Qt::SHIFT + Qt::Key_R);
    viewMenu->addAction (a);
    switchboard.addSwitch ("mapRotateCounterClockwise", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( viewRotateCounterClockwise() ) );
    actionListFiles.append (a);
    actionRotateCounterClockwise=a;

    a = new QAction( QPixmap(":/rotate-cw.png"), tr( "Rotate rclockwise","View action" ), this);
    a->setShortcut(Qt::Key_R);
    viewMenu->addAction (a);
    switchboard.addSwitch ("mapRotateClockwise", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( viewRotateClockwise() ) );
    actionListFiles.append (a);
    actionRotateClockwise=a;

    a = new QAction(QPixmap(":/viewmag-reset.png"), tr( "reset Zoom","View action" ), this);
    a->setShortcut (Qt::Key_Comma);
    switchboard.addSwitch ("mapZoomReset", shortcutScope, a, tag);
    viewMenu->addAction (a);
    connect( a, SIGNAL( triggered() ), this, SLOT(viewZoomReset() ) );
    actionListFiles.append (a);
    actionZoomReset=a;

    a = new QAction( QPixmap(":/viewshowsel.png"), tr( "Center on selection","View action" ), this);
    a->setShortcut(Qt::Key_Period);
    viewMenu->addAction (a);
    switchboard.addSwitch ("mapCenterOn", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( viewCenter() ) );
    actionListFiles.append (a);
    actionCenterOn=a;

    viewMenu->addSeparator();	

    //a=noteEditorDW->toggleViewAction();
    a = new QAction(QPixmap(":/flag-note.png"), tr( "Note editor","View action" ),this);
    a->setShortcut ( Qt::Key_N );
    a->setShortcutContext (Qt::WidgetShortcut);
    a->setCheckable(true);
    viewMenu->addAction (a);
    mapEditorActions.append( a );
    switchboard.addSwitch ("mapToggleNoteEditor", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT(windowToggleNoteEditor() ) );
    actionViewToggleNoteEditor=a;

    //a=headingEditorDW->toggleViewAction();
    a = new QAction(QPixmap(":/headingeditor.png"), tr( "Heading editor","View action" ),this);
    a->setCheckable(true);
    a->setIcon (QPixmap(":/headingeditor.png"));
    a->setShortcut ( Qt::Key_E );
    a->setShortcutContext (Qt::WidgetShortcut);
    mapEditorActions.append( a );
    viewMenu->addAction (a);
    switchboard.addSwitch ("mapToggleHeadingEditor", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT(windowToggleHeadingEditor() ) );
    actionViewToggleHeadingEditor=a;

    // Original icon is "category" from KDE
    a = new QAction(QPixmap(":/treeeditor.png"), tr( "Tree editor","View action" ),this);
    a->setShortcut ( Qt::CTRL + Qt::Key_T );
    a->setCheckable(true);
    viewMenu->addAction (a);
    switchboard.addSwitch ("mapToggleTreeEditor", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT(windowToggleTreeEditor() ) );
    actionViewToggleTreeEditor=a;

    a = new QAction(QPixmap(":/taskeditor.png"), tr( "Task editor","View action" ),this);
    a->setCheckable(true);
    a->setShortcut ( Qt::Key_Q );
    a->setShortcutContext (Qt::WidgetShortcut);
    mapEditorActions.append( a );
    viewMenu->addAction (a);
    switchboard.addSwitch ("mapToggleTaskEditor", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT(windowToggleTaskEditor() ) );
    actionViewToggleTaskEditor=a;

    a = new QAction(QPixmap(":/slideeditor.png"), tr( "Slide editor","View action" ),this);
    a->setShortcut ( Qt::SHIFT + Qt::Key_S );
    a->setCheckable(true);
    viewMenu->addAction (a);
    switchboard.addSwitch ("mapToggleSlideEditor", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT(windowToggleSlideEditor() ) );
    actionViewToggleSlideEditor=a;

    a = new QAction(QPixmap(":/scripteditor.png"), tr("Script editor","View action"), this);
    a->setShortcut ( Qt::ALT + Qt::Key_S );
    a->setCheckable(true);
    viewMenu->addAction (a);
    switchboard.addSwitch ("mapToggleScriptEditor", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( windowToggleScriptEditor() ) );
    actionViewToggleScriptEditor=a;

    a = new QAction(QPixmap(":/history.png"),  tr( "History Window","View action" ),this );
    a->setShortcut ( Qt::CTRL + Qt::Key_H  );
    a->setShortcutContext (Qt::WidgetShortcut);
    a->setCheckable(true);
    viewMenu->addAction (a);
    mapEditorActions.append( a );
    switchboard.addSwitch ("mapToggleHistoryWindow", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT(windowToggleHistory() ) );
    actionViewToggleHistoryWindow=a;

    viewMenu->addAction (actionViewTogglePropertyEditor);

    viewMenu->addSeparator();	

    a = new QAction(tr( "Antialiasing","View action" ),this );
    a->setCheckable(true);
    a->setChecked (settings.value("/mainwindow/view/AntiAlias",true).toBool());
    viewMenu->addAction (a);
    connect( a, SIGNAL( triggered() ), this, SLOT(windowToggleAntiAlias() ) );
    actionViewToggleAntiAlias=a;

    a = new QAction(tr( "Smooth pixmap transformations","View action" ),this );
    a->setStatusTip (a->text());
    a->setCheckable(true);
    a->setChecked (settings.value("/mainwindow/view/SmoothPixmapTransformation",true).toBool());
    viewMenu->addAction (a);
    connect( a, SIGNAL( triggered() ), this, SLOT(windowToggleSmoothPixmap() ) );
    actionViewToggleSmoothPixmapTransform=a;

    a = new QAction(tr( "Next Map","View action" ), this);
    a->setStatusTip (a->text());
    a->setShortcut (Qt::SHIFT+ Qt::Key_Right );
    viewMenu->addAction (a);
    switchboard.addSwitch ("mapPrevious", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT(windowNextEditor() ) );

    a = new QAction (tr( "Previous Map","View action" ), this );
    a->setStatusTip (a->text());
    a->setShortcut (Qt::SHIFT+ Qt::Key_Left );
    viewMenu->addAction (a);
    switchboard.addSwitch ("mapNext", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT(windowPreviousEditor() ) );

    a = new QAction (tr( "Next slide","View action" ), this );
    a->setStatusTip (a->text());
    a->setShortcut (Qt::Key_Space);
    viewMenu->addAction (a);
    switchboard.addSwitch ("mapNextSlide", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT(nextSlide() ) );

    a = new QAction (tr( "Previous slide","View action" ), this );
    a->setStatusTip (a->text());
    a->setShortcut (Qt::Key_Backspace);
    viewMenu->addAction (a);
    switchboard.addSwitch ("mapPreviousSlide", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT(previousSlide() ) );
}

// Mode Actions
void Main::setupModeActions()
{
    //QPopupMenu *menu = new QPopupMenu( this );
    //menuBar()->insertItem( tr( "&Mode (using modifiers)" ), menu );

    QString tag = tr("Modifier modes","Shortcuts");
    QAction *a;
    actionGroupModModes=new QActionGroup ( this);
    actionGroupModModes->setExclusive (true);
    a= new QAction( QPixmap(":/modecolor.png"), tr( "Use modifier to color branches","Mode modifier" ), actionGroupModModes);
    a->setShortcut (Qt::Key_J);
    addAction(a);
    switchboard.addSwitch ("mapModModeColor", shortcutScope, a, tag);
    a->setCheckable(true);
    a->setChecked(true);
    actionListFiles.append (a);
    actionModModeColor=a;

    a->setShortcut( Qt::Key_K); 
    addAction(a);
    switchboard.addSwitch ("mapModModeCopy", shortcutScope, a, tag);
    a->setCheckable(true);
    actionListFiles.append (a);
    actionModModeCopy=a;

    a= new QAction(QPixmap(":/modelink.png"), tr( "Use modifier to draw xLinks","Mode modifier" ), actionGroupModModes );
    a->setShortcut (Qt::Key_L);
    addAction(a);
    switchboard.addSwitch ("mapModModeXLink", shortcutScope, a, tag);
    a->setCheckable(true);
    actionListFiles.append (a);
    actionModModeXLink=a;
}

// Flag Actions
void Main::setupFlagActions()
{
    // Create System Flags
    Flag *flag;

    // Tasks
    // Origin: ./share/icons/oxygen/48x48/status/task-reject.png
    flag=new Flag(":/flag-task-new.png");
    flag->setGroup("system-tasks");
    setupFlag (flag,NULL,"system-task-new",tr("Note","SystemFlag"));
    flag=new Flag(":/flag-task-new-morning.png");
    flag->setGroup("system-tasks");
    setupFlag (flag,NULL,"system-task-new-morning",tr("Note","SystemFlag"));
    flag=new Flag(":/flag-task-new-sleeping.png");
    flag->setGroup("system-tasks");
    setupFlag (flag,NULL,"system-task-new-sleeping",tr("Note","SystemFlag"));
    // Origin: ./share/icons/oxygen/48x48/status/task-reject.png
    flag=new Flag(":/flag-task-wip.png");
    flag->setGroup("system-tasks");
    setupFlag (flag,NULL,"system-task-wip",tr("Note","SystemFlag"));
    flag=new Flag(":/flag-task-wip-morning.png");
    flag->setGroup("system-tasks");
    setupFlag (flag,NULL,"system-task-wip-morning",tr("Note","SystemFlag"));
    flag=new Flag(":/flag-task-wip-sleeping.png");
    flag->setGroup("system-tasks");
    setupFlag (flag,NULL,"system-task-wip-sleeping",tr("Note","SystemFlag"));
    // Origin: ./share/icons/oxygen/48x48/status/task-complete.png
    flag=new Flag(":/flag-task-finished.png");
    flag->setGroup("system-tasks");
    setupFlag (flag,NULL,"system-task-finished",tr("Note","SystemFlag"));

    flag=new Flag(":/flag-note.png");
    setupFlag (flag,NULL,"system-note",tr("Note","SystemFlag"));

    flag=new Flag(":/flag-url.png");
    setupFlag (flag,NULL,"system-url",tr("URL to Document ","SystemFlag"));

    flag=new Flag(":/flag-url-bugzilla-novell.png");
    setupFlag (flag,NULL,"system-url-bugzilla-novell",tr("URL to Bugzilla ","SystemFlag"));

    flag=new Flag(":/flag-url-bugzilla-novell-closed.png");
    setupFlag (flag,NULL,"system-url-bugzilla-novell-closed",tr("URL to Bugzilla ","SystemFlag"));

    flag=new Flag(":/flag-target.png");
    setupFlag (flag,NULL,"system-target",tr("Map target","SystemFlag"));

    flag=new Flag(":/flag-vymlink.png");
    setupFlag (flag,NULL,"system-vymLink",tr("Link to another vym map","SystemFlag"));

    flag=new Flag(":/flag-scrolled-right.png");
    setupFlag (flag,NULL,"system-scrolledright",tr("subtree is scrolled","SystemFlag"));

    flag=new Flag(":/flag-tmpUnscrolled-right.png");
    setupFlag (flag,NULL,"system-tmpUnscrolledRight",tr("subtree is temporary scrolled","SystemFlag"));

    flag=new Flag(":/flag-hideexport.png");
    setupFlag (flag,NULL,"system-hideInExport",tr("Hide object in exported maps","SystemFlag"));

    addToolBarBreak();

    // Create Standard Flags
    standardFlagsToolbar=addToolBar (tr ("Standard Flags toolbar","Standard Flag Toolbar"));
    standardFlagsToolbar->setObjectName ("standardFlagTB");
    standardFlagsMaster->setToolBar (standardFlagsToolbar);

    // Add entry now, to avoid chicken and egg problem and position toolbar 
    // after all others:
    toolbarsMenu->addAction (standardFlagsToolbar->toggleViewAction() );

    flag=new Flag(":/flag-stopsign.png");
    setupFlag (flag,standardFlagsToolbar,"stopsign",tr("This won't work!","Standardflag"),Qt::Key_1);
    flag->unsetGroup();

    flag=new Flag(":/flag-hook-green.png");
    flag->setGroup("standard-status");
    setupFlag (flag,standardFlagsToolbar,"hook-green",tr("Status - ok,done","Standardflag"),Qt::Key_2);

    flag=new Flag(":/flag-wip.png");
    flag->setGroup("standard-status");
    setupFlag (flag,standardFlagsToolbar,"wip",tr("Status - work in progress","Standardflag"),Qt::Key_3);

    flag=new Flag(":/flag-cross-red.png");
    flag->setGroup("standard-status");
    setupFlag (flag,standardFlagsToolbar,"cross-red",tr("Status - missing, not started","Standardflag"),Qt::Key_4);

    flag=new Flag(":/flag-exclamationmark.png");
    flag->setGroup("standard-mark");
    setupFlag (flag,standardFlagsToolbar,"exclamationmark",tr("Take care!","Standardflag"),Qt::Key_Exclam);

    flag=new Flag(":/flag-questionmark.png");
    flag->setGroup("standard-mark");
    setupFlag (flag,standardFlagsToolbar,"questionmark",tr("Really?","Standardflag"),Qt::Key_Question);

    flag=new Flag(":/flag-smiley-good.png");
    flag->setGroup("standard-smiley");
    setupFlag (flag,standardFlagsToolbar,"smiley-good",tr("Good","Standardflag"),Qt::Key_ParenRight);

    flag=new Flag(":/flag-smiley-sad.png");
    flag->setGroup("standard-smiley");
    setupFlag (flag,standardFlagsToolbar,"smiley-sad",tr("Bad","Standardflag"),Qt::Key_ParenLeft);

    flag=new Flag(":/flag-smiley-omb.png");
    flag->setGroup("standard-smiley");
    setupFlag (flag,standardFlagsToolbar,"smiley-omb",tr("Oh no!","Standardflag"));
    // Original omg.png (in KDE emoticons)
    flag->unsetGroup();

    flag=new Flag(":/flag-clock.png");
    setupFlag (flag,standardFlagsToolbar,"clock",tr("Time critical","Standardflag"));

    flag=new Flag(":/flag-phone.png");
    setupFlag (flag,standardFlagsToolbar,"phone",tr("Call...","Standardflag"));

    flag=new Flag(":/flag-lamp.png");
    setupFlag (flag,standardFlagsToolbar,"lamp",tr("Idea!","Standardflag"),Qt::Key_Asterisk);

    flag=new Flag(":/flag-arrow-up.png");
    flag->setGroup("standard-arrow");
    setupFlag (flag,standardFlagsToolbar,"arrow-up",tr("Important","Standardflag"),Qt::SHIFT + Qt::Key_PageUp);

    flag=new Flag(":/flag-arrow-down.png");
    flag->setGroup("standard-arrow");
    setupFlag (flag,standardFlagsToolbar,"arrow-down",tr("Unimportant","Standardflag"),Qt::SHIFT + Qt::Key_PageDown);

    flag=new Flag(":/flag-2arrow-up.png");
    flag->setGroup("standard-arrow");
    setupFlag (flag,standardFlagsToolbar,"2arrow-up",tr("Very important!","Standardflag"),Qt::SHIFT + +Qt::CTRL + Qt::Key_PageUp);

    flag=new Flag(":/flag-2arrow-down.png");
    flag->setGroup("standard-arrow");
    setupFlag (flag,standardFlagsToolbar,"2arrow-down",tr("Very unimportant!","Standardflag"),Qt::SHIFT + Qt::CTRL + Qt::Key_PageDown);
    flag->unsetGroup();

    flag=new Flag(":/flag-thumb-up.png");
    flag->setGroup("standard-thumb");
    setupFlag (flag,standardFlagsToolbar,"thumb-up",tr("I like this","Standardflag"));

    flag=new Flag(":/flag-thumb-down.png");
    flag->setGroup("standard-thumb");
    setupFlag (flag,standardFlagsToolbar,"thumb-down",tr("I do not like this","Standardflag"));
    flag->unsetGroup();

    flag=new Flag(":/flag-rose.png");
    setupFlag (flag,standardFlagsToolbar,"rose",tr("Rose","Standardflag"));

    flag=new Flag(":/flag-heart.png");
    setupFlag (flag,standardFlagsToolbar,"heart",tr("I just love...","Standardflag"));

    flag=new Flag(":/flag-present.png");
    setupFlag (flag,standardFlagsToolbar,"present",tr("Surprise!","Standardflag"));

    flag=new Flag(":/flag-flash.png");
    setupFlag (flag,standardFlagsToolbar,"flash",tr("Dangerous","Standardflag"));

    // Original: xsldbg_output.png
    flag=new Flag(":/flag-info.png");
    setupFlag (flag,standardFlagsToolbar,"info",tr("Info","Standardflag"),Qt::Key_I);

    // Original khelpcenter.png
    flag=new Flag(":/flag-lifebelt.png");
    setupFlag (flag,standardFlagsToolbar,"lifebelt",tr("This will help","Standardflag"));

    // Freemind flags
    flag=new Flag(":/freemind/warning.png");
    flag->setVisible(false);
    setupFlag (flag,standardFlagsToolbar,  "freemind-warning",tr("Important","Freemind-Flag"));

    for (int i=1; i<8; i++)
    {
	flag=new Flag(QString(":/freemind/priority-%1.png").arg(i));
	flag->setVisible(false);
	flag->setGroup ("Freemind-priority");
	setupFlag (flag,standardFlagsToolbar, QString("freemind-priority-%1").arg(i),tr("Priority","Freemind-Flag"));
    }

    flag=new Flag(":/freemind/back.png");
    flag->setVisible(false);
    setupFlag (flag,standardFlagsToolbar,"freemind-back",tr("Back","Freemind-Flag"));

    flag=new Flag(":/freemind/forward.png");
    flag->setVisible(false);
    setupFlag (flag,standardFlagsToolbar,"freemind-forward",tr("forward","Freemind-Flag"));

    flag=new Flag(":/freemind/attach.png");
    flag->setVisible(false);
    setupFlag (flag,standardFlagsToolbar,"freemind-attach",tr("Look here","Freemind-Flag"));

    flag=new Flag(":/freemind/clanbomber.png");
    flag->setVisible(false);
    setupFlag (flag,standardFlagsToolbar,"freemind-clanbomber",tr("Dangerous","Freemind-Flag"));

    flag=new Flag(":/freemind/desktopnew.png");
    flag->setVisible(false);
    setupFlag (flag,standardFlagsToolbar,"freemind-desktopnew",tr("Don't flagrget","Freemind-Flag"));

    flag=new Flag(":/freemind/flag.png");
    flag->setVisible(false);
    setupFlag (flag,standardFlagsToolbar,"freemind-flag",tr("Flag","Freemind-Flag"));


    flag=new Flag(":/freemind/gohome.png");
    flag->setVisible(false);
    setupFlag (flag,standardFlagsToolbar,"freemind-gohome",tr("Home","Freemind-Flag"));

    flag=new Flag(":/freemind/kaddressbook.png");
    flag->setVisible(false);
    setupFlag (flag,standardFlagsToolbar,"freemind-kaddressbook",tr("Telephone","Freemind-Flag"));

    flag=new Flag(":/freemind/knotify.png");
    flag->setVisible(false);
    setupFlag (flag,standardFlagsToolbar,"freemind-knotify",tr("Music","Freemind-Flag"));

    flag=new Flag(":/freemind/korn.png");
    flag->setVisible(false);
    setupFlag (flag,standardFlagsToolbar,"freemind-korn",tr("Mailbox","Freemind-Flag"));

    flag=new Flag(":/freemind/mail.png");
    flag->setVisible(false);
    setupFlag (flag,standardFlagsToolbar,"freemind-mail",tr("Maix","Freemind-Flag"));

    flag=new Flag(":/freemind/password.png");
    flag->setVisible(false);
    setupFlag (flag,standardFlagsToolbar,"freemind-password",tr("Password","Freemind-Flag"));

    flag=new Flag(":/freemind/pencil.png");
    flag->setVisible(false);
    setupFlag (flag,standardFlagsToolbar,"freemind-pencil",tr("To be improved","Freemind-Flag"));

    flag=new Flag(":/freemind/stop.png");
    flag->setVisible(false);
    setupFlag (flag,standardFlagsToolbar,"freemind-stop",tr("Stop","Freemind-Flag"));

    flag=new Flag(":/freemind/wizard.png");
    flag->setVisible(false);
    setupFlag (flag,standardFlagsToolbar,"freemind-wizard",tr("Magic","Freemind-Flag"));

    flag=new Flag(":/freemind/xmag.png");
    flag->setVisible(false);
    setupFlag (flag,standardFlagsToolbar,"freemind-xmag",tr("To be discussed","Freemind-Flag"));

    flag=new Flag(":/freemind/bell.png");
    flag->setVisible(false);
    setupFlag (flag,standardFlagsToolbar,"freemind-bell",tr("Reminder","Freemind-Flag"));

    flag=new Flag(":/freemind/bookmark.png");
    flag->setVisible(false);
    setupFlag (flag,standardFlagsToolbar,"freemind-bookmark",tr("Excellent","Freemind-Flag"));

    flag= new Flag(":/freemind/penguin.png");
    flag->setVisible(false);
    setupFlag (flag,standardFlagsToolbar,"freemind-penguin",tr("Linux","Freemind-Flag"));

    flag=new Flag (":/freemind/licq.png");
    flag->setVisible(false);
    setupFlag (flag,standardFlagsToolbar,"freemind-licq",tr("Sweet","Freemind-Flag"));
}

void Main::setupFlag (Flag *flag, QToolBar *tb, const QString &name, const QString &tooltip, const QKeySequence &keyseq)
{
    flag->setName(name);
    flag->setToolTip (tooltip);
    QAction *a;
    if (tb)
    {
        a=new QAction (flag->getPixmap(),name,this);
        // StandardFlag
        flag->setAction (a);
        a->setVisible (flag->isVisible());
        a->setCheckable(true);
        a->setObjectName(name);
        a->setToolTip(tooltip);
        if (keyseq != 0)
        {
            a->setShortcut (keyseq);
            a->setShortcutContext (Qt::WidgetShortcut);

            // Allow mapEditors to actually trigger this action
            mapEditorActions.append( a );
            taskEditorActions.append( a );
        }

        tb->addAction(a);
        connect (a, SIGNAL( triggered() ), this, SLOT( standardFlagChanged() ) );
        standardFlagsMaster->addFlag (flag);
    } else
    {
        // SystemFlag
        systemFlagsMaster->addFlag (flag);
    }
}

// Network Actions
void Main::setupNetworkActions()
{
    if (!settings.value( "/mainwindow/showTestMenu",false).toBool() ) return;

    QAction *a;

    a = new QAction(  "Start TCPserver for MapEditor",this);
    //a->setShortcut ( Qt::ALT + Qt::Key_T );	 
    connect( a, SIGNAL( triggered() ), this, SLOT( networkStartServer() ) );

    a = new QAction(  "Connect MapEditor to server",this);
    //a->setShortcut ( Qt::ALT + Qt::Key_C );
    connect( a, SIGNAL( triggered() ), this, SLOT( networkConnect() ) );
}

// Settings Actions
void Main::setupSettingsActions()
{
    QMenu *settingsMenu = menuBar()->addMenu( tr( "Settings" ));

    QAction *a;

    a = new QAction( tr( "Check for release notes and updates","Settings action"), this);
    a->setCheckable(true);
    a->setChecked ( settings.value ("/downloads/enabled",true).toBool());
    connect( a, SIGNAL( triggered() ), this, SLOT( settingsToggleDownloads() ) );
    settingsMenu->addAction (a);
    actionSettingsToggleDownloads = a;

    a = new QAction( tr( "Set author for new maps","Settings action"), this);
    connect( a, SIGNAL( triggered() ), this, SLOT( settingsDefaultMapAuthor() ) );
    settingsMenu->addAction (a);

    settingsMenu->addSeparator();

    a = new QAction( tr( "Set application to open pdf files","Settings action"), this);
    connect( a, SIGNAL( triggered() ), this, SLOT( settingsPDF() ) );
    settingsMenu->addAction (a);

    a = new QAction( tr( "Set application to open external links","Settings action"), this);
    connect( a, SIGNAL( triggered() ), this, SLOT( settingsURL() ) );
    settingsMenu->addAction (a);

    a = new QAction( tr( "Set application to zip/unzip files","Settings action"), this);
    connect( a, SIGNAL( triggered() ), this, SLOT( settingsZipTool() ) );
    settingsMenu->addAction (a);

    a = new QAction( tr( "Set path for macros","Settings action")+"...", this);
    connect( a, SIGNAL( triggered() ), this, SLOT( settingsMacroDir() ) );
    settingsMenu->addAction (a);

    a = new QAction( tr( "Set number of undo levels","Settings action")+"...", this);
    connect( a, SIGNAL( triggered() ), this, SLOT( settingsUndoLevels() ) );
    settingsMenu->addAction (a);

    settingsMenu->addSeparator();

    a = new QAction( tr( "Autosave","Settings action"), this);
    a->setCheckable(true);
    a->setChecked ( settings.value ("/system/autosave/use",true).toBool());
    settingsMenu->addAction (a);
    actionSettingsToggleAutosave=a;

    a = new QAction( tr( "Autosave time","Settings action")+"...", this);
    connect( a, SIGNAL( triggered() ), this, SLOT( settingsAutosaveTime() ) );
    settingsMenu->addAction (a);
    actionSettingsAutosaveTime=a;

    // Disable certain actions during testing
    if (testmode)
    {
	actionSettingsToggleAutosave->setChecked (false);
	actionSettingsToggleAutosave->setEnabled (false);
	actionSettingsAutosaveTime->setEnabled (false);
    }

    a = new QAction( tr( "Write backup file on save","Settings action"), this);
    a->setCheckable(true);
    a->setChecked ( settings.value ("/system/writeBackupFile",false).toBool());
    connect( a, SIGNAL( triggered() ), this, SLOT( settingsToggleWriteBackupFile() ) );
    settingsMenu->addAction (a);
    actionSettingsWriteBackupFile=a;

    settingsMenu->addSeparator();

    a = new QAction( tr( "Edit branch after adding it","Settings action" ), this );
    a->setCheckable(true);
    a->setChecked ( settings.value ("/mapeditor/editmode/autoEditNewBranch",true).toBool());
    settingsMenu->addAction (a);
    actionSettingsAutoEditNewBranch=a;

    a= new QAction( tr( "Select branch after adding it","Settings action" ), this );
    a->setCheckable(true);
    a->setChecked ( settings.value ("/mapeditor/editmode/autoSelectNewBranch",false).toBool() );
    settingsMenu->addAction (a);
    actionSettingsAutoSelectNewBranch=a;

    a= new QAction(tr( "Select existing heading","Settings action" ), this);
    a->setCheckable(true);
    a->setChecked ( settings.value ("/mapeditor/editmode/autoSelectText",true).toBool() );
    settingsMenu->addAction (a);
    actionSettingsAutoSelectText=a;

    a= new QAction( tr( "Exclusive flags","Settings action" ), this);
    a->setCheckable(true);
    a->setChecked ( settings.value ("/mapeditor/editmode/useFlagGroups",true).toBool() );
    settingsMenu->addAction (a);
    actionSettingsUseFlagGroups=a;

    a= new QAction( tr( "Use hide flags","Settings action" ), this);
    a->setCheckable(true);
    a->setChecked ( settings.value ("/export/useHideExport",true).toBool() );
    settingsMenu->addAction (a);
    actionSettingsUseHideExport=a;

    settingsMenu->addSeparator();

    a = new QAction( tr( "Number of visible parents in task editor","Settings action"), this); 
    connect( a, SIGNAL( triggered() ), this, SLOT( settingsShowParentsLevelTasks() ) );
    settingsMenu->addAction (a);
    actionSettingsShowParentsLevelTasks=a;

    a = new QAction( tr( "Number of visible parents in find results window","Settings action"), this); 
    connect( a, SIGNAL( triggered() ), this, SLOT( settingsShowParentsLevelFindResults() ) );
    settingsMenu->addAction (a);
    actionSettingsShowParentsLevelFindResults=a;

    a = new QAction( tr( "Animation","Settings action"), this);
    a->setCheckable(true);
    a->setChecked (settings.value("/animation/use",true).toBool() );
    connect( a, SIGNAL( triggered() ), this, SLOT( settingsToggleAnimation() ) );
    settingsMenu->addAction (a);
    actionSettingsUseAnimation=a;

    a = new QAction( tr( "Automatic layout","Settings action"), this);
    a->setCheckable(true);
    a->setChecked ( settings.value ("/mainwindow/autoLayout/use",true).toBool());
    connect( a, SIGNAL( triggered() ), this, SLOT( settingsToggleAutoLayout() ) );
    settingsMenu->addAction (a);
    actionSettingsToggleAutoLayout=a;
}

// Test Actions
void Main::setupTestActions()
{
    QMenu *testMenu = menuBar()->addMenu( tr( "Test" ));

    QString tag = "Testing";
    QAction *a;
    a = new QAction( "Test function 1" , this);
    a->setShortcut (Qt::ALT + Qt::Key_T); 
    testMenu->addAction(a);
    switchboard.addSwitch ("mapTest1", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( testFunction1() ) );

    a = new QAction( "Test function 2" , this);
    testMenu->addAction(a);
    connect( a, SIGNAL( triggered() ), this, SLOT( testFunction2() ) );

    a = new QAction( "Toggle hide export mode" , this);
    a->setCheckable (true);
    a->setChecked (false);
    testMenu->addAction(a);
    connect( a, SIGNAL( triggered() ), this, SLOT( toggleHideExport() ) );
    actionToggleHideMode=a;

    a = new QAction( "Toggle winter mode" , this);
    a->setShortcut (Qt::ALT + Qt::Key_Asterisk); 
    testMenu->addAction(a);
    switchboard.addSwitch ("mapWinterMode", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( toggleWinter() ) );
    actionToggleWinter=a;
}

// Help Actions
void Main::setupHelpActions()
{
    QMenu *helpMenu = menuBar()->addMenu ( tr( "&Help","Help menubar entry" ));

    QAction *a;
    a = new QAction(  tr( "Open VYM Documentation (pdf) ","Help action" ), this );
    helpMenu->addAction(a);
    connect( a, SIGNAL( triggered() ), this, SLOT( helpDoc() ) );

    a = new QAction(  tr( "Open VYM example maps ","Help action" ), this );
    helpMenu->addAction(a);
    connect( a, SIGNAL( triggered() ), this, SLOT( helpDemo() ) );
    helpMenu->addSeparator();

    a = new QAction(  tr( "Download and show release notes","Help action" ), this );
    helpMenu->addAction(a);
    connect( a, SIGNAL( triggered() ), this, SLOT( checkReleaseNotes() ) );

    a = new QAction(  tr( "Check, if updates are available","Help action" ), this );
    helpMenu->addAction(a);
    connect( a, SIGNAL( triggered() ), this, SLOT( checkUpdates() ) );
    helpMenu->addSeparator();

    a = new QAction(  tr( "Show keyboard shortcuts","Help action" ), this );
    helpMenu->addAction(a);
    connect( a, SIGNAL( triggered() ), this, SLOT( helpShortcuts() ) );

    a = new QAction( tr("Debug info","Option to show debugging info") , this);  
    helpMenu->addAction(a);
    connect( a, SIGNAL( triggered() ), this, SLOT( debugInfo() ) );

    a = new QAction( tr( "About VYM","Help action" ), this);
    connect( a, SIGNAL( triggered() ), this, SLOT( helpAbout() ) );
    helpMenu->addAction (a);

    a = new QAction( tr( "About QT","Help action" ), this);
    connect( a, SIGNAL( triggered() ), this, SLOT( helpAboutQT() ) );
    helpMenu->addAction (a);
}

// Context Menus
void Main::setupContextMenus()
{
    // Context menu for goto/move targets  (populated on demand)
    targetsContextMenu = new QMenu (this);

    // Context Menu for branch or mapcenter
    branchContextMenu =new QMenu (this);
    branchContextMenu->addAction (actionViewTogglePropertyEditor);
    branchContextMenu->addSeparator();	

	// Submenu "Add"
	branchAddContextMenu =branchContextMenu->addMenu (tr("Add"));
	branchAddContextMenu->addAction (actionPaste );
	branchAddContextMenu->addAction ( actionAddMapCenter );
	branchAddContextMenu->addAction ( actionAddBranch );
	branchAddContextMenu->addAction ( actionAddBranchBefore );
	branchAddContextMenu->addAction ( actionAddBranchAbove);
	branchAddContextMenu->addAction ( actionAddBranchBelow );
	branchAddContextMenu->addSeparator();	
	branchAddContextMenu->addAction ( actionImportAdd );
	branchAddContextMenu->addAction ( actionImportReplace );

	// Submenu "Remove"
	branchRemoveContextMenu =branchContextMenu->addMenu (tr ("Remove","Context menu name"));
	branchRemoveContextMenu->addAction (actionCut);
	branchRemoveContextMenu->addAction ( actionDelete );
	branchRemoveContextMenu->addAction ( actionDeleteKeepChildren );
	branchRemoveContextMenu->addAction ( actionDeleteChildren );
	

    branchContextMenu->addAction(actionSaveBranch);
    branchContextMenu->addAction( actionFileNewCopy);
    branchContextMenu->addAction(actionDetach);

    branchContextMenu->addSeparator();	
    branchContextMenu->addAction ( actionLoadImage);
    if (settings.value( "/mainwindow/showTestMenu",false).toBool() )
	branchContextMenu->addAction ( actionAddAttribute);


    branchContextMenu->addSeparator();  

    // Context menu for tasks
    taskContextMenu = branchContextMenu->addMenu (tr("Tasks","Context menu"));
	taskContextMenu->addAction (actionToggleTask);
	taskContextMenu->addAction (actionCycleTaskStatus);
	taskContextMenu->addSeparator();
	taskContextMenu->addAction (actionTaskSleep0);
	taskContextMenu->addAction (actionTaskSleepN);
	taskContextMenu->addAction (actionTaskSleep1);
	taskContextMenu->addAction (actionTaskSleep2);
	taskContextMenu->addAction (actionTaskSleep3);
	taskContextMenu->addAction (actionTaskSleep5);
	taskContextMenu->addAction (actionTaskSleep7);
	taskContextMenu->addAction (actionTaskSleep14);
	taskContextMenu->addAction (actionTaskSleep28);

    // Submenu for Links (URLs, vymLinks)
    branchLinksContextMenu =new QMenu (this);

	branchLinksContextMenu=branchContextMenu->addMenu(tr("References (URLs, vymLinks, ...)","Context menu name"));	
	branchLinksContextMenu->addAction ( actionOpenURL );
	branchLinksContextMenu->addAction ( actionOpenURLTab );
	branchLinksContextMenu->addAction ( actionOpenMultipleVisURLTabs );
	branchLinksContextMenu->addAction ( actionOpenMultipleURLTabs );
	branchLinksContextMenu->addAction ( actionURLNew );
	branchLinksContextMenu->addAction ( actionLocalURL );
	branchLinksContextMenu->addAction ( actionGetURLsFromNote );
	branchLinksContextMenu->addAction ( actionHeading2URL );
	branchLinksContextMenu->addAction ( actionBugzilla2URL );
	branchLinksContextMenu->addAction ( actionGetBugzillaData );
	branchLinksContextMenu->addAction ( actionGetBugzillaDataSubtree );
	if (settings.value( "/mainwindow/showTestMenu",false).toBool() )
	    branchLinksContextMenu->addAction ( actionFATE2URL );
	branchLinksContextMenu->addSeparator();	
	branchLinksContextMenu->addAction ( actionOpenVymLink );
	branchLinksContextMenu->addAction ( actionOpenVymLinkBackground );
	branchLinksContextMenu->addAction ( actionOpenMultipleVymLinks );
	branchLinksContextMenu->addAction ( actionEditVymLink );
	branchLinksContextMenu->addAction ( actionDeleteVymLink );
	

    // Context Menu for XLinks in a branch menu
    // This will be populated "on demand" in updateActions
    QString tag = tr ("XLinks","Menu for file actions");
    branchContextMenu->addSeparator();	
    branchXLinksContextMenuEdit =branchContextMenu->addMenu (tr ("Edit XLink","Context menu name"));
    connect( 
	branchXLinksContextMenuEdit, SIGNAL( triggered(QAction *) ), 
	this, SLOT( editEditXLink(QAction * ) ) );
    QAction *a;
    a = new QAction( tr("Follow XLink","Context menu") , this);
    a->setShortcut (Qt::Key_F); 
    addAction(a);
    switchboard.addSwitch ("mapFollowXLink", shortcutScope, a, tag);
    connect( a, SIGNAL( triggered() ), this, SLOT( popupFollowXLink() ) );

    branchXLinksContextMenuFollow =branchContextMenu->addMenu (tr ("Follow XLink","Context menu name"));
    connect( 
	branchXLinksContextMenuFollow, SIGNAL( triggered(QAction *) ), 
	this, SLOT( editFollowXLink(QAction * ) ) );



    // Context menu for floatimage
    floatimageContextMenu =new QMenu (this);
    a= new QAction (tr ("Save image","Context action"),this);
    connect (a, SIGNAL (triggered()), this, SLOT (editSaveImage()));
    floatimageContextMenu->addAction (a);

    floatimageContextMenu->addSeparator();  
    floatimageContextMenu->addAction(actionCopy);
    floatimageContextMenu->addAction(actionCut);

    floatimageContextMenu->addSeparator();  
    floatimageContextMenu->addAction ( actionFormatHideLinkUnselected );

    // Context menu for canvas
    canvasContextMenu =new QMenu (this);

    canvasContextMenu->addAction (actionAddMapCenter);

    canvasContextMenu->addSeparator();   

    canvasContextMenu->addAction(actionMapProperties);
    canvasContextMenu->addAction(actionFormatFont);

    canvasContextMenu->addSeparator();   

    canvasContextMenu->addActions(actionGroupFormatLinkStyles->actions() );

    canvasContextMenu->addSeparator();   

    canvasContextMenu->addAction(actionFormatLinkColorHint);

    canvasContextMenu->addSeparator();   

    canvasContextMenu->addAction(actionFormatLinkColor);
    canvasContextMenu->addAction(actionFormatSelectionColor);
    canvasContextMenu->addAction(actionFormatBackColor);
    //if (settings.value( "/mainwindow/showTestMenu",false).toBool() )
    //    canvasContextMenu->addAction( actionFormatBackImage );  //FIXME-3 makes vym too slow: postponed for later version 


    // Menu for last opened files
    // Create actions
    for (int i = 0; i < MaxRecentFiles; ++i) 
    {
	recentFileActions[i] = new QAction(this);
	recentFileActions[i]->setVisible(false);
	fileLastMapsMenu->addAction(recentFileActions[i]);
	connect(recentFileActions[i], SIGNAL(triggered()),
		this, SLOT(fileLoadRecent()));
    }
    setupRecentMapsMenu();
}

void Main::setupRecentMapsMenu()
{
    QStringList files = settings.value("/mainwindow/recentFileList").toStringList();

    int numRecentFiles = qMin(files.size(), (int)MaxRecentFiles);

    for (int i = 0; i < numRecentFiles; ++i) {
	QString text = QString("&%1 %2").arg(i + 1).arg(files[i]);
	recentFileActions[i]->setText(text);
	recentFileActions[i]->setData(files[i]);
	recentFileActions[i]->setVisible(true);
    }
    for (int j = numRecentFiles; j < MaxRecentFiles; ++j)
	recentFileActions[j]->setVisible(false);
}

void Main::setupMacros()
{
    for (int i = 0; i <= 12; i++) 
    {
	macroActions[i] = new QAction(this);
	macroActions[i]->setData(i);
	addAction (macroActions[i]);
	connect(macroActions[i], SIGNAL(triggered()), this, SLOT(callMacro()));
    }		
    macroActions[0]->setShortcut ( Qt::Key_F1 );
    macroActions[1]->setShortcut ( Qt::Key_F2 );
    macroActions[2]->setShortcut ( Qt::Key_F3 );
    macroActions[3]->setShortcut ( Qt::Key_F4 );
    macroActions[4]->setShortcut ( Qt::Key_F5 );
    macroActions[5]->setShortcut ( Qt::Key_F6 );
    macroActions[6]->setShortcut ( Qt::Key_F7 );
    macroActions[7]->setShortcut ( Qt::Key_F8 );
    macroActions[8]->setShortcut ( Qt::Key_F9 );
    macroActions[9]->setShortcut ( Qt::Key_F10 );
    macroActions[10]->setShortcut ( Qt::Key_F11 );
    macroActions[11]->setShortcut ( Qt::Key_F12 );

    macroActions[12]->setShortcut ( Qt::Key_F1 + Qt::SHIFT);
}

void Main::setupToolbars()
{
    // File actions
    fileToolbar = addToolBar( tr ("File actions toolbar","Toolbar for file actions"));
    fileToolbar->setObjectName ("fileTB");
    fileToolbar->addAction(actionFileNew);
    fileToolbar->addAction(actionFileOpen);
    fileToolbar->addAction(actionFileSave);
    fileToolbar->addAction(actionFileExportLast);
    fileToolbar->addAction(actionFilePrint);

    // Undo/Redo and clipboard
    clipboardToolbar =addToolBar( tr ("Undo and clipboard toolbar","Toolbar for redo/undo and clipboard"));
    clipboardToolbar->setObjectName ("clipboard toolbar");
    clipboardToolbar->addAction (actionUndo);
    clipboardToolbar->addAction (actionRedo);
    clipboardToolbar->addAction (actionCopy);
    clipboardToolbar->addAction (actionCut);
    clipboardToolbar->addAction (actionPaste);

    // Basic edits
    editActionsToolbar = addToolBar( tr ("Edit actions toolbar","Toolbar name") );
    editActionsToolbar->setObjectName ("basic edit actions TB");
    editActionsToolbar->addAction (actionAddMapCenter);
    editActionsToolbar->addAction (actionAddBranch);
    editActionsToolbar->addAction (actionMoveUp);
    editActionsToolbar->addAction (actionMoveDown);
    editActionsToolbar->addAction (actionSortChildren);
    editActionsToolbar->addAction (actionSortBackChildren);
    editActionsToolbar->addAction (actionToggleScroll);
    editActionsToolbar->addAction (actionToggleHideExport);
    editActionsToolbar->addAction (actionToggleTask);
    //editActionsToolbar->addAction (actionExpandAll);
    //editActionsToolbar->addAction (actionExpandOneLevel);
    //editActionsToolbar->addAction (actionCollapseOneLevel);
    //editActionsToolbar->addAction (actionCollapseUnselected);

    // Selections
    selectionToolbar = addToolBar( tr ("Selection toolbar","Toolbar name") );
    selectionToolbar->setObjectName ("toolbar for selecting items");
    selectionToolbar->addAction (actionToggleTarget);
    selectionToolbar->addAction (actionSelectPrevious);
    selectionToolbar->addAction (actionSelectNext);
    selectionToolbar->addAction (actionFind);

    // URLs and vymLinks
    referencesToolbar=addToolBar( tr ("URLs and vymLinks toolbar","Toolbar for URLs and vymlinks"));
    referencesToolbar->setObjectName ("URLs and vymlinks toolbar");
    //referencesToolbar->addAction (actionOpenURL); //FIXME-4 removed 2015-06-22
    referencesToolbar->addAction (actionURLNew);
    //referencesToolbar->addAction (actionOpenVymLink)//FIXME-4 removed 2015-06-22;
    referencesToolbar->addAction (actionEditVymLink);

    // Format and colors
    colorsToolbar = addToolBar( tr("Colors toolbar","Colors toolbar name"));
    colorsToolbar->setObjectName ("colorsTB");
    colorsToolbar->addAction(actionFormatColor);
    colorsToolbar->addAction(actionFormatPickColor);
    colorsToolbar->addAction(actionFormatColorBranch);
    colorsToolbar->addAction(actionFormatColorSubtree);

    // Zoom
    zoomToolbar = addToolBar( tr("Zoom toolbar","View Toolbar name") );
    zoomToolbar->setObjectName ("viewTB");
    zoomToolbar->addAction(actionZoomIn);
    zoomToolbar->addAction(actionZoomOut);
    zoomToolbar->addAction(actionZoomReset);
    zoomToolbar->addAction(actionCenterOn);
    zoomToolbar->addAction(actionRotateCounterClockwise);
    zoomToolbar->addAction(actionRotateClockwise);

    // Editors
    editorsToolbar = addToolBar( tr("Editors toolbar","Editor Toolbar name") );
    editorsToolbar->setObjectName ("editorsTB");
    editorsToolbar->addAction (actionViewToggleNoteEditor);
    editorsToolbar->addAction (actionViewToggleHeadingEditor);
    editorsToolbar->addAction (actionViewToggleTreeEditor);
    editorsToolbar->addAction (actionViewToggleTaskEditor);
    editorsToolbar->addAction (actionViewToggleSlideEditor);
    editorsToolbar->addAction (actionViewToggleScriptEditor);
    editorsToolbar->addAction (actionViewToggleHistoryWindow);


    // Modifier modes
    modModesToolbar = addToolBar( tr ("Modifier modes toolbar","Modifier Toolbar name") );
    modModesToolbar->setObjectName ("modesTB");
    modModesToolbar->addAction(actionModModeColor);
    modModesToolbar->addAction(actionModModeCopy);
    modModesToolbar->addAction(actionModModeXLink);
    
    // Add all toolbars to View menu
    toolbarsMenu->addAction (fileToolbar->toggleViewAction() );
    toolbarsMenu->addAction (clipboardToolbar->toggleViewAction() );
    toolbarsMenu->addAction (editActionsToolbar->toggleViewAction() );
    toolbarsMenu->addAction (selectionToolbar->toggleViewAction() );
    toolbarsMenu->addAction (colorsToolbar->toggleViewAction() );
    toolbarsMenu->addAction (zoomToolbar->toggleViewAction() );
    toolbarsMenu->addAction (modModesToolbar->toggleViewAction() );
    toolbarsMenu->addAction (referencesToolbar->toggleViewAction() );
    toolbarsMenu->addAction (editorsToolbar->toggleViewAction() );

    // Default visibility to not overload new users
    fileToolbar->show();
    clipboardToolbar->show();
    editActionsToolbar->show();
    selectionToolbar->hide();
    colorsToolbar->show();
    zoomToolbar->show();
    modModesToolbar->hide();
    referencesToolbar->hide();
    editorsToolbar->hide();

}

VymView* Main::currentView() const
{
    if ( tabWidget->currentWidget() )
    {
	int i=tabWidget->currentIndex();
	if (i>=0 && i< vymViews.count() ) return vymViews.at(i);
    }
    return NULL;
}

MapEditor* Main::currentMapEditor() const
{
    if ( tabWidget->currentWidget())
	return vymViews.at(tabWidget->currentIndex())->getMapEditor();
    return NULL;    
}

uint  Main::currentModelID() const
{
    VymModel *m=currentModel();
    if (m)
	return m->getModelID();
    else
	return 0;    
}

VymModel* Main::currentModel() const
{
    VymView *vv=currentView();
    if (vv) 
	return vv->getModel();
    else
	return NULL;    
}

VymModel* Main::getModel(uint id) const	
{
    if (id <=0) return NULL;

    // Used in BugAgent
    for (int i=0; i<vymViews.count();i++)
	if (vymViews.at(i)->getModel()->getModelID()==id)
	    return vymViews.at(i)->getModel();
    return NULL;    
}

void Main::gotoModel (VymModel *m)
{
    for (int i=0; i<vymViews.count();i++)
	if (vymViews.at(i)->getModel()==m)
	{
	    tabWidget->setCurrentIndex (i);
	    return;
	}
}

int Main::modelCount()
{
    return vymViews.count();
}

void Main::updateTabName( VymModel *vm)
{
    for( int i = 0; i < vymViews.count(); i++)
    {
        if (vymViews.at(i)->getModel() == vm )
        {
            if ( vm->isReadOnly() )
                tabWidget->setTabText( i, vm->getFileName() + " " + tr("(readonly)") );
            else
                tabWidget->setTabText( i, vm->getFileName() );
            break;
        }
    }
}

void Main::editorChanged()
{
    VymModel *vm=currentModel();
    if (vm) 
    {	
	updateNoteEditor (vm->getSelectedIndex() );
	updateQueries (vm);

	taskEditor->setMapName (vm->getMapName() );
    }	

    // Update actions to in menus and toolbars according to editor
    updateActions();
}

void Main::fileNew()
{
    VymModel *vm=new VymModel;

    /////////////////////////////////////
//  new ModelTest(vm, this);	
    /////////////////////////////////////

    VymView *vv=new VymView (vm);
    vymViews.append (vv);

    tabWidget->addTab (vv,tr("unnamed","MainWindow: name for new and empty file"));
    tabWidget->setCurrentIndex (vymViews.count() );
    vv->initFocus();

    // Create MapCenter for empty map
    vm->addMapCenter(false);
    vm->makeDefault();

    // For the very first map we do not have flagrows yet...
    vm->select("mc:");

    // Switch to new tab
    tabWidget->setCurrentIndex (tabWidget->count() -1);
}

void Main::fileNewCopy() 
{
    QString fn="unnamed";
    VymModel *srcModel=currentModel();
    if (srcModel)
    {
	srcModel->copy();
	fileNew();
	VymModel *dstModel=vymViews.last()->getModel();
	if (dstModel->select("mc:0"))
	    dstModel->loadMap (clipboardDir+"/"+clipboardFile,ImportReplace);
	else
	    qWarning ()<<"Main::fileNewCopy couldn't select mapcenter";
    }
}

File::ErrorCode Main::fileLoad(QString fn, const LoadMode &lmode, const FileType &ftype) 
{
    File::ErrorCode err=File::Success;

    // fn is usually the archive, mapfile the file after uncompressing
    QString mapfile;

    // Make fn absolute (needed for unzip)
    fn=QDir (fn).absolutePath();

    VymModel *vm;

    if (lmode==NewMap)
    {
	// Check, if map is already loaded
	int i=0;
	while (i<=tabWidget->count() -1)
	{
	    if (vymViews.at(i)->getModel()->getFilePath() == fn)
	    {
		// Already there, ask for confirmation
		QMessageBox mb( vymName,
		    tr("The map %1\nis already opened."
		    "Opening the same map in multiple editors may lead \n"
		    "to confusion when finishing working with vym."
		    "Do you want to").arg(fn),
		    QMessageBox::Warning,
		    QMessageBox::Yes | QMessageBox::Default,
		    QMessageBox::Cancel | QMessageBox::Escape,
		    QMessageBox::NoButton);
		mb.setButtonText( QMessageBox::Yes, tr("Open anyway") );
		mb.setButtonText( QMessageBox::Cancel, tr("Cancel"));
		switch( mb.exec() ) 
		{
		    case QMessageBox::Yes:
			// end loop and load anyway
			i=tabWidget->count();
			break;
		    case QMessageBox::Cancel:
			// do nothing
			return File::Aborted;
			break;
		}
	    }
	    i++;
	}
    }


    // Try to load map
    if ( !fn.isEmpty() )
    {
	vm = currentModel();
	// Check first, if mapeditor exists
	// If it is not default AND we want a new map, 
	// create a new mapeditor in a new tab
	if ( lmode==NewMap && (!vm || !vm->isDefault() )  )
	{
	    vm=new VymModel;
	    VymView *vv=new VymView (vm);
	    vymViews.append (vv);

	    tabWidget->addTab (vv,fn);
	    vv->initFocus();
	}
	
	// Check, if file exists (important for creating new files
	// from command line
	if (!QFile(fn).exists() )
	{
	    QMessageBox mb( vymName,
		tr("This map does not exist:\n  %1\nDo you want to create a new one?").arg(fn),
		QMessageBox::Question,
		QMessageBox::Yes ,
		QMessageBox::Cancel | QMessageBox::Default,
		QMessageBox::NoButton );

	    mb.setButtonText( QMessageBox::Yes, tr("Create"));
	    mb.setButtonText( QMessageBox::No, tr("Cancel"));

            VymModel *vm = currentMapEditor()->getModel();
	    switch( mb.exec() ) 
	    {
		case QMessageBox::Yes:
		    // Create new map
                    vm->setFilePath(fn);
                    updateTabName( vm );
		    statusBar()->showMessage( "Created " + fn , statusbarTime );
		    return File::Success;
			
		case QMessageBox::Cancel:
		    // don't create new map
		    statusBar()->showMessage( "Loading " + fn + " failed!", statusbarTime );
		    int cur=tabWidget->currentIndex();
		    tabWidget->setCurrentIndex (tabWidget->count()-1);
		    fileCloseMap();
		    tabWidget->setCurrentIndex (cur);
		    return File::Aborted;
	    }
	}   

	if (err!=File::Aborted)
	{
	    // Save existing filename in case  we import
	    QString fn_org = vm->getFilePath();

	    // Finally load map into mapEditor
	    progressDialog.setLabelText (tr("Loading: %1","Progress dialog while loading maps").arg(fn));
	    vm->setFilePath (fn);
	    vm->saveStateBeforeLoad (lmode,fn);
	    err = vm->loadMap(fn,lmode,ftype);

	    // Restore old (maybe empty) filepath, if this is an import
	    if (lmode != NewMap)
		vm->setFilePath (fn_org);
	}   

	// Finally check for errors and go home
	if (err == File::Aborted) 
	{
	    if (lmode == NewMap) fileCloseMap();
	    statusBar()->showMessage( "Could not load " + fn, statusbarTime );
	} else 
	{
	    if (lmode == NewMap)
            {
                vm->setFilePath (fn);
                updateTabName( vm );
                actionFilePrint->setEnabled (true);
            }	
	    editorChanged();
	    vm->emitShowSelection();
            addRecentMap( fn );
	    statusBar()->showMessage( "Loaded " + fn, statusbarTime );
	}   
    }
    return err;
}


void Main::fileLoad(const LoadMode &lmode)
{
    QString caption;
    switch (lmode)
    {
	case NewMap:
	    caption=vymName+ " - " +tr("Load vym map");
	    break;
	case ImportAdd:
	    caption=vymName+ " - " +tr("Import: Add vym map to selection");
	    break;
	case ImportReplace:
	    caption=vymName+ " - " +tr("Import: Replace selection with vym map");
	    break;
    }

    QString filter;
    filter+="VYM map " + tr("or","File Dialog") +" Freemind map" + " (*.xml *.vym *.vyp *.mm);;";  
    filter+="VYM map (*.vym *.vyp);;";
    filter+="VYM Backups (*.vym~);;";
    filter+="Freemind map (*.mm);;";
    filter+="XML (*.xml);;";
    filter+="All (* *.*)";
    QStringList fns=QFileDialog::getOpenFileNames( 
	    this,
	    caption,
	    lastMapDir.path(), 
	    filter);

    if (!fns.isEmpty() )
    {
        initProgressCounter (fns.count() );
	lastMapDir.setPath(fns.first().left(fns.first().lastIndexOf ("/")) );
	foreach (QString fn, fns)
	    fileLoad(fn, lmode, getMapType (fn) );		   
    }
    removeProgressCounter();

    fileSaveSession();
}

void Main::fileLoad()
{
    fileLoad (NewMap);
    tabWidget->setCurrentIndex (tabWidget->count()-1);
}

void Main::fileSaveSession()
{
    QStringList flist;
    for (int i=0;i<vymViews.count(); i++)
	flist.append (vymViews.at(i)->getModel()->getFilePath() );
    settings.setValue("/mainwindow/sessionFileList", flist);
}

void Main::fileRestoreSession()
{
    QStringList flist= settings.value("/mainwindow/sessionFileList").toStringList();
    QStringList::Iterator it=flist.begin();

    initProgressCounter (flist.count());
    while (it !=flist.end() )
    {
        FileType type=getMapType (*it);
        fileLoad (*it, NewMap,type);
        *it++;
    }
    removeProgressCounter();
}

void Main::fileLoadRecent()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (action)
    {
        initProgressCounter ();
        QString fn=action->data().toString();
        FileType type=getMapType (fn);
        fileLoad (fn, NewMap,type);
        removeProgressCounter();
        tabWidget->setCurrentIndex (tabWidget->count()-1);
    }
}

void Main::addRecentMap (const QString &fileName)
{

    QStringList files = settings.value("/mainwindow/recentFileList").toStringList();
    files.removeAll(fileName);
    files.prepend(fileName);
    while (files.size() > MaxRecentFiles)
        files.removeLast();

    settings.setValue("/mainwindow/recentFileList", files);

    setupRecentMapsMenu();
}

void Main::fileSave(VymModel *m, const SaveMode &savemode)
{
    if (!m) return;

    if (m->isReadOnly() ) return;

    if ( m->getFilePath().isEmpty() )
    {
        // We have  no filepath yet,
        // call fileSaveAs() now, this will call fileSave()
        // again.
        // First switch to editor
        fileSaveAs(savemode);
        return; // avoid saving twice...
    }

    if (m->save (savemode)==File::Success)
    {
        statusBar()->showMessage(
                    tr("Saved  %1").arg(m->getFilePath()),
                    statusbarTime );
    } else
        statusBar()->showMessage(
                    tr("Couldn't save ").arg(m->getFilePath()),
                    statusbarTime );
}

void Main::fileSave()
{
    fileSave (currentModel(), CompleteMap);
}

void Main::fileSave(VymModel *m)
{
    fileSave (m,CompleteMap);
}

void Main::fileSaveAs(const SaveMode& savemode)
{
    if (currentMapEditor())
    {
        QString filter;
        if (savemode == CompleteMap)
            filter = "VYM map (*.vym)";
        else
            filter = "VYM part of map (*vyp)";
        filter += ";;All (* *.*)";

        QString fn = QFileDialog::getSaveFileName (
                    this,
                    tr("Save map as"),
                    lastMapDir.path(),
                    filter,
                    NULL,
                    QFileDialog::DontConfirmOverwrite);
        if (!fn.isEmpty() )
        {
            // Check for existing file
            if (QFile (fn).exists())
            {
                QMessageBox mb( vymName,
                                tr("The file %1\nexists already. Do you want to").arg(fn),
                                QMessageBox::Warning,
                                QMessageBox::Yes | QMessageBox::Default,
                                QMessageBox::Cancel | QMessageBox::Escape,
                                QMessageBox::NoButton);
                mb.setButtonText( QMessageBox::Yes, tr("Overwrite") );
                mb.setButtonText( QMessageBox::Cancel, tr("Cancel"));
                switch( mb.exec() )
                {
                case QMessageBox::Yes:
                    // save
                    break;
                case QMessageBox::Cancel:
                    // do nothing
                    return;
                    break;
                }
                lastMapDir.setPath(fn.left(fn.lastIndexOf ("/")) );
            } else
            {
                // New file, add extension to filename, if missing
                // This is always .vym or .vyp, depending on savemode
                if (savemode == CompleteMap)
                {
                    if (!fn.contains (".vym") && !fn.contains (".xml"))
                        fn += ".vym";
                } else
                {
                    if (!fn.contains (".vyp") && !fn.contains (".xml"))
                        fn += ".vyp";
                }
            }

            // Save now
            VymModel *m = currentModel();
            QString fn_org = m->getFilePath(); // Restore fn later, if savemode != CompleteMap
            if (savemode == CompleteMap )
            {
                // Check for existing lockfile
                QFile lockFile( fn + ".lock" );
                if (lockFile.exists() )
                {
                    QMessageBox::critical( 0, tr( "Critical Error" ), tr("Couldn't save %1,\nbecause of existing lockfile:\n\n%2").arg(fn).arg( lockFile.fileName()  ));
                    return;
                }

                if ( !m->renameMap( fn ) )
                {
                    QMessageBox::critical( 0, tr( "Critical Error" ), tr("Couldn't save %1").arg( fn ));
                    return;
                }
            }
            fileSave(m, savemode);

            // Set name of tab
            if (savemode == CompleteMap)
                updateTabName( m );
            else
            {   // Renaming map to original name, because we only saved the selected part of it
                m->setFilePath (fn_org);
            }
            return;

        }
    }
}

void Main::fileSaveAs()
{
    fileSaveAs (CompleteMap);
}

void Main::fileImportFirefoxBookmarks()
{
    QFileDialog fd;
    fd.setDirectory (vymBaseDir.homePath()+"/.mozilla/firefox");
    fd.setFileMode (QFileDialog::ExistingFiles);
    QStringList filters;
    filters<<"Firefox "+tr("Bookmarks")+" (*.html)";
    fd.setNameFilters(filters);
    fd.setAcceptMode (QFileDialog::AcceptOpen);
    fd.setWindowTitle(tr("Import")+" "+"Firefox "+tr("Bookmarks"));

    if ( fd.exec() == QDialog::Accepted )
    {
	ImportFirefoxBookmarks im;
	QStringList flist = fd.selectedFiles();
	QStringList::Iterator it = flist.begin();
	while( it != flist.end() ) 
	{
	    im.setFile (*it);
	    if (im.transform() && 
		File::Aborted!=fileLoad (im.getTransformedFile(),NewMap,FreemindMap) && 
		currentMapEditor() )
		currentMapEditor()->getModel()->setFilePath ("");
	    ++it;
	}
    }
}

void Main::fileImportFreemind()
{
    QStringList filters;
    filters <<"Freemind map (*.mm)"<<"All files (*)";
    QFileDialog fd;
    fd.setDirectory (lastMapDir);
    fd.setFileMode (QFileDialog::ExistingFiles);
    fd.setNameFilters (filters);
    fd.setWindowTitle(vymName+ " - " +tr("Load Freemind map"));
    fd.setAcceptMode (QFileDialog::AcceptOpen);

    QString fn;
    if ( fd.exec() == QDialog::Accepted )
    {
	lastMapDir=fd.directory().path();
	QStringList flist = fd.selectedFiles();
	QStringList::Iterator it = flist.begin();
	while( it != flist.end() ) 
	{
	    fn = *it;
	    if ( fileLoad (fn,NewMap, FreemindMap)  )
	    {	
		currentMapEditor()->getModel()->setFilePath ("");
	    }	
	    ++it;
	}
    }
}


void Main::fileImportMM()
{
    ImportMM im;

    QFileDialog fd;
    fd.setDirectory (lastMapDir);
    fd.setFileMode (QFileDialog::ExistingFiles);
    QStringList filters;
    filters<<"Mind Manager (*.mmap)";
    fd.setNameFilters (filters);
    fd.setAcceptMode (QFileDialog::AcceptOpen);
    fd.setWindowTitle(tr("Import")+" "+"Mind Manager");

    if ( fd.exec() == QDialog::Accepted )
    {
	lastMapDir=fd.directory();
	QStringList flist = fd.selectedFiles();
	QStringList::Iterator it = flist.begin();
	while( it != flist.end() ) 
	{
	    im.setFile (*it);
	    if (im.transform() && 
		File::Success==fileLoad (im.getTransformedFile(),NewMap,VymMap) && 
		currentMapEditor() )
		currentMapEditor()->getModel()->setFilePath ("");
	    ++it;
	}
    }
}

void Main::fileImportDir()
{
    VymModel *m=currentModel();
    if (m) m->importDir();
}

void Main::fileExportXML()
{
    VymModel *m=currentModel();
    if (m) m->exportXML();
}

void Main::fileExportHTML() 
{
    VymModel *m=currentModel();
    if (m) m->exportHTML();
}

void Main::fileExportImage()	
{
    VymModel *m=currentModel();
    if (m) m->exportImage();
}

void Main::fileExportPDF()	
{
    VymModel *m=currentModel();
    if (m) m->exportPDF();
}

void Main::fileExportSVG()	
{
    VymModel *m=currentModel();
    if (m) m->exportSVG();
}

void Main::fileExportAO()
{
    VymModel *m=currentModel();
    if (m) m->exportAO();
}

void Main::fileExportASCII()
{
    VymModel *m=currentModel();
    if (m) m->exportASCII();
}

void Main::fileExportASCIITasks()
{
    VymModel *m=currentModel();
    if (m) m->exportASCII(true);
}

void Main::fileExportCSV()  //FIXME-3 not scriptable yet
{
    VymModel *m=currentModel();
    if (m)
    {
	ExportCSV ex;
	ex.setModel (m);
	ex.addFilter ("CSV (*.csv)");
	ex.setDirPath(lastImageDir.absolutePath());
	ex.setWindowTitle(vymName+ " -" + tr("Export as CSV") + " " + tr("(still experimental)"));
	if (ex.execDialog() ) 
	{
	    m->setExportMode(true);
	    ex.doExport();
	    m->setExportMode(false);
	}
    }
}

void Main::fileExportLaTeX()
{
    VymModel *m=currentModel();
    if (m) m->exportLaTeX();
}

void Main::fileExportOrgMode()	
{
    VymModel *m=currentModel();
    if (m) m->exportOrgMode();
}

void Main::fileExportTaskjuggler()  //FIXME-3 not scriptable yet
{
    ExportTaskjuggler ex;
    VymModel *m=currentModel();
    if (m)
    {
        ex.setModel (m);
        ex.setWindowTitle ( vymName + " - " + tr("Export to") + " Taskjuggler" + tr("(still experimental)"));
        ex.setDirPath (lastImageDir.absolutePath());
        ex.addFilter ("Taskjuggler (*.tjp)");

        if (ex.execDialog() )
        {
            m->setExportMode(true);
            ex.doExport();
            m->setExportMode(false);
        }
    }
}

void Main::fileExportImpress()	
{
    ExportOOFileDialog fd;
    // TODO add preview in dialog
    fd.setWindowTitle(vymName+" - "+tr("Export to")+" LibreOffice");
    fd.setDirectory (QDir().current());
    fd.setAcceptMode (QFileDialog::AcceptSave);
    fd.setFileMode (QFileDialog::AnyFile);
    if (fd.foundConfig())
    {
        if ( fd.exec() == QDialog::Accepted )
        {
            if (!fd.selectedFiles().isEmpty())
            {
                QString fn=fd.selectedFiles().first();
                if (!fn.contains (".odp")) fn +=".odp";

                //lastImageDir=fn.left(fn.findRev ("/"));
                VymModel *m=currentModel();
                if (m) m->exportImpress (fn,fd.selectedConfig());
            }
        }
    } else
    {
        QMessageBox::warning(0,
                             tr("Warning"),
                             tr("Couldn't find configuration for export to LibreOffice\n"));
    }
}

void Main::fileExportLast()
{
    VymModel *m=currentModel();
    if (m) m->exportLast();
}

bool Main::fileCloseMap(int i)
{
    VymModel *m;
    VymView *vv;
    if ( i<0) i  = tabWidget->currentIndex();

    vv = vymViews.at(i);
    m  = vv->getModel();

    if (m)
    {
        if (m->hasChanged())
        {
            QMessageBox mb( vymName,
                            tr("The map %1 has been modified but not saved yet. Do you want to").arg(m->getFileName()),
                            QMessageBox::Warning,
                            QMessageBox::Yes | QMessageBox::Default,
                            QMessageBox::No,
                            QMessageBox::Cancel | QMessageBox::Escape );
            mb.setButtonText( QMessageBox::Yes, tr("Save modified map before closing it") );
            mb.setButtonText( QMessageBox::No, tr("Discard changes"));
            mb.setModal (true);
            mb.show();
            switch( mb.exec() )
            {
            case QMessageBox::Yes:
                // save and close
                fileSave(m, CompleteMap);
                break;
            case QMessageBox::No:
                // close  without saving
                break;
            case QMessageBox::Cancel:
                // do nothing
                return true;
            }
        }

        vymViews.removeAt (i);
        tabWidget->removeTab (i);

        // Destroy stuff, order is important
        delete (m->getMapEditor());
        delete(vv);
        delete (m);

        updateActions();
        return false;
    }
    return true; // Better don't exit vym if there is no currentModel()...
}

void Main::filePrint()
{
    if (currentMapEditor())
	currentMapEditor()->print();
}

bool Main::fileExitVYM()    
{
    fileSaveSession();

    // Check if one or more editors have changed
    while (vymViews.count()>0)
    {
        tabWidget->setCurrentIndex(0);
        if (fileCloseMap()) return true;
    }
    qApp->quit();
    return false;
}

void Main::editUndo()
{
    VymModel *m=currentModel();
    if (m) m->undo();
}

void Main::editRedo()	   
{
    VymModel *m=currentModel();
    if (m) m->redo();
}

void Main::gotoHistoryStep (int i)     
{
    VymModel *m=currentModel();
    if (m) m->gotoHistoryStep(i);
}

void Main::editCopy()
{
    VymModel *m=currentModel();
    if (m) m->copy();
}

void Main::editPaste()
{
    VymModel *m=currentModel();
    if (m) m->paste();
}

void Main::editCut()
{
    VymModel *m=currentModel();
    if (m) m->cut();
}

bool Main::openURL(const QString &url)
{
    if (url.isEmpty()) return false;

    QString browser=settings.value("/system/readerURL" ).toString();
    QStringList args;
    args<<url;
    if (!QProcess::startDetached(browser,args,QDir::currentPath(),browserPID))
    {
        // try to set path to browser
        QMessageBox::warning(0, 
            tr("Warning"),
            tr("Couldn't find a viewer to open %1.\n").arg(url)+
            tr("Please use Settings->")+tr("Set application to open an URL"));
        settingsURL() ; 
        return false;
    }   
    return true;
}

void Main::openTabs(QStringList urls)
{
    if (urls.isEmpty()) return;
    	
    QStringList args;
    QString browser=settings.value("/system/readerURL" ).toString();
#if defined(VYM_DBUS)
    if ( browser.contains("konqueror") && 
            (browserPID==0 || !QDBusConnection::sessionBus().interface()->registeredServiceNames().value().contains (QString("org.kde.konqueror-%1").arg(*browserPID)))
       )	 
    {
        // Start a new browser, if there is not one running already or
        // if a previously started konqueror is gone.
        if (debug) qDebug() <<"Main::openTabs no konqueror with PID "<<*browserPID<<" found";
        openURL(urls.takeFirst());
        if (debug) qDebug() << "Main::openTabs Started konqueror, new PID is "<<*browserPID;
    }

    if (browser.contains("konqueror"))
    {
        foreach (QString u, urls) 
        {
            // Open new browser
            // Try to open new tab in existing konqueror started previously by vym
            args.clear();

            args<< QString("org.kde.konqueror-%1").arg(*browserPID)<<
                "/konqueror/MainWindow_1"<<
                "newTab" << 
                u <<
                "false";
            if (!QProcess::startDetached ("qdbus",args))
            {
                QMessageBox::warning(0, 
                    tr("Warning"),
                    tr("Couldn't start %1 to open a new tab in %2.").arg("qdbus").arg("konqueror"));
                return;
            }
        }
        return;	
    } 
#endif
    //
    // Other browser, e.g. xdg-open
    // Just open all urls and leave it to the system to cope with it
    foreach (QString u, urls) 
    {
        openURL(u);

        // Now give the browser some time before opening the next tab
#if defined(Q_OS_WIN32)
        // There's no sleep in VCEE, replace it with Qt's QThread::wait().
        this->thread()->wait(1000);
#else
        sleep (1);	
#endif
    }
}


void Main::editOpenURL()
{
    // Open new browser
    VymModel *m=currentModel();
    if (m)
    {	
	QString url=m->getURL();
	if (url=="") return;
        openURL(url);
    }	
}
void Main::editOpenURLTab()
{
    VymModel *m=currentModel();
    if (m)
    {	
	QStringList urls;
	urls.append(m->getURL());
	openTabs (urls);
    }	
}

void Main::editOpenMultipleVisURLTabs(bool ignoreScrolled)
{
    VymModel *m=currentModel();
    if (m)
    {	
	QStringList urls;
	urls=m->getURLs(ignoreScrolled);
	openTabs (urls);
    }	
}

void Main::editOpenMultipleURLTabs()
{
    editOpenMultipleVisURLTabs (false);
}

void Main::editNote2URLs()
{
    VymModel *m=currentModel();
    if (m) m->note2URLs();
}

void Main::editURL()
{
    VymModel *m=currentModel();
    if (m) 
    {
	QInputDialog *dia=new QInputDialog (this);
	dia->setLabelText (tr("Enter URL:"));
	dia->setWindowTitle (vymName);
	dia->setInputMode (QInputDialog::TextInput);
	TreeItem *selti=m->getSelectedItem();
	if (selti) dia->setTextValue (selti->getURL());
	dia->resize(width()*0.6,80);
        centerDialog(dia);

	if ( dia->exec() ) m->setURL (dia->textValue() );
        delete dia;
    }
}

void Main::editLocalURL()
{
    VymModel *m=currentModel();
    if (m) 
    {
	TreeItem *selti=m->getSelectedItem();
	if (selti)
	{	    
	    QString filter;
	    filter+="All files (*);;";
	    filter+=tr("HTML","Filedialog") + " (*.html,*.htm);;";
	    filter+=tr("Text","Filedialog") + " (*.txt);;";
	    filter+=tr("Spreadsheet","Filedialog") + " (*.odp,*.sxc);;";
	    filter+=tr("Textdocument","Filedialog") +" (*.odw,*.sxw);;";
	    filter+=tr("Images","Filedialog") + " (*.png *.bmp *.xbm *.jpg *.png *.xpm *.gif *.pnm)";

	    QString fn=QFileDialog::getOpenFileName( 
		this,
		vymName+" - " +tr("Set URL to a local file"), 
		lastMapDir.path(), 
		filter);

	    if (!fn.isEmpty() )
	    {
		lastMapDir.setPath(fn.left(fn.lastIndexOf ("/")) );
		if (!fn.startsWith("file://") ) 
		    fn="file://" + fn;
		m->setURL (fn);
	    }
	}
    }
}

void Main::editHeading2URL()
{
    VymModel *m=currentModel();
    if (m) m->editHeading2URL();
}

void Main::editBugzilla2URL()
{
    VymModel *m=currentModel();
    if (m) m->editBugzilla2URL();
}

void Main::getBugzillaData()
{
    VymModel *m=currentModel();
    /*
    QProgressDialog progress ("Doing stuff","cancl",0,10,this);
    progress.setWindowModality(Qt::WindowModal);
    //progress.setCancelButton (NULL);
    progress.show();
    progress.setMinimumDuration (0);
    progress.setValue (1);
    progress.setValue (5);
    progress.update();
    */
    /*
    QProgressBar *pb=new QProgressBar;
    pb->setMinimum (0);
    pb->setMaximum (0);
    pb->show();
    pb->repaint();
    */
    if (m) m->getBugzillaData(false);
}

void Main::getBugzillaDataSubtree()
{
    VymModel *m=currentModel();
    if (m) m->getBugzillaData(true);
}

void Main::editFATE2URL()
{
    VymModel *m=currentModel();
    if (m) m->editFATE2URL();
}

void Main::editHeading()
{
    MapEditor *me=currentMapEditor();
    if (me) me->editHeading();
}

void Main::editHeadingFinished(VymModel *m)
{
    if (m)
    {
        if (!actionSettingsAutoSelectNewBranch->isChecked() &&
                !prevSelection.isEmpty())
            m->select(prevSelection);
        prevSelection = "";
    }
}

void Main::openVymLinks(const QStringList &vl, bool background)
{
    QStringList vlmin;
    int index = -1;
    for (int j = 0; j < vl.size(); ++j)
    {
	// compare path with already loaded maps
        QString absPath = QFileInfo(vl.at(j)).absoluteFilePath();
	index = -1;
	for (int i = 0;i <= vymViews.count() -1; i++)
	{
	    if ( absPath == vymViews.at(i)->getModel()->getFilePath() )
	    {
		index = i;
		break;
	    }
	}   
	if (index < 0) vlmin.append ( absPath );
    }

    
    progressCounterTotal = vlmin.size();
    for (int j = 0; j < vlmin.size(); j++)
    {
	// Load map
	if (!QFile(vlmin.at(j)).exists() )
	    QMessageBox::critical( 0, tr( "Critical Error" ),
	       tr("Couldn't open map %1").arg(vlmin.at(j)));
	else
	{
	    fileLoad (vlmin.at(j), NewMap,VymMap);
            if (!background) 
                tabWidget->setCurrentIndex (tabWidget->count()-1);	
	}
    }	    
    // Go to tab containing the map
    if (index >= 0)
	tabWidget->setCurrentIndex (index);	
    removeProgressCounter();
}

void Main::editOpenVymLink(bool background)
{
    VymModel *m = currentModel();
    if (m)
    {
	QStringList vl;
	vl.append(m->getVymLink()); 
	openVymLinks (vl, background);
    }
}

void Main::editOpenVymLinkBackground()
{
    editOpenVymLink (true);
}

void Main::editOpenMultipleVymLinks()
{
    QString currentVymLink;
    VymModel *m = currentModel();
    if (m)
    {
	QStringList vl = m->getVymLinks();
	openVymLinks (vl, true);
    }
}

void Main::editVymLink()
{
    VymModel *m = currentModel();
    if (m)
    {
	BranchItem *bi = m->getSelectedBranch();
	if (bi)
	{	    
	    QStringList filters;
	    filters <<"VYM map (*.vym)";
	    QFileDialog fd;
	    fd.setWindowTitle (vymName+" - " +tr("Link to another map"));
	    fd.setNameFilters (filters);
	    fd.setWindowTitle(vymName+" - " +tr("Link to another map"));
	    fd.setDirectory (lastMapDir);
	    fd.setAcceptMode (QFileDialog::AcceptOpen);
	    if (! bi->getVymLink().isEmpty() )
		fd.selectFile( bi->getVymLink() );
	    fd.show();

	    QString fn;
	    if ( fd.exec() == QDialog::Accepted &&!fd.selectedFiles().isEmpty() )
	    {
		QString fn = fd.selectedFiles().first();
		lastMapDir = QDir (fd.directory().path());
		m->setVymLink (fn);
	    }
	}
    }
}

void Main::editDeleteVymLink()
{
    VymModel *m=currentModel();
    if (m) m->deleteVymLink();	
}

void Main::editToggleHideExport()
{
    VymModel *m=currentModel();
    if (m) m->toggleHideExport();   
}

void Main::editToggleTask()
{
    VymModel *m=currentModel();
    if (m) m->toggleTask();   
}

void Main::editCycleTaskStatus()
{
    VymModel *m=currentModel();
    if (m) m->cycleTaskStatus();   
}

void Main::editTaskSleepN()
{
    VymModel *m=currentModel();
    if (m) 
    {
	int n=((QAction*)sender())->data().toInt();
	Task *task=m->getSelectedTask();
	if (task)
	{
	    bool ok=true;
            QString s;
	    if (n<0)
            {
                n=task->getDaysSleep();
                if (n<=0) n=0;

                LineEditDialog *dia=new LineEditDialog(this);
                dia->setLabel(tr("Enter sleep time (number of days or date YYYY-MM-DD or DD.MM[.YYYY]","task sleep time dialog"));
                dia->setText(QString("%1").arg(n));
                centerDialog (dia);
                if (dia->exec() == QDialog::Accepted)
                {
                    ok=true;
                    s=dia->getText();
                } else
                    ok=false;

                delete dia;
            } else
                s=QString("%1").arg(n);

            if (ok && !m->setTaskSleep(s) )
                QMessageBox::warning(0, 
                    tr("Warning"),
                    tr("Couldn't set sleep time to %1.\n").arg(s));
	}
    }
}

void Main::editAddTimestamp()
{
    VymModel *m=currentModel();
    if (m) m->addTimestamp();	
}

void Main::editMapProperties()    
{
    VymModel *m = currentModel();
    if (!m) return;

    ExtraInfoDialog dia;
    dia.setMapName  ( m->getFileName() );
    dia.setMapTitle ( m->getTitle() );
    dia.setAuthor   ( m->getAuthor() );
    dia.setComment  ( m->getComment() );
    dia.setReadOnly ( m->isReadOnly() );

    // Calc some stats
    QString stats;
    stats += tr("%1 items on map\n","Info about map").arg (m->getScene()->items().size(),6);

    uint b  = 0;
    uint f  = 0;
    uint n  = 0;
    uint xl = 0;
    BranchItem *cur  = NULL;
    BranchItem *prev = NULL;
    m->nextBranch(cur, prev);
    while (cur) 
    {
        if (!cur->getNote().isEmpty() ) n++;
        f += cur->imageCount();
        b++;
        xl += cur->xlinkCount();
        m->nextBranch(cur,prev);
    }

    stats += QString ("%1 %2\n").arg (m->branchCount(),6).arg(tr("branches","Info about map") );
    stats += QString ("%1 %2\n").arg (n,6).arg(tr("notes","Info about map") );
    stats += QString ("%1 %2\n").arg (f,6).arg(tr("images","Info about map") );
    stats += QString ("%1 %2\n").arg (m->taskCount(),6 ).arg(tr("tasks","Info about map") );
    stats += QString ("%1 %2\n").arg (m->slideCount(),6 ).arg(tr("slides","Info about map") );
    stats += QString ("%1 %2\n").arg (xl/2,6).arg(tr("xLinks","Info about map") );
    dia.setStats (stats);

    // Finally show dialog
    if (dia.exec() == QDialog::Accepted)
    {
	m->setAuthor (dia.getAuthor() );
	m->setComment (dia.getComment() );
	m->setTitle (dia.getMapTitle() );
    }
}

void Main::editMoveUp()
{
    MapEditor *me = currentMapEditor();
    VymModel  *m  = currentModel();
    if (me && m && me->getState() != MapEditor::EditingHeading) m->moveUp();
}

void Main::editMoveDown()
{
    MapEditor *me = currentMapEditor();
    VymModel  *m  = currentModel();
    if (me && m && me->getState() != MapEditor::EditingHeading) m->moveDown();
}

void Main::editDetach()
{
    VymModel *m=currentModel();
    if (m) m->detach();
}

void Main::editSortChildren()
{
    VymModel *m=currentModel();
    if (m) m->sortChildren(false);
}

void Main::editSortBackChildren()
{
    VymModel *m=currentModel();
    if (m) m->sortChildren(true);
}

void Main::editToggleScroll()
{
    VymModel *m=currentModel();
    if (m) m->toggleScroll();
}

void Main::editExpandAll()
{
    VymModel *m=currentModel();
    if (m) m->emitExpandAll();
}

void Main::editExpandOneLevel()
{
    VymModel *m=currentModel();
    if (m) m->emitExpandOneLevel();
}

void Main::editCollapseOneLevel()
{
    VymModel *m=currentModel();
    if (m) m->emitCollapseOneLevel();
}

void Main::editCollapseUnselected()
{
    VymModel *m=currentModel();
    if (m) m->emitCollapseUnselected();
}

void Main::editUnscrollChildren()
{
    VymModel *m=currentModel();
    if (m) m->unscrollChildren();
}

void Main::editGrowSelectionSize()
{
    VymModel *m=currentModel();
    if (m) m->growSelectionSize();
}

void Main::editShrinkSelectionSize()
{
    VymModel *m=currentModel();
    if (m) m->shrinkSelectionSize();
}

void Main::editResetSelectionSize()
{
    VymModel *m=currentModel();
    if (m) m->resetSelectionSize();
}

void Main::editAddAttribute()
{
    VymModel *m=currentModel();
    if (m) 
    {

	m->addAttribute();
    }
}

void Main::editAddMapCenter() 
{
    VymModel *m=currentModel();
    if (m) 
    {
	m->select (m->addMapCenter ());
	MapEditor *me=currentMapEditor();
	if (me) 
	{
        m->setHeadingPlainText("");
	    me->editHeading();
	}    
    }
}

void Main::editNewBranch()
{
    VymModel *m=currentModel();
    if (m)
    {
	BranchItem *bi=m->addNewBranch();
	if (!bi) return;

	if (actionSettingsAutoEditNewBranch->isChecked() 
	     && !actionSettingsAutoSelectNewBranch->isChecked() )
	    prevSelection=m->getSelectString();
	else	
	    prevSelection=QString();

	if (actionSettingsAutoSelectNewBranch->isChecked()  
	    || actionSettingsAutoEditNewBranch->isChecked()) 
	{
	    m->select (bi);
	    if (actionSettingsAutoEditNewBranch->isChecked())
		currentMapEditor()->editHeading();
	}
    }	
}

void Main::editNewBranchBefore()
{
    VymModel *m=currentModel();
    if (m)
    {
	BranchItem *bi=m->addNewBranchBefore();

	if (bi) 
	    m->select (bi);
	else
	    return;

	if (actionSettingsAutoEditNewBranch->isChecked())
	{
	    if (!actionSettingsAutoSelectNewBranch->isChecked())
		prevSelection=m->getSelectString(bi); 
	    currentMapEditor()->editHeading();
	}
    }	
}

void Main::editNewBranchAbove()	
{
    VymModel *m=currentModel();
    if (m)
    {
	BranchItem *selbi=m->getSelectedBranch();
	if (selbi)
	{
	    BranchItem *bi=m->addNewBranch(selbi,-3);

	    if (bi) 
		m->select (bi);
	    else
		return;

	    if (actionSettingsAutoEditNewBranch->isChecked())
	    {
		if (!actionSettingsAutoSelectNewBranch->isChecked())
		    prevSelection=m->getSelectString (bi);	
		currentMapEditor()->editHeading();
	    }
	}
    }	
}

void Main::editNewBranchBelow()
{
    VymModel *m=currentModel();
    if (m)
    {
	BranchItem *selbi=m->getSelectedBranch();
	if (selbi)
	{
	    BranchItem *bi=m->addNewBranch(selbi,-1);

	    if (bi) 
		m->select (bi);
	    else
		return;

	    if (actionSettingsAutoEditNewBranch->isChecked())
	    {
		if (!actionSettingsAutoSelectNewBranch->isChecked())
		    prevSelection=m->getSelectString(bi);
		currentMapEditor()->editHeading();
	    }
	}
    }	
}

void Main::editImportAdd()
{
    fileLoad (ImportAdd);
}

void Main::editImportReplace()
{
    fileLoad (ImportReplace);
}

void Main::editSaveBranch() 
{
    fileSaveAs (PartOfMap);
}

void Main::editDeleteKeepChildren()
{
    VymModel *m=currentModel();
     if (m) m->deleteKeepChildren();
}

void Main::editDeleteChildren()
{
    VymModel *m=currentModel();
    if (m) m->deleteChildren();
}

void Main::editDeleteSelection()
{
    VymModel *m=currentModel();
    if (m) m->deleteSelection();
}

void Main::editLoadImage()
{
    VymModel *m=currentModel();
    if (m) m->loadImage();
}

void Main::editSaveImage()
{
    VymModel *m=currentModel();
    if (m) m->saveImage();
}

void Main::editEditXLink(QAction *a)
{
    VymModel *m=currentModel();
    if (m)
    {
	BranchItem *selbi=m->getSelectedBranch();
	if (selbi)
	{
	    Link *l=selbi->getXLinkItemNum(branchXLinksContextMenuEdit->actions().indexOf(a))->getLink();
	    if (l && m->select (l->getBeginLinkItem() ) )
		m->editXLink();
	}    
    }	
}

void Main::popupFollowXLink()
{
    branchXLinksContextMenuFollow->exec( QCursor::pos());
}

void Main::editFollowXLink(QAction *a)
{
    VymModel *m=currentModel();

    if (m)
	m->followXLink(branchXLinksContextMenuFollow->actions().indexOf(a));
}

void Main::editToggleTarget()  
{
    VymModel *m=currentModel();
    if (m) m->toggleTarget();
}

bool Main::initTargetsMenu( VymModel *model, QMenu *menu)
{
    if (model)
    {
        ItemList targets = model->getTargets();

        menu->clear();

        QStringList targetNames;
        QList <uint> targetIDs;

        // Build QStringList with all names of targets
        QMap<uint,QString>::const_iterator i;
        i = targets.constBegin();
        while (i != targets.constEnd()) 
        {
            targetNames.append( i.value() );
            targetIDs.append( i.key() );
            ++i;
        }

        // Sort list of names
        targetNames.sort( Qt::CaseInsensitive );

        // Build menu based on sorted names
        while ( !targetNames.isEmpty() )
        {
            // Find target by value
            i = targets.constBegin();
            while (i != targets.constEnd()) 
            {
                if ( i.value() == targetNames.first() ) break;
                ++i;
            }

            menu->addAction( targetNames.first() )->setData( i.key() );
            targetNames.removeFirst();
            targets.remove( i.key() );
        }
        return true;
    }
    return false;
}

void Main::editGoToTarget()  
{
    VymModel *model = currentModel();
    if (initTargetsMenu( model, targetsContextMenu ) )
    {
	QAction *a = targetsContextMenu->exec (QCursor::pos());
	if (a) model->select (model->findID (a->data().toUInt() ) );
    }
}

void Main::editMoveToTarget()  
{
    VymModel *model = currentModel();
    if (initTargetsMenu( model, targetsContextMenu ) )
    {
	QAction *a = targetsContextMenu->exec (QCursor::pos());
	if (a) 
	{
	    TreeItem *ti = model->findID ( a->data().toUInt() );
	    BranchItem *selbi = model->getSelectedBranch();
	    if (!selbi) return;

	    if (ti && ti->isBranchLikeType() && selbi)
	    {
		BranchItem *pi = selbi->parentBranch();
		// If branch below exists, select that one
		// Makes it easier to quickly resort using the MoveTo function
		BranchItem *below = pi->getBranchNum( selbi->num() + 1 );
		LinkableMapObj *lmo = selbi->getLMO();
		QPointF orgPos;
		if (lmo) orgPos = lmo->getAbsPos();

		if (model->relinkBranch ( selbi, (BranchItem*)ti, -1, true, orgPos) )
		{
		    if (below) 
			model->select (below);
		    else    
			if (pi) model->select (pi);
		}    
	    }	    
	}
    }
}

void Main::editSelectPrevious()  
{
    VymModel *m=currentModel();
    if (m) m->selectPrevious();
}

void Main::editSelectNext()  
{
    VymModel *m=currentModel();
    if (m) m->selectNext();
}

void Main::editSelectNothing()  
{
    VymModel *m=currentModel();
    if (m) m->unselectAll();
}

void Main::editOpenFindResultWidget()  
{
    if (!findResultWidget->parentWidget()->isVisible())
    {
//	findResultWidget->parentWidget()->show();
	findResultWidget->popup();
    } else 
	findResultWidget->parentWidget()->hide();
}

#include "findwidget.h" // FIXME-4 Integrated FRW and FW
void Main::editFindNext(QString s)  
{
    Qt::CaseSensitivity cs=Qt::CaseInsensitive;
    VymModel *m=currentModel();
    if (m) 
    {
	if (m->findAll (findResultWidget->getResultModel(),s,cs) )
	    findResultWidget->setStatus (FindWidget::Success);
	else
	    findResultWidget->setStatus (FindWidget::Failed);
    }
}

void Main::editFindDuplicateURLs() //FIXME-4 feature: use FindResultWidget for display
{
    VymModel *m=currentModel();
    if (m) m->findDuplicateURLs();
}

void Main::updateQueries (VymModel* ) //FIXME-4 disabled for now to avoid selection in FRW
{
    return;
/*
    qDebug() << "MW::updateQueries m="<<m<<"   cM="<<currentModel();
    if (m && currentModel()==m)
    {
	QString s=findResultWidget->getFindText();
	if (!s.isEmpty() ) editFindNext (s);
    }	
*/
}

void Main::formatPickColor()
{
    VymModel *m=currentModel();
    if (m)
	setCurrentColor ( m->getCurrentHeadingColor() );
}

QColor Main::getCurrentColor()
{
    return currentColor;
}

void Main::setCurrentColor(QColor c)
{
    QPixmap pix( 16, 16 );
    pix.fill( c );
    actionFormatColor->setIcon( pix );
    currentColor=c;
}

void Main::formatSelectColor()
{
    QColor col = QColorDialog::getColor((currentColor ), this );
    if ( !col.isValid() ) return;
    setCurrentColor( col );
}

void Main::formatColorBranch()
{
    VymModel *m=currentModel();
    if (m) m->colorBranch(currentColor);
}

void Main::formatColorSubtree()
{
    VymModel *m=currentModel();
    if (m) m->colorSubtree (currentColor);
}

void Main::formatLinkStyleLine()
{
    VymModel *m=currentModel();
    if (m)
    {
	m->setMapLinkStyle("StyleLine");
        actionFormatLinkStyleLine->setChecked(true);
    }
}

void Main::formatLinkStyleParabel()
{
    VymModel *m=currentModel();
    if (m)
    {
	m->setMapLinkStyle("StyleParabel");
        actionFormatLinkStyleParabel->setChecked(true);
    }
}

void Main::formatLinkStylePolyLine()
{
    VymModel *m=currentModel();
    if (m)
    {
	m->setMapLinkStyle("StylePolyLine");
        actionFormatLinkStylePolyLine->setChecked(true);
    }
}

void Main::formatLinkStylePolyParabel()
{
    VymModel *m=currentModel();
    if (m)
    {
	m->setMapLinkStyle("StylePolyParabel");
        actionFormatLinkStylePolyParabel->setChecked(true);
    }
}

void Main::formatSelectBackColor()
{
    VymModel *m=currentModel();
    if (m) m->selectMapBackgroundColor();
}

void Main::formatSelectBackImage()
{
    VymModel *m=currentModel();
    if (m)
	m->selectMapBackgroundImage();
}

void Main::formatSelectLinkColor()
{
    VymModel *m=currentModel();
    if (m)
    {
	QColor col = QColorDialog::getColor( m->getMapDefLinkColor(), this );
	m->setMapDefLinkColor( col );
    }
}

void Main::formatSelectSelectionColor()
{
    VymModel *m=currentModel();
    if (m)
    {
	QColor col = QColorDialog::getColor( m->getMapDefLinkColor(), this );
	m->setSelectionColor (col);
    }

}

void Main::formatSelectFont()
{
    VymModel *m=currentModel();
    if (m) 
    {
	bool ok;
	QFont font = QFontDialog::getFont( &ok, m->getMapDefaultFont(), this);
	if (ok) m->setMapDefaultFont (font);
    }
}

void Main::formatToggleLinkColorHint()
{
    VymModel *m=currentModel();
    if (m) m->toggleMapLinkColorHint();
}

void Main::formatHideLinkUnselected()	//FIXME-4 get rid of this with imagepropertydialog
{
    VymModel *m=currentModel();
    if (m)
	m->setHideLinkUnselected(actionFormatHideLinkUnselected->isChecked());
}

void Main::viewZoomReset()
{
    MapEditor *me=currentMapEditor();
    if (me) me->setViewCenterTarget();
}

void Main::viewZoomIn()
{
    MapEditor *me=currentMapEditor();
    if (me) me->setZoomFactorTarget (me->getZoomFactorTarget()*1.15);
}

void Main::viewZoomOut()
{
    MapEditor *me=currentMapEditor();
    if (me) me->setZoomFactorTarget (me->getZoomFactorTarget()*0.85);
}

void Main::viewRotateCounterClockwise()
{
    MapEditor *me=currentMapEditor();
    if (me) me->setAngleTarget (me->getAngleTarget()-10);
}

void Main::viewRotateClockwise()
{
    MapEditor *me=currentMapEditor();
    if (me) me->setAngleTarget (me->getAngleTarget()+10);
}

void Main::viewCenter()
{
    VymModel *m=currentModel();
    if (m) m->emitShowSelection();
}

void Main::networkStartServer()
{
    VymModel *m=currentModel();
    if (m) m->newServer();
}

void Main::networkConnect()
{
    VymModel *m=currentModel();
    if (m) m->connectToServer();
}

void Main::downloadFinished()   // only used for drop events in mapeditor and VM::downloadImage 
{
    QString s;
    DownloadAgent *agent = static_cast<DownloadAgent*>(sender());
    agent->isSuccess() ? s="Success" : s="Error  "; 

    /*
    qDebug()<<"Main::downloadFinished ";
    qDebug()<<"  result" <<  s;
    qDebug()<<"     msg" << agent->getResultMessage();
    */

    QString script = agent->getFinishedScript();
    VymModel *model=getModel (agent->getFinishedScriptModelID());
    if (!script.isEmpty() && model)
    {
        script.replace("$TMPFILE",agent->getDestination());
        model->execute(script);
    }
}

bool Main::settingsPDF()
{
    // Default browser is set in constructor
    bool ok;
    QString text = QInputDialog::getText(
	this,
	"VYM", tr("Set application to open PDF files")+":", QLineEdit::Normal,
	settings.value("/system/readerPDF").toString(), &ok);
    if (ok)
	settings.setValue ("/system/readerPDF",text);
    return ok;
}


bool Main::settingsURL()
{
    // Default browser is set in constructor
    bool ok;
    QString text = QInputDialog::getText(
	this,
	"VYM", tr("Set application to open an URL")+":", QLineEdit::Normal,
	settings.value("/system/readerURL").toString()
	, &ok);
    if (ok)
	settings.setValue ("/system/readerURL",text);
    return ok;
}

void Main::settingsZipTool()
{
    // Default zip tool is 7z on windows, zip/unzip elsewhere
    bool ok = false;

#if defined(Q_OS_WIN32)
    QString filter;
    filter = "Windows executable (*.exe);;";

    QString fn = QFileDialog::getOpenFileName( 
        this,
        vymName + " - " + tr("Set application to zip/unzip files") + ":", 
        zipToolPath,
        filter);

    if (!fn.isEmpty() ) ok = true;

#else

    QString fn = QInputDialog::getText(
                this,
                vymName, tr("Set application to zip/unzip files")+":", QLineEdit::Normal,
                zipToolPath, &ok);
#endif

    if (ok)
    {
        zipToolPath = fn;
        settings.setValue ("/system/zipToolPath", zipToolPath);
    }
}

void Main::settingsMacroDir()
{
    QDir defdir(vymBaseDir.path() + "/macros");
    if (!defdir.exists())
	defdir=vymBaseDir;
    QDir dir=QFileDialog::getExistingDirectory (
	this,
	tr ("Directory with vym macros:"), 
	settings.value ("/macros/macroDir",defdir.path()).toString()
    );
    if (dir.exists())
	settings.setValue ("/macros/macroDir",dir.absolutePath());
}

void Main::settingsUndoLevels()	    
{
    bool ok;
    int i = QInputDialog::getInt(
	this, 
	"QInputDialog::getInt()",
	tr("Number of undo/redo levels:"), settings.value("/history/stepsTotal",1000).toInt(), 0, 100000, 1, &ok);
    if (ok)
    {
	settings.setValue ("/history/stepsTotal",i);
	QMessageBox::information( this, tr( "VYM -Information:" ),
	   tr("Settings have been changed. The next map opened will have \"%1\" undo/redo levels").arg(i)); 
   }	
}

bool Main::useAutosave()
{
    return actionSettingsToggleAutosave->isChecked();
}

void Main::setAutosave(bool b)
{
    actionSettingsToggleAutosave->setChecked(b);
}

void Main::settingsAutosaveTime()
{
    bool ok;
    int i = QInputDialog::getInt(
	this, 
	vymName,
	tr("Number of seconds before autosave:"), settings.value("/system/autosave/ms").toInt() / 1000, 10, 60000, 1, &ok);
    if (ok)
	settings.setValue ("/system/autosave/ms",i * 1000);
}

void Main::settingsDefaultMapAuthor()	    
{
    bool ok;
    QString s = QInputDialog::getText(
                this,
                vymName, tr("Set author for new maps (used in lockfile)") + ":", QLineEdit::Normal,
                settings.value("/user/name", tr("unknown user","default name for map author in settings")).toString(), 
                &ok);
    if (ok) settings.setValue("/user/name", s);
}

void Main::settingsShowParentsLevelFindResults()	    
{
    bool ok;
    int i = QInputDialog::getInt(
	this, 
	vymName,
	tr("Number of parents shown in find results:"), findResultWidget->getResultModel()->getShowParentsLevel(), 0, 10, 0, &ok);
    if (ok) findResultWidget->getResultModel()->setShowParentsLevel(i);
}

void Main::settingsShowParentsLevelTasks()	    
{
    bool ok;
    int i = QInputDialog::getInt(
	this, 
	vymName,
	tr("Number of parents shown for a task:"), taskModel->getShowParentsLevel(), 0, 10, 0, &ok);
    if (ok) taskModel->setShowParentsLevel(i);
}

void Main::settingsToggleAutoLayout()
{
    settings.setValue ("/mainwindow/autoLayout/use",actionSettingsToggleAutoLayout->isChecked() );
}

void Main::settingsToggleWriteBackupFile()
{
    settings.setValue ("/system/writeBackupFile",actionSettingsWriteBackupFile->isChecked() );
}

void Main::settingsToggleAnimation()
{
    settings.setValue ("/animation/use",actionSettingsUseAnimation->isChecked() );
}

void Main::settingsToggleDownloads()
{
    downloadsEnabled(true);
}

void Main::windowToggleNoteEditor()
{
    if (noteEditor->parentWidget()->isVisible() )
        noteEditor->parentWidget()->hide();
    else
    {
        noteEditor->parentWidget()->show();
        noteEditor->setFocus();
    }
}

void Main::windowToggleTreeEditor()
{
    if ( tabWidget->currentWidget())
	vymViews.at(tabWidget->currentIndex())->toggleTreeEditor();
}

void Main::windowToggleTaskEditor()
{
    if (taskEditor->parentWidget()->isVisible() )
    {
	taskEditor->parentWidget()->hide();
	actionViewToggleTaskEditor->setChecked (false);
    } else
    {
	taskEditor->parentWidget()->show();
	actionViewToggleTaskEditor->setChecked (true);
    }
}

void Main::windowToggleSlideEditor()
{
    if ( tabWidget->currentWidget())
	vymViews.at(tabWidget->currentIndex())->toggleSlideEditor();
}

void Main::windowToggleScriptEditor()
{
    if (scriptEditor->parentWidget()->isVisible() )
    {
	scriptEditor->parentWidget()->hide();
	actionViewToggleScriptEditor->setChecked (false);
    } else
    {
	scriptEditor->parentWidget()->show();
	actionViewToggleScriptEditor->setChecked (true);
    }
}

void Main::windowToggleHistory()
{
    if (historyWindow->isVisible())
	historyWindow->parentWidget()->hide();
    else    
	historyWindow->parentWidget()->show();
}

void Main::windowToggleProperty()
{
    if (branchPropertyEditor->parentWidget()->isVisible())
	branchPropertyEditor->parentWidget()->hide();
    else    
	branchPropertyEditor->parentWidget()->show();
    branchPropertyEditor->setModel (currentModel() );
}

void Main::windowShowHeadingEditor()
{
    headingEditorDW->show();
}

void Main::windowToggleHeadingEditor()
{
    if (headingEditor->parentWidget()->isVisible() )
        headingEditor->parentWidget()->hide();
    else
    {
        headingEditor->parentWidget()->show();
        headingEditor->setFocus();
    }
}

void Main::windowToggleAntiAlias()
{
    bool b=actionViewToggleAntiAlias->isChecked();
    MapEditor *me;
    for (int i=0;i<vymViews.count();i++)
    {
	me=vymViews.at(i)->getMapEditor();
	if (me) me->setAntiAlias(b);
    }	

}

bool Main::isAliased()
{
    return actionViewToggleAntiAlias->isChecked();
}

bool Main::hasSmoothPixmapTransform()
{
    return actionViewToggleSmoothPixmapTransform->isChecked();
}

void Main::windowToggleSmoothPixmap()
{
    bool b=actionViewToggleSmoothPixmapTransform->isChecked();
    MapEditor *me;
    for (int i=0;i<vymViews.count();i++)
    {
	
	me=vymViews.at(i)->getMapEditor();
	if (me) me->setSmoothPixmap(b);
    }	
}

void Main::updateHistory(SimpleSettings &undoSet)
{
    historyWindow->update (undoSet);
}

void Main::updateHeading()
{
    VymModel *m=currentModel();
    if (m) m->setHeading (headingEditor->getVymText() );
}

void Main::updateNoteFlag() 
{
    // this slot is connected to noteEditor::textHasChanged()
    VymModel *m=currentModel();
    if (m) m->updateNoteFlag();
}

void Main::updateNoteEditor(QModelIndex index ) //FIXME-4 maybe change to TreeItem as parameter?
{
    if (index.isValid() )
    {
        TreeItem *ti=((VymModel*) QObject::sender())->getItem(index);
        /*
    qDebug()<< "Main::updateNoteEditor model="<<sender()
        << "  item="<<ti->getHeading()<<" ("<<ti<<")";
    qDebug()<< "RT="<<ti->getNote().isRichText();
    */
        if (ti)
            noteEditor->setNote (ti->getNote() );
        updateDockWidgetTitles( ti->getModel());
    }
}

void Main::selectInNoteEditor(QString s,int i)
{
    // TreeItem is already selected at this time, therefor
    // the note is already in the editor
    noteEditor->findText (s,0,i);
}

void Main::setFocusMapEditor()
{
    VymView *vv=currentView();
    if (vv) vv->setFocusMapEditor();
}

void Main::changeSelection (VymModel *model, const QItemSelection &newsel, const QItemSelection &)
{
    branchPropertyEditor->setModel (model ); 

    if (model && model == currentModel() )
    {
	TreeItem *ti;
	if (!newsel.indexes().isEmpty() )
        {
            ti = model->getItem(newsel.indexes().first());

            // Update note editor
            if (!ti->hasEmptyNote() )
                noteEditor->setNote(ti->getNote() );
            else
                noteEditor->setNote(VymNote() );
            // Show URL and link in statusbar
            QString status;
            QString s = ti->getURL();
            if (!s.isEmpty() ) status += "URL: " + s + "  ";
            s = ti->getVymLink();
            if (!s.isEmpty() ) status += "Link: " + s;
            if (!status.isEmpty() ) statusMessage (status);

            headingEditor->setVymText (ti->getHeading() );

            // Select in TaskEditor, if necessary
            Task *t = NULL;
            if (ti->isBranchLikeType() )
                t = ((BranchItem*)ti)->getTask();

            if (t)
                taskEditor->select (t);
            else
                taskEditor->clearSelection();
        } else
            noteEditor->setInactive();

        updateActions();
    }
}

void Main::updateDockWidgetTitles( VymModel *model)
{
    QString s;
    if (model && !model->isRepositionBlocked() ) 
    {
	BranchItem *bi = model->getSelectedBranch();
        if (bi) s = bi->getHeadingPlain();

        noteEditor->setEditorTitle(s);
        noteEditorDW->setWindowTitle (noteEditor->getEditorTitle() );
    }
}

void Main::updateActions()
{
    // updateActions is also called when satellites are closed	
    actionViewToggleNoteEditor->setChecked (noteEditor->parentWidget()->isVisible());
    actionViewToggleTaskEditor->setChecked (taskEditor->parentWidget()->isVisible());
    actionViewToggleHistoryWindow->setChecked (historyWindow->parentWidget()->isVisible());
    actionViewTogglePropertyEditor->setChecked (branchPropertyEditor->parentWidget()->isVisible());
    actionViewToggleScriptEditor->setChecked (scriptEditor->parentWidget()->isVisible());

    VymView *vv=currentView();
    if (vv)
    {
	actionViewToggleTreeEditor->setChecked ( vv->treeEditorIsVisible() );
	actionViewToggleSlideEditor->setChecked( vv->slideEditorIsVisible() );
    } else	
    {
	actionViewToggleTreeEditor->setChecked  ( false );
	actionViewToggleSlideEditor->setChecked ( false );
    }

    VymModel  *m =currentModel();
    if ( m ) 
    {
        // readonly mode
        if (m->isReadOnly() )
        {
            // Disable toolbars
            standardFlagsMaster->setEnabled (false);

            // Disable map related actions
            foreach (QAction *a, restrictedMapActions)
                a->setEnabled( false );

            // FIXME-2 updateactions: refactor actionListFiles: probably not needed, wrong actions there atm
        } else
        {   // not readonly     // FIXME-2 updateactions: maybe only required in testing, as mode should not change
            
            // Enable toolbars
            standardFlagsMaster->setEnabled (true);

            // Enable map related actions
            foreach (QAction *a, restrictedMapActions)
                a->setEnabled( true );

        }
	// Enable all files actions first   
	for (int i=0; i<actionListFiles.size(); ++i)	
	    actionListFiles.at(i)->setEnabled(true);

        foreach (QAction *a, unrestrictedMapActions)
            a->setEnabled( true  );
        
	// Disable other actions for now
	for (int i=0; i<actionListBranches.size(); ++i) 
	    actionListBranches.at(i)->setEnabled(false);

	for (int i=0; i<actionListItems.size(); ++i) 
	    actionListItems.at(i)->setEnabled(false);

	// Link style in context menu
	switch (m->getMapLinkStyle())
	{
	    case LinkableMapObj::Line: 
		actionFormatLinkStyleLine->setChecked(true);
		break;
	    case LinkableMapObj::Parabel:
		actionFormatLinkStyleParabel->setChecked(true);
		break;
	    case LinkableMapObj::PolyLine:  
		actionFormatLinkStylePolyLine->setChecked(true);
		break;
	    case LinkableMapObj::PolyParabel:	
		actionFormatLinkStylePolyParabel->setChecked(true);
		break;
	    default:
		break;
	}	

	// Update colors
	QPixmap pix( 16, 16 );
	pix.fill( m->getMapBackgroundColor() );
	actionFormatBackColor->setIcon( pix );
	pix.fill( m->getSelectionColor() );
	actionFormatSelectionColor->setIcon( pix );
	pix.fill( m->getMapDefLinkColor() );
	actionFormatLinkColor->setIcon( pix );

	// Selection history
	if (!m->canSelectPrevious() )
	    actionSelectPrevious->setEnabled(false);

	if (!m->canSelectNext() )
	    actionSelectNext->setEnabled(false);
	    
	if (!m->getSelectedItem() )
	    actionSelectNothing->setEnabled (false);

	// Save
	if (! m->hasChanged() )
	    actionFileSave->setEnabled( false);

	// Undo/Redo
	if (! m->isUndoAvailable())
	    actionUndo->setEnabled( false);

	if (! m->isRedoAvailable())
	    actionRedo->setEnabled( false);

	// History window
	historyWindow->setWindowTitle (vymName + " - " +tr("History for %1","Window Caption").arg(m->getFileName()));

	// Expanding/collapsing
	actionExpandAll->setEnabled (true);
	actionExpandOneLevel->setEnabled (true);
	actionCollapseOneLevel->setEnabled (true);
	actionCollapseUnselected->setEnabled (true);

	if (m->getMapLinkColorHint()==LinkableMapObj::HeadingColor) 
	    actionFormatLinkColorHint->setChecked(true);
	else    
	    actionFormatLinkColorHint->setChecked(false);

	// Export last
	QString s, t, u, v;
	if (m && m->exportLastAvailable(s,t,u, v) )
	    actionFileExportLast->setEnabled (true);
	else
	{
	    actionFileExportLast->setEnabled (false);
	    t=u="";
	    s=" - ";
	}	
	actionFileExportLast->setText( tr( "Export in last used format (%1) to: %2","status tip" ).arg(s).arg(u));

	TreeItem *selti=m->getSelectedItem();
	BranchItem *selbi=m->getSelectedBranch();

	if (selti)
	{   // Tree Item selected
	    actionToggleTarget->setChecked (selti->isTarget() );
	    actionDelete->setEnabled (true);
	    actionDeleteChildren->setEnabled (true);

	    if (selbi || selti->getType()==TreeItem::Image)
	    {
		actionFormatHideLinkUnselected->setChecked (((MapItem*)selti)->getHideLinkUnselected());
		actionFormatHideLinkUnselected->setEnabled (true);
	    }

	    if (selbi)	
	    {	// Branch Item selected
		for (int i=0; i<actionListBranches.size(); ++i)	
		    actionListBranches.at(i)->setEnabled(true);

		actionHeading2URL->setEnabled (true);  

		// Note
                actionGetURLsFromNote->setEnabled (!selbi->getNote().isEmpty());

		// Take care of xlinks  
		// FIXME-4 similar code in mapeditor mousePressEvent
		int b=selbi->xlinkCount();
		branchXLinksContextMenuEdit->setEnabled(b);
		branchXLinksContextMenuFollow->setEnabled(b);
		branchXLinksContextMenuEdit->clear();
		branchXLinksContextMenuFollow->clear();
		if (b)
		{
		    BranchItem *bi;
		    QString s;
		    for (int i=0; i<selbi->xlinkCount();++i)
		    {
			bi=selbi->getXLinkItemNum(i)->getPartnerBranch();
			if (bi)
			{
                s=bi->getHeadingPlain();
			    if (s.length()>xLinkMenuWidth)
				s=s.left(xLinkMenuWidth)+"...";
			    branchXLinksContextMenuEdit->addAction (s);
			    branchXLinksContextMenuFollow->addAction (s);
			}   
		    }
		}
		//Standard Flags
		standardFlagsMaster->updateToolBar (selbi->activeStandardFlagNames() );

		// System Flags
		actionToggleScroll->setEnabled (true);
		if ( selbi->isScrolled() )
		    actionToggleScroll->setChecked(true);
		else	
		    actionToggleScroll->setChecked(false);

		actionGetBugzillaDataSubtree->setEnabled (bugzillaClientAvailable);
		if ( selti->getURL().isEmpty() )
		{
		    actionOpenURL->setEnabled (false);
		    actionOpenURLTab->setEnabled (false);
		    actionGetBugzillaData->setEnabled (false);
		}   
		else	
		{
		    actionOpenURL->setEnabled (true);
		    actionOpenURLTab->setEnabled (true);
		    actionGetBugzillaData->setEnabled (
			selti->getURL().contains("bugzilla") && bugzillaClientAvailable);
		}
		if ( selti->getVymLink().isEmpty() )
		{
		    actionOpenVymLink->setEnabled (false);
		    actionOpenVymLinkBackground->setEnabled (false);
		    actionDeleteVymLink->setEnabled (false);
		} else	
		{
		    actionOpenVymLink->setEnabled (true);
		    actionOpenVymLinkBackground->setEnabled (true);
		    actionDeleteVymLink->setEnabled (true);
		}   

		if (!selbi->canMoveUp()) 
		    actionMoveUp->setEnabled (false);

		if (!selbi->canMoveDown()) 
		    actionMoveDown->setEnabled (false);

		if (selbi->branchCount() <2 )
		{
		    actionSortChildren->setEnabled (false);
		    actionSortBackChildren->setEnabled (false);
		}

		actionToggleHideExport->setEnabled (true);  
		actionToggleHideExport->setChecked (selbi->hideInExport() );	

		actionToggleTask->setEnabled (true);  
		if (!selbi->getTask() )
		    actionToggleTask->setChecked (false);
		else
		    actionToggleTask->setChecked (true);

		if (!clipboardEmpty)
		    actionPaste->setEnabled (true); 
		else	
		    actionPaste->setEnabled (false);	

		actionToggleTarget->setEnabled (true);
	    }	// end of BranchItem

	    if ( selti->getType()==TreeItem::Image)
	    {
		for (int i=0; i<actionListBranches.size(); ++i)	
		    actionListBranches.at(i)->setEnabled(false);

		standardFlagsMaster->setEnabled (false);

		actionOpenURL->setEnabled (false);
		actionOpenVymLink->setEnabled (false);
		actionOpenVymLinkBackground->setEnabled (false);
		actionDeleteVymLink->setEnabled (false);    
		actionToggleHideExport->setEnabled (true);  
		actionToggleHideExport->setChecked (selti->hideInExport() );	

		actionToggleTarget->setEnabled (true);

		actionPaste->setEnabled (false); 
		actionDelete->setEnabled (true);

		actionGrowSelectionSize->setEnabled (true);
		actionShrinkSelectionSize->setEnabled (true);
		actionResetSelectionSize->setEnabled (true);
	    }	// Image
	} // TreeItem 
	
	// Check (at least for some) multiple selection //FIXME-4
	QList <TreeItem*> selItems=m->getSelectedItems();
	if (selItems.count()>0 )
	{
	    actionDelete->setEnabled (true);
	    actionToggleHideExport->setEnabled (true);  
	    actionToggleHideExport->setChecked (false);	
	}

	QList <BranchItem*> selbis=m->getSelectedBranches();
	if (selbis.count()>0 )
	    actionFormatColorBranch->setEnabled (true);

    } else
    {
        // No map available 
        for (int i=0; i<actionListFiles.size(); ++i)	
            actionListFiles.at(i)->setEnabled(false);

        foreach (QAction *a, unrestrictedMapActions)
            a->setEnabled( false );

        // Disable toolbars
        standardFlagsMaster->setEnabled (false);
    }
}

Main::ModMode Main::getModMode()
{
    if (actionModModeColor->isChecked()) return ModModeColor;
    if (actionModModeCopy->isChecked()) return ModModeCopy;
    if (actionModModeXLink->isChecked()) return ModModeXLink;
    return ModModeNone;
}

bool Main::autoEditNewBranch()
{
    return actionSettingsAutoEditNewBranch->isChecked();
}

bool Main::autoSelectNewBranch()
{
    return actionSettingsAutoSelectNewBranch->isChecked();
}

void Main::setScriptFile (const QString &fn)
{
    scriptEditor->setScriptFile (fn);
}

QVariant Main::execute (const QString &script)
{
    VymModel *m=currentModel();
    if (m) return m->execute (script);
    return QVariant();
}

void Main::executeEverywhere (const QString &script)
{
    foreach (VymView *vv,vymViews)
    {
	VymModel *m=vv->getModel();
	if (m) m->execute (script);
    }
}

void Main::gotoWindow (const int &n)
{
    if (n < tabWidget->count() && n>=0 )
	tabWidget->setCurrentIndex (n);
}

void Main::windowNextEditor()
{
    if (tabWidget->currentIndex() < tabWidget->count())
	tabWidget->setCurrentIndex (tabWidget->currentIndex() +1);
}

void Main::windowPreviousEditor()
{
    if (tabWidget->currentIndex() >0)
	tabWidget->setCurrentIndex (tabWidget->currentIndex() -1);
}

void Main::nextSlide()
{
    VymView *cv=currentView();
    if (cv) cv->nextSlide();
}


void Main::previousSlide()
{
    VymView *cv=currentView();
    if (cv) cv->previousSlide();
}

void Main::standardFlagChanged()
{
    MapEditor *me = currentMapEditor();
    VymModel  *m  = currentModel();
    if (me && m && me->getState() != MapEditor::EditingHeading) 
    {
        if ( actionSettingsUseFlagGroups->isChecked() )
            m->toggleStandardFlag(sender()->objectName(),standardFlagsMaster);
        else
            m->toggleStandardFlag(sender()->objectName());
        updateActions();
    }
}

void Main::testFunction1()
{
    /*
    VymModel *m = currentModel();
    if (m)
    {
        m->getMapEditor()->minimizeView();
    }
    */

    /*
    QString zp = "c:\\Program Files\\7-Zip\\7z.exe";
    QString zipName = "c:\\Users\\uwdr9542\\x\\ü1.vym";
    QString zipDir  = "c:\\Users\\uwdr9542\\x\\y\\";
    */
    QString zp = "c:/Program Files/7-Zip/7z.exe";
    QString zipName = "c:/Users/uwdr9542/x/ü1.vym";
    QString zipDir  = "c:/Users/uwdr9542/x/y/";

    zp = QDir::toNativeSeparators(zp);
    zipName = QDir::toNativeSeparators(zipName);
    zipDir  = QDir::toNativeSeparators(zipDir);
    QStringList args;
    args << "a" << zipName << "-tzip" << "-scsUTF-8" << "-sccUTF-8" << "*" ;

    VymProcess *zipProc=new VymProcess ();
    zipProc->setWorkingDirectory (zipDir);

    zipProc->start(zipToolPath, args);
    qDebug() << "7z:" << zp;
    qDebug() << "7z started in dir: " << zipProc->workingDirectory();
    qDebug() << "args:" << args;
    qDebug() << zipProc->getStdout()<<flush;
}

void Main::testFunction2()
{
}

void Main::toggleWinter()
{
    if (!currentMapEditor()) return;
    currentMapEditor()->toggleWinter();
}

void Main::toggleHideExport()
{
    VymModel *m=currentModel();
    if (!m) return;
    if (actionToggleHideMode->isChecked() )
        m->setHideTmpMode (TreeItem::HideExport);
    else
        m->setHideTmpMode (TreeItem::HideNone);
}

void Main::testCommand()
{
    if (!currentMapEditor()) return;
    scriptEditor->show();
}

void Main::helpDoc()
{
    QString locale = QLocale::system().name();
    QString docname;
    if (locale.left(2)=="es")
	docname="vym_es.pdf";
    else    
	docname="vym.pdf";

    QStringList searchList;
    QDir docdir;
    #if defined(Q_OS_MACX)
        searchList << vymBaseDir.path() + "/doc";
    #elif defined(Q_OS_WIN32)
        searchList << vymInstallDir.path() + "doc/" + docname;
    #else
	#if defined(VYM_DOCDIR)
	    searchList << VYM_DOCDIR;
	#endif
        // default path in SUSE LINUX
        searchList << "/usr/share/doc/packages/vym";
    #endif

    searchList << "doc";    // relative path for easy testing in tarball
    searchList << "/usr/share/doc/vym";	// Debian
    searchList << "/usr/share/doc/packages";// Knoppix

    bool found=false;
    QFile docfile;
    for (int i=0; i<searchList.count(); ++i)
    {
	docfile.setFileName(searchList.at(i)+"/"+docname);
	if (docfile.exists())
	{
	    found=true;
	    break;
	}   
    }

    if (!found)
    {
	QMessageBox::critical(0, 
	    tr("Critcal error"),
        tr("Couldn't find the documentation %1 in:\n%2").arg(docname).arg(searchList.join("\n")));
	return;
    }	

    QStringList args;
    VymProcess *pdfProc = new VymProcess();
    args << QDir::toNativeSeparators(docfile.fileName());

    if (!pdfProc->startDetached( settings.value("/system/readerPDF").toString(),args) )
    {
	// error handling
	QMessageBox::warning(0, 
	    tr("Warning"),
	    tr("Couldn't find a viewer to open %1.\n").arg(docfile.fileName())+
	    tr("Please use Settings->")+tr("Set application to open PDF files"));
	settingsPDF();	
	return;
    }
}

void Main::helpDemo()
{
    QStringList filters;
    filters <<"VYM example map (*.vym)";
    QFileDialog fd;
    fd.setDirectory (vymBaseDir.path() + "/demos");
    fd.setFileMode (QFileDialog::ExistingFiles);
    fd.setNameFilters (filters);
    fd.setWindowTitle (vymName+ " - " +tr("Load vym example map"));
    fd.setAcceptMode (QFileDialog::AcceptOpen);

    QString fn;
    if ( fd.exec() == QDialog::Accepted )
    {
        lastMapDir=fd.directory().path();
        QStringList flist = fd.selectedFiles();
        QStringList::Iterator it = flist.begin();
        initProgressCounter( flist.count());
        while( it != flist.end() )
        {
            fn = *it;
            fileLoad(*it, NewMap,VymMap);
            ++it;
        }
        removeProgressCounter();
    }
}

void Main::helpShortcuts()
{
    ShowTextDialog dia;
    dia.useFixedFont (true);
    dia.setText( switchboard.getASCII() );
    dia.exec();
}

void Main::debugInfo()  
{
    QString s;
    s = QString ("Platform: %1\n").arg(vymPlatform);
    s += QString ("localeName: %1\nPath: %2\n")
        .arg(localeName)
        .arg(vymBaseDir.path() + "/lang");
    s += QString("vymBaseDir: %1\n").arg(vymBaseDir.path());
    s += QString("currentPath: %1\n").arg(QDir::currentPath());
    s += QString("appDirPath: %1\n").arg(QCoreApplication::applicationDirPath());
    QMessageBox mb;
    mb.setText(s);
    mb.exec();
}

void Main::helpAbout()
{
    AboutDialog ad;
    ad.setMinimumSize(500,500);
    ad.resize (QSize (500,500));
    ad.exec();
}

void Main::helpAboutQT()
{
    QMessageBox::aboutQt( this, "Qt Application Example" );
}

void Main::callMacro ()
{
    QAction *action = qobject_cast<QAction *>(sender());
    int i=-1;
    if (action)
    {
        i=action->data().toInt();
        QString s=macros.getMacro (i+1);
        if (!s.isEmpty())
        {
            VymModel *m=currentModel();
            if (m) m->execute(s);
        }
    }	
}

void Main::downloadReleaseNotesFinished()
{
    DownloadAgent *agent = static_cast<DownloadAgent*>(sender());
    QString s;
    
    if (agent->isSuccess() )
    {
        QString page;
        if (agent->isSuccess() )
        {
            if ( loadStringFromDisk(agent->getDestination(), page) )
            {
                ShowTextDialog dia(this);
                dia.setText(page);
                dia.exec();

                // Don't load the release notes automatically again
                settings.setValue("/downloads/releaseNotes/shownVersion", __VYM_VERSION);
            } 
        }
    } else
    {
        if (debug)
        {
            qDebug()<<"Main::downloadReleaseNotesFinished ";
            qDebug()<<"  result: failed";
            qDebug()<<"     msg: " << agent->getResultMessage();
        }
    }
}

void Main::checkReleaseNotes()
{
    bool userTriggered;
    if (qobject_cast<QAction *>(sender()) )
        userTriggered = true;
    else
        userTriggered = false;

    if (downloadsEnabled())
    {
        if ( userTriggered ||
             versionLowerThanVym( settings.value("/downloads/releaseNotes/shownVersion", "0.0.1").toString() ) )
        {
            QUrl releaseNotesUrl(
                //QString("http://localhost/release-notes.php?vymVersion=%1") /
                QString("http://www.insilmaril.de/vym/release-notes.php?vymVersion=%1")
                .arg(vymVersion)
            );
            DownloadAgent *agent = new DownloadAgent(releaseNotesUrl);
            connect (agent, SIGNAL( downloadFinished()), this, SLOT(downloadReleaseNotesFinished()));
            QTimer::singleShot(0, agent, SLOT(execute()));
        }
    } else
    {
        // No downloads enabled
        if (userTriggered)
        {
            // Notification: vym could not download release notes
            QMessageBox::warning(0,  tr("Warning"), tr("Please allow vym to download release notes!"));
            if (downloadsEnabled(userTriggered)) checkUpdates();
        }
    }
}

bool Main::downloadsEnabled (bool userTriggered)
{
    bool result;
    if (!userTriggered && settings.value("/downloads/enabled", false).toBool())
    {
        result = true;
    } else
    {
        QDate lastAsked = settings.value("/downloads/permissionLastAsked", QDate(1970,1,1) ).toDate();
        if (userTriggered ||
            !settings.contains("/downloads/permissionLastAsked") ||
            lastAsked.daysTo( QDate::currentDate()) > 7
           )
        {
            QString infotext;
            infotext = tr("<html>"
                          "<h3>Do you allow vym to check online for updates or release notes?</h3>"
                          "If you allow, vym will "
                          "<ul>"
                            "<li>check once for release notes</li>"
                            "<li>check regulary for updates and notify you in case you should update, e.g. if there are "
                              "important bug fixes available</li>"
                            "<li>receive a cookie with a random ID and send vym version and platform name and the ID  "
                              "(e.g. \"Windows\" or \"Linux\") back to me, Uwe Drechsel."
                              "<p>As vym developer I am motivated to see "
                              "many people using vym. Of course I am curious to see, on which system vym is used. Maintaining each "
                              "of the systems requires a lot of my (spare) time.</p> "
                              "<p>No other data than above will be sent, especially no private data will be collected or sent."
                              "(Check the source code, if you don't believe.)"
                              "</p>"
                            "</li>"
                          "</ul>"
                          "If you do not allow, "
                          "<ul>"
                            "<li>nothing will be downloaded and especially I will <b>not be motivated</b> "
                            "to spend some more thousands of hours on developing a free software tool."
                          "</ul>"
                          "Please allow vym to check for updates :-)"
                          );
            QMessageBox mb( vymName, infotext,
                QMessageBox::Information,
                QMessageBox::Yes | QMessageBox::Default,
                QMessageBox::Cancel | QMessageBox::Escape,
                QMessageBox::NoButton);
            mb.setButtonText( QMessageBox::Yes, tr("Allow") );
            mb.setButtonText( QMessageBox::Cancel, tr("Do not allow"));
            switch( mb.exec() )
            {
                case QMessageBox::Yes:
                    result = true;
                    QMessageBox::information(0, vymName,
                        tr("Thank you for enabling downloads!"));
                break;
                default :
                    result = false;
                break;
            }
        } else
            result = false;
        actionSettingsToggleDownloads->setChecked( result );
        settings.setValue("/downloads/enabled", result);
        settings.setValue("/downloads/permissionLastAsked", QDate::currentDate().toString(Qt::ISODate));
    }
    return result;
}

void Main::downloadUpdatesFinished(bool userTriggered)
{
    DownloadAgent *agent = static_cast<DownloadAgent*>(sender());
    QString s;

    if (agent->isSuccess() )
    {
        ShowTextDialog dia;
        dia.setWindowTitle( vymName + " - " + tr("Update information") );
        QString page;
        if (loadStringFromDisk(agent->getDestination(), page) )
        {
            if (page.contains("vymisuptodate"))
            {
                statusMessage( tr("vym is up to date.","MainWindow"));
                if (userTriggered)
                {
                    // Notification: vym is up to date!
                    dia.setHtml( page );
                    dia.exec();
                }
            } else if (page.contains("vymneedsupdate"))
            {
                // Notification: updates available
                dia.setHtml( page );
                dia.exec();
            } else 
            {
                // Notification: Unknown version found
                dia.setHtml( page );
                dia.exec();
            } 

            // Prepare to check again later
            settings.setValue("/downloads/updates/lastChecked", QDate::currentDate().toString(Qt::ISODate));
        } else
            statusMessage( "Couldn't load update page from " + agent->getDestination());

    } else
    {
        statusMessage( "Check for updates failed.");
        if (debug)
        {
            qDebug()<<"Main::downloadUpdatesFinished ";
            qDebug()<<"  result: failed";
            qDebug()<<"     msg: " << agent->getResultMessage();
        }
    }
}
void Main::downloadUpdatesFinishedInt()
{
    downloadUpdatesFinished(true);
}

void Main::downloadUpdates(bool userTriggered)
{
    QUrl updatesUrl(
        QString("http://www.insilmaril.de/vym/updates.php?vymVersion=%1")
        .arg(vymVersion)
    );
    DownloadAgent *agent = new DownloadAgent(updatesUrl);
    if (userTriggered)
        connect (agent, SIGNAL( downloadFinished()), this, SLOT(downloadUpdatesFinishedInt()));
    else
        connect (agent, SIGNAL( downloadFinished()), this, SLOT(downloadUpdatesFinished()));
    statusMessage( tr("Checking for updates...","MainWindow"));
    QTimer::singleShot(0, agent, SLOT(execute()));
}

void Main::checkUpdates()
{
    bool userTriggered;
    if (qobject_cast<QAction *>(sender()) )
        userTriggered = true;
    else
        userTriggered = false;

    if (downloadsEnabled())
    {
        // Too much time passed since last update check?
        QDate lastChecked = settings.value("/downloads/updates/lastChecked", QDate(1970,1,1) ).toDate();
        if ( !lastChecked.isValid()) lastChecked = QDate(1970,1,1);
        if ( lastChecked.daysTo( QDate::currentDate()) > settings.value("/downloads/updates/checkInterval",3).toInt() ||
             userTriggered == true)
        {
            downloadUpdates( userTriggered );
        }
    } else
    {
        // No downloads enabled
        if (userTriggered)
        {
            // Notification: vym could not check for updates
            QMessageBox::warning(0,  tr("Warning"), tr("Please allow vym to check for updates!"));
            if (downloadsEnabled(userTriggered)) checkUpdates();
        }
    }
}
