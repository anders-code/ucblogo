/* This file implements the logo frame, which is the main frame that
   contains the terminal, turtle graphics and the editor.
*/


//TODO: deferUpdate isn't very consistent right now
//TODO: clearSelection should happen less often.

#include <iostream>

#ifdef __GNUG__
    #pragma implementation "wxTerminal.h"
#endif

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

// for all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWindows headers
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include <wx/timer.h>
extern std::string pathString;
#include <wx/stdpaths.h>
#include <ctype.h>

extern unsigned char *cmdHistory[];
extern unsigned char **hist_inptr, **hist_outptr;
extern int readingInstruction;

#include <wx/print.h>
#include "LogoFrame.h"
#include "wxGlobals.h"
#include <wx/clipbrd.h>
#include <wx/html/htmprint.h>
#include <wx/print.h>
#include <wx/printdlg.h>
#include <wx/dcbuffer.h>  //buffered_DC
#include "wxTurtleGraphics.h"
#include "config.h"
#include "TextEditor.h"
#include "wxTerminal.h"		/* must come after wxTurtleGraphics.h */
#ifdef __WXMAC__                                                        
#include <Carbon/Carbon.h>                                              
#endif                                                                  

using namespace std;


// ----------------------------------------------------------------------------
// Globals
// ----------------------------------------------------------------------------
// This is for the input for terminal-like behavior
unsigned char inputBuffer [8000];
// How far into the inputBuffer we are
int input_index = 0;
// Where the cursor is in the inputBuffer
int input_current_pos = 0;

// if logo is in character mode
int logo_char_mode;
// the terminal DC
wxFont old_font;
wxTextAttr old_style;
// when edit is called in logo
TextEditor * editWindow;
// the turtle graphics we are using
TurtleCanvas * turtleGraphics; 
// this contains the previous 3 window
wxBoxSizer *topsizer;
LogoFrame *logoFrame;
LogoEventManager *logoEventManager;

// used to calculate where the cursor should be
int cur_x = 0, cur_y = 0;
int first = 1;
int last_logo_x = 0, last_logo_y = 0;
int last_input_x = 0, last_input_y = 0;
// the menu
wxMenuBar* menuBar;

extern "C" void wxTextScreen();

char *argv[2] = {"UCBLogo", 0};

// This is for stopping logo asynchronously
#ifdef SIG_TAKES_ARG
extern "C" RETSIGTYPE logo_stop(int);
extern "C" RETSIGTYPE logo_pause(int);
#else
extern "C" RETSIGTYPE logo_stop();
extern "C" RETSIGTYPE logo_pause();
#endif
int logo_stop_flag = 0;
int logo_pause_flag = 0;

// this is a static reference to the main terminal
wxTerminal *wxTerminal::terminal;

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

enum
{
	
    Menu_File = 200,
	Menu_File_Save,
	Menu_File_Load,
    Menu_File_Page_Setup,
	Menu_File_Print_Text,
	Menu_File_Print_Text_Prev,
	Menu_File_Print_Turtle,
	Menu_File_Print_Turtle_Prev,
	Menu_File_Quit,
	
	Menu_Edit = 300,
    Menu_Edit_Copy,
	Menu_Edit_Paste,
	
	Menu_Logo = 400,
	Menu_Logo_Stop,
	Menu_Logo_Pause,
	
	Menu_Font = 500,
	Menu_Font_Inc,
	Menu_Font_Dec,
	
	Menu_Help = 600,
	Menu_Help_Man,
	
	Edit_Menu_File = 700,
	Edit_Menu_File_Close_Accept,
	Edit_Menu_File_Close_Reject,
    Edit_Menu_File_Page_Setup,
    Edit_Menu_File_Print_Text,

	
	Edit_Menu_Edit = 800,
    Edit_Menu_Edit_Copy,
	Edit_Menu_Edit_Paste,
	Edit_Menu_Edit_Cut,
	Edit_Menu_Edit_Find,
	Edit_Menu_Edit_Find_Next
	
};

// ----------------------------------------------------------------------------
// LogoApplication class
// ----------------------------------------------------------------------------


bool LogoApplication::OnInit()
{

  logoFrame = new LogoFrame
    ("Berkeley Logo",
     50, 50, 900, 500);

  logoFrame->Show(TRUE);
#ifndef __WXMAC__
  m_mainLoop = new wxEventLoop();
#endif
  logoEventManager = new LogoEventManager(this);
  SetTopWindow(logoFrame);
  return TRUE;	
}

extern "C" int start (int, char **);


int LogoApplication::OnRun()
{
#ifndef __WXMAC__
  wxEventLoop::SetActive(m_mainLoop);
#endif
  //SetExitOnFrameDelete(true);

#ifndef __WXMAC__   /* needed for wxWidgets 2.6 */
  wxSetWorkingDirectory(wxStandardPaths::Get().GetDocumentsDir());
#endif

  // fix the working directory in mac
#ifdef __WXMAC__
  char path[1024];
  CFBundleRef mainBundle = CFBundleGetMainBundle();
  assert( mainBundle );
  
  CFURLRef mainBundleURL = CFBundleCopyBundleURL( mainBundle);
  assert( mainBundleURL);
  
  CFStringRef cfStringRef = CFURLCopyFileSystemPath( mainBundleURL, kCFURLPOSIXPathStyle);
  assert( cfStringRef);
  
  CFStringGetCString( cfStringRef, path, 1024, kCFStringEncodingASCII);
  
  CFRelease( mainBundleURL);
  CFRelease( cfStringRef);
  
  //std::string pathString(path);
  pathString = path;
  pathString+="/Contents/Resources/";
  //	chdir(pathString.c_str());
  
#endif
  
  start(1, argv);
  return 0;
}

int LogoApplication::OnExit() {
  return 0;
}



// ----------------------------------------------------------------------------
// LogoEventManager class
// ----------------------------------------------------------------------------

LogoEventManager::LogoEventManager(LogoApplication *logoApp)
{
  m_logoApp = logoApp;
}

void LogoEventManager::ProcessAnEvent(int force_yield)
{
  if( m_logoApp->Pending() ) {
    m_logoApp->Dispatch();
  }
  else {
    if(force_yield) {
      m_logoApp->Yield(TRUE);
    }
    else {
      static int foo = 500;    // carefully tuned fudge factor
      if (--foo == 0) {
	  m_logoApp->Yield(TRUE);
	  foo = 500;
      }
    }
  }
}

void LogoEventManager::ProcessAllEvents()
{
  while( m_logoApp->Pending() ) {
    m_logoApp->Dispatch();
  }
  m_logoApp->Yield(TRUE);
}


// ----------------------------------------------------------------------------
// LogoFrame class
// ----------------------------------------------------------------------------

BEGIN_EVENT_TABLE (LogoFrame, wxFrame)
EVT_MENU(Menu_File_Save,				LogoFrame::OnSave)
EVT_MENU(Menu_File_Load,				LogoFrame::OnLoad)
EVT_MENU(Menu_File_Page_Setup,			TurtleCanvas::OnPageSetup)
EVT_MENU(Menu_File_Print_Text,			LogoFrame::OnPrintText)
EVT_MENU(Menu_File_Print_Text_Prev,		LogoFrame::OnPrintTextPrev)
EVT_MENU(Menu_File_Print_Turtle,	TurtleCanvas::PrintTurtleWindow)
EVT_MENU(Menu_File_Print_Turtle_Prev,   TurtleCanvas::TurtlePrintPreview)
EVT_MENU(Menu_File_Quit,			LogoFrame::OnQuit)
EVT_MENU(Menu_Edit_Copy,			LogoFrame::DoCopy)
EVT_MENU(Menu_Edit_Paste,			LogoFrame::DoPaste)
EVT_MENU(Menu_Logo_Pause,			LogoFrame::DoPause)
EVT_MENU(Menu_Logo_Stop,			LogoFrame::DoStop)
EVT_MENU(Menu_Font_Inc,				LogoFrame::OnIncreaseFont)
EVT_MENU(Menu_Font_Dec,				LogoFrame::OnDecreaseFont)
EVT_MENU(Edit_Menu_File_Close_Accept,	LogoFrame::OnEditCloseAccept)
EVT_MENU(Edit_Menu_File_Close_Reject,	LogoFrame::OnEditCloseReject)
EVT_MENU(Edit_Menu_File_Page_Setup,		TurtleCanvas::OnPageSetup)
EVT_MENU(Edit_Menu_File_Print_Text,		LogoFrame::OnEditPrint)
EVT_MENU(Edit_Menu_Edit_Copy,			LogoFrame::OnEditCopy)
EVT_MENU(Edit_Menu_Edit_Cut,			LogoFrame::OnEditCut)
EVT_MENU(Edit_Menu_Edit_Paste,			LogoFrame::OnEditPaste)
EVT_MENU(Edit_Menu_Edit_Find,			LogoFrame::OnEditFind)
EVT_MENU(Edit_Menu_Edit_Find_Next,		LogoFrame::OnEditFindNext)
EVT_CLOSE(LogoFrame::OnCloseWindow)
END_EVENT_TABLE()

#include "ucblogo.xpm"


LogoFrame::LogoFrame (const wxChar *title,
 int xpos, int ypos,
 int width, int height)
  : wxFrame( (wxFrame *) NULL, -1, title,
	     wxPoint(xpos, ypos),
	     wxSize(width, height)) {
  // the topsizer allows different resizeable segments in the main frame (i.e. for 
  // turtle graphics and the terminal displaying simultaneously)
  SetMinSize(wxSize(100, 100));
    SetIcon(wxIcon(ucblogo));
  logoFrame = this;
  topsizer = new wxBoxSizer( wxVERTICAL );
  wxTerminal::terminal = new wxTerminal (this, -1, wxPoint(-1, -1), 82, 25,  wxString(""));
  turtleGraphics = new TurtleCanvas( this );
  wxFont f(18, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL,
	   false, wxString("Courier"));
  editWindow = new TextEditor( this, -1, "", wxDefaultPosition, wxSize(100,60), wxTE_MULTILINE, f);
  wxTerminal::terminal->isEditFile=0;
  
  topsizer->Add(
		editWindow,
		1,            // make vertically stretchable
		wxEXPAND |    // make horizontally stretchable
		wxALL,        //   and make border all around
		2 );  
 
    topsizer->Add(
		turtleGraphics,
		4,            // make vertically stretchable
		wxEXPAND |    // make horizontally stretchable
		wxALL,        //   and make border all around
		2 );
     topsizer->Add(
		wxTerminal::terminal,
		1,            // make vertically stretchable
		wxEXPAND |    // make horizontally stretchable
		wxALL,        //   and make border all around
		2 ); 

     topsizer->Show(wxTerminal::terminal, 1);
    topsizer->Show(turtleGraphics, 0);
    topsizer->Show(editWindow, 0);
   
    SetSizer( topsizer ); 
	
	SetAutoLayout(true);
	//topsizer->Fit(this);
	//topsizer->SetSizeHints(this);
	
    wxTerminal::terminal->SetFocus();
	SetUpMenu();
    wxSleep(1);
}


