/* 
 * Device window display. 
 * Copyright (C) 2007 Petr Kubanek <petr@kubanek,net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __RTS2_NDEVICEWINDOW__
#define __RTS2_NDEVICEWINDOW__

#include "rts2daemonwindow.h"
#include "rts2nvaluebox.h"

class Rts2NDeviceWindow:public Rts2NSelWindow
{
private:
  WINDOW * valueList;
  Rts2Conn *connection;
  void printState ();
  Rts2Value *getSelValue ();
  void printValueDesc (Rts2Value * val);
  void endValueBox ();
  void createValueBox ();
  Rts2NValueBox *valueBox;
  /** Index from which start value box */
  int valueBegins;

  double now;
protected:
  struct timeval tvNow;

  Rts2Conn *getConnection ()
  {
    return connection;
  }

  /**
   * Prints out one value.
   *
   * @param value	Value.
   */
  void printValue (Rts2Value * value);

  virtual void drawValuesList ();

public:
  Rts2NDeviceWindow (Rts2Conn * in_connection);
  virtual ~ Rts2NDeviceWindow (void);

  virtual keyRet injectKey (int key);
  virtual void draw ();
  virtual void refresh ();
  virtual bool setCursor ();
  virtual bool needEnter ()
  {
    return valueBox != NULL;
  }
};

/**
 * Class used to display extra centrald values, which are calculated on client side.
 */
class Rts2NDeviceCentralWindow:public Rts2NDeviceWindow
{
private:
  /** Private values. */
  Rts2ValueTime * nightStart;
  Rts2ValueTime *nightStop;

  Rts2ValueDouble *sunAlt;
  Rts2ValueDouble *sunAz;

  Rts2ValueTime *sunRise;
  Rts2ValueTime *sunSet;

  Rts2ValueDouble *moonAlt;
  Rts2ValueDouble *moonAz;

  Rts2ValueDouble *moonPhase;

  Rts2ValueTime *moonRise;
  Rts2ValueTime *moonSet;

  void printValues ();
protected:
    virtual void drawValuesList ();

public:
    Rts2NDeviceCentralWindow (Rts2Conn * in_connection);
    virtual ~ Rts2NDeviceCentralWindow (void);
};

#endif /* ! __RTS2_NDEVICEWINDOW__ */
