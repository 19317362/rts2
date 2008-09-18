/* 
 * Central server state.
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

#include <sstream>
#include "status.h"

#include "rts2centralstate.h"

const char *
Rts2CentralState::getStringShort ()
{
	switch (getValue () & SERVERD_STATUS_MASK)
	{
		case SERVERD_DAY:
			return "day";
			break;
		case SERVERD_EVENING:
			return "evening";
		case SERVERD_DUSK:
			return "dusk";
		case SERVERD_NIGHT:
			return "night";
		case SERVERD_DAWN:
			return "dawn";
		case SERVERD_MORNING:
			return "morning";
	}
	return "unknow";
}


std::string Rts2CentralState::getString ()
{
	std::ostringstream os;
	if ((getValue () & SERVERD_STATUS_MASK) == SERVERD_HARD_OFF)
	{
		os << "HARD OFF";
		return os.str ();
        }
	if ((getValue () & SERVERD_STATUS_MASK) == SERVERD_SOFT_OFF)
	{
		os << "SOFT OFF";
		return os.str ();
	}
	if ((getValue () & SERVERD_STANDBY_MASK) == SERVERD_STANDBY)
	{
		os << "standby ";
	}
	else
	{
		os << "ready ";
	}
	os << getStringShort ();
	// check for blocking
	if (getValue () & BOP_EXPOSURE)
		os << ", block exposure";
	if (getValue () & BOP_READOUT)
		os << ", block readout";
	if (getValue () & BOP_TEL_MOVE)
		os << ", block telescope move";
	return os.str ();
}


std::ostream & operator << (std::ostream & _os, Rts2CentralState c_state)
{
	_os << c_state.getStringShort ();
	return _os;
}