void LogoFrame::OnCloseWindow(wxCloseEvent& event)
{
  extern int wx_leave_mainloop;
  logo_stop_flag = 1;
  wx_leave_mainloop++;
  Destroy();
}


void LogoFrame::OnQuit(wxCommandEvent& WXUNUSED(event))
{
  Close(TRUE);
}



void LogoFrame::SetUpMenu(){
	int i;
	if(!menuBar)
		menuBar = new wxMenuBar( wxMB_DOCKABLE );
	else
		for(i=menuBar->GetMenuCount()-1;i>=0;i--)
			delete menuBar->Remove(i);

	
	wxMenu *fileMenu = new wxMenu;
	fileMenu->Append( Menu_File_Save, _T("Save Logo Session \tCtrl-S"));
	fileMenu->Append( Menu_File_Load, _T("Load Logo Session \tCtrl-O"));
	fileMenu->AppendSeparator();
	fileMenu->Append( Menu_File_Page_Setup, _T("Page Setup"));
	fileMenu->Append( Menu_File_Print_Text, _T("Print Text Window"));
	fileMenu->Append( Menu_File_Print_Text_Prev, _T("Print Preview Text Window"));
	fileMenu->Append( Menu_File_Print_Turtle, _T("Print Turtle Graphics"));
	fileMenu->Append( Menu_File_Print_Turtle_Prev, _T("Turtle Graphics Print Preview"));
	fileMenu->AppendSeparator();
	fileMenu->Append(Menu_File_Quit, _T("Quit UCBLogo \tCtrl-Q"));
	
	
	wxMenu *editMenu = new wxMenu;
		
	menuBar->Append(fileMenu, _T("&File"));
	menuBar->Append(editMenu, _T("&Edit"));

	wxMenu *logoMenu = new wxMenu;
#ifdef __WXMSW__
	editMenu->Append(Menu_Edit_Copy, _T("Copy \tCtrl-C"));
	editMenu->Append(Menu_Edit_Paste, _T("Paste \tCtrl-V"));

	logoMenu->Append(Menu_Logo_Pause, _T("Pause \tCtrl-P"));
	logoMenu->Append(Menu_Logo_Stop, _T("Stop \tCtrl-S"));	
#else
	editMenu->Append(Menu_Edit_Copy, _T("Copy \tCtrl-C"));
	editMenu->Append(Menu_Edit_Paste, _T("Paste \tCtrl-V"));

	logoMenu->Append(Menu_Logo_Pause, _T("Pause \tAlt-P"));
	logoMenu->Append(Menu_Logo_Stop, _T("Stop \tAlt-S"));
#endif
	menuBar->Append(logoMenu, _T("&Logo"));
	
	wxMenu *fontMenu = new wxMenu;
	fontMenu->Append(Menu_Font_Inc, _T("Increase Font Size \tCtrl-+"));
	fontMenu->Append(Menu_Font_Dec, _T("Decrease Font Size \tCtrl--"));
	menuBar->Append(fontMenu, _T("&Font"));
	
	/*wxMenu *helpMenu = new wxMenu;
	helpMenu->Append(Menu_Help_Man, _T("Browse Online Manual"));
	menuBar->Append(helpMenu, _T("&Help"));*/
	
	SetMenuBar(menuBar);
}
void LogoFrame::DoCopy(wxCommandEvent& WXUNUSED(event)){
	wxTerminal::terminal->DoCopy();
}

void LogoFrame::DoPaste(wxCommandEvent& WXUNUSED(event)){
	wxTerminal::terminal->DoPaste();
}

extern "C" void new_line(FILE *);
int firstloadsave = 1;

void doSave(char * name, int length);
void doLoad(char * name, int length);
void LogoFrame::OnSave(wxCommandEvent& WXUNUSED(event)) {
	wxFileDialog dialog(this,
			    _T("Save Logo Workspace"),
			    (firstloadsave ?
#ifdef __WXMAC__   /* needed for wxWidgets 2.6 */
			      *wxEmptyString :
#else
			      wxStandardPaths::Get().GetDocumentsDir() :
#endif
			      *wxEmptyString),
			    wxEmptyString,
//			    "Logo workspaces(*.lg)|All files(*.*)",
			    "*.*",
#ifdef __WXMAC__   /* needed for wxWidgets 2.6 */
			    wxSAVE|wxOVERWRITE_PROMPT|wxCHANGE_DIR
#else
			    wxFD_SAVE|wxFD_OVERWRITE_PROMPT|wxFD_CHANGE_DIR
#endif
			    );
	
	dialog.SetFilterIndex(1);
	
	if (dialog.ShowModal() == wxID_OK)
	{
	    doSave((char *)dialog.GetPath().c_str(),
		   dialog.GetPath().length());
	    new_line(stdout);
	}
    firstloadsave = 0;
}

void LogoFrame::OnLoad(wxCommandEvent& WXUNUSED(event)){
	wxFileDialog dialog
	(
	 this,
	 _T("Load Logo Workspace"),
	 (firstloadsave ?
#ifdef __WXMAC__   /* needed for wxWidgets 2.6 */
	    *wxEmptyString :
#else
	    wxStandardPaths::Get().GetDocumentsDir() :
#endif
			  *wxEmptyString),
	 wxEmptyString,
//	 "Logo workspaces(*.lg)|All files(*.*)",
	 "*",
#ifdef __WXMAC__   /* needed for wxWidgets 2.6 */
	 wxOPEN|wxFILE_MUST_EXIST|wxCHANGE_DIR
#else
	 wxFD_OPEN|wxFD_FILE_MUST_EXIST|wxFD_CHANGE_DIR
#endif
	 );
		
	if (dialog.ShowModal() == wxID_OK) {
	    doLoad((char *)dialog.GetPath().c_str(),
		   dialog.GetPath().length());
	    new_line(stdout);
	}
    firstloadsave = 0;
}


void LogoFrame::OnPrintText(wxCommandEvent& WXUNUSED(event)){
	wxHtmlEasyPrinting *htmlPrinter=wxTerminal::terminal->htmlPrinter;
	if(!htmlPrinter){
		htmlPrinter = new wxHtmlEasyPrinting();
		int fontsizes[] = { 6, 8, 12, 14, 16, 20, 24 };
		htmlPrinter->SetFonts("Courier","Courier", fontsizes);
	}
	wxString *textString = wxTerminal::terminal->get_text();
	
	
	htmlPrinter->PrintText(*textString);	
	delete textString;
}

void LogoFrame::OnPrintTextPrev(wxCommandEvent& WXUNUSED(event)){
	wxHtmlEasyPrinting *htmlPrinter=wxTerminal::terminal->htmlPrinter;
	if(!htmlPrinter){
		htmlPrinter = new wxHtmlEasyPrinting();
		int fontsizes[] = { 6, 8, 12, 14, 16, 20, 24 };
		htmlPrinter->SetFonts("Courier","Courier", fontsizes);
	}
	wxString *textString = wxTerminal::terminal->get_text();
	
	htmlPrinter->PreviewText(*textString,wxString(""));
	
}

void LogoFrame::OnIncreaseFont(wxCommandEvent& WXUNUSED(event)){
	int expected;
	
	// get original size and number of characters per row and column
	int width, height, numCharX, numCharY, m_charWidth, m_charHeight;
	wxTerminal::terminal->GetCharSize(&m_charWidth, &m_charHeight);
	//wxClientDC dc(wxTerminal::terminal);
	//	dc.GetMultiLineTextExtent("M", &m_charWidth, &m_charHeight,
	//			  &m_lineHeight);	
	GetSize(&width, &height);
	numCharX = width/m_charWidth;
	numCharY = height/m_charHeight;
	
	wxdprintf("m_charWidth: %d, m_charHeight: %d, width: %d, height: %d, numCharX: %d, numCharY: %d\n", m_charWidth, m_charHeight, width, height, numCharX, numCharY);
	
	wxFont font = wxTerminal::terminal->GetFont();
	expected = font.GetPointSize()+1;
	
	// see that we have the font we are trying to use
	while(font.GetPointSize() != expected && expected <= 24){
		expected++;
		font.SetPointSize(expected);
	}
	wxTerminal::terminal->SetFont(font);
	editWindow->SetFont(font);
	//wxSizeEvent event;
	//	if(wxTerminal::terminal->IsShown())
	//	wxTerminal::terminal->OnSize(event);
	
	// resize the frame according to the new font size
	int new_m_charWidth, new_m_charHeight;
	wxTerminal::terminal->GetCharSize(&new_m_charWidth, &new_m_charHeight);
	//wxClientDC newdc(wxTerminal::terminal);
	//newdc.GetMultiLineTextExtent("M", &new_m_charWidth,
	//			     &new_m_charHeight, &new_m_lineHeight);
	if (new_m_charWidth != m_charWidth ||
		    new_m_charHeight != m_charHeight) {
	    SetSize(numCharX*new_m_charWidth, numCharY*new_m_charHeight);
	}
	//GetSize(&width, &height); 
	//printf("new m_charWidth: %d, new m_charHeight: %d, new width: %d, new height: %d\n", new_m_charWidth, new_m_charHeight, width, height);
	Layout();
}

