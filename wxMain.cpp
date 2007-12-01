#include <iostream>
#include <wx/wx.h>
#include "LogoFrame.h"
#include <stdio.h>
#include <wx/thread.h>
#include <wx/app.h>
#include "stdio.h"
#include "wxTurtleGraphics.h"
#include "wxGlobals.h"
#ifdef __WXMAC__
#include <CoreServices/CoreServices.h>
#endif
#include <string>
#include "wxTerminal.h"		/* must come after wxTurtleGraphics.h */
std::string pathString;
#include <wx/stdpaths.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

int wx_Debugging = 0;

/* The thread for Logo */
class AppThread : public wxThread {
	
public:
	AppThread(int , char **);
	virtual void * Entry();
	virtual void doExit(int code);
	virtual void OnExit();
private:
		int argc;
	char ** argv;
};



// start the application
IMPLEMENT_APP(LogoApplication)


// ----------------------------------------------------------------------------
// Globals
// ----------------------------------------------------------------------------


// externs from the logo interpreter 
extern "C" int start (int , char **);
extern "C" void redraw_graphics(); 

// need to redraw turtle graphics
int needToRefresh = 0;
// need to load
int load_flag = 0;
// need to save
int save_flag = 0;
// alreadyAlerted indicated there is a pending message from logo to the GUI
int alreadyAlerted = 0;

// IO buffer handling
char nameBuffer [NAME_BUFFER_SIZE];
int nameBufferSize = 0;
int MAXOUTBUFF  =  MAXOUTBUFF_;
char out_buff1[MAXOUTBUFF_];
char out_buff2[MAXOUTBUFF_];
int out_buff_index_public = 0;
int out_buff_index_private = 0;
char * out_buff_public = out_buff1;
char * out_buff_private = out_buff2;

// mutexes for the buffers
wxMutex out_mut;
wxMutex * init_m;
wxMutex buff_full_m;
wxCondition buff_full_cond(out_mut);

// used for sleeping
wxMutex sleepMut;
wxCondition sleepCond(sleepMut);

// the buffer from the GUI to the interpreter
char buff[1024];
int buff_index = 0;
wxMutex in_mut;
wxCondition read_buff (in_mut);


// ----------------------------------------------------------------------------
// AppThread
// ----------------------------------------------------------------------------

// this class corresponds to the logo interpereter thread that is started when 
// UCBLogo starts up

void * AppThread::Entry()
{
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
	
	/*wxTerminal::terminal->x_coord=0;
	wxTerminal::terminal->y_coord=0;
	wxTerminal::terminal->x_max=80;
	wxTerminal::terminal->y_max=24;*/
  start(argc, argv);
  return 0;

}


void AppThread::doExit(int code) {
  this->Exit((ExitCode)code);
}

void AppThread::OnExit()
{
  turtleGraphics->exitApplication();
  while (1) {
    this->Sleep(1000);
  }
}


AppThread:: AppThread(int c, char ** v) 
  : wxThread(wxTHREAD_DETACHED)
{
  argc = c;
  argv = v;
}

// ----------------------------------------------------------------------------
// misc functions
// ----------------------------------------------------------------------------

extern "C" void wxLogoExit(int code) {
	AppThread * thisThread =  (AppThread *)AppThread::This();
	thisThread->doExit(code);
}

// start the interpreter thread
void init_Logo_Interpreter ( int argc, char ** argv) {
  AppThread * a = new AppThread(argc, argv);
  
   if (a->Create() != wxTHREAD_NO_ERROR) {
     wxdprintf("There was an error during create");
   }

   if (a->Run() != wxTHREAD_NO_ERROR) {
     wxdprintf("There was an error during run");
   }
}

#define LINEPAUSE 25

// flush the output
extern "C" void flushFile(FILE * stream, int justPost) {
  static int numLines = 0;
  int doPost = 0;
  char * temp;

  if (!justPost) {

    out_mut.Lock();
    
    while (out_buff_index_public != 0) {	  
      buff_full_cond.Wait();
    }
    
    if (numLines >= LINEPAUSE) {
      // this is to give the user a chance to pause
      buff_full_cond.WaitTimeout(1);
      numLines = 0;
    }
    else 
      numLines++;
    
    temp = out_buff_public;
    out_buff_public = out_buff_private;
    out_buff_private = temp;
    out_buff_index_public = out_buff_index_private;
    out_buff_index_private = 0;
    if (alreadyAlerted == 0)
      doPost = 1;
  
      out_mut.Unlock();
    
  }

  if (doPost || justPost) {
    if (!doPost) {
      // This is so that I don't have to grab the lock twice
      if (numLines >= LINEPAUSE) {
	out_mut.Lock();
	buff_full_cond.WaitTimeout(1);
	out_mut.Unlock();
	numLines = 0;
      }
      else 
	numLines++;
    }
    haveInputEvent->SetClientData(NULL);
    wxPostEvent(wxTerminal::terminal, *(haveInputEvent));
  }
}

// have the interpreter go to sleep
extern "C" void wxLogoSleep(unsigned int milli) {
  flushFile(stdout, 0);
  if(needToRefresh){
      redraw_graphics();
	  // to make sure we always get an entire refresh
	  if(needToRefresh == 1)
		  needToRefresh = 0;
	  else needToRefresh = 1;
  }
  sleepMut.Lock();
  sleepCond.WaitTimeout(milli);
  sleepMut.Unlock();
}

