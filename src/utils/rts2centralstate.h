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


#ifndef __RTS2_CENTRALSTATE__
#define __RTS2_CENTRALSTATE__

#include <ostream>

#include "rts2serverstate.h"

/**
 * Class which represents state of the central daemon.
 *
 * @ingroup RTS2Block
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class Rts2CentralState:public Rts2ServerState
{
	private:

	public:
		Rts2CentralState (int in_state):Rts2ServerState ()
		{
			setValue (in_state);
		}

		const char *getStringShort ();
		std::string getString ();

		friend std::ostream & operator << (std::ostream & _os,
			Rts2CentralState c_state);
};

std::ostream & operator << (std::ostream & _os, Rts2CentralState c_state);
#endif							 /* !__RTS2_CENTRALSTATE__ */