void LogoFrame::OnDecreaseFont(wxCommandEvent& WXUNUSED(event)){
	int expected;
	
	// get original size and number of characters per row and column
	int width, height, numCharX, numCharY, m_charWidth, m_charHeight;
	wxTerminal::terminal->GetCharSize(&m_charWidth, &m_charHeight);
	//wxClientDC dc(wxTerminal::terminal);
	//dc.GetTextExtent("M", &m_charWidth, &m_charHeight);	
	GetSize(&width, &height);
	numCharX = width/m_charWidth;
	numCharY = height/m_charHeight;
	//printf("m_charWidth: %d, m_charHeight: %d, width: %d, height: %d, numCharX: %d, numCharY: %d\n", m_charWidth, m_charHeight, width, height, numCharX, numCharY);
	
	wxFont font = wxTerminal::terminal->GetFont();
	expected = font.GetPointSize()-1;
	
	// see that we have the font we are trying to use
	while(font.GetPointSize() != expected && expected >= 6){
		expected--;
		font.SetPointSize(expected);
	}	
	wxTerminal::terminal->SetFont(font);
	editWindow->SetFont(font);
	//	wxSizeEvent event;
	//wxTerminal::terminal->OnSize(event);
	
	// resize the frame according to the new font size
	int new_m_charWidth, new_m_charHeight;
	wxTerminal::terminal->GetCharSize(&new_m_charWidth, &new_m_charHeight);
	//wxClientDC newdc(wxTerminal::terminal);
	//newdc.GetTextExtent("M", &new_m_charWidth, &new_m_charHeight);
	if (new_m_charWidth != m_charWidth || new_m_charHeight != m_charHeight) {
		SetSize(numCharX*new_m_charWidth, numCharY*new_m_charHeight); 
	}
	//GetSize(&width, &height); 
	//printf("new m_charWidth: %d, new m_charHeight: %d, new width: %d, new height: %d\n", new_m_charWidth, new_m_charHeight, width, height);
	Layout();
	
}

void LogoFrame::DoStop(wxCommandEvent& WXUNUSED(event)){
  logo_stop_flag = 1;
}


void LogoFrame::DoPause(wxCommandEvent& WXUNUSED(event)){
  logo_pause_flag = 1;
}

void LogoFrame::OnEditCloseAccept(wxCommandEvent& WXUNUSED(event)){
	editWindow->OnCloseAccept();
}
void LogoFrame::OnEditCloseReject(wxCommandEvent& WXUNUSED(event)){
	editWindow->OnCloseReject();
}
void LogoFrame::OnEditPrint(wxCommandEvent& WXUNUSED(event)){
	editWindow->DoPrint();
}
void LogoFrame::OnEditCopy(wxCommandEvent& WXUNUSED(event)){
	editWindow->DoCopy();
}
void LogoFrame::OnEditCut(wxCommandEvent& WXUNUSED(event)){
	editWindow->DoCut();
}
void LogoFrame::OnEditPaste(wxCommandEvent& WXUNUSED(event)){
	editWindow->Paste();
}
void LogoFrame::OnEditFind(wxCommandEvent& WXUNUSED(event)){
	editWindow->OnFind();
}
void LogoFrame::OnEditFindNext(wxCommandEvent& WXUNUSED(event)){
	editWindow->OnFindNext();
}
void LogoFrame::OnEditSave(wxCommandEvent& WXUNUSED(event)){
	editWindow->OnSave();
}

void LogoFrame::SetUpEditMenu(){
	int i;
	for(i=menuBar->GetMenuCount()-1;i>=0;i--)
		delete menuBar->Remove(i);
	
	wxMenu *fileMenu = new wxMenu;
	fileMenu->Append( Edit_Menu_File_Close_Accept, _T("Close and Accept Changes \tCtrl-Q"));
	fileMenu->Append( Edit_Menu_File_Close_Reject, _T("Close and Revert Changes \tCtrl-R"));
	fileMenu->AppendSeparator();
	fileMenu->Append( Edit_Menu_File_Page_Setup, _T("Page Setup"));
	fileMenu->Append( Edit_Menu_File_Print_Text, _T("Print... \tCtrl-P"));
	
	wxMenu *editMenu = new wxMenu;
	
	menuBar->Append(fileMenu, _T("&File"));
	menuBar->Append(editMenu, _T("&Edit"));
	
	
	editMenu->Append(Edit_Menu_Edit_Cut, _T("Cut \tCtrl-X"));
	editMenu->Append(Edit_Menu_Edit_Copy, _T("Copy \tCtrl-C"));
	editMenu->Append(Edit_Menu_Edit_Paste, _T("Paste \tCtrl-V"));
	editMenu->AppendSeparator();
	editMenu->Append(Edit_Menu_Edit_Find, _T("Find... \tCtrl-F"));
	editMenu->Append(Edit_Menu_Edit_Find_Next, _T("Find Next \tCtrl-G"));
	
	wxMenu *fontMenu = new wxMenu;
	fontMenu->Append(Menu_Font_Inc, _T("Increase Font Size \tCtrl-+"));
	fontMenu->Append(Menu_Font_Dec, _T("Decrease Font Size \tCtrl--"));
	menuBar->Append(fontMenu, _T("&Font"));
	
	
	logoFrame->SetMenuBar(menuBar);
}

     

// ----------------------------------------------------------------------------
// wxTerminal
// ----------------------------------------------------------------------------

BEGIN_DECLARE_EVENT_TYPES()
  DECLARE_EVENT_TYPE(wxEVT_MY_CUSTOM_COMMAND, 7777)
  END_DECLARE_EVENT_TYPES()
  DEFINE_EVENT_TYPE(wxEVT_MY_CUSTOM_COMMAND)

#define EVT_MY_CUSTOM_COMMAND(id, fn) \
    DECLARE_EVENT_TABLE_ENTRY( \
        wxEVT_MY_CUSTOM_COMMAND, id, -1, \
        (wxObjectEventFunction)(wxEventFunction)(wxCommandEventFunction)&fn, \
        (wxObject *) NULL \
    ),


BEGIN_DECLARE_EVENT_TYPES()
  DECLARE_EVENT_TYPE(wxEVT_TERM_CUSTOM_COMMAND, 7777)
  END_DECLARE_EVENT_TYPES()
  DEFINE_EVENT_TYPE(wxEVT_TERM_CUSTOM_COMMAND)

#define wxEVT_TERM_CUSTOM_COMMAND(id, fn) \
    DECLARE_EVENT_TABLE_ENTRY( \
        wxEVT_TERM_CUSTOM_COMMAND, id, -1, \
        (wxObjectEventFunction)(wxEventFunction)(wxCommandEventFunction)&fn, \
        (wxObject *) NULL \
    ),


BEGIN_EVENT_TABLE(wxTerminal, wxWindow)
  EVT_ERASE_BACKGROUND(wxTerminal::OnEraseBackground)
  EVT_PAINT(wxTerminal::OnPaint)
  EVT_CHAR(wxTerminal::OnChar)
  EVT_LEFT_DOWN(wxTerminal::OnLeftDown)
  EVT_LEFT_UP(wxTerminal::OnLeftUp)
  EVT_MOTION(wxTerminal::OnMouseMove)
  EVT_MY_CUSTOM_COMMAND(-1, wxTerminal::Flush)
  EVT_SIZE(wxTerminal::OnSize)
  EVT_KILL_FOCUS(wxTerminal::LoseFocus)
END_EVENT_TABLE()

wxCommandEvent * haveInputEvent = new wxCommandEvent(wxEVT_MY_CUSTOM_COMMAND);

 wxTerminal::wxTerminal(wxWindow* parent, wxWindowID id,
               const wxPoint& pos,
               int width, int height,
               const wxString& name) :
  wxScrolledWindow(parent, id, pos, wxSize(-1, -1), wxWANTS_CHARS|wxVSCROLL, name)
//    wxScrolledWindow(parent, id, pos, wxSize(-1, -1), wxWANTS_CHARS, name)
{
  //allocating char structures.
  term_chars = (wxterm_char_buffer *) malloc(sizeof(wxterm_char_buffer));
  memset(term_chars, '\0', sizeof(wxterm_char_buffer));
  term_lines = (wxterm_line_buffer *) malloc(sizeof(wxterm_line_buffer));
  memset(term_lines, 0, sizeof(wxterm_line_buffer));
  term_chars->next = 0;
  term_lines->next = 0;

  //initializations
  curr_line_pos.buf = term_lines;
  curr_line_pos.offset = 0;
  line_of(curr_line_pos).buf = term_chars;
  line_of(curr_line_pos).offset = 0;

  curr_char_pos.buf = term_chars;
  curr_char_pos.offset = 0;
  //curr_char_pos line_length not used!


  m_currMode = 0;
  y_max = 0;
  x_max = m_width - 1;


  // start us out not in char mode
  logo_char_mode = 0;
  // For printing the text
  htmlPrinter = 0;
  //  set_mode_flag(DESTRUCTBS);
  wxTerminal::terminal = this;
  int
    i;

  m_selecting = FALSE;
  m_selx1 = m_sely1 = m_selx2 = m_sely2 = 0;
  m_seloldx1 = m_seloldy1= m_seloldx2 = m_seloldy2 = 0;
  //m_marking = FALSE;
  m_curX = -1;
  m_curY = -1;

  //  m_boldStyle = FONT;

  GetColors(m_colors);

  m_curFG = 15;
  m_curBG = 0;
  SetBackgroundStyle(wxBG_STYLE_CUSTOM);
  SetBackgroundColour(m_colors[0]);
  SetMinSize(wxSize(50, 50));

  for(i = 0; i < 16; i++)
    m_colorPens[i] = wxPen(m_colors[i], 1, wxSOLID);

  m_width = width;
  m_height = height;

  m_printerFN = 0;
  m_printerName = 0;

  wxFont f(18, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL,
	   false, "Courier");
  SetFont(f);

  wxClientDC
    dc(this);
  
  GetCharSize(&m_charWidth, &m_charHeight);
  //dc.GetTextExtent("M", &m_charWidth, &m_charHeight);
  //  m_charWidth--;

  m_vscroll_enabled = TRUE;
  m_inputReady = FALSE;
  m_inputLines = 0;
  
  //  int x, y;
  //GetSize(&x, &y);  
  
  //  parent->SetSize(-1,-1, m_charWidth * width, m_charHeight * height + 1);
  //  SetScrollbars(m_charWidth, m_charHeight, 0, 30);
  //SetScrollRate(0, m_charHeight);
  //SetVirtualSize(m_charWidth * width, 2 * m_charHeight);  //1 row (nothing displayed yet)


  //  ResizeTerminal(width, height);

  

}

