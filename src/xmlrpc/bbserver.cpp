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

#include "bbserver.h"

#include "hoststring.h"
#include "daemon.h"

using namespace rts2xmlrpc;

void BBServer::sendUpdate (XmlRpcd *server)
{
	if (client == NULL)
	{
		client = new XmlRpcClient ((char *) _serverApi.c_str (), &_uri);
	}

	std::ostringstream body;

	body << "DEVICE \n";

	char * reply;
	int reply_length;

	std::ostringstream url;
	url << "/api/observatory?observatory_id=" << _observatoryId;

	int ret = client->executeGetRequest (url.str ().c_str (), body.str ().c_str (), reply, reply_length);
	if (!ret)
	{
		logStream (MESSAGE_ERROR) << "Error requesting " << _serverApi.c_str () << sendLog;
		delete client;
		client = NULL;
		return;
	}

	free (reply);
}

void BBServers::sendUpdate (XmlRpcd *server)
{
	for (BBServers::iterator iter = begin (); iter != end (); iter++)
		iter->sendUpdate (server);
}
