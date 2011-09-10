/* 
 * NCurses based monitoring
 * Copyright (C) 2003-2007 Petr Kubanek <petr@kubanek.net>
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

#include <libnova/libnova.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <list>

#include <iostream>
#include <fstream>

#include "nmonitor.h"
#include "rts2config.h"

#ifdef HAVE_XCURSES
char *XCursesProgramName = "rts2-mon";
#endif

using namespace rts2ncurses;

void NMonitor::sendCommand ()
{
	int curX = comWindow->getCurX ();
	char command[curX + 1];
	char *cmd_top = command;
	Rts2Conn *conn = NULL;
	comWindow->getWinString (command, curX);
	command[curX] = '\0';
	// try to find ., which show DEVICE.command notation..
	while (*cmd_top && !isspace (*cmd_top))
	{
		if (*cmd_top == '.')
		{
			*cmd_top = '\0';
			conn = getConnection (command);
			*cmd_top = '.';
			cmd_top++;
			break;
		}
		cmd_top++;
	}
	if (conn == NULL)
	{
		conn = connectionAt (deviceList->getSelRow ());
		cmd_top = command;
	}
	if (*cmd_top)
	{
		oldCommand = new rts2core::Rts2Command (this, cmd_top);
		conn->queCommand (oldCommand);
		comWindow->winclear ();
		comWindow->printCommand (command);
		wmove (comWindow->getWriteWindow (), 0, 0);
	}
}

class sortConnName
{
	public:
		bool operator () (Rts2Conn *con1, Rts2Conn *con2) const { return strcmp (con1->getName (), con2->getName ()) < 0; }
};

void NMonitor::refreshConnections ()
{
	orderedConn.clear ();
	orderedConn = *getConnections ();
	// look in ordered list
	switch (connOrder)
	{
		case ORDER_RTS2:
			break;
		case ORDER_ALPHA:
			std::sort (orderedConn.begin (), orderedConn.end (), sortConnName());
			break;
	}
	daemonWindow = NULL;
}

Rts2Conn *NMonitor::connectionAt (unsigned int i)
{
	if (i < getCentraldConns ()->size ())
		return (*getCentraldConns ())[i];
	i -= getCentraldConns ()->size ();
	if (orderedConn.size () == 0)
		refreshConnections ();
	if (i >= orderedConn.size ())
		return NULL;
	return orderedConn[i];
}


int NMonitor::processOption (int in_opt)
{
	switch (in_opt)
	{
		case 'c':
			colorsOff = true;
			break;
		default:
			return Rts2Client::processOption (in_opt);
	}
	return 0;
}

int NMonitor::processArgs (const char *arg)
{
#ifdef HAVE_PGSQL
	tarArg = new rts2db::SimbadTarget (arg);
	try
	{
		tarArg->load ();
	}
	catch (rts2core::Error err)
	{
		std::cerr << "cannot resolve target " << arg << ":" << err << std::endl;
		return -1;
	}

	std::cout << "Type Y and press enter.." << std::endl;

	char c;
	std::cin >> c;
	return 0;
#else
	return -1;
#endif
}

void NMonitor::addSelectSocks ()
{
	// add stdin for ncurses input
	FD_SET (1, &read_set);
	Rts2Client::addSelectSocks ();
}

Rts2ConnCentraldClient * NMonitor::createCentralConn ()
{
	return new NMonCentralConn (this, getCentralLogin (),
		getCentralPassword (), getCentralHost (),
		getCentralPort ());
}

void NMonitor::selectSuccess ()
{
	Rts2Client::selectSuccess ();
	while (1)
	{
		int input = getch ();
		if (input == ERR)
			break;
		processKey (input);
	}
}

void NMonitor::showHelp ()
{
	messageBox (
"WARNING:\n"
"If you would like to keep dome closed, please switch state to OFF.\n"
"==================================================================\n"
"Keys:\n"
"F1         .. this help\n"
"F2         .. switch to OFF (will not observe)\n"
"F3         .. switch to STANDBY (might observe if weather is good)\n"
"F4         .. switch to ON (will observe if weather is good)\n"
"F9         .. menu\n"
"F10,ctrl+c .. exit\n"
"arrow keys .. move between items\n"
"tab        .. move between windows\n"
"enter,F6   .. edit value");
}

void NMonitor::messageBox (const char *text)
{
	const static char *buts[] = { "OK" };
	if (msgBox)
		return;
	msgAction = NONE;
	msgBox = new NMsgBoxWin (text, buts, 1);
	msgBox->draw ();
	windowStack.push_back (msgBox);
}

void NMonitor::messageBox (const char *query, messageAction action)
{
	const static char *buts[] = { "Yes", "No" };
	if (msgBox)
		return;
	msgAction = action;
	msgBox = new NMsgBox (query, buts, 2);
	msgBox->draw ();
	windowStack.push_back (msgBox);
}

void NMonitor::messageBoxEnd ()
{
	if (msgBox->exitState == 0)
	{
		const char *cmd = "off";
		switch (msgAction)
		{
			case SWITCH_OFF:
				cmd = "off";
				break;
			case SWITCH_STANDBY:
				cmd = "standby";
				break;
			case SWITCH_ON:
				cmd = "on";
				break;
			default:
				cmd = NULL;
		}
		if (cmd)
		{
		  	queAllCentralds (cmd);
		}
	}
	delete msgBox;
	msgBox = NULL;
	windowStack.pop_back ();
}

void NMonitor::menuPerform (int code)
{
	switch (code)
	{
		case MENU_OFF:
			messageBox ("Are you sure to switch off?", SWITCH_OFF);
			break;
		case MENU_STANDBY:
			messageBox ("Are you sure to switch to standby?", SWITCH_STANDBY);
			break;
		case MENU_ON:
			messageBox ("Are you sure to switch to on?", SWITCH_ON);
			break;
		case MENU_ABOUT:
			messageBox ("   rts2-mon " PACKAGE_VERSION "\n"
"  (C) Petr Kubánek <petr@kubanek.net>");
			break;
		case MENU_MANUAL:
			showHelp ();
			break;
		case MENU_DEBUG_BASIC:
			msgwindow->setMsgMask (MESSAGE_CRITICAL | MESSAGE_ERROR | MESSAGE_WARNING);
			break;
		case MENU_DEBUG_LIMITED:
			msgwindow->setMsgMask (MESSAGE_CRITICAL | MESSAGE_ERROR | MESSAGE_WARNING | MESSAGE_INFO);
			break;
		case MENU_DEBUG_FULL:
			msgwindow->setMsgMask (MESSAGE_MASK_ALL);
			break;
		case MENU_EXIT:
			endRunLoop ();
			break;
		case MENU_SORT_ALPHA:
		case MENU_SORT_RTS2:
			switch (code)
			{
				case MENU_SORT_ALPHA:
					connOrder = ORDER_ALPHA;
					break;
				case MENU_SORT_RTS2:
					connOrder = ORDER_RTS2;
					break;
			}
			refreshConnections ();
			repaint ();
			break;
	}
}

void NMonitor::leaveMenu ()
{
	menu->leave ();
	windowStack.pop_back ();
}

void NMonitor::changeActive (NWindow * new_active)
{
	NWindow *activeWindow = *(--windowStack.end ());
	windowStack.pop_back ();
	activeWindow->leave ();
	windowStack.push_back (new_active);
	new_active->enter ();
}

void NMonitor::changeListConnection ()
{
	Rts2Conn *conn = connectionAt (deviceList->getSelRow ());
	if (conn)
	{
		delete daemonWindow;
		daemonWindow = NULL;
		// if connection is among centrald connections..
		connections_t::iterator iter;
		for (iter = getCentraldConns ()->begin (); iter != getCentraldConns ()->end (); iter++)
			if (conn == (*iter))
				daemonWindow = new NDeviceCentralWindow (conn);
		// otherwise it is normal device connection
		if (daemonWindow == NULL)
			daemonWindow = new NDeviceWindow (conn);
	}
	else
	{
	  	// and if the connetion does not exists, we should draw status overview
		delete daemonWindow;
		daemonWindow = new NCentraldWindow (this);
	}
	daemonLayout->setLayoutA (daemonWindow);
	resize ();
}

NMonitor::NMonitor (int in_argc, char **in_argv):Rts2Client (in_argc, in_argv)
{
	masterLayout = NULL;
	daemonLayout = NULL;
	statusWindow = NULL;
	deviceList = NULL;
	daemonWindow = NULL;
	comWindow = NULL;
	menu = NULL;
	msgwindow = NULL;
	msgBox = NULL;
	cmd_col = 0;

	oldCommand = NULL;

	colorsOff = false;

	old_lines = 0;
	old_cols = 0;

	connOrder = ORDER_ALPHA; //RTS2;

#ifdef HAVE_PGSQL
	tarArg = NULL;
#endif

	addOption ('c', NULL, 0, "don't use colors");

	char buf[HOST_NAME_MAX];

	gethostname (buf, HOST_NAME_MAX);

	std::ostringstream _os;
	_os << "rts2-mon@" << buf;

	setXtermTitle (_os.str ());
}

NMonitor::~NMonitor (void)
{
	erase ();
	refresh ();

	nocbreak ();
	echo ();
	endwin ();

	delete msgBox;
	delete statusWindow;

	delete masterLayout;

#ifdef HAVE_PGSQL
	delete tarArg;
#endif
}

int NMonitor::repaint ()
{
	curs_set (0);
	if (getConnections ()->size () != orderedConn.size ())
		refreshConnections ();
	if (LINES != old_lines || COLS != old_cols)
		resize ();
	deviceList->draw ();
	if (daemonWindow == NULL)
	{
		changeListConnection ();
	}
	daemonWindow->draw ();
	msgwindow->draw ();
	statusWindow->draw ();
	comWindow->draw ();
	menu->draw ();
	if (msgBox)
		msgBox->draw ();

	NWindow *activeWindow = getActiveWindow ();
	if (!activeWindow->setCursor ())
		comWindow->setCursor ();
	curs_set (1);
	doupdate ();
	return 0;
}

int NMonitor::init ()
{
	int ret;
	ret = Rts2Client::init ();
	if (ret)
		return ret;

	Rts2Config::instance ()->loadFile ();

	// init ncurses
	cursesWin = initscr ();
	if (!cursesWin)
	{
		std::cerr << "ncurses not supported (initscr call failed)!" << std::
			endl;
		return -1;
	}

	cbreak ();
	noecho ();
	nonl ();
	intrflush (stdscr, FALSE);
	keypad (stdscr, TRUE);
	timeout (0);

	ESCDELAY = 0;

	// create & init menu
	menu = new NMenu ();
	NSubmenu *sub = new NSubmenu ("System");
	sub->createAction ("Exit", MENU_EXIT);
	sub->createAction ("Alpha sort", MENU_SORT_ALPHA);
	sub->createAction ("RTS2 sort", MENU_SORT_RTS2);
	menu->addSubmenu (sub);

	sub = new NSubmenu ("States");
	sub->createAction ("Off", MENU_OFF);
	sub->createAction ("Standby", MENU_STANDBY);
	sub->createAction ("On", MENU_ON);
	menu->addSubmenu (sub);

	sub = new NSubmenu ("Debug");
	sub->createAction ("Basic", MENU_DEBUG_BASIC);
	sub->createAction ("Limited", MENU_DEBUG_LIMITED);
	sub->createAction ("Full", MENU_DEBUG_FULL);
	menu->addSubmenu (sub);

	sub = new NSubmenu ("Help");
	sub->createAction ("Manual", MENU_MANUAL);
	sub->createAction ("About", MENU_ABOUT);
	menu->addSubmenu (sub);

	// start color mode
	start_color ();
	use_default_colors ();

	if (has_colors () && !colorsOff)
	{
		init_pair (CLR_DEFAULT, -1, -1);
		init_pair (CLR_OK, COLOR_GREEN, -1);
		init_pair (CLR_TEXT, COLOR_BLUE, -1);
		init_pair (CLR_PRIORITY, COLOR_CYAN, -1);
		init_pair (CLR_WARNING, COLOR_YELLOW, -1);
		init_pair (CLR_FAILURE, COLOR_RED, -1);
		init_pair (CLR_STATUS, COLOR_RED, COLOR_CYAN);
		init_pair (CLR_FITS, COLOR_BLUE, -1);
		init_pair (CLR_MENU, COLOR_RED, COLOR_CYAN);
		init_pair (CLR_SCRIPT_CURRENT, COLOR_RED, COLOR_CYAN);
	}

	// init windows
	deviceList = new NDevListWindow (this, &orderedConn);
	comWindow = new NComWin ();
	msgwindow = new NMsgWindow ();
	windowStack.push_back (deviceList);
	deviceList->enter ();
	statusWindow = new NStatusWindow (comWindow, this);
	daemonWindow = new NDeviceCentralWindow (*(getCentraldConns ()->begin ()));

	// init layout
	daemonLayout = new LayoutBlockFixedB (daemonWindow, comWindow, false, 3);
	masterLayout = new LayoutBlock (deviceList, daemonLayout, true, 8);
	masterLayout = new LayoutBlock (masterLayout, msgwindow, false, 75);

	connections_t::iterator iter;
	for (iter = getCentraldConns ()->begin (); iter != getCentraldConns ()->end (); iter++)
		(*iter)->queCommand (new rts2core::Rts2Command (this, "info"));

	setMessageMask (MESSAGE_MASK_ALL);

	resize ();

	return repaint ();
}

int NMonitor::idle ()
{
	int ret = Rts2Client::idle ();
	repaint ();
	setTimeout (USEC_SEC);
	return ret;
}

Rts2ConnClient * NMonitor::createClientConnection (int _centrald_num, char *_deviceName)
{
	return new NMonConn (this, _centrald_num, _deviceName);
}

rts2core::Rts2DevClient * NMonitor::createOtherType (Rts2Conn * conn, int other_device_type)
{
	rts2core::Rts2DevClient *retC = Rts2Client::createOtherType (conn, other_device_type);
#ifdef HAVE_PGSQL
	if (other_device_type == DEVICE_TYPE_MOUNT && tarArg)
	{
		struct ln_equ_posn tarPos;
		tarArg->getPosition (&tarPos);
		conn->queCommand (new rts2core::Rts2CommandMove (this, (rts2core::Rts2DevClientTelescope *) retC, tarPos.ra, tarPos.dec));
	}
#endif
	return retC;
}

int NMonitor::deleteConnection (Rts2Conn * conn)
{
	if (conn == connectionAt (deviceList->getSelRow ()))
	{
		// that will trigger daemonWindow reregistration before repaint
		daemonWindow = NULL;
	}
	return Rts2Client::deleteConnection (conn);
}

void NMonitor::message (Rts2Message & msg)
{
	*msgwindow << msg;
}

void NMonitor::resize ()
{
	menu->resize (0, 0, COLS, 1);
	statusWindow->resize (0, LINES - 1, COLS, 1);
	masterLayout->resize (0, 1, COLS, LINES - 2);

	old_lines = LINES;
	old_cols = COLS;
}

void NMonitor::processKey (int key)
{
	NWindow *activeWindow = getActiveWindow ();
	keyRet ret = RKEY_HANDLED;
	switch (key)
	{
		case '\t':
		case KEY_STAB:
			if (activeWindow == deviceList)
			{
				changeActive (daemonWindow);
				activeWindow = NULL;
			}
			else if (activeWindow == daemonWindow)
			{
				if (daemonWindow->hasEditBox ())
				{
					ret = daemonWindow->injectKey (key);
				}
				else
				{
					changeActive (msgwindow);
					activeWindow = NULL;
				}
			}
			else if (activeWindow == msgwindow)
			{
				changeActive (deviceList);
				activeWindow = NULL;
			}
			break;
		case KEY_BTAB:
			if (activeWindow == deviceList)
			{
				changeActive (msgwindow);
				activeWindow = NULL;
			}
			else if (activeWindow == daemonWindow)
			{
				if (daemonWindow->hasEditBox ())
				{
					ret = daemonWindow->injectKey (key);
				}
				else
				{
					changeActive (deviceList);
					activeWindow = NULL;
				}
			}
			else if (activeWindow == msgwindow)
			{
				changeActive (daemonWindow);
				activeWindow = NULL;
			}
			break;
		case KEY_F (1):
			showHelp ();
			break;
		case KEY_F (2):
			messageBox ("Are you sure to switch off?", SWITCH_OFF);
			break;
		case KEY_F (3):
			messageBox ("Are you sure to switch to standby?", SWITCH_STANDBY);
			break;
		case KEY_F (4):
			messageBox ("Are you sure to switch to on?", SWITCH_ON);
			break;
		case KEY_F (5):
			queAll ("info");
			break;
		case KEY_F (8):
			doupdate ();
			break;
		case KEY_F (9):
			windowStack.push_back (menu);
			menu->enter ();
			break;
		case KEY_F (10):
			endRunLoop ();
			break;
		case KEY_RESIZE:
			resize ();
			break;
			// default for active window
		case KEY_ENTER:
		case K_ENTER:
			// preproccesed enter in case device window is selected..
			if (activeWindow == daemonWindow && comWindow->getCurX () != 0
				&& !daemonWindow->hasEditBox ())
			{
				ret = comWindow->injectKey (key);
				sendCommand ();
				break;
			}
		default:
			ret = activeWindow->injectKey (key);
			if (ret == RKEY_NOT_HANDLED)
			{
				ret = comWindow->injectKey (key);
				if (key == KEY_ENTER || key == K_ENTER)
					sendCommand ();
			}
	}
	// draw device values
	if (activeWindow == deviceList)
	{
		changeListConnection ();
	}
	// handle msg box
	if (activeWindow == msgBox && ret != RKEY_HANDLED)
	{
		messageBoxEnd ();
	}
	else if (activeWindow == menu && ret != RKEY_HANDLED)
	{
		NAction *action;
		action = menu->getSelAction ();
		if (action)
		{
			leaveMenu ();
			menuPerform (action->getCode ());
		}
		else
		{
			leaveMenu ();
		}
	}
}

void NMonitor::commandReturn (rts2core::Rts2Command * cmd, int cmd_status)
{
	if (oldCommand == cmd)
		comWindow->commandReturn (cmd, cmd_status);
}

int main (int argc, char **argv)
{
	NMonitor monitor = NMonitor (argc, argv);
	return monitor.run ();
}
