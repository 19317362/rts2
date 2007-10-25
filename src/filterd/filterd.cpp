/* 
 * Filter base class.
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

#include "filterd.h"

Rts2DevFilterd::Rts2DevFilterd (int in_argc, char **in_argv):
Rts2Device (in_argc, in_argv, DEVICE_TYPE_FW, "W0")
{
	createValue (filter, "filter", "used filter", false);

	addOption ('F', NULL, 1, "filter names, separated by space(s)");

	filterType = NULL;
	serialNumber = NULL;
}


Rts2DevFilterd::~Rts2DevFilterd (void)
{
}


int
Rts2DevFilterd::processOption (int in_opt)
{
	switch (in_opt)
	{
		case 'F':
			return setFilters (optarg);
		default:
			return Rts2Device::processOption (in_opt);
	}
	return 0;
}


int
Rts2DevFilterd::initValues ()
{
	addConstValue ("type", filterType);
	addConstValue ("serial", serialNumber);

	return Rts2Device::initValues ();
}


int
Rts2DevFilterd::info ()
{
	filter->setValueInteger (getFilterNum ());
	return Rts2Device::info ();
}


int
Rts2DevFilterd::getFilterNum ()
{
	return -1;
}


int
Rts2DevFilterd::setFilterNum (int new_filter)
{
	return -1;
}


int
Rts2DevFilterd::setValue (Rts2Value * old_value, Rts2Value * new_value)
{
	if (old_value == filter)
		return setFilterNumMask (new_value->getValueInteger ()) == 0 ? 0 : -2;
	return Rts2Device::setValue (old_value, new_value);
}


int
Rts2DevFilterd::homeFilter ()
{
	return -1;
}


int
Rts2DevFilterd::setFilters (char *filters)
{
	char *top;
	while (*filters)
	{
		// skip leading spaces
		while (*filters
			&& (*filters == ':' || *filters == '"' || *filters == '\''))
			filters++;
		if (!*filters)
			break;
		top = filters;
		// find filter string
		while (*top && *top != ':' && *top != '"' && *top != '\'')
			top++;
		// it's natural end, add and break..
		if (!*top)
		{
			if (top != filters)
				filter->addSelVal (filters);
			break;
		}
		*top = '\0';
		filter->addSelVal (filters);
		filters = top + 1;
	}
	if (filter->selSize () == 0)
		return -1;
	return 0;
}


int
Rts2DevFilterd::setFilterNumMask (int new_filter)
{
	int ret;
	maskState (FILTERD_MASK | BOP_EXPOSURE, FILTERD_MOVE | BOP_EXPOSURE,
		"filter move started");
	ret = setFilterNum (new_filter);
	infoAll ();
	if (ret == -1)
	{
		maskState (DEVICE_ERROR_MASK | FILTERD_MASK | BOP_EXPOSURE,
			DEVICE_ERROR_HW | FILTERD_IDLE | ~BOP_EXPOSURE);
		return ret;
	}
	maskState (FILTERD_MASK | BOP_EXPOSURE, FILTERD_IDLE);
	return ret;
}


int
Rts2DevFilterd::setFilterNum (Rts2Conn * conn, int new_filter)
{
	int ret;
	ret = setFilterNumMask (new_filter);
	if (ret == -1)
	{
		conn->sendCommandEnd (DEVDEM_E_HW, "filter set failed");
	}
	return ret;
}


int
Rts2DevFilterd::commandAuthorized (Rts2Conn * conn)
{
	if (conn->isCommand ("filter"))
	{
		int new_filter;
		if (conn->paramNextInteger (&new_filter) || !conn->paramEnd ())
			return -2;
		return setFilterNum (conn, new_filter);
	}
	else if (conn->isCommand ("home"))
	{
		if (!conn->paramEnd ())
			return -2;
		return homeFilter ();
	}
	else if (conn->isCommand ("help"))
	{
		conn->send ("ready - is filter ready?");
		conn->send ("info - information about camera");
		conn->send ("exit - exit from connection");
		conn->send ("help - print, what you are reading just now");
		return 0;
	}
	return Rts2Device::commandAuthorized (conn);
}
