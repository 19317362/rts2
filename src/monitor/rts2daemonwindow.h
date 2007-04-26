#ifndef __RTS2_DAEMONWINDOW__
#define __RTS2_DAEMONWINDOW__

#include "rts2nwindow.h"

#include "../utils/rts2client.h"
#include "../utils/rts2devclient.h"

class Rts2NSelWindow:public Rts2NWindow
{
protected:
  int selrow;
  int maxrow;
  int padoff_x;
  int padoff_y;
  int lineOffset;
  WINDOW *scrolpad;
public:
    Rts2NSelWindow (WINDOW * master_window, int x, int y, int w, int h,
		    int border = 1, int sw = 300, int sh = 100);
    virtual ~ Rts2NSelWindow (void);
  virtual int injectKey (int key);
  virtual void draw ();
  virtual void refresh ();
  int getSelRow ()
  {
    if (selrow == -1)
      return (maxrow - 1);
    return selrow;
  }
  void setSelRow (int new_sel)
  {
    selrow = new_sel;
  }

  virtual void erase ()
  {
    werase (scrolpad);
  }

  virtual WINDOW *getWriteWindow ()
  {
    return scrolpad;
  }

  int getScrollWidth ()
  {
    int w, h;
    getmaxyx (scrolpad, h, w);
    return w;
  }
  int getScrollHeight ()
  {
    int w, h;
    getmaxyx (scrolpad, h, w);
    return h;
  }
  void setLineOffset (int new_lineOffset)
  {
    lineOffset = new_lineOffset;
  }
};

class Rts2NDevListWindow:public Rts2NSelWindow
{
private:
  Rts2Block * block;
public:
  Rts2NDevListWindow (WINDOW * master_window, Rts2Block * in_block);
  virtual ~ Rts2NDevListWindow (void);
  virtual void draw ();
};

class Rts2NCentraldWindow:public Rts2NSelWindow
{
private:
  Rts2Client * client;
  void drawDevice (Rts2Conn * conn);
public:
    Rts2NCentraldWindow (WINDOW * master, Rts2Client * in_client);
    virtual ~ Rts2NCentraldWindow (void);
  virtual void draw ();
};

#endif /*! __RTS2_DAEMONWINDOW__ */
