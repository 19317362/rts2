#include "rts2nwindow.h"
#include "nmonitor.h"

#include <iostream>

Rts2NWindow::Rts2NWindow (WINDOW * master_window, int x, int y, int w, int h,
			  int border):
Rts2NLayout ()
{
  if (h <= 0)
    h = 0;
  if (w <= 0)
    w = 0;
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  window = newwin (h, w, y, x);
  if (!window)
    errorMove ("newwin", y, x, h, w);
  switch (border)
    {
    case 0:
      _haveBox = false;
      break;
    case 1:
      box (window, 0, 0);
      _haveBox = true;
      break;
    }
}

Rts2NWindow::~Rts2NWindow (void)
{
  delwin (window);
}

void
Rts2NWindow::draw ()
{
  werase (window);
  if (haveBox ())
    {
      box (window, 0, 0);
    }
}

int
Rts2NWindow::getX ()
{
  int x, y;
  getbegyx (window, y, x);
  return x;
}

int
Rts2NWindow::getY ()
{
  int x, y;
  getbegyx (window, y, x);
  return y;
}

int
Rts2NWindow::getCurX ()
{
  int x, y;
  getyx (getWriteWindow (), y, x);
  return x;
}

int
Rts2NWindow::getCurY ()
{
  int x, y;
  getyx (getWriteWindow (), y, x);
  return y;
}


int
Rts2NWindow::getWidth ()
{
  int w, h;
  getmaxyx (window, h, w);
  return w;
}

int
Rts2NWindow::getHeight ()
{
  int w, h;
  getmaxyx (window, h, w);
  return h;
}

int
Rts2NWindow::getWriteWidth ()
{
  int ret = getHeight ();
  if (haveBox ())
    return ret - 2;
  return ret;
}

int
Rts2NWindow::getWriteHeight ()
{
  int ret = getHeight ();
  if (haveBox ())
    return ret - 2;
  return ret;
}

void
Rts2NWindow::errorMove (const char *op, int y, int x, int h, int w)
{
  endwin ();
  std::cout << "Cannot perform ncurses operation " << op
    << " y x h w "
    << y << " "
    << x << " "
    << h << " "
    << w << " " << "LINES COLS " << LINES << " " << COLS << std::endl;
  exit (EXIT_FAILURE);
}

void
Rts2NWindow::move (int x, int y)
{
  int _x, _y;
  getbegyx (window, _y, _x);
  if (x == _x && y == _y)
    return;
  if (mvwin (window, y, x) == ERR)
    errorMove ("mvwin", y, x, -1, -1);
}

void
Rts2NWindow::resize (int x, int y, int w, int h)
{
  if (wresize (window, h, w) == ERR)
    errorMove ("wresize", 0, 0, h, w);
  move (x, y);
}

void
Rts2NWindow::grow (int max_w, int h_dif)
{
  int x, y, w, h;
  getbegyx (window, y, x);
  getmaxyx (window, h, w);
  if (max_w > w)
    w = max_w;
  resize (x, y, w, h + h_dif);
}

void
Rts2NWindow::refresh ()
{
  wnoutrefresh (window);
}

void
Rts2NWindow::enter ()
{
}

void
Rts2NWindow::leave ()
{
}

void
Rts2NWindow::setCursor ()
{
  setsyx (getCurY (), getCurX ());
}

void
Rts2NWindow::printState (Rts2Conn * conn)
{
  if (conn->getErrorState ())
    wcolor_set (getWriteWindow (), CLR_FAILURE, NULL);
  else if (conn->havePriority ())
    wcolor_set (getWriteWindow (), CLR_OK, NULL);
  wprintw (getWriteWindow (), "%s %s (%i) priority: %s\n", conn->getName (),
	   conn->getStateString ().c_str (), conn->getState (),
	   conn->havePriority ()? "yes" : "no");
  wcolor_set (getWriteWindow (), CLR_DEFAULT, NULL);
}