wxTerminal::~wxTerminal()
{
  //clean up the buffers
  
}

void wxTerminal::deferUpdate(int flag) {
    if (flag)
	set_mode_flag(DEFERUPDATE);
    else
	clear_mode_flag(DEFERUPDATE);
}

void wxTerminal::set_mode_flag(int flag) {
  m_currMode |= flag;
}

void wxTerminal::clear_mode_flag(int flag) {
  m_currMode &= ~flag;
}


bool
wxTerminal::SetFont(const wxFont& font)
{
  wxWindow::SetFont(font);
  m_normalFont = font;
  m_underlinedFont = font;
  m_underlinedFont.SetUnderlined(TRUE);
  m_boldFont = GetFont();
  m_boldFont.SetWeight(wxBOLD);
  m_boldUnderlinedFont = m_boldFont;
  m_boldUnderlinedFont.SetUnderlined(TRUE);
  GetCharSize(&m_charWidth, &m_charHeight);
  //  wxClientDC
  //	  dc(this);
  
  //  dc.GetTextExtent("M", &m_charWidth, &m_charHeight);
  //  m_charWidth--;
  ResizeTerminal(m_width, m_height);
  Refresh();
  
  return TRUE;
}

void 
wxTerminal::GetCharSize(int *cw, int *ch) {
  wxClientDC
    dc(this);
  
  //int dummy;
  //dc.GetTextExtent("M", cw, &dummy);
  //dc.GetTextExtent("(", &dummy, ch);
  
  int descent, extlead;
  dc.GetTextExtent("M", cw, ch, &descent, &extlead);
  *ch += descent + extlead + 1;
}

void
wxTerminal::GetColors(wxColour colors[16] /*, wxTerminal::BOLDSTYLE boldStyle*/)
{
    colors[0] = wxColour(0, 0, 0);                             // black
    colors[1] = wxColour(255, 0, 0);                           // red
    colors[2] = wxColour(0, 255, 0);                           // green
    colors[3] = wxColour(255, 0, 255);                         // yellow
    colors[4] = wxColour(0, 0, 255);                           // blue
    colors[5] = wxColour(255, 255, 0);                         // magenta
    colors[6] = wxColour(0, 255, 255);                         // cyan
    colors[7] = wxColour(255, 255, 255);                       // white
    colors[8] = wxColour(0, 0, 0);                             // black
    colors[9] = wxColour(255, 0, 0);                           // red
    colors[10] = wxColour(0, 255, 0);                          // green
    colors[11] = wxColour(255, 0, 255);                        // yellow
    colors[12] = wxColour(0, 0, 255);                          // blue
    colors[13] = wxColour(255, 255, 0);                        // magenta
    colors[14] = wxColour(0, 255, 255);                        // cyan
    colors[15] = wxColour(255, 255, 255);                      // white
}



/*
 * ProcessInput()
 *
 * passes input to logo, one line at a time
 * and prints to terminal as well
 *
 * assumes cursor is set at last logo position already 
 */
void wxTerminal::ProcessInput(wxDC &dc) {
  //pass input up to newline.
  int i;
  for(i = 0; i < input_index; i++) {
    if(inputBuffer[i] == '\n') break;
  }
  PassInputToTerminal(dc,i+1,inputBuffer); //include '\n'
  last_logo_x = cursor_x;
  last_logo_y = cursor_y;
  
  PassInputToInterp();
  m_inputLines--;
  if(!m_inputLines) m_inputReady = FALSE;  
}


/*
 * Flush()
 * 
 * Handles output from logo
 */
void  wxTerminal::Flush (wxCommandEvent& event){
  set_mode_flag(BOLD);

  if(out_buff_index_public == 0) {
    clear_mode_flag(BOLD);
    return;
  }

  wxClientDC unbuffered_dc(this);
  wxSize sz = unbuffered_dc.GetSize();    
  wxBufferedDC dc(&unbuffered_dc, sz);
  dc.Blit(0,0,sz.GetWidth(), sz.GetHeight(), &unbuffered_dc, 0, 0);

  bool cursor_moved = FALSE;

  if(input_index != 0){
    // set the cursor in the proper place 
    setCursor(dc,last_logo_x, last_logo_y);
    //    scroll_region(last_logo_y, last_logo_y + 1, -1);
    cursor_moved = TRUE;
    
  }    //calculate new cursor location
  if (out_buff_index_public != 0) {
     ClearSelection();
     PassInputToTerminal(dc,out_buff_index_public, (unsigned char *)out_buff_public);
     out_buff_index_public = 0;
  }

  //save current cursor position to last_logo
  last_logo_x = cursor_x;
  last_logo_y = cursor_y;

  clear_mode_flag(BOLD);
  if(cursor_moved/* && readingInstruction*/){   
    //TODO : investigate placements of ClearSelection
    ClearSelection();
    cursor_moved = FALSE;

    if(m_inputReady && readingInstruction) {
      ProcessInput(dc);
    }
    else {
      //pass the input up to the current input location to terminal
      //e.g. cpos is 6, then pass chars 0 to 5 to terminal (6 chars)
      PassInputToTerminal(dc,input_current_pos, inputBuffer);
      int new_cursor_x = cursor_x, new_cursor_y = cursor_y;
      //pass the rest of input to terminal
      //e.g. inputindex is 20, then pass chars 6 to 19 to terminal (14 chars)
      PassInputToTerminal(dc,input_index-input_current_pos, inputBuffer+input_current_pos);
      // set last_input coords
      last_input_x = cursor_x;
      last_input_y = cursor_y;
      // and set cursor back to proper location
      setCursor(dc,new_cursor_x, new_cursor_y);
    }
  }
}


/* 
	PassInputToInterp() 
        takes all characters in the input buffer 
	up to the FIRST '\n' and hands
	them off to the logo interpreter
        if logo_char_mode, then just send the character
 ** does not edit cursor locations!
 */
void wxTerminal::PassInputToInterp() {
  int i;  
  if(logo_char_mode){
    //buff[buff_index++] = inputBuffer[--input_index];
    buff_push(inputBuffer[--input_index]);
    
    input_index = 0;
    input_current_pos = 0;
  }
  else {
    for (i = 0; i < input_index; i++) {
      buff_push(inputBuffer[i]);	
      if(inputBuffer[i] == '\n') {
	break;
      }
    }
    int saw_newline = i;
    for(i = saw_newline + 1; i < input_index; i++) {
      inputBuffer[i - saw_newline - 1] = inputBuffer[i];
    }
    // a to b, length is b - a + 1
    input_index = input_index - saw_newline - 1;
    input_current_pos = input_index;
  }
}


void wxTerminal::DoCopy(){
	
	if (wxTheClipboard->Open())
	{
		// This data objects are held by the clipboard, 
		// so do not delete them in the app.
		wxTheClipboard->SetData( new wxTextDataObject(GetSelection()) );
		wxTheClipboard->Close();
	}
	
}


void wxTerminal::DoPaste(){
	if (wxTheClipboard->Open())
	{
		if (wxTheClipboard->IsSupported( wxDF_TEXT ))
		{
		  wxClientDC dc(this);

		  wxTextDataObject data;
		  wxTheClipboard->GetData( data );
		  wxString s = data.GetText();
		  
		  unsigned int i; 
		  //char chars[2];
		  unsigned char c;
		  int num_newlines = 0;
		  int len;
		  char prev = ' ';
		  for (i = 0; i < s.Length(); i++){
		    len = 1;
		    c = s.GetChar(i);
		    if (c == '\n') {
		      num_newlines++;
		    }
		    if (prev == ' ' && c == ' ') {
		      continue;
		    }
		    prev = c;
		    inputBuffer[input_index++] = c;
		    ClearSelection();
		    PassInputToTerminal(dc,len, &c);
		  }
		  m_inputLines = num_newlines;
		  input_current_pos = input_index;
		}  
		wxTheClipboard->Close();
	}
	
}

void wxTerminal::LoseFocus (wxFocusEvent & event){
}


/*
 OnChar is called each time the user types a character
 in the main terminal window
 */


