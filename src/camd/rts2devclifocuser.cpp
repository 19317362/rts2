/* 
 * Client for focuser attached to camera.
 * Copyright (C) 2005-2008 Petr Kubanek <petr@kubanek.net>
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

#include "rts2devclifocuser.h"
#include "../utils/rts2command.h"

Rts2DevClientFocusCamera::Rts2DevClientFocusCamera (Rts2Conn * in_connection):Rts2DevClientFocus
(in_connection)
{
}


Rts2DevClientFocusCamera::~Rts2DevClientFocusCamera (void)
{
}


void
Rts2DevClientFocusCamera::postEvent (Rts2Event * event)
{
	struct focuserMove *fm;
	fm = (focuserMove *) event->getArg ();
	switch (event->getType ())
	{
		case EVENT_FOCUSER_SET:
		case EVENT_FOCUSER_STEP:
			if (!strcmp (getName (), fm->focuserName))
			{
				if (event->getType () == EVENT_FOCUSER_SET)
					connection->
						queCommand (new Rts2CommandSetFocus (this, fm->value));
				else
					connection->
						queCommand (new Rts2CommandChangeFocus (this, fm->value));
				// we process message
				fm->focuserName = NULL;
			}
			break;
		case EVENT_FOCUSER_GET:
			if (!strcmp (getName (), fm->focuserName))
			{
				fm->value = getConnection ()->getValueInteger ("FOC_POS");
				fm->focuserName = NULL;
			}
			break;
	}
	Rts2DevClientFocus::postEvent (event);
}


void
Rts2DevClientFocusCamera::focusingEnd ()
{
	getMaster ()->postEvent (new Rts2Event (EVENT_FOCUSER_END_MOVE));
	Rts2DevClientFocus::focusingEnd ();
}


void
Rts2DevClientFocusCamera::focusingFailed (int status)
{
	getMaster ()->postEvent (new Rts2Event (EVENT_FOCUSER_END_MOVE));
	Rts2DevClientFocus::focusingFailed (status);
}
