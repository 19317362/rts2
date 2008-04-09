/* 
 * Interface to ask users among diferent options.
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

#include "rts2askchoice.h"

std::ostream & operator << (std::ostream & _os, Rts2Choice & choice)
{
	_os << "  " << choice.key << " .. " << choice.desc << std::endl;
	return _os;
}


Rts2AskChoice::Rts2AskChoice (Rts2App * in_app)
{
	app = in_app;
}


Rts2AskChoice::~Rts2AskChoice (void)
{
}


void
Rts2AskChoice::addChoice (char key, const char *desc)
{
	push_back (Rts2Choice (key, desc));
}


char
Rts2AskChoice::query (std::ostream & _os)
{
	char selection;
	for (Rts2AskChoice::iterator iter = begin ();
		iter != end (); iter++)
	{
		_os << (*iter);
	}
	if (app->askForChr ("Your selection", selection))
		return '\0';
	return selection;
}