void
wxTerminal::OnChar(wxKeyEvent& event)
{
  ClearSelection();
  static int xval;
  static int yval;
  int
      keyCode = 0,
      len;
  unsigned char
    buf[10];

  wxClientDC unbuffered_dc(this);
  wxSize sz = unbuffered_dc.GetSize();    
  wxBufferedDC dc(&unbuffered_dc, sz);
  dc.Blit(0,0,sz.GetWidth(), sz.GetHeight(), &unbuffered_dc, 0, 0);


  keyCode = (int)event.GetKeyCode();
  if(logo_char_mode){
    if (keyCode == WXK_RETURN) {
      keyCode = '\n';
    }
    else if (keyCode == WXK_BACK) {
      keyCode = 8;
    }
    else if (keyCode >= WXK_START) {
	/* ignore it */
      return;  //not sure about this (evan)
    } 
    else {
      
    }
    //also not sure about this (evan)
    if (event.ControlDown()) keyCode -= 0140;
    if (event.AltDown()) keyCode += 0200;
    inputBuffer[input_index++] = keyCode;
    input_current_pos++;
    PassInputToInterp();
  }
  else if (keyCode == WXK_RETURN) {
    // for the new terminal:
    
    if(input_current_pos < input_index) {
      setCursor(dc,last_input_x, last_input_y);
    }

    //    buf[0] = 10;
    //buf[1] = 13;
    //len = 2;
    
    inputBuffer[input_index++] = '\n';
    input_current_pos = input_index;

    unsigned char newline = '\n';
    PassInputToTerminal(dc,1,&newline);

    m_inputReady = TRUE;
    m_inputLines++;

    if(readingInstruction) {
      setCursor(dc,last_logo_x, last_logo_y);
      ProcessInput(dc); 
    }
    else {
      last_input_x = cursor_x;
      last_input_y = cursor_y;
    }
  }
  else if (keyCode == WXK_BACK) {
    if (input_index == 0)
      return;
    

    if (input_current_pos == 0)
      return;
    
    bool removing_newline = FALSE;
    if(inputBuffer[input_current_pos-1] == '\n') {
      removing_newline = TRUE;
    }
    
    for (int i = input_current_pos; i < input_index; i++) {
      inputBuffer[i-1] = inputBuffer[i]; 
    }
    input_index--;
    input_current_pos--;

    cur_x = cursor_x - 1, cur_y = cursor_y;
    if(cur_x < 0) {
      wxterm_linepos cpos = GetLinePosition(cursor_y - 1);
      // x_max if wrapped line,  line_length otherwise.
      cur_x = min(x_max, line_of(cpos).line_length);
      cur_y = cursor_y - 1;
      setCursor(dc,cur_x, cur_y);
    }
    else {
      setCursor(dc,cur_x, cur_y);
    }
    
    PassInputToTerminal(dc,input_index - input_current_pos,
			(unsigned char *)inputBuffer + input_current_pos);
    
    //cursor_x , cursor_y now at input's last location
    last_input_x = cursor_x;
    last_input_y = cursor_y;
    
    
    if(removing_newline) {
      //add a second newline, to erase contents of the last
      //input line.
      //this causes a very specific "feature"....
      //if last input line contains other noninput chars
      //then doing this will erase them too
      //this situation can happen when you use setcursor and hit Backspace...
      //it's a very specific situation that should not clash with 
      //intended behavior...
      unsigned char nl = '\n';
      PassInputToTerminal(dc,1, &nl);
      PassInputToTerminal(dc,1, &nl);  //pass two newlines

      m_inputLines--; //merged two lines
    }
    else {
      unsigned char spc = ' ';
      PassInputToTerminal(dc,1, &spc);
    }
    
    //set cursor back to backspace'd location
    setCursor(dc,cur_x,cur_y); 
  }
  else if (keyCode == WXK_UP) { // up
    xval = last_logo_x;
    yval = last_logo_y;

    int i;
    setCursor(dc,xval, yval); 
    if (input_index != 0) { // we have to swipe what is already there
      for (i = 0; i < input_index; i++) {
	if(inputBuffer[i] == '\n') {
	  inputBuffer[i] = '\n';
	}
	else {
	  inputBuffer[i] = ' ';
	}
      }
      PassInputToTerminal(dc,input_index, (unsigned char *)inputBuffer);
    }
    setCursor(dc,xval, yval); 
    // Now get a history entry
    if (--hist_outptr < cmdHistory) {
      hist_outptr = &cmdHistory[HIST_MAX-1];
    }
    if (*hist_outptr == 0) {
      wxBell();
      hist_outptr++;
      input_index = 0;
      input_current_pos = 0;
      m_inputLines = 0;
    } 
    else {
      PassInputToTerminal(dc,strlen((const char *)*hist_outptr), *hist_outptr);
      
      int num_newlines = 0;
      for (i = 0; (*hist_outptr)[i]; i++) {
	inputBuffer[i] = (*hist_outptr)[i];
	if(inputBuffer[i] == '\n') num_newlines++;
      }
      m_inputLines = num_newlines;
      input_index = i;
      input_current_pos = input_index;
    }

    //cursor_x , cursor_y now at input's last location
    last_input_x = cursor_x;
    last_input_y = cursor_y;	
  }
  else if  (keyCode == WXK_DOWN) { // down
    xval = last_logo_x;
    yval = last_logo_y;

    int i;
    setCursor(dc,xval, yval); 
    if (input_index != 0) { // we have to swipe what is already there
      for (i = 0; i < input_index; i++) {
	if(inputBuffer[i] == '\n') {
	  inputBuffer[i] = '\n';
	}
	else {	    
	  inputBuffer[i] = ' ';
	}
      }
      PassInputToTerminal(dc,input_index, (unsigned char *)inputBuffer);
    }
    setCursor(dc,xval, yval); 
    if (*hist_outptr != 0) {
      hist_outptr++;
    }
    if (hist_outptr >= &cmdHistory[HIST_MAX]) {
      hist_outptr = cmdHistory;
    }
    if (*hist_outptr == 0) {
      wxBell();
      input_index = 0;
      input_current_pos = 0;
      m_inputLines = 0;
    } else {
      PassInputToTerminal(dc,strlen((const char *)*hist_outptr),
			  *hist_outptr);
      int num_newlines = 0;
      for (i = 0; (*hist_outptr)[i]; i++) {
	inputBuffer[i] = (*hist_outptr)[i];
	if(inputBuffer[i] == '\n') num_newlines++;
      }
      m_inputLines = num_newlines;

      input_index = i;
      input_current_pos = input_index;
    }
    //cursor_x , cursor_y now at input's last location
    last_input_x = cursor_x;
    last_input_y = cursor_y;	

  }
  else if  (keyCode == WXK_LEFT) { // left    
    if(input_current_pos > 0) {
      if (cursor_x - 1 < 0) {	  
	//if previous char is a newline, then cursor goes to line_length
	//otherwise, it's a wrapped line, and should go to the end
	// just use min...
	wxterm_linepos cpos = GetLinePosition(cursor_y - 1);
	setCursor(dc,min(x_max, line_of(cpos).line_length), cursor_y - 1);   
      }   
      else {
	setCursor(dc,cursor_x - 1, cursor_y);
      }
      input_current_pos--;
    }
  }
  else if  (keyCode == WXK_RIGHT) { // right
    if(input_current_pos < input_index) {
      if (inputBuffer[input_current_pos] == '\n' ||
	  cursor_x + 1 > x_max) {
	setCursor(dc,0, cursor_y + 1);
      }
      else {
	setCursor(dc,cursor_x + 1, cursor_y);	  
      }
      input_current_pos++;
    }
  }
  else if (keyCode == WXK_TAB) { // tab
    //do nothing for now. could be tab completion later.
  }
  else if (keyCode >= WXK_START) {
    /* ignore it */
  } 
  else {
    buf[0] = keyCode;
    len = 1;
    int doInsert = 0;
    if (input_current_pos < input_index ) { // we are in the middle of input
      doInsert = 1;
      int i;
      for (i = input_index; i >= input_current_pos + 1; i--) {
	inputBuffer[i] = inputBuffer[i - 1]; 
      }
      inputBuffer[input_current_pos] = keyCode;
      input_index++;
      input_current_pos++;
    }
    else {
      inputBuffer[input_index++] = keyCode;
      input_current_pos++;
    }

    if (doInsert) {
      cur_x = cursor_x; cur_y = cursor_y;


      //remember, input_current_pos - 1 has last character typed
      PassInputToTerminal(dc,input_index - (input_current_pos - 1),
			  (unsigned char *)(inputBuffer + (input_current_pos - 1)));
	
      //now the cursor is where the last input position is
      last_input_x = cursor_x;
      last_input_y = cursor_y;
	
      //set cursor back to cursorPos
      if (cur_x == x_max)
	setCursor(dc,0, cur_y + 1);
      else
	setCursor(dc,cur_x+1, cur_y);
    } 
    else {
      PassInputToTerminal(dc,len, buf);
      //now the cursor is where the last input position is
      last_input_x = cursor_x;
      last_input_y = cursor_y;	
    }   
  }
}

void wxTerminal::setCursor (wxDC &dc, int x, int y, bool fromLogo) {


  int vis_x,vis_y;
  GetViewStart(&vis_x,&vis_y);

  if(!(m_currMode & DEFERUPDATE)) {
    //undraw cursor
    InvertArea(dc, cursor_x*m_charWidth, (cursor_y-vis_y)*m_charHeight, m_charWidth, m_charHeight);
  }

  if(fromLogo) {
    //need to change to unscrolled coordinates

    if(x < 0 || x > m_width ||
       y < 0 || y > m_height) {
      return;
    }
    //GetViewStart(&cursor_x,&cursor_y);
    /*
    if(!m_vscroll_enabled) {
      cursor_x = x;
      cursor_y = m_vscroll_pos;
      }*/

    cursor_x = x;
    cursor_y = vis_y + y;

    
  }
  else {
    cursor_x = x;
    cursor_y = y;
  }


  int want_x, want_y;
  want_x = cursor_x;
  want_y = cursor_y;

  if(cursor_y < 0) {
    fprintf(stderr, "cursor_y < 0 in setcursor. BAD \n");
  }
  cursor_y = min(cursor_y,y_max);
  curr_line_pos = GetLinePosition(cursor_y);
  curr_char_pos = line_of(curr_line_pos);
  if(cursor_y < want_y ||  
     cursor_x > curr_char_pos.line_length) {
    cursor_x = curr_char_pos.line_length;
  }
  curr_char_pos.offset = curr_char_pos.offset + cursor_x;
  adjust_charpos(curr_char_pos);

  if(fromLogo && 
     (cursor_x != want_x ||
      cursor_y != want_y)) {
    //add spaces until we get to desired location

    if(cursor_x > x_max) {
      cursor_x = 0;
      cursor_y++;
      inc_linepos(curr_line_pos);
    }

    unsigned char space = ' ';  
    unsigned char newline = '\n';

    while(cursor_y != want_y) {
      PassInputToTerminal(dc,1,&newline);
    }
    while(cursor_x != want_x) {
      PassInputToTerminal(dc,1,&space);
    }
  }
  
  if(!(m_currMode & DEFERUPDATE)) {
    if(cursor_y >= vis_y &&
       cursor_y <= vis_y + m_height - 1) {
      InvertArea(dc, cursor_x*m_charWidth, (cursor_y-vis_y)*m_charHeight, m_charWidth, m_charHeight);
    }
    else {

      Scroll(-1, cursor_y);
      
      Refresh();
    }
  }

}

void wxTerminal::OnSize(wxSizeEvent& event) {      
	
  int x, y;
  GetSize(&x, &y);
  
  //leave one char on the right for scroll bar width
  if (m_width == (x / m_charWidth) - 1 && m_height == y / m_charHeight)
    return;
  x = (x / m_charWidth) - 1;
  y = y / m_charHeight;
  
  if (x < 1) 
    x = 1;
  if (y < 1) 
    y = 1;
  ResizeTerminal(x,y);
  Scroll(-1, cursor_y);
  Refresh();
}

wxMemoryDC * currentMemDC = NULL;
wxBitmap * currentBitmap = NULL;
int oldWidth = -1;
int oldHeight = -1;

void wxTerminal::OnEraseBackground(wxEraseEvent &WXUNUSED(event)) 
{
  //don't erase background.. for double buffering!
}

void wxTerminal::OnPaint(wxPaintEvent &WXUNUSED(event)) 
{
  wxAutoBufferedPaintDC dc(this);
  DoPrepareDC(dc);
  dc.SetBackground(m_colors[0]);
  dc.Clear();
  OnDraw(dc);
}

