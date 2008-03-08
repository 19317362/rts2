/* 
 * Logger base.
 * Copyright (C) 2007-2008 Petr Kubanek <petr@kubanek.net>
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

#include "rts2loggerbase.h"

#include "../utils/rts2expander.h"

Rts2DevClientLogger::Rts2DevClientLogger (Rts2Conn * in_conn,
double in_numberSec,
std::list < std::string >
&in_logNames):
Rts2DevClient (in_conn)
{
	time (&nextInfoCall);
	numberSec = (time_t) in_numberSec;
	logNames = in_logNames;

	outputStream = &std::cout;
}


Rts2DevClientLogger::~Rts2DevClientLogger (void)
{
	if (outputStream != &std::cout)
		delete outputStream;
}


void
Rts2DevClientLogger::fillLogValues ()
{
	for (std::list < std::string >::iterator iter = logNames.begin ();
		iter != logNames.end (); iter++)
	{
		Rts2Value *val = getConnection ()->getValue ((*iter).c_str ());
		if (val)
		{
			logValues.push_back (val);
		}
		else
		{
			logStream (MESSAGE_ERROR) << "Cannot find value '" << *iter <<
				"' of device '" << getName () << "', exiting." << sendLog;
			getMaster ()->endRunLoop ();
		}
	}
}


void
Rts2DevClientLogger::setOutputFile (const char *filename)
{
	Rts2Expander exp = Rts2Expander ();
	std::ofstream * nstream =
		new std::ofstream (exp.expand (filename).c_str (), std::ios_base::app);
	if (nstream->fail ())
	{
		delete nstream;
		return;
	}
	if (outputStream != &std::cout)
		delete outputStream;

	outputStream = nstream;
}


void
Rts2DevClientLogger::infoOK ()
{
	if (logValues.empty ())
		fillLogValues ();
	*outputStream << getName ();
	for (std::list < Rts2Value * >::iterator iter = logValues.begin ();
		iter != logValues.end (); iter++)
	{
		*outputStream << " " << getDisplayValue (*iter);
	}
	*outputStream << std::endl;
}


void
Rts2DevClientLogger::infoFailed ()
{
	*outputStream << "info failed" << std::endl;
}


void
Rts2DevClientLogger::idle ()
{
	time_t now;
	time (&now);
	if (nextInfoCall < now)
	{
		queCommand (new Rts2CommandInfo (getMaster ()));
		nextInfoCall = now + numberSec;
	}
}


void
Rts2DevClientLogger::postEvent (Rts2Event * event)
{
	switch (event->getType ())
	{
		case EVENT_SET_LOGFILE:
			setOutputFile ((const char *) event->getArg ());
			break;
	}
	Rts2DevClient::postEvent (event);
}


Rts2LoggerBase::Rts2LoggerBase ()
{
}


int
Rts2LoggerBase::readDevices (std::istream & is)
{
	while (!is.eof () && !is.fail ())
	{
		std::string devName;
		double timeout;
		std::string values;
		std::list < std::string > valueList;
		valueList.clear ();

		is >> devName;
		if (is.eof ())
			return 0;

		is >> timeout;

		if (is.eof () || is.fail ())
		{
			logStream (MESSAGE_ERROR) <<
				"Cannot read device name or timeout, please correct line " <<
				sendLog;
			return -1;
		}
		getline (is, values);
		// split values..
		std::istringstream is2 (values);
		while (true)
		{
			std::string value;
			is2 >> value;
			if (is2.fail ())
				break;
			valueList.push_back (std::string (value));
		}
		if (valueList.empty ())
		{
			logStream (MESSAGE_ERROR) << "Value list for device " << devName <<
				" is empty, please correct that." << sendLog;
			return -1;
		}
		devicesNames.push_back (Rts2LogValName (devName, timeout, valueList));
	}
	return 0;
}


Rts2LogValName *
Rts2LoggerBase::getLogVal (const char *name)
{
	for (std::list < Rts2LogValName >::iterator iter = devicesNames.begin ();
		iter != devicesNames.end (); iter++)
	{
		if ((*iter).devName == name)
			return &(*iter);
	}
	return NULL;
}


int
Rts2LoggerBase::willConnect (Rts2Address * in_addr)
{
	if (getLogVal (in_addr->getName ()))
		return 1;
	return 0;
}


Rts2DevClient *
Rts2LoggerBase::createOtherType (Rts2Conn * conn, int other_device_type)
{
	Rts2LogValName *val = getLogVal (conn->getName ());
	if (val)
		return new Rts2DevClientLogger (conn, val->timeout, val->valueList);
	return NULL;
}
