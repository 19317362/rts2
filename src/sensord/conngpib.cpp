/* 
 * Connection for GPIB bus.
 * Copyright (C) 2007-2009 Petr Kubanek <petr@kubanek.net>
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

#include "conngpib.h"

#include "../utils/error.h"
#include "../utils/rts2app.h"

using namespace rts2sensord;

void ConnGpib::readInt (const char *buf, int &val)
{
	char rb[50];
	gpibWriteRead (buf,rb, 50);
	char *sep = strchr (rb, '\n');
	if (sep)
		*sep = '\0';
	val = atol (rb);
}

void ConnGpib::readValue (const char *buf, Rts2Value * val)
{
	switch (val->getValueType ())
	{
		case RTS2_VALUE_DOUBLE:
			readValue (buf, (Rts2ValueDouble *) val);
			return;
		case RTS2_VALUE_FLOAT:
			readValue (buf, (Rts2ValueFloat *) val);
			return;
		case RTS2_VALUE_BOOL:
			readValue (buf, (Rts2ValueBool *) val);
			return;
		case RTS2_VALUE_SELECTION:
			readValue (buf, (Rts2ValueSelection *) val);
			return;
		default:
			logStream (MESSAGE_ERROR) << "Do not know how to read value " << val->
				getName () << " of type " << val->getValueType () << sendLog;
			throw rts2core::Error ("unknow value type");
	}
}


void ConnGpib::readValue (const char *subsystem, std::list < Rts2Value * >&vals, int prefix_num)
{
	char rb[500];
	char *retTop;
	char *retEnd;
	std::list < Rts2Value * >::iterator iter;

	strcpy (rb, subsystem);

	for (iter = vals.begin (); iter != vals.end (); iter++)
	{
		if (iter != vals.begin ())
			strcat (rb, ";");
		strcat (rb, (*iter)->getName ().c_str () + prefix_num);
		strcat (rb, "?");
	}
	gpibWriteRead (rb, rb, 500);
	// spit reply and set values..
	retTop = rb;
	for (iter = vals.begin (); iter != vals.end (); iter++)
	{
		retEnd = retTop;
		if (*retEnd == '\0')
		{
			logStream (MESSAGE_ERROR) << "Cannot find reply for value " <<
				(*iter)->getName () << sendLog;
			throw rts2core::Error ("Cannot find reply");
		}
		while (*retEnd && *retEnd != ';')
		{
			if (*retEnd == ' ')
				*retEnd = '\0';
			retEnd++;
		}
		*retEnd = '\0';

		int ret = (*iter)->setValueString (retTop);
		if (ret)
		{
			logStream (MESSAGE_ERROR) << "Error when setting value " <<
				(*iter)->getName () << " to " << retTop << sendLog;
			throw rts2core::Error ("Error when setting value");
		}
		retTop = retEnd + 1;
	}
}


void ConnGpib::readValue (const char *buf, Rts2ValueString * val)
{
	char rb[200];
	gpibWriteRead (buf, rb, 200);
	char *sep = strchr (rb, '\n');
	if (sep)
		*sep = '\0';
	sep = strchr (rb, '\r');
	if (sep)
		*sep = '\0';
	val->setValueString (rb);
}


void ConnGpib::readValue (const char *buf, Rts2ValueDouble * val)
{
	char rb[50];
	gpibWriteRead (buf, rb, 50);
	val->setValueDouble (atof (rb));
}


void ConnGpib::readValue (const char *buf, Rts2ValueFloat * val)
{
	char rb[50];
	gpibWriteRead (buf, rb, 50);
	val->setValueFloat (atof (rb));
}


void ConnGpib::readValue (const char *buf, Rts2ValueBool * val)
{
	char rb[50];
	gpibWriteRead (buf, rb, 50);
	val->setValueBool (!strncmp (rb, "ON", 2));
}


void ConnGpib::readValue (const char *buf, Rts2ValueSelection * val)
{
	char rb[50];
	gpibWriteRead (buf, rb, 50);
	val->setSelIndex (rb);
}

char ConnGpib::getTimeoutTmo (float &_sec)
{
	if (_sec <= 1)
	{
		_sec = 1;
		return 11;
	}
	else if (_sec <= 3)
	{
		_sec = 3;
		return 12;
	}
	else if (_sec <= 10)
	{
		_sec = 10;
		return 13;
	}
	else if (_sec <= 30)
	{
		_sec = 30;
		return 14;
	}
	else if (_sec <= 100)
	{
		_sec = 100;
		return 15;
	}
	else if (_sec <= 300)
	{
		_sec = 300;
		return 16;
	}
	else if (_sec <= 1000)
	{
		_sec = 1000;
		return 17;
	}
	_sec = NAN;
	return 0;
}