void wxTerminal::OnDraw(wxDC& dc)
{  
  //DebugOutputBuffer();
  
  wxRect rectUpdate = GetUpdateRegion().GetBox();
  CalcUnscrolledPosition(rectUpdate.x, rectUpdate.y,
			 &rectUpdate.x, &rectUpdate.y);
  int lineFrom = rectUpdate.y / m_charHeight;
  int lineTo = rectUpdate.GetBottom() / m_charHeight;

  /*  if(!m_vscroll_enabled) {
    lineFrom += m_vscroll_pos;
    lineTo += m_vscroll_pos;
    fprintf(stderr, "OnDraw: lines %d to %d\n", lineFrom, lineTo);
    }*/

  if ( lineTo > y_max)
    lineTo = y_max;
  
  //find the position!
  wxterm_linepos tlpos;
  tlpos.buf = term_lines;
  wxterm_charpos tline;
  tlpos.offset = lineFrom;
  adjust_linepos(tlpos);

  for ( int line = lineFrom; line <= lineTo; line++ )
  {
    tline = line_of(tlpos);
    for ( int col = 0; col < tline.line_length; col++ ) {      
      DrawText(dc, m_curFG, m_curBG, mode_of(tline), col, line, 1, &char_of(tline));
      inc_charpos(tline);
    }

    inc_linepos(tlpos);
  }

  //draw cursor if visible
  if(lineFrom <= cursor_y  && cursor_y <= lineTo &&
     !(m_currMode & CURSORINVISIBLE)) {
    int c_x = cursor_x;
    int c_y = cursor_y;
    /*    if(!m_vscroll_enabled) {
      c_y = c_y - m_vscroll_pos;
      }*/
    //    dc.Blit( c_x*m_charWidth, c_y*m_charHeight, m_charWidth, m_charHeight, &dc, c_x*m_charWidth, c_y*m_charHeight, wxINVERT);
    InvertArea(dc, c_x*m_charWidth, c_y*m_charHeight, m_charWidth, m_charHeight);
  }

  MarkSelection(dc,FALSE);
  
}

// gets the click coordinate (unscrolled) in terms of characters
void wxTerminal::GetClickCoords(wxMouseEvent& event, int *click_x, int *click_y) {
  // pixel coordinates
  *click_x = event.GetX();
  *click_y = event.GetY();
  CalcUnscrolledPosition(*click_x, *click_y, click_x, click_y);
  /*  if(!m_vscroll_enabled) {
    *click_y += m_vscroll_pos * m_charHeight;
    }*/
  // convert to character coordinates
  *click_x = *click_x / m_charWidth;
  *click_y = *click_y / m_charHeight;
  if(*click_x < 0) {
    *click_x = 0;
  }
  else if(*click_x > x_max) {
    *click_x = x_max;
  }

  if(*click_y < 0) { 
    *click_y = 0; 
  }
  else if(*click_y > y_max) {
    *click_y = y_max;
  }
}

void
wxTerminal::OnLeftDown(wxMouseEvent& event)
{
  m_selecting = TRUE;
  int click_x, click_y;
  GetClickCoords(event, &click_x, &click_y);

  m_selx1 = m_selx2 = click_x;
  m_sely1 = m_sely2 = click_y;
  Refresh();

  event.Skip();  //let native handler reset focus
}

void
wxTerminal::OnLeftUp(wxMouseEvent& event)
{
  
  if ((m_sely2 != m_sely1 || m_selx2 != m_selx1))
    {
      if (wxTheClipboard->Open() ) {
	// This data objects are held by the clipboard, 
	// so do not delete them in the app.
	wxTheClipboard->SetData( new wxTextDataObject(GetSelection()) );
	wxTheClipboard->Close();
      }
    }
  m_selecting = FALSE;

  if (HasCapture()) {
    // uncapture mouse
    ReleaseMouse();
  }
}

void
wxTerminal::OnMouseMove(wxMouseEvent& event)
{
  if(m_selecting)
  { 
    wxClientDC unbuffered_dc(this);
    wxSize sz = unbuffered_dc.GetSize();    
    wxBufferedDC dc(&unbuffered_dc, sz);
    dc.Blit(0,0,sz.GetWidth(), sz.GetHeight(), &unbuffered_dc, 0, 0);

    MarkSelection(dc, TRUE);

    int click_x, click_y;
    GetClickCoords(event, &click_x, &click_y);
    m_selx2 = click_x;
    m_sely2 = click_y;
    

    MarkSelection(dc, TRUE);

      
    if(!HasCapture()) {
      CaptureMouse();
    }      
  }
}

void
wxTerminal::ClearSelection()
{
  if (m_sely2 != m_sely1 || m_selx2 != m_selx1) {
    wxClientDC dc(this);

    MarkSelection(dc, TRUE); //undraw

    m_sely2 = m_sely1;
    m_selx2 = m_selx1;   

  }
}

void 
wxTerminal::InvertArea(wxDC &dc, int t_x, int t_y, int w, int h, bool scrolled_coord) {
  if(!m_vscroll_enabled) {
    t_y -= m_vscroll_pos * m_charWidth;
  }
  if(scrolled_coord) {
    CalcScrolledPosition(t_x,t_y,&t_x,&t_y);
    //calculate if out of bounds
    //    if(t_x < 0 || t_x > m_width * m_charWidth ||
    //   t_y < 0 || t_y > m_height * m_charHeight) {
    //  return;
    //}
  }
  dc.Blit( t_x, t_y, w, h, &dc, t_x, t_y, wxINVERT);
}


void
wxTerminal::MarkSelection(wxDC &dc, bool scrolled_coord) {

  int 
    pic_x1, pic_y1,
    pic_x2, pic_y2;

  //invert temporarily if out of order

  if(m_sely1 > m_sely2 ||
     (m_sely1 == m_sely2 && m_selx1 > m_selx2)) {
    pic_x1 = m_selx2;
    pic_y1 = m_sely2;
    pic_x2 = m_selx1;
    pic_y2 = m_sely1;
  }
  else {
    pic_x1 = m_selx1;
    pic_y1 = m_sely1;
    pic_x2 = m_selx2;
    pic_y2 = m_sely2;
  }

  if(pic_y1 == pic_y2) {
    InvertArea(dc, 
	       pic_x1 * m_charWidth, pic_y1 * m_charHeight, 
	       (pic_x2 - pic_x1)*m_charWidth, m_charHeight,
	       scrolled_coord);
  }
  else if(pic_y1 < pic_y2) {
    InvertArea(dc, 
	       pic_x1 * m_charWidth, pic_y1 * m_charHeight, 
	       (x_max - pic_x1) * m_charWidth, m_charHeight,
	       scrolled_coord);
    InvertArea(dc, 
	       0, (pic_y1 + 1)*m_charHeight,
	       x_max * m_charWidth, (pic_y2 - pic_y1 - 1)*m_charHeight,
	       scrolled_coord);
    InvertArea(dc, 
	       0, pic_y2*m_charHeight, 
	       pic_x2*m_charWidth, m_charHeight,
	       scrolled_coord);
  }
}

bool
wxTerminal::HasSelection()
{
  return(m_selx1 != m_selx2 || m_sely1 != m_sely2);
}


/*
 * Gets characters from x1,y1 up to , but not including x2,y2
 */
wxString
wxTerminal::GetChars(int x1, int y1, int x2, int y2) 
{
  int tx,ty;
  if(y1 > y2 || 
     (y1 == y2 && x1 > x2)) {
    tx = x1;
    ty = y1;
    x1 = x2;
    y1 = y2;
    x2 = tx;
    y2 = ty;
  }

  //this case from dragging the mouse position off screen.
  if(x1 < 0) x1 = 0;
  if(x2 < 0) x2 = 0;
  if(y1 < 0) y1 = 0;
  if(y2 < 0) y2 = 0;

  wxString ret;

  if(0 < y1 && y1 > y_max) {
    return ret;
  }

  wxterm_charpos a = GetCharPosition(0,y1);

  a.offset = a.offset + min(x1, max(0,a.line_length - 1)); 
  adjust_charpos(a);

  wxterm_charpos b = GetCharPosition(0,min(y2, y_max));
  
  b.offset = b.offset + min(x2, b.line_length); 
  adjust_charpos(b);

  while(a.buf != b.buf) {
    //  size 10, offset 5,  copy 10-5=5 chars... yup
    ret.Append((wxChar*)a.buf->cbuf+a.offset, WXTERM_CB_SIZE-a.offset);
    if(a.buf->next) {
      a.offset = 0;
      a.buf = a.buf->next;
    }
    else {
      //bad
      fprintf(stderr, "BAD (getchars)\n");
    }
  }
  ret.Append((wxChar*)a.buf->cbuf+a.offset, b.offset - a.offset);
  return ret;
}

wxString
wxTerminal::GetSelection()
{
  return GetChars(m_selx1,m_sely1,m_selx2,m_sely2);
}

void
wxTerminal::SelectAll()
{
  m_selx1 = 0;
  m_sely1 = 0;
  m_selx2 = x_max;
  m_sely2 = y_max;

  Refresh();
}

wxterm_linepos wxTerminal::GetLinePosition(int y) 
{
  wxterm_linepos lpos;
  lpos.buf = term_lines;
  lpos.offset = y;
  adjust_linepos(lpos);
  return lpos;
}

wxterm_charpos wxTerminal::GetCharPosition(int x, int y)
{ 
  wxterm_charpos ret = line_of(GetLinePosition(y));

  if(x > ret.line_length) {    
    fprintf(stderr, "invalid longer than linelength: %d\n", ret.line_length);
  }

  
  ret.offset = ret.offset + x;
  adjust_charpos(ret);
  return ret;  
}

