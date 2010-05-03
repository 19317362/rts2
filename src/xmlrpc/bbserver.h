/* 
 * Big Brother server connector.
 * Copyright (C) 2010 Petr Kubánek <petr@kubanek.net>
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

#ifndef __RTS2__BBSERVER__
#define __RTS2__BBSERVER__

#include "xmlrpc++/XmlRpcValue.h"
#include "xmlrpc++/XmlRpcClient.h"

#include <vector>

using namespace XmlRpc;

namespace rts2xmlrpc
{

/**
 * Big Brother server.
 *
 * @author Petr Kubánek <petr@kubanek.net>
 */
class BBServer
{
	public:
		BBServer (char *serverName, const char *observatoryName):_serverName (serverName), _observatoryName(observatoryName) {xmlClient = NULL; }
		~BBServer () {}

		/**
		 * Sends update message to BB server. Data part is specified in data parameter.
		 *
		 * @param data  data to send to BB server
		 */
		void sendUpdate (XmlRpcValue *data);

	private:
		std::string _serverName;
		std::string _observatoryName;
		XmlRpcClient* xmlClient;
};


class BBServers:public std::vector <BBServer>
{
	public:
		BBServers (): std::vector <BBServer> () {}

		void sendUpdate (XmlRpcValue *values);
};

}

#endif // !__RTS2__BBSERVER__
