/* 
 * Basic RTS2 devices and clients building block.
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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "rts2block.h"
#include "rts2command.h"
#include "rts2client.h"

#include "imghdr.h"

Rts2Block::Rts2Block (int in_argc, char **in_argv):
Rts2App (in_argc, in_argv)
{
	idle_timeout = USEC_SEC * 10;
	priority_client = -1;

	signal (SIGPIPE, SIG_IGN);

	masterState = SERVERD_OFF;
	// allocate ports dynamically
	port = 0;
}


Rts2Block::~Rts2Block (void)
{
	for (connections_t::iterator iter = connections.begin ();
		iter != connections.end ();)
	{
		Rts2Conn *conn = *iter;
		iter = connections.erase (iter);
		delete conn;
	}
	connections.clear ();
	blockAddress.clear ();
	blockUsers.clear ();
}


void
Rts2Block::setPort (int in_port)
{
	port = in_port;
}


int
Rts2Block::getPort (void)
{
	return port;
}


bool Rts2Block::commandQueEmpty ()
{
	for (connections_t::iterator iter = connectionBegin (); iter != connectionEnd (); iter++)
	{
		if (!(*iter)->queEmpty ())
			return false;
	}
	return true;
}


void
Rts2Block::postEvent (Rts2Event * event)
{
	// send to all connections
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
	{
		Rts2Conn *conn = *iter;
		conn->postEvent (new Rts2Event (event));
	}
	return Rts2App::postEvent (event);
}


Rts2Conn *
Rts2Block::createConnection (int in_sock)
{
	return new Rts2Conn (in_sock, this);
}


void
Rts2Block::addConnection (Rts2Conn * conn)
{
	connections.push_back (conn);
}


Rts2Conn *
Rts2Block::findName (const char *in_name)
{
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
	{
		Rts2Conn *conn = *iter;
		if (!strcmp (conn->getName (), in_name))
			return conn;
	}
	// if connection not found, try to look to list of available
	// connections
	return NULL;
}


Rts2Conn *
Rts2Block::findCentralId (int in_id)
{
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
	{
		Rts2Conn *conn = *iter;
		if (conn->getCentraldId () == in_id)
			return conn;
	}
	return NULL;
}


int
Rts2Block::sendAll (char *msg)
{
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
	{
		Rts2Conn *conn = *iter;
		conn->sendMsg (msg);
	}
	return 0;
}


void
Rts2Block::sendValueAll (char *val_name, char *value)
{
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
	{
		Rts2Conn *conn = *iter;
		conn->sendValue (val_name, value);
	}
}


void
Rts2Block::sendMessageAll (Rts2Message & msg)
{
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
	{
		Rts2Conn *conn = *iter;
		conn->sendMessage (msg);
	}
}


void
Rts2Block::sendStatusMessage (int state)
{
	char *msg;

	asprintf (&msg, PROTO_STATUS " %i", state);
	sendAll (msg);
	free (msg);
}


void
Rts2Block::sendStatusMessage (int state, Rts2Conn * conn)
{
	char *msg;

	asprintf (&msg, PROTO_STATUS " %i", state);
	conn->sendMsg (msg);
	free (msg);
}


void
Rts2Block::sendBopMessage (int state)
{
	char *msg;

	asprintf (&msg, PROTO_BOP_STATE " %i", state);
	sendAll (msg);
	free (msg);
}


void
Rts2Block::sendBopMessage (int state, Rts2Conn * conn)
{
	char *msg;

	asprintf (&msg, PROTO_BOP_STATE " %i", state);
	conn->sendMsg (msg);
	free (msg);
}


int
Rts2Block::idle ()
{
	int ret;
	Rts2Conn *conn;
	ret = waitpid (-1, NULL, WNOHANG);
	if (ret > 0)
	{
		childReturned (ret);
	}
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
	{
		conn = *iter;
		conn->idle ();
	}
	return 0;
}


void
Rts2Block::addSelectSocks ()
{
	Rts2Conn *conn;
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
	{
		conn = *iter;
		conn->add (&read_set);
	}
}


void
Rts2Block::selectSuccess ()
{
	Rts2Conn *conn;
	int ret;

	connections_t::iterator iter;

	for (iter = connections.begin (); iter != connections.end ();)
	{
		conn = *iter;
		if (conn->receive (&read_set) == -1)
		{
			#ifdef DEBUG_EXTRA
			logStream (MESSAGE_DEBUG) <<
				"Will delete connection " << " name: " << conn->
				getName () << sendLog;
			#endif
			ret = deleteConnection (conn);
			// delete connection only when it really requested to be deleted..
			if (!ret)
			{
				iter = connections.erase (iter);
				connectionRemoved (conn);
				delete conn;
			}
			else
			{
				iter++;
			}
		}
		else
		{
			iter++;
		}
	}
}


void
Rts2Block::setMessageMask (int new_mask)
{
	Rts2Conn *conn = getCentraldConn ();
	if (!conn)
		return;
	conn->queCommand (new Rts2CommandMessageMask (this, new_mask));
}


void
Rts2Block::oneRunLoop ()
{
	int ret;
	struct timeval read_tout;

	read_tout.tv_sec = idle_timeout / USEC_SEC;
	read_tout.tv_usec = idle_timeout % USEC_SEC;

	FD_ZERO (&read_set);
	FD_ZERO (&write_set);
	FD_ZERO (&exp_set);

	addSelectSocks ();
	ret = select (FD_SETSIZE, &read_set, &write_set, &exp_set, &read_tout);
	if (ret > 0)
	{
		selectSuccess ();
	}
	ret = idle ();
	if (ret == -1)
		endRunLoop ();
}


int
Rts2Block::deleteConnection (Rts2Conn * conn)
{
	if (conn->havePriority ())
	{
		cancelPriorityOperations ();
	}
	if (conn->isConnState (CONN_DELETE))
	{
		// try to look if there are any references to connection in other connections
		for (connections_t::iterator iter = connectionBegin (); iter != connectionEnd (); iter++)
		{
			if (conn != *iter)
				(*iter)->deleteConnection (conn);
		}
		return 0;
	}
	// don't delete us when we are in incorrect state
	return -1;
}


void
Rts2Block::connectionRemoved (Rts2Conn * conn)
{
}


int
Rts2Block::setPriorityClient (int in_priority_client, int timeout)
{
	int discard_priority = 1;
	Rts2Conn *priConn;
	priConn = findCentralId (in_priority_client);
	if (priConn && priConn->getHavePriority ())
		discard_priority = 0;

	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
	{
		Rts2Conn *conn = *iter;
		if (conn->getCentraldId () == in_priority_client)
		{
			if (discard_priority)
			{
				cancelPriorityOperations ();
				if (priConn)
					priConn->setHavePriority (0);
			}
			conn->setHavePriority (1);
			break;
		}
	}
	priority_client = in_priority_client;
	return 0;
}


void
Rts2Block::cancelPriorityOperations ()
{
}


void
Rts2Block::deviceReady (Rts2Conn * conn)
{
}


void
Rts2Block::priorityChanged (Rts2Conn * conn, bool have)
{
}


void
Rts2Block::deviceIdle (Rts2Conn * conn)
{
}


int
Rts2Block::changeMasterState (int new_state)
{
	// send message to all connections that they can possibly continue executing..
	for (connections_t::iterator iter = connectionBegin ();
		iter != connectionEnd (); iter++)
	{
		(*iter)->masterStateChanged ();
	}
	return 0;
}


void
Rts2Block::bopStateChanged ()
{
	// send message to all connections that they can possibly continue executing..
	for (connections_t::iterator iter = connectionBegin ();
		iter != connectionEnd (); iter++)
	{
		(*iter)->masterStateChanged ();
	}
}


int
Rts2Block::setMasterState (int new_state)
{
	int old_state = masterState;
	// change state NOW, before it will mess in processing routines
	masterState = new_state;
	if ((old_state & ~BOP_MASK) != (new_state & ~BOP_MASK))
	{
		// call changeMasterState only if something except BOP_MASK changed
		changeMasterState (new_state);
	}
	if ((old_state & BOP_MASK) != (new_state & BOP_MASK))
	{
		bopStateChanged ();
	}
	return 0;
}


void
Rts2Block::childReturned (pid_t child_pid)
{
	#ifdef DEBUG_EXTRA
	logStream (MESSAGE_DEBUG) << "child returned: " << child_pid << sendLog;
	#endif
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
	{
		Rts2Conn *conn = *iter;
		conn->childReturned (child_pid);
	}
}


int
Rts2Block::willConnect (Rts2Address * in_addr)
{
	return 0;
}


Rts2Address *
Rts2Block::findAddress (const char *blockName)
{
	std::list < Rts2Address * >::iterator addr_iter;

	for (addr_iter = blockAddress.begin (); addr_iter != blockAddress.end ();
		addr_iter++)
	{
		Rts2Address *addr = (*addr_iter);
		if (addr->isAddress (blockName))
		{
			return addr;
		}
	}
	return NULL;
}


void
Rts2Block::addAddress (const char *p_name, const char *p_host, int p_port,
int p_device_type)
{
	int ret;
	Rts2Address *an_addr;
	an_addr = findAddress (p_name);
	if (an_addr)
	{
		ret = an_addr->update (p_name, p_host, p_port, p_device_type);
		if (!ret)
		{
			addAddress (an_addr);
			return;
		}
	}
	an_addr = new Rts2Address (p_name, p_host, p_port, p_device_type);
	blockAddress.push_back (an_addr);
	addAddress (an_addr);
}


int
Rts2Block::addAddress (Rts2Address * in_addr)
{
	Rts2Conn *conn;
	// recheck all connections waiting for our address
	conn = getOpenConnection (in_addr->getName ());
	if (conn)
		conn->addressUpdated (in_addr);
	else if (willConnect (in_addr))
	{
		conn = createClientConnection (in_addr);
		if (conn)
			addConnection (conn);
	}
	return 0;
}


void
Rts2Block::deleteAddress (const char *p_name)
{
	std::list < Rts2Address * >::iterator addr_iter;

	for (addr_iter = blockAddress.begin (); addr_iter != blockAddress.end ();
		addr_iter++)
	{
		Rts2Address *addr = (*addr_iter);
		if (addr->isAddress (p_name))
		{
			blockAddress.erase (addr_iter);
			delete addr;
			return;
		}
	}
}


Rts2DevClient *
Rts2Block::createOtherType (Rts2Conn * conn, int other_device_type)
{
	switch (other_device_type)
	{
		case DEVICE_TYPE_MOUNT:
			return new Rts2DevClientTelescope (conn);
		case DEVICE_TYPE_CCD:
			return new Rts2DevClientCamera (conn);
		case DEVICE_TYPE_DOME:
			return new Rts2DevClientDome (conn);
		case DEVICE_TYPE_COPULA:
			return new Rts2DevClientCupola (conn);
		case DEVICE_TYPE_PHOT:
			return new Rts2DevClientPhot (conn);
		case DEVICE_TYPE_FW:
			return new Rts2DevClientFilter (conn);
		case DEVICE_TYPE_EXECUTOR:
			return new Rts2DevClientExecutor (conn);
		case DEVICE_TYPE_IMGPROC:
			return new Rts2DevClientImgproc (conn);
		case DEVICE_TYPE_SELECTOR:
			return new Rts2DevClientSelector (conn);
		case DEVICE_TYPE_GRB:
			return new Rts2DevClientGrb (conn);

		default:
			return new Rts2DevClient (conn);
	}
}


void
Rts2Block::addUser (int p_centraldId, int p_priority, char p_priority_have,
const char *p_login)
{
	int ret;
	std::list < Rts2User * >::iterator user_iter;
	for (user_iter = blockUsers.begin (); user_iter != blockUsers.end ();
		user_iter++)
	{
		ret =
			(*user_iter)->update (p_centraldId, p_priority, p_priority_have,
			p_login);
		if (!ret)
			return;
	}
	addUser (new Rts2User (p_centraldId, p_priority, p_priority_have, p_login));
}


int
Rts2Block::addUser (Rts2User * in_user)
{
	blockUsers.push_back (in_user);
	return 0;
}


Rts2Conn *
Rts2Block::getOpenConnection (const char *deviceName)
{
	connections_t::iterator iter;

	// try to find active connection..
	for (iter = connections.begin (); iter != connections.end (); iter++)
	{
		Rts2Conn *conn = *iter;
		if (conn->isName (deviceName))
			return conn;
	}
	return NULL;
}


Rts2Conn *
Rts2Block::getConnection (char *deviceName)
{
	Rts2Conn *conn;
	Rts2Address *devAddr;

	conn = getOpenConnection (deviceName);
	if (conn)
		return conn;

	devAddr = findAddress (deviceName);

	if (!devAddr)
	{
		#ifdef DEBUG_EXTRA
		logStream (MESSAGE_ERROR) <<
			"Cannot find device with name " << deviceName <<
			", creating new connection" << sendLog;
		#endif
		conn = createClientConnection (deviceName);
		if (conn)
			addConnection (conn);
		return conn;
	}

	// open connection to given address..
	conn = createClientConnection (devAddr);
	if (conn)
		addConnection (conn);
	return conn;
}


void
Rts2Block::message (Rts2Message & msg)
{
}


void
Rts2Block::clearAll ()
{
	connections_t::iterator iter;
	for (iter = connectionBegin (); iter != connectionEnd (); iter++)
		(*iter)->queClear ();
}


int
Rts2Block::queAll (Rts2Command * command)
{
	// go throught all adresses
	std::list < Rts2Address * >::iterator addr_iter;
	Rts2Conn *conn;

	for (addr_iter = blockAddress.begin (); addr_iter != blockAddress.end ();
		addr_iter++)
	{
		conn = getConnection ((*addr_iter)->getName ());
		if (conn)
		{
			Rts2Command *newCommand = new Rts2Command (command);
			conn->queCommand (newCommand);
		}
		else
		{
			#ifdef DEBUG_EXTRA
			logStream (MESSAGE_DEBUG) << "Rts2Block::queAll no connection for "
				<< (*addr_iter)->getName () << sendLog;
			#endif
		}
	}
	delete command;
	return 0;
}


int
Rts2Block::queAll (char *text)
{
	return queAll (new Rts2Command (this, text));
}


Rts2Conn *
Rts2Block::getMinConn (const char *valueName)
{
	int lovestValue = INT_MAX;
	Rts2Conn *minConn = NULL;
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
	{
		Rts2Value *que_size;
		Rts2Conn *conn = *iter;
		que_size = conn->getValue (valueName);
		if (que_size)
		{
			if (que_size->getValueInteger () >= 0
				&& que_size->getValueInteger () < lovestValue)
			{
				minConn = conn;
				lovestValue = que_size->getValueInteger ();
			}
		}
	}
	return minConn;
}


Rts2Value *
Rts2Block::getValue (const char *device_name, const char *value_name)
{
	Rts2Conn *conn = getOpenConnection (device_name);
	if (!conn)
		return NULL;
	return conn->getValue (value_name);
}


int
Rts2Block::statusInfo (Rts2Conn * conn)
{
	return 0;
}


bool
Rts2Block::commandPending (Rts2Command * cmd, Rts2Conn * exclude_conn)
{
	for (connections_t::iterator iter = connectionBegin ();
		iter != connectionEnd (); iter++)
	{
		if ((*iter) != exclude_conn && (*iter)->commandPending (cmd))
		{
			#ifdef DEBUG_EXTRA
			logStream (MESSAGE_DEBUG) << "command pending on " << (*iter)->
				getName () << sendLog;
			#endif				 /* DEBUG_EXTRA */
			return true;
		}
	}
	return false;
}


bool
Rts2Block::commandOriginatorPending (Rts2Object * object, Rts2Conn * exclude_conn)
{
	for (connections_t::iterator iter = connectionBegin ();
		iter != connectionEnd (); iter++)
	{
		if ((*iter) != exclude_conn && (*iter)->commandOriginatorPending (object))
		{
			#ifdef DEBUG_EXTRA
			std::cout << "command originator pending on " << (*iter)->getName () << std::endl;
			#endif				 /* DEBUG_EXTRA */
			return true;
		}
	}
	return false;
}