void
wxTerminal::DrawText(wxDC& dc, int fg_color, int bg_color, int flags,
                 int x, int y, int len, unsigned char *string)
{
  wxString
    str(string, len);

    if(flags & BOLD)
    {
      if(flags & UNDERLINE)
        dc.SetFont(m_boldUnderlinedFont);
      else
        dc.SetFont(m_boldFont);
    }
    else
    {
      if(flags & UNDERLINE)
        dc.SetFont(m_underlinedFont);
      else
        dc.SetFont(m_normalFont);
    }

  int coord_x, coord_y;
  dc.SetBackgroundMode(wxSOLID);
  dc.SetTextBackground(m_colors[bg_color]);
  dc.SetTextForeground(m_colors[fg_color]);
  coord_y = y * m_charHeight; 
  coord_x = x * (m_charWidth);
	  
  for(unsigned int i = 0; i < str.Length(); i++, coord_x+=m_charWidth){
    //clear the pixels first
    dc.Blit(coord_x, coord_y, m_charWidth, m_charHeight, &dc, coord_x, coord_y, wxCLEAR);
    dc.DrawText(str.Mid(i, 1), coord_x, coord_y);
    //	  if(flags & BOLD && m_boldStyle == OVERSTRIKE)
    //	  dc.DrawText(str, x + 1, y);
    if(flags & INVERSE) {
      InvertArea(dc, coord_x, coord_y, m_charWidth, m_charHeight);
      //	    dc.Blit( coord_x, coord_y, m_charWidth, m_charHeight, &dc, coord_x, coord_y, wxINVERT);
    }
  }
}

void
wxTerminal::Bell()
{
  wxBell();
}

void 
wxTerminal::RenumberLines(int new_width) 
{
  //m_width is the OLD WIDTH at this point of the code.
  
  wxterm_linepos l_pos;
  l_pos.buf = term_lines;
  l_pos.offset = 0;

  wxterm_charpos c_pos = (wxterm_charpos) { term_chars, 0, 0 };
  wxterm_charpos last_logo_pos = GetCharPosition(last_logo_x,last_logo_y);

  //IMPORTANT: 
  //  * line_number stores the current line we're looking at,
  //  * c_pos.line_length is how far into the current line we are
  //  * c_pos.offset is the offset into the BUFFER

  int line_number = 0;
  
  //run until see '\0'
  int next_line = 0; // flag to say "mark next line".
  
  while(char_of(c_pos) != '\0') {
    if(c_pos.buf == curr_char_pos.buf && c_pos.offset == curr_char_pos.offset) {
      cursor_x = c_pos.line_length;
      cursor_y = line_number;
      curr_line_pos = l_pos;
    }
    if(c_pos.buf == last_logo_pos.buf && c_pos.offset == last_logo_pos.offset) {
      last_logo_x = c_pos.line_length;
      last_logo_y = line_number;      
    }
    if(char_of(c_pos) == '\n') {
      next_line = 1;
    }
    else {
      c_pos.line_length++;
      if(c_pos.line_length == new_width) {
	next_line = 1;
      }
    }

    inc_charpos(c_pos);

    if(next_line) {
      line_of(l_pos).line_length = c_pos.line_length;
      inc_linepos(l_pos);

      line_of(l_pos).buf = c_pos.buf;
      line_of(l_pos).offset = c_pos.offset;
      
      c_pos.line_length = 0;
      next_line = 0;
      line_number++;
    }



  }
  if(c_pos.buf == curr_char_pos.buf && c_pos.offset == curr_char_pos.offset) {
    cursor_x = c_pos.line_length;
    cursor_y = line_number;
    curr_line_pos = l_pos;
  }
  if(c_pos.buf == last_logo_pos.buf && c_pos.offset == last_logo_pos.offset) {
    last_logo_x = c_pos.line_length;
    last_logo_y = line_number;      
  }

  line_of(l_pos).line_length = c_pos.line_length;
  y_max = line_number;

  //sanity check on variables
  //fprintf(stderr, "cursor %d %d\n", cursor_x, cursor_y);
  //fprintf(stderr, "lastlogopos xy %d %d\n", last_logo_x, last_logo_y);
}

void
wxTerminal::ResizeTerminal(int width, int height)
{

  /*
  **  Set terminal size
  */
  if(width != m_width) {
    //need to renumber the lines
    RenumberLines(width);
    m_width = width;
    x_max = m_width - 1;
  }
  m_height = height;


  //reset virtual size
  SetScrollRate(0, m_charHeight);
  //number of lines is y_max + 1
  SetVirtualSize(m_width*m_charWidth,(y_max+1)*m_charHeight);
}


void wxTerminal::DebugOutputBuffer() {
  //debugging
  wxterm_linepos lpos;
  lpos.buf = term_lines;
  lpos.offset = 0;
  wxterm_charpos pos_1 = line_of(lpos);
  
    fprintf(stderr, "WXTERMINAL STATS: \n  width: %d, height: %d, \n cw: %d, ch: %d \n x_max: %d, y_max: %d \n cursor_x: %d, cursor_y: %d \n last_logo_x : %d, last_logo_y: %d \ncurr_charpos buf %d offset %d  \ncurr_line buf %d offset %d\n", m_width, m_height, m_charWidth, m_charHeight, x_max, y_max,cursor_x, cursor_y, last_logo_x, last_logo_y,(int)curr_char_pos.buf, curr_char_pos.offset, (int)curr_line_pos.buf, curr_line_pos.offset);
    fprintf(stderr, "WXTERMINAL CHARACTER BUFFER\n###############\n");
  while(char_of(pos_1) != '\0') {
    if(char_of(pos_1) == '\n') {
      fprintf(stderr, "\\n\n");
    }
    else {
      fprintf(stderr,"%c", char_of(pos_1));
      
    }
    inc_charpos(pos_1);
  }
  fprintf(stderr, "|");
    fprintf(stderr, "\n#############\n");
    fprintf(stderr, "WXTERMINAL LINE BUFFER\n##############\n");
  for(int i = 0; i <= y_max; i++) {
    fprintf(stderr, "LINE %d: buf: %d, offset: %d, len: %d\n", i,(int)line_of(lpos).buf, line_of(lpos).offset, line_of(lpos).line_length);
    inc_linepos(lpos);
  }
    fprintf(stderr, "\n#############\n\n");
}

void wxTerminal::InsertChar(char c)
{
  
  //insert a character at current location

  //if there is a newline at the current location and we're not inserting one
  if(char_of(curr_char_pos) == '\n' &&
     c != '\n') {
    // check case if we're about to insert the last character of the line
    if(line_of(curr_line_pos).line_length < m_width - 1) {       

      //have to add characters to current line, 

      //bump characters up.      
      wxterm_linepos lpos = curr_line_pos;
      wxterm_charpos pos_1 = curr_char_pos;
      wxterm_charpos pos_2 = pos_1;

      inc_charpos(pos_2);


      //  Method of relocating characters:
      //   need two variables, S = save, G = grab
      // 
      //   save 1, 
      //   S1  G      
      //  12345678
      //
      //   grab 2, place 1, save 2
      //  
      //  S2 G2
      //  _1345678
      //
      //  grab 3, place 2, save 3...
      //
      //  S3 G3
      //  _1245678
      // 
      //  etc...
      // ends when next position to grab is '\0'
      // (allocate buffer when necesary)
      // grab 8, place 7, save 8
      // place 8 in that last position

      unsigned char save_char, save_mode; 
      unsigned char grab_char, grab_mode;       
      save_char = char_of(pos_1);
      save_mode = mode_of(pos_1);

      while(char_of(pos_2) != '\0') {

	grab_char = char_of(pos_2);
	grab_mode = mode_of(pos_2);
	char_of(pos_2) = save_char;
	mode_of(pos_2) = save_mode;
	save_char = grab_char;
	save_mode = grab_mode;

	inc_charpos(pos_1);
	inc_charpos(pos_2);	
      }
      //insert "save" into pos2
      char_of(pos_2) = save_char;
      mode_of(pos_2) = save_mode;
      
      //now bump all the lines up.
      //look at the current line number (cursor_y)
      // go until  y_max, and adjust position of all lines +1
      inc_linepos(lpos);
      for(int lnum = cursor_y+1; lnum <= y_max; lnum++) {
	inc_charpos(line_of(lpos));
	inc_linepos(lpos);      
      }
    }
  }
  char_of(curr_char_pos) = c;
  mode_of(curr_char_pos) = m_currMode;
  inc_charpos(curr_char_pos);
}

void wxTerminal::NextLine() {
  //points next line position to current char position, and increments line offset.

  inc_linepos(curr_line_pos);

  line_of(curr_line_pos).buf = curr_char_pos.buf;
  line_of(curr_line_pos).offset = curr_char_pos.offset;
  //line_of(curr_line_pos).line_length = 0;


  cursor_y++;
  if(cursor_y > y_max) {
    y_max = cursor_y;
    int x,y;
    GetVirtualSize(&x,&y);
    if(m_vscroll_enabled) {
      //y_max + 1 = number of lines
      SetVirtualSize(max(x,m_width*m_charWidth),max(y,(y_max+1)*m_charHeight));
    }
  }
  cursor_x = 0;
}

