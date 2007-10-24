/* 
 * NCurses based monitoring
 * Copyright (C) 2007 Petr Kubanek <petr@kubanek.net>
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

#ifndef __RTS2_NMONITOR__
#define __RTS2_NMONITOR__

#include "../utils/rts2block.h"
#include "../utils/rts2client.h"
#include "../utils/rts2command.h"

#include "../writers/rts2image.h"
#include "../writers/rts2devcliimg.h"

#ifdef HAVE_PGSQL_SOAP
#include "../db/simbad/rts2simbadtarget.h"
#endif /* HAVE_PGSQL_SOAP */

#include "rts2nlayout.h"
#include "rts2daemonwindow.h"
#include "rts2ndevicewindow.h"
#include "rts2nmenu.h"
#include "rts2nmsgbox.h"
#include "rts2nmsgwindow.h"
#include "rts2nstatuswindow.h"
#include "rts2ncomwin.h"

// colors used in monitor
#define CLR_DEFAULT	1
#define CLR_OK		2
#define CLR_TEXT	3
#define CLR_PRIORITY	4
#define CLR_WARNING	5
#define CLR_FAILURE	6
#define CLR_STATUS      7
#define CLR_FITS	8
#define CLR_MENU	9
#define CLR_SCRIPT_CURRENT 10

#define K_ENTER		13
#define K_ESC		27

#define CMD_BUF_LEN    100

#define MENU_OFF	1
#define MENU_STANDBY	2
#define MENU_ON		3
#define MENU_EXIT	4
#define MENU_ABOUT	5

#define MENU_DEBUG_BASIC	11
#define MENU_DEBUG_LIMITED	12
#define MENU_DEBUG_FULL		13

enum messageAction
{ SWITCH_OFF, SWITCH_STANDBY, SWITCH_ON };

/**
 *
 * This class hold "root" window of display,
 * takes care about displaying it's connection etc..
 *
 */
class Rts2NMonitor:public Rts2Client
{
private:
  WINDOW * cursesWin;
  Rts2NLayout *masterLayout;
  Rts2NLayoutBlock *daemonLayout;
  Rts2NDevListWindow *deviceList;
  Rts2NWindow *daemonWindow;
  Rts2NMsgWindow *msgwindow;
  Rts2NComWin *comWindow;
  Rts2NMenu *menu;

  Rts2NMsgBox *msgBox;

  Rts2NStatusWindow *statusWindow;

  Rts2Command *oldCommand;

    std::list < Rts2NWindow * >windowStack;

  int cmd_col;
  char cmd_buf[CMD_BUF_LEN];

  int repaint ();

  messageAction msgAction;

  bool colorsOff;

  void messageBox (const char *query, messageAction action);
  void messageBoxEnd ();
  void menuPerform (int code);
  void leaveMenu ();

  void changeActive (Rts2NWindow * new_active);
  void changeListConnection ();

  int old_lines;
  int old_cols;

  Rts2NWindow *getActiveWindow ()
  {
    return *(--windowStack.end ());
  }

  void sendCommand ();

#ifdef HAVE_PGSQL_SOAP
  Rts2SimbadTarget *tarArg;
#endif /* HAVE_PGSQL_SOAP */

protected:
  virtual int processOption (int in_opt);
#ifdef HAVE_PGSQL_SOAP
  virtual int processArgs (const char *arg);
#endif /* HAVE_PGSQL_SOAP */

  virtual void addSelectSocks ();
  virtual void selectSuccess ();

  virtual Rts2ConnCentraldClient *createCentralConn ();

public:
  Rts2NMonitor (int argc, char **argv);
  virtual ~ Rts2NMonitor (void);

  virtual int init ();
  virtual int idle ();

  virtual Rts2ConnClient *createClientConnection (char *in_deviceName);
  virtual Rts2DevClient *createOtherType (Rts2Conn * conn,
					  int other_device_type);

  virtual int deleteConnection (Rts2Conn * conn);

  virtual void message (Rts2Message & msg);

  void resize ();

  void processKey (int key);

  void commandReturn (Rts2Command * cmd, int cmd_status);
};

/**
 * Make sure that update of connection state is notified in monitor.
 */
class Rts2NMonConn:public Rts2ConnClient
{
private:
  Rts2NMonitor * master;
public:
  Rts2NMonConn (Rts2NMonitor * in_master,
		char *in_name):Rts2ConnClient (in_master, in_name)
  {
    master = in_master;
  }

  virtual void commandReturn (Rts2Command * cmd, int in_status)
  {
    master->commandReturn (cmd, in_status);
    return Rts2ConnClient::commandReturn (cmd, in_status);
  }
};

/**
 * Make sure that command end is properly reflected
 */
class Rts2NMonCentralConn:public Rts2ConnCentraldClient
{
private:
  Rts2NMonitor * master;
public:
  Rts2NMonCentralConn (Rts2NMonitor * in_master,
		       const char *in_login,
		       const char *in_password,
		       const char *in_master_host,
		       const char
		       *in_master_port):Rts2ConnCentraldClient (in_master,
								in_login,
								in_password,
								in_master_host,
								in_master_port)
  {
    master = in_master;
  }

  virtual void commandReturn (Rts2Command * cmd, int in_status)
  {
    master->commandReturn (cmd, in_status);
    Rts2ConnCentraldClient::commandReturn (cmd, in_status);
  }
};

#endif /* !__RTS2_NMONITOR__ */
