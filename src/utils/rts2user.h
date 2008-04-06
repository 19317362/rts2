/* 
 * Management class for connection users.
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

#ifndef __RTS2_CONN_USER__
#define __RTS2_CONN_USER__

#include "status.h"

/**
 * Holder class for users on Rts2Conn.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class Rts2ConnUser
{
	private:
		int centralId;
		int priority;
		char havePriority;
		char login[DEVICE_NAME_SIZE];
	public:
		Rts2ConnUser (int in_centralId, int in_priority, char in_priority_have,
			const char *in_login);
		virtual ~ Rts2ConnUser (void);
		int update (int in_centralId, int new_priority, char new_priority_have,
			const char *new_login);
};
#endif							 /* !__RTS2_CONN_USER__ */