// have the interpreter wake up
void wxLogoWakeup() {
  sleepMut.Lock();
  sleepCond.Broadcast();
  sleepMut.Unlock();
  in_mut.Lock();
  read_buff.Broadcast();
  in_mut.Unlock();
}

/* Called by the logo thread to display a character onto the terminal screen */
extern "C" void printToScreen(char c, FILE * stream) 
{ 
  if (stream != stdout) {
    putc(c, stream);
    return;
  }
	if(TurtleFrame::in_graphics_mode && !TurtleFrame::in_splitscreen)
		// we are in fullscreen mode
	  wxSplitScreen();
  
  if (out_buff_index_private >= (MAXOUTBUFF - 1)) {
   
    flushFile(stdout, 0);
   
  }
    
  if (c == '\n') {
    out_buff_private[out_buff_index_private++] = 10;
    out_buff_private[out_buff_index_private++] = 13;
    flushFile(stdout, 1);
  }
  else {
     out_buff_private[out_buff_index_private++] = c;
  }
  
}



extern "C" char getFromWX_2(FILE * f) ;

extern "C" char getFromWX() 
{ 
  return getFromWX_2(stdin);
}

extern "C" char getFromWX_2(FILE * f) 
{ 
  int putReturn = 0;
 if (f != stdin) {
    return getc(f);
  }
  in_mut.Lock();
	
  while (buff_index == 0 && !putReturn) {
    if(needToRefresh){
      in_mut.Unlock();
      redraw_graphics();
	  wxdprintf("wxMain after calling redraw graphics\n");
      in_mut.Lock();
      needToRefresh = 0;
	  turtleGraphics->Refresh();
	  wxdprintf("after wxMain calling refresh()");	  
    }
    if (load_flag) {
      load_flag = 0;
      int i;
      buff[buff_index++] = '\n';
      for (i = 0; i < nameBufferSize; i++) {
	buff[buff_index++] = nameBuffer[nameBufferSize - i - 1];
      }
      buff[buff_index++] = '"'; buff[buff_index++] = ' '; buff[buff_index++] = 'd'; buff[buff_index++] = 'a'; buff[buff_index++] = 'o'; buff[buff_index++] = 'l';
    }
    if (save_flag) {
      save_flag = 0;
      int i;
      buff[buff_index++] = '\n';
      for (i = 0; i < nameBufferSize; i++) {
	buff[buff_index++] = nameBuffer[nameBufferSize - i - 1];
      }
      buff[buff_index++] = '"'; buff[buff_index++] = ' '; buff[buff_index++] = 'e'; buff[buff_index++] = 'v'; buff[buff_index++] = 'a'; buff[buff_index++] = 's';
    }
    in_mut.Unlock();
    // Do this while the lock is released just in case the longjump occurs
    if (check_wx_stop()) {
      putReturn = 1;
    }
    flushFile(stdout, 0);
    in_mut.Lock();
    if (buff_index == 0 && !putReturn)
      read_buff.WaitTimeout(1000);
  }
  char c;
  if (putReturn)
    c = '\n';
  else
    c= buff[--buff_index];
  in_mut.Unlock();
  return c;
}


extern "C" int wxKeyp() {
  int ret = 0;
  in_mut.Lock();
  if (buff_index != 0)
    ret = 1;
  in_mut.Unlock();
  return ret;
}

extern "C" int wxUnget_c(int c, FILE * f) {
  if (f != stdin)
    return ungetc(c, f);
  else {
    in_mut.Lock();
    buff[++buff_index] = (char)c;
    in_mut.Unlock();
    return c;
  }
}

extern "C" char* wx_fgets(char* s, int n, FILE* stream) {
  if (stream != stdin) {
    return fgets(s, n, stream);
  }
  char c;
  char * orig = s;
  n --;
  c = ' ';
  while (c != '\n' && n != 0) {
    c = getFromWX_2(stream);
    s[0] = c;
    s++;
    n--;
  }
  return orig;
}


void doLoad(char * name, int length) {
  int i = 0;

  in_mut.Lock();
  while (i < length && i < NAME_BUFFER_SIZE) {
    nameBuffer[i] = name[i];
    i++;
  }
  nameBufferSize = (length >= NAME_BUFFER_SIZE? NAME_BUFFER_SIZE : length);

  load_flag = 1;
  read_buff.Broadcast();

  in_mut.Unlock();
  
}

void doSave(char * name, int length) {
  int i = 0;
  in_mut.Lock();
  while (i < length && i < NAME_BUFFER_SIZE) {
    nameBuffer[i] = name[i];
    i++;
  }
  nameBufferSize = (length >= NAME_BUFFER_SIZE? NAME_BUFFER_SIZE : length);
  
  save_flag = 1;
  read_buff.Broadcast();

  in_mut.Unlock();
  }

extern "C" const char* wxMacGetLibloc(){
#ifdef __WXMAC__
	static std::string libloc;
	libloc=pathString;
	libloc+="logolib";
	return libloc.c_str();
#endif
	return 0;
	
}

extern "C" const char* wxMacGetCslsloc(){
#ifdef __WXMAC__
	static std::string cslsloc;
	cslsloc=pathString;
	cslsloc+="csls";
	return cslsloc.c_str();	
#endif
	return 0;
}

extern "C" const char* wxMacGetHelploc(){
#ifdef __WXMAC__
	static std::string helploc;
	helploc=pathString;
	helploc+="helpfiles";
	return helploc.c_str();
#endif
	return 0;
}
