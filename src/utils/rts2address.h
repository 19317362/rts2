/* 
 * Class which contains routines to resolve IP address from hostname.
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

#ifndef __RTS2_ADDRESS__
#define __RTS2_ADDRESS__

#include <string.h>

#include "status.h"
#include <config.h>

// CYGWIN workaround
#ifndef HAVE_GETADDRINFO
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "getaddrinfo.h"
#endif


/**
 * Class representing device TCP/IP address. It is ussed to hold device
 * informations obtained from central server.
 *
 * @ingroup RTS2Block
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class Rts2Address
{
	private:
		// our centrald number - centrald which created address record
		int host_num;
		// device centrald number
		int centrald_num;
		// device id
		int centrald_id;
		char name[DEVICE_NAME_SIZE];
		char *host;
		int port;
		int type;
	public:
		/**
		 * Construct Rts2Address object.
		 *
		 * @param _host_num      Number of centrald connection which crated the record
		 * @param _centrald_num  Number of centrald connection (usefull in scenarios with multiple centrald)
		 * @param _centrald_id   Device id as seen from centrald.
		 * @param _name          Block name.
		 * @param _host          Hostname of the host running the block.
		 * @param _port          Port on which block listen for incoming requests.
		 * @param _type          Block type.
		 */
		Rts2Address (int _host_num, int _centrald_num, int _centrald_id, const char *_name, const char *_host, int _port, int _type);
		virtual ~ Rts2Address (void);
		int update (int _centrald_num, const char *_name, const char *new_host, int new_port, int new_type);
		int getSockaddr (struct addrinfo **info);

		/**
		 * Return number of centrald connection which creates
		 * this address record.
		 */
		int getHostNum ()
		{
			return host_num;
		}

		/**
		 * Return number of device centrald registration.
		 */
		int getCentraldNum ()
		{
			return centrald_num;
		}

		/**
		 * Return centrald id of the device.
		 */
		int getCentraldId ()
		{
			return centrald_id;
		}

		/**
		 * Return block name.
		 *
		 * @return Block name.
		 */
		char *getName ()
		{
			return name;
		}

		/**
		 * Return block type.
		 *
		 * @return Block type.
		 */
		int getType ()
		{
			return type;
		}

		/**
		 * Check if the block is for given name.
		 *
		 * @return True if it is block address for given centrald number and name, false otherwise.
		 */
		bool isAddress (const char *_name)
		{
			return !strcmp (_name, name);
		}

		/**
		 * Check if the block is for given name.
		 *
		 * @return True if it is block address for given centrald number and name, false otherwise.
		 */
		bool isAddress (int _centrald_num, const char *_name)
		{
			return centrald_num == _centrald_num && !strcmp (_name, name);
		}
};
#endif							 /* !__RTS2_ADDRESS__ */
