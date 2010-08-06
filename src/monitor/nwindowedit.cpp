/* 
 * Windows for edditing various variables.
 * Copyright (C) 2003-2008 Petr Kubanek <petr@kubanek.net>
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

#include "nmonitor.h"
#include "nwindowedit.h"

#include <ctype.h>

#define MIN(x,y) ((x < y) ? x : y)

NWindowEdit::NWindowEdit (int _x, int _y, int w, int h, int _ex, int _ey, int _ew, int _eh, bool border):NWindow (_x, _y, w, h, border)
{
	ex = _ex;
	ey = _ey;
	eh = _eh;
	ew = _ew;
	comwin = newpad (_eh, _ew);
}

NWindowEdit::~NWindowEdit (void)
{
	delwin (comwin);
}

bool NWindowEdit::passKey (int key)
{
	return isalnum (key) || isspace (key) || key == '.'  || key == ',';
}

keyRet NWindowEdit::injectKey (int key)
{
	switch (key)
	{
		case KEY_BACKSPACE:
		case 127:
			getyx (comwin, y, x);
			mvwdelch (comwin, y, x - 1);
			return RKEY_HANDLED;
		case KEY_EXIT:
		case K_ESC:
			return RKEY_ESC;
		case KEY_ENTER:
		case K_ENTER:
			return RKEY_ENTER;
		case KEY_LEFT:
			x = getCurX ();
			if (x > 0)
				wmove (getWriteWindow (), getCurY (), x - 1);
			break;
		case KEY_RIGHT:
			x = getCurX ();
			if (x < getWidth () - 3)
				wmove (getWriteWindow (), getCurY (), x + 1);
			break;
		default:
			if (isalnum (key) || isspace (key) || key == '+' || key == '-'
				|| key == '.' || key == ',')
			{
				if (passKey (key))
				{
					waddch (getWriteWindow (), key);
				}
				// alfanum key, and it does not passed..
				else
				{
					beep ();
					flash ();
				}
				return RKEY_HANDLED;
			}
			return NWindow::injectKey (key);
	}
	return RKEY_HANDLED;
}

void NWindowEdit::winrefresh ()
{
	int w, h;
	NWindow::winrefresh ();
	getbegyx (window, y, x);
	getmaxyx (window, h, w);
	// window coordinates
	int mwidth = x + w;
	int mheight = y + h;
	if (haveBox ())
	{
		mwidth -= 2;
		mheight -= 1;
	}

	if (pnoutrefresh (getWriteWindow (), 0, 0, y + ey, x + ex,
		MIN (y + ey + eh, mheight),
		MIN (x + ex + ew, mwidth)) == ERR)
	{
		errorMove ("pnoutrefresh comwin", y + ey, x + ey,
			MIN (y + ey + eh, mheight), MIN (x + ex + ew, mwidth));
	}
}

bool NWindowEdit::setCursor ()
{
	getbegyx (getWriteWindow (), y, x);
	x += getCurX ();
	y += getCurY ();
	setsyx (y, x);
	return true;
}

NWindowEditIntegers::NWindowEditIntegers (int _x, int _y, int w, int h, int _ex, int _ey, int _ew, int _eh, bool border):NWindowEdit (_x, _y, w, h, _ex, _ey, _ew, _eh, border)
{
}

bool NWindowEditIntegers::passKey (int key)
{
	if (isdigit (key) || key == '+' || key == '-')
		return true;
	return false;
}

int NWindowEditIntegers::getValueInteger ()
{
	char buf[200];
	char *endptr;
	mvwinnstr (getWriteWindow (), 0, 0, buf, 199);
	int tval = strtol (buf, &endptr, 10);
	if (*endptr != '\0' && *endptr != ' ')
	{
		// log error;
		return 0;
	}
	return tval;
}

NWindowEditDigits::NWindowEditDigits (int _x, int _y, int w, int h, int _ex, int _ey, int _ew, int _eh, bool border):NWindowEdit (_x, _y, w, h, _ex, _ey, _ew, _eh, border)
{
}

bool NWindowEditDigits::passKey (int key)
{
	if (isdigit (key) || key == '.' || key == ',' || key == '+' || key == '-')
		return true;
	return false;
}

double NWindowEditDigits::getValueDouble ()
{
	char buf[200];
	char *endptr;
	mvwinnstr (getWriteWindow (), 0, 0, buf, 199);
	double tval = strtod (buf, &endptr);
	if (*endptr != '\0' && *endptr != ' ')
	{
		// log error;
		return 0;
	}
	return tval;
}

NWindowEditBool::NWindowEditBool (int _x, int _y, int w, int h, int _ex, int _ey, int _ew, int _eh, bool border):NWindowEdit (_x, _y, w, h, _ex, _ey, _ew, _eh, border)
{
}

bool NWindowEditBool::passKey (int key)
{
	return false;
}

bool NWindowEditBool::getValueBool ()
{
	char buf[200];
	mvwinnstr (getWriteWindow (), 0, 0, buf, 199);
	return strncasecmp (buf, "true", 4) ? false : true;
}
