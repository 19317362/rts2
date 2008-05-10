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

#ifndef __RTS2_CONN_NOSEND__
#define __RTS2_CONN_NOSEND__

#include "rts2block.h"
#include "rts2conn.h"

class Rts2Conn;

/**
 * Class which does not send out anything. This class have sendMsg method
 * disabled, so it does not sends out anything. It typycaly used for connections
 * running other applications with fork call.
 *
 * @ingroup RTS2Block
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class Rts2ConnNoSend:public Rts2Conn
{
	public:
		Rts2ConnNoSend (Rts2Block * in_master);
		Rts2ConnNoSend (int in_sock, Rts2Block * in_master);
		virtual ~ Rts2ConnNoSend (void);

		virtual int sendMsg (const char *msg);
};
#endif							 /* !__RTS2_CONN_NOSEND__ */
