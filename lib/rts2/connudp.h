/* 
 * Infrastructure for UDP connection.
 * Copyright (C) 2010 Petr Kubanek <petr@kubanek.net>
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


#ifndef __RTS2_CONNUDP__
#define __RTS2_CONNUDP__

#include <sys/types.h>
#include <time.h>

#include "connnosend.h"

namespace rts2core
{

/**
 * UDP connection.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class ConnUDP:public ConnNoSend
{
	public:
		ConnUDP (int _port, rts2core::Block * _master, size_t _maxSize = 500);
		virtual int init ();
		virtual int receive (fd_set * set);
	protected:
		/**
		 * Process received data. Data are stored in buf member variable.
		 *
		 * @param len   received data length
		 * @param from  address of the source (originator) of data
		 */
		virtual int process (size_t len, struct sockaddr_in &from) = 0;
		virtual void connectionError (int last_data_size)
		{
			// do NOT call Connection::connectionError, UDP connection must run forewer.
			return;
		}
	private:
		size_t maxSize;
};

}

#endif // !__RTS2_CONNUDP__