void
wxTerminal::PassInputToTerminal(wxDC &dc, int len, unsigned char *data)
{

  int i;
  int numspaces, j;
  wxterm_linepos lpos;
  wxterm_charpos pos_1, pos_2;
  int new_line_length;

  int vis_x,vis_y;
  GetViewStart(&vis_x,&vis_y);
  static int old_vis_y = 0;

  unsigned char spc = ' ';

  //first undraw cursor
  if(!(m_currMode & DEFERUPDATE)) {
    InvertArea(dc, cursor_x*m_charWidth, (cursor_y-vis_y)*m_charHeight, m_charWidth, m_charHeight);
  }

  for(i = 0; i < len; i++) {
    switch(data[i]) {
    case TOGGLE_STANDOUT:  // char 17
      //enter/exit standout mode , ignore character
      m_currMode ^= INVERSE;
      break;
    case '\t':
      // formula is: (8 - (cursorpos%8)) spaces
      numspaces = (8 - (cursor_x % 8));
      if(numspaces == 0) {
	numspaces = 8;
      }
      for(j = 0; j < numspaces; j++) {
	InsertChar(spc);
	
	//draw
	if(!(m_currMode & DEFERUPDATE) &&
	   cursor_y >= vis_y &&
	   cursor_y <= vis_y + m_height - 1) {
	  DrawText(dc, m_curFG, m_curBG, m_currMode, cursor_x, cursor_y - vis_y, 1, &spc);
	}


	cursor_x++;
	if(cursor_x > line_of(curr_line_pos).line_length) {
	  line_of(curr_line_pos).line_length = cursor_x;
	}
	if(cursor_x > x_max) {
	  //tab should stop inserting spaces.	   
	  NextLine();
	  break; //out of the for loop
	}
      }
      break;
    case '\r':
    case '\n':
      new_line_length = cursor_x;
      InsertChar('\n');

      //clear the region from cursor to end of line
      if(!(m_currMode & DEFERUPDATE) &&
	 cursor_y >= vis_y &&
	 cursor_y <= vis_y + m_height - 1) {
	dc.Blit(cursor_x*m_charWidth, (cursor_y-vis_y)*m_charHeight, (x_max - cursor_x+1)*m_charWidth, m_charHeight, &dc, cursor_x*m_charWidth, cursor_y*m_charHeight, wxCLEAR);
      }

      
      if(i + 1 < len && 
	 ((data[i] == '\r' && data[i+1] == '\n') ||  //LF+CR
	  (data[i] == '\n' && data[i+1] == '\r')))  { //CR+LF
	i++;
      }
      if(new_line_length < line_of(curr_line_pos).line_length &&
	 cursor_y < y_max) {
	//shift all the characters down.
	//(remove characters between inserted newline and end of line)
	
	lpos = curr_line_pos;
	inc_linepos(lpos);
	pos_1 = curr_char_pos;
	pos_2 = line_of(lpos);
	while(char_of(pos_1) != '\0') {
	  if(char_of(pos_2) != '\0' &&
	     pos_2.buf == line_of(lpos).buf &&
	     pos_2.offset == line_of(lpos).offset) {
	    line_of(lpos).buf = pos_1.buf;
	    line_of(lpos).offset = pos_1.offset;
	    inc_linepos(lpos);
	  }
	  char_of(pos_1) = char_of(pos_2);
	  mode_of(pos_1) = mode_of(pos_2);
	  inc_charpos(pos_1);
	  inc_charpos(pos_2);
	}
      }
      line_of(curr_line_pos).line_length = new_line_length;
      
      NextLine();
      
      break;
    default:
      InsertChar(data[i]);

      //draw
      if(!(m_currMode & DEFERUPDATE) &&
	 cursor_y >= vis_y &&
	 cursor_y <= vis_y + m_height - 1) {
	DrawText(dc, m_curFG, m_curBG, m_currMode, cursor_x, cursor_y - vis_y, 1, &data[i]);
      }

      cursor_x++;
      if(cursor_x > line_of(curr_line_pos).line_length) {
	line_of(curr_line_pos).line_length = cursor_x;
      }
      if(cursor_x > x_max) {
	NextLine();
      }      
      break;
    }   
  }

  if(!(m_currMode & DEFERUPDATE)) {
    //draw the cursor back
    if(cursor_y >= vis_y &&
       cursor_y <= vis_y + m_height - 1) {
      InvertArea(dc, cursor_x*m_charWidth, (cursor_y-vis_y)*m_charHeight, m_charWidth, m_charHeight);
    }

    //scroll if cursor current cursor not visible or 
    // if we're not reading an instruction (logo output)
    // old_vis_y keeps track of whether the screen has changed lately
    
    if((!readingInstruction &&
	1 ||                     //don't use old_vis_y??
	vis_y != old_vis_y) || 
       cursor_y < vis_y  ||
       cursor_y > vis_y + m_height - 1) {
      Scroll(-1, cursor_y);
      old_vis_y = vis_y;
      
      Refresh();
    }  
    
  }
}

wxString * wxTerminal::get_text() 
{
  //  int i;
  wxString *outputString = new wxString();
  outputString->Clear();
  outputString->Append("<HTML>\n");
  outputString->Append("<BODY>\n");
  outputString->Append("<FONT SIZE=2>\n");
  wxString txt = GetChars(0,0,x_max,y_max);
  txt.Replace("\n","<BR>\n");
  outputString->Append(txt);
  /*
  wxterm_linepos tlpos = term_lines;
  for(i=0;i<ymax;i++){
    outputString->append(textString->Mid(linenumbers[i]*MAXWIDTH),MAXWIDTH);
    outputString->append("<BR>");		
    }*/
  outputString->Append("<\\FONT>");
  outputString->Append("<\\BODY>");
  outputString->Append("<\\HTML>");
  //  delete textString;
  return outputString;
}


void
wxTerminal::SelectPrinter(char *PrinterName)
{
  if(m_printerFN)
  {
    if(m_printerName[0] == '#')
      fclose(m_printerFN);
    else
#if defined(__WXMSW__)
      fclose(m_printerFN);
#else
      pclose(m_printerFN);
#endif

    m_printerFN = 0;
  }

  if(m_printerName)
  {
    free(m_printerName);
    m_printerName = 0;
  }

  if(strlen(PrinterName))
  {
    m_printerName = strdup(PrinterName);
  }
}

void
wxTerminal::PrintChars(int len, unsigned char *data)
{
  char
    pname[100];

  if(!m_printerFN)
  {
    if(!m_printerName)
      return;

    if(m_printerName[0] == '#')
    {
#if defined(__WXMSW__)
      sprintf(pname, "lpt%d", m_printerName[1] - '0' + 1);
#else
      sprintf(pname, "/dev/lp%d", m_printerName[1] - '0');
#endif
      m_printerFN = fopen(pname, "wb");
    }
    else
    {
#if defined(__WXMSW__)
      m_printerFN = fopen(m_printerName, "wb");
#else
      sprintf(pname, "lpr -P%s", m_printerName);
      m_printerFN = popen(pname, "w");
#endif
    }
  }
  
  if(m_printerFN)
  {
    fwrite(data, len, 1, m_printerFN);
  }
}



// ----------------------------------------------------------------------------
// Functions called from the interpreter thread
// ----------------------------------------------------------------------------

extern "C" void setCharMode(int mode){
	logo_char_mode = mode;
}

extern "C" void wxClearText() {
  wxTerminal::terminal->ClearScreen();
  out_buff_index_public = 0;
  out_buff_index_private = 0;
}

void wxTerminal::ClearScreen() {
  if(y_max > 0) {
    int x,y;
    GetVirtualSize(&x,&y);
    SetVirtualSize(max(x,m_width*m_charWidth),max(y,(y_max+1+m_height)*m_charHeight));
    Scroll(-1,y_max+1);
    int vx,vy;
    GetViewStart(&vx,&vy);
    //pretend setcursor from logo so that it can insert spaces if needed
    wxClientDC dc(this);
    setCursor(dc,0,y_max + 1 - vy, TRUE); 
    Refresh();
  }
}

void wxTerminal::EnableScrolling(bool want_scrolling) {
  //wxScrolledWindow::EnableScrolling(FALSE,want_scrolling); //doesn't work
  static int 
    scroll_disabled = FALSE,
    scroll_pos = GetScrollPos(wxVERTICAL),
    scroll_range = GetScrollRange(wxVERTICAL),
    scroll_thumb = GetScrollThumb(wxVERTICAL);
  if(want_scrolling) {
    if(scroll_disabled) {
      SetScrollbar(wxVERTICAL, scroll_pos, scroll_thumb, scroll_range);
      scroll_disabled = FALSE;
    }
  }
  else {
    scroll_pos = GetScrollPos(wxVERTICAL);
    scroll_range = GetScrollRange(wxVERTICAL);
    scroll_thumb = GetScrollThumb(wxVERTICAL);

    SetScrollbar(wxVERTICAL, 0,0,0);
    scroll_disabled = TRUE;
  }
  return;
}

extern "C" void flushFile(FILE * stream, int);

extern "C" void wxSetCursor(int x, int y){
  //just call wxTerminal::setCursor with a special flag.
  flushFile(stdout, 0);
  wxTerminal::terminal->EnableScrolling(FALSE);
  wxClientDC dc(wxTerminal::terminal);
  wxTerminal::terminal->setCursor(dc,x,y,TRUE);
}


extern "C" void wx_enable_scrolling() {
  wxTerminal::terminal->EnableScrolling(TRUE);
}

extern "C" int check_wx_stop(int force_yield) {
  logoEventManager->ProcessAnEvent(force_yield); 

  if (logo_stop_flag) {
    logo_stop_flag = 0;
#ifdef SIG_TAKES_ARG
    logo_stop(0);
#else
    logo_stop();
#endif
    return 1;
  }
  if (logo_pause_flag) {
    logo_pause_flag = 0;
#ifdef SIG_TAKES_ARG
    logo_pause(0);
#else
    logo_pause();
#endif
    return 0;
  }
  return 0;
}

extern "C" int internal_check(){
 if (logo_stop_flag) {
    logo_stop_flag = 0;
#ifdef SIG_TAKES_ARG
    logo_stop(0);
#else
    logo_stop();
#endif
    return 1;
  }
  if (logo_pause_flag) {
    logo_pause_flag = 0;
#ifdef SIG_TAKES_ARG
    logo_pause(0);
#else
    logo_pause();
#endif
    return 1;
  }
  return 0;
}

extern "C" int getTermInfo(int type){
	switch (type){
	case X_COORD:
		return wxTerminal::terminal->cursor_x;
		break;
	case Y_COORD:
	  int vx,vy;
	  wxTerminal::terminal->GetViewStart(&vx,&vy);
		return wxTerminal::terminal->cursor_y - vy;
		break;
	case X_MAX:
	  //return wxTerminal::terminal->x_max;
	  return wxTerminal::terminal->m_width;
		break;
	case Y_MAX:
	  //return wxTerminal::terminal->y_max;
	  return wxTerminal::terminal->m_height;
		break;
	case EDIT_STATE:
		return editWindow->stateFlag;
		break;
	default:
		return -1;
	}
	
	return -1;
}

extern "C" void setTermInfo(int type, int val){
	switch (type){
		case X_COORD:
		  //wxTerminal::terminal->x_coord=val;
		  return;
			break;
		case Y_COORD:
		  //wxTerminal::terminal->y_coord=val;
		  return;
			break;
		case X_MAX:
		  return;
		  //wxTerminal::terminal->x_max=val;
			break;
		case Y_MAX:
		  return;
		  //wxTerminal::terminal->y_max=val;
			break;
		case EDIT_STATE:
			editWindow->stateFlag=val;
			break;
	}
}


extern "C" void doClose() {
  extern int wx_leave_mainloop;

  if(!wx_leave_mainloop) {
    logoFrame->Close(TRUE);
  }
}

