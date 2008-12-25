/* 
 * Connection which does not sends out anything.
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

#include "rts2connnosend.h"

Rts2ConnNoSend::Rts2ConnNoSend (Rts2Block * in_master):Rts2Conn (in_master)
{
	setConnTimeout (-1);
}


Rts2ConnNoSend::Rts2ConnNoSend (int in_sock, Rts2Block * in_master):
Rts2Conn (in_sock, in_master)
{
	setConnTimeout (-1);
}


Rts2ConnNoSend::~Rts2ConnNoSend (void)
{
}


int
Rts2ConnNoSend::sendMsg (const char *msg)
{
	return 0;
}
