/* 
 * Connection class.
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

#include "rts2block.h"
#include "rts2centralstate.h"
#include "rts2command.h"

#ifdef DEBUG_ALL
#include <iostream>
#endif							 /* DEBUG_ALL */

#include <errno.h>
#include <syslog.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>

Rts2Conn::Rts2Conn (Rts2Block * in_master):Rts2Object ()
{
	buf = new char[MAX_DATA + 1];
	buf_size = MAX_DATA;

	sock = -1;
	master = in_master;
	buf_top = buf;
	full_data_end = NULL;

	*name = '\0';
	key = 0;
	priority = -1;
	have_priority = 0;
	centrald_id = -1;
	conn_state = CONN_UNKNOW;
	type = NOT_DEFINED_SERVER;
	runningCommand = NULL;
	runningCommandStatus = RETURNING;
	serverState = new Rts2ServerState ();
	bopState = new Rts2ServerState ();
	otherDevice = NULL;
	otherType = -1;

	connectionTimeout = 300;	 // 5 minutes timeout

	commandInProgress = false;
	info_time = NULL;
	last_info_time = 0;

	time (&lastGoodSend);
	lastData = lastGoodSend;
	lastSendReady = lastGoodSend - connectionTimeout;

	activeReadData = -1;
	dataConn = 0;
}


Rts2Conn::Rts2Conn (int in_sock, Rts2Block * in_master):Rts2Object ()
{
	buf = new char[MAX_DATA + 1];
	buf_size = MAX_DATA;

	sock = in_sock;
	master = in_master;
	buf_top = buf;
	full_data_end = NULL;

	*name = '\0';
	key = 0;
	priority = -1;
	have_priority = 0;
	centrald_id = -1;
	conn_state = CONN_CONNECTED;
	type = NOT_DEFINED_SERVER;
	runningCommand = NULL;
	runningCommandStatus = RETURNING;
	serverState = new Rts2ServerState ();
	bopState = new Rts2ServerState ();
	otherDevice = NULL;
	otherType = -1;

	connectionTimeout = 300;	 // 5 minutes timeout (150 + 150)

	commandInProgress = false;
	info_time = NULL;

	time (&lastGoodSend);
	lastData = lastGoodSend;
	lastSendReady = lastData - connectionTimeout;

	activeReadData = -1;
	dataConn = 0;
}


Rts2Conn::~Rts2Conn (void)
{
	if (sock >= 0)
		close (sock);
	delete serverState;
	delete bopState;
	delete otherDevice;
	queClear ();
	delete[]buf;
}


int
Rts2Conn::add (fd_set * readset, fd_set * writeset, fd_set * expset)
{
	if (sock >= 0)
	{
		FD_SET (sock, readset);
		if (isConnState (CONN_INPROGRESS))
			FD_SET (sock, writeset);
	}
	return 0;
}


std::string Rts2Conn::getCameraChipState (int chipN)
{
	int chip_state = (getRealState () & (CAM_MASK_CHIP << (chipN * 4))) >> (chipN * 4);
	std::ostringstream _os;
	if (chip_state == 0)
	{
		_os << chipN << " idle";
	}
	else
	{
		if (chip_state & CAM_EXPOSING)
		{
			_os << chipN << " EXPOSING";
		}
		if (chip_state & CAM_READING)
		{
			if (_os.str ().size ())
				_os << " | ";
			_os << chipN << " READING";
		}
		if (chip_state & CAM_FT)
		{
			if (_os.str ().size ())
				_os << " | ";
			_os << chipN << " FT";
		}
	}
	return _os.str ();
}


std::string Rts2Conn::getStateString ()
{
	std::ostringstream _os;
	int real_state = getRealState ();
	int chipN;
	switch (getOtherType ())
	{
		case DEVICE_TYPE_SERVERD:
			_os << Rts2CentralState (getState ()).getString ();
			break;
		case DEVICE_TYPE_MOUNT:
			switch (real_state & TEL_MASK_MOVING)
			{
				case TEL_OBSERVING:
					_os << "observing";
					break;
				case TEL_MOVING:
					_os << "MOVING";
					break;
				case TEL_PARKED:
					_os << "PARKED";
					break;
				case TEL_PARKING:
					_os << "PARKING";
					break;
				default:
					_os << "unknow state " << real_state;
			}
			if (real_state & TEL_CORRECTING)
				_os << " | CORRECTING";
			if (real_state & TEL_WAIT_COP)
				_os << " | WAIT_FOR_CUPOLA";
			if (real_state & TEL_SEARCH)
				_os << " | SEARCHING";
			if (real_state & TEL_GUIDE_NORTH)
				_os << " | GUIDE_NORTH";
			if (real_state & TEL_GUIDE_EAST)
				_os << " | GUIDE_EAST";
			if (real_state & TEL_GUIDE_SOUTH)
				_os << " | GUIDE_SOUTH";
			if (real_state & TEL_GUIDE_WEST)
				_os << " | GUIDE_WEST";
			if (real_state & TEL_NEED_STOP)
				_os << " | NEED_FLIP";
			break;
		case DEVICE_TYPE_CCD:
			chipN = getValueInteger ("chips");
			for (int i = 0; i < chipN; i++)
				_os << std::hex << getCameraChipState (i);
			if (real_state & CAM_FOCUSING)
				_os << " | FOCUSING";
			switch (real_state & CAM_MASK_SHUTTER)
			{
				case CAM_SHUT_CLEARED:
					_os << " | SHUTTER_CLEARED";
					break;
				case CAM_SHUT_SET:
					_os << " | SHUTTER_SET";
					break;
				case CAM_SHUT_TRANS:
					_os << " | SHUTTER_TRANS";
					break;
				default:
					_os << " | shutter unknow";
			}
			switch (real_state & CAM_MASK_COOLING)
			{
				case CAM_COOL_OFF:
					_os << " | COOLING_OFF ";
					break;
				case CAM_COOL_FAN:
					_os << " | COOLING_FAN ";
					break;
				case CAM_COOL_PWR:
					_os << " | COOLING_ON_SET_POWER ";
					break;
				case CAM_COOL_TEMP:
					_os << " | COOLING_ON_SET_TEMP ";
					break;
			}
			break;
		case DEVICE_TYPE_DOME:
		case DEVICE_TYPE_COPULA:
			switch (real_state & DOME_DOME_MASK)
			{
				case DOME_CLOSED:
					_os << "CLOSED";
					break;
				case DOME_OPENING:
					_os << "OPENING";
					break;
				case DOME_OPENED:
					_os << "OPENED";
					break;
				case DOME_CLOSING:
					_os << "CLOSING";
					break;
				default:
					_os << "UNKNOW";
			}
			switch (real_state & DOME_WEATHER_MASK)
			{
				case DOME_WEATHER_OK:
					_os << " | WEATHER OK";
					break;
				case DOME_WEATHER_BAD:
					_os << " | WEATHER BAD";
					break;
				default:
					_os << " | WEATHER STATE UNKNOW";
			}
			if (getOtherType () == DEVICE_TYPE_COPULA)
			{
				if (real_state & DOME_COP_MOVE)
					_os << " | CUPOLA_MOVING";
				else
					_os << " | cupola_idle";
				if (real_state & DOME_COP_SYNC)
					_os << " | cupola_synced";
				else
					_os << " | CUPOLA_NOT_SYNCED";
			}
			break;
		case DEVICE_TYPE_WEATHER:
			_os << "weather " << real_state;
			break;
		case DEVICE_TYPE_ARCH:
			_os << "arch " << real_state;
			break;
		case DEVICE_TYPE_PHOT:
			if (real_state & PHOT_INTEGRATE)
				_os << "INTEGRATING";
			else
				_os << "idle";
			if (real_state & PHOT_FILTER_MOVE)
				_os << " | FILTER_MOVING";
			break;
		case DEVICE_TYPE_PLAN:
			_os << "plan (not supported) ";
			break;
		case DEVICE_TYPE_GRB:
			_os << "grbd " << real_state;
			break;
		case DEVICE_TYPE_FOCUS:
			if (real_state & FOC_FOCUSING)
				_os << "CHANGING";
			else
				_os << "idle";
			break;
		case DEVICE_TYPE_MIRROR:
			switch (real_state & MIRROR_MASK)
			{
				case MIRROR_MOVE:
					_os << "MOVING";
					break;
				case MIRROR_NOTMOVE:
					_os << "idle";
					break;
				case MIRROR_A:
					_os << "A";
					break;
				case MIRROR_A_B:
					_os << "MOVING_A_B";
					break;
				case MIRROR_B:
					_os << "B";
					break;
				case MIRROR_B_A:
					_os << "MOVING_B_A";
					break;
				default:
					_os << "unknow";
			}
			break;
		case DEVICE_TYPE_FW:
			if (real_state & FILTERD_MOVE)
				_os << "MOVING";
			else
				_os << "idle";
			break;
		case DEVICE_TYPE_AUGERSH:
			_os << "augershooter " << real_state;
			break;
		case DEVICE_TYPE_EXECUTOR:
			switch (real_state & EXEC_STATE_MASK)
			{
				case EXEC_IDLE:
					_os << "idle";
					break;
				case EXEC_MOVE:
					_os << "MOVING TO NEXT TARGET";
					break;
				case EXEC_ACQUIRE:
					_os << "ACQUIRING";
					break;
				case EXEC_ACQUIRE_WAIT:
					_os << "ACQUIRE WAIT FOR PROCESSING";
					break;
				case EXEC_OBSERVE:
					_os << "OBSERVING";
					break;
				case EXEC_LASTREAD:
					_os << "OBSERVING IN LAST READ";
					break;
				default:
					_os << "UNKNOW";
			}
			if (real_state & EXEC_END)
				_os << " | WILL ENDS";
			else
				_os << " | not ending";
			switch (real_state & EXEC_MASK_ACQ)
			{
				case EXEC_NOT_ACQ:
					break;
				case EXEC_ACQ_OK:
					_os << " | ACQUSITION OK";
					break;
				case EXEC_ACQ_FAILED:
					_os << " | ACQUSITION FAILED";
					break;
				default:
					_os << " | UNKNOW ACQUSTION " << (real_state & EXEC_MASK_ACQ);
			}
			break;
		case DEVICE_TYPE_IMGPROC:
			if (real_state & IMGPROC_RUN)
				_os << "PROCESS RUNNING";
			else
				_os << "idle";
			break;
		case DEVICE_TYPE_SELECTOR:
			_os << "selector " << real_state;
			break;
		case DEVICE_TYPE_SOAP:
			_os << "soapd " << real_state;
			break;
		case DEVICE_TYPE_SENSOR:
			_os << "sensor " << real_state;
			break;
		case DEVICE_TYPE_LOGD:
			_os << "logd " << real_state;
			break;
		default:
			_os << "UNKNOW DEVICE " << getOtherType () << " " << real_state;
	}
	if (getState () & DEVICE_ERROR_KILL)
		_os << " | PRIORITY CHANGED";
	if (getState () & DEVICE_ERROR_HW)
		_os << " | HW ERROR";
	if (getState () & DEVICE_NOT_READY)
		_os << " | NOT READY";

	// block states and full BOP states
	if (getState () & BOP_EXPOSURE)
		_os << " | BLOCK EXPOSURE";
	else if (getFullBopState () & BOP_EXPOSURE)
		_os << " # BLOCK EXPOSING";

	if (getState () & BOP_READOUT)
		_os << " | BLOCK READOUT";
	else if (getFullBopState () & BOP_READOUT)
		_os << " # BLOCK READOUT";

	if (getState () & BOP_TEL_MOVE)
		_os << " | BLOCK TELESCOPE MOVEMENT";
	else if (getFullBopState () & BOP_TEL_MOVE)
		_os << " # BLOCK TELESCOPE MOVEMENT";

	return _os.str ();
}


void
Rts2Conn::postEvent (Rts2Event * event)
{
	if (otherDevice)
		otherDevice->postEvent (new Rts2Event (event));
	Rts2Object::postEvent (event);
}


void
Rts2Conn::postMaster (Rts2Event * event)
{
	master->postEvent (event);
}


int
Rts2Conn::idle ()
{
	int ret;
	if (connectionTimeout > 0)
	{
		time_t now;
		time (&now);
		if (now > lastData + getConnTimeout ()
			&& now > lastSendReady + getConnTimeout () / 4)
		{
			ret = sendMsg (PROTO_TECHNICAL " ready");
			#ifdef DEBUG_EXTRA
			std::cout << "Send T ready ret: " << ret <<
				" name: " << getName () << " type: " << type << std::endl;
			#endif				 /* DEBUG_EXTRA */
			time (&lastSendReady);
		}
		if (now > (lastData + getConnTimeout () * 2))
		{
			logStream (MESSAGE_DEBUG) << "Connection timeout: " << lastGoodSend
				<< " " << lastData << " " << now << " " << getName () << " " <<
				type << sendLog;
			connectionError (-1);
		}
	}
	if (otherDevice != NULL)
		otherDevice->idle ();
	return 0;
}


int
Rts2Conn::authorizationOK ()
{
	logStream (MESSAGE_ERROR) << "authorization called on wrong connection" <<
		sendLog;
	return -1;
}


int
Rts2Conn::authorizationFailed ()
{
	logStream (MESSAGE_ERROR) << "authorization failed on wrong connection" <<
		sendLog;
	return -1;
}


int
Rts2Conn::acceptConn ()
{
	int new_sock;
	struct sockaddr_in other_side;
	socklen_t addr_size = sizeof (struct sockaddr_in);
	new_sock = accept (sock, (struct sockaddr *) &other_side, &addr_size);
	if (new_sock == -1)
	{
		logStream (MESSAGE_ERROR) << "Rts2Conn::acceptConn data accept " <<
			strerror (errno) << sendLog;
		return -1;
	}
	else
	{
		close (sock);
		sock = new_sock;
		#ifdef DEBUG_EXTRA
		logStream (MESSAGE_DEBUG) << "Rts2Conn::acceptConn connection accepted"
			<< sendLog;
		#endif
		setConnState (CONN_CONNECTED);
		return 0;
	}
}


void
Rts2Conn::setState (int in_value)
{
	serverState->setValue (in_value);
	if (otherDevice)
		otherDevice->stateChanged (serverState);
	if (serverState->maskValueChanged (DEVICE_NOT_READY))
	{
		if ((serverState->getValue () & DEVICE_NOT_READY) == 0)
			master->deviceReady (this);
	}
	else if (serverState->maskValueChanged (DEVICE_STATUS_MASK)
		&& (serverState->getValue () & DEVICE_STATUS_MASK) == DEVICE_IDLE)
	{
		master->deviceIdle (this);
	}
}


void
Rts2Conn::setBopState (int in_value)
{
	bopState->setValue (in_value);
	if (runningCommand && runningCommand->getStatusCallProgress () == CIP_RUN)
		sendCommand ();
}


void
Rts2Conn::setOtherType (int other_device_type)
{
	delete otherDevice;
	otherDevice = master->createOtherType (this, other_device_type);
	otherType = other_device_type;
}


int
Rts2Conn::getOtherType ()
{
	if (otherDevice)
		return otherType;
	return -1;
}


void
Rts2Conn::updateStatusWait (Rts2Conn * conn)
{
	if (runningCommand && runningCommand->getStatusCallProgress () == CIP_WAIT)
		sendCommand ();
}


void
Rts2Conn::masterStateChanged ()
{
	if (runningCommand && runningCommand->getStatusCallProgress () == CIP_RUN)
		sendCommand ();
}


int
Rts2Conn::processLine ()
{
	// starting at command_start, we have complete line, which was
	// received
	int ret;

	// find command parameters end
	command_buf_top = command_start;

	while (*command_buf_top && !isspace (*command_buf_top))
		command_buf_top++;
	// mark command end..
	if (*command_buf_top)
	{
		*command_buf_top = '\0';
		command_buf_top++;
	}
	// priority change
	if (isCommand (PROTO_PRIORITY))
	{
		ret = priorityChange ();
	}
	else if (isCommand (PROTO_PRIORITY_INFO))
	{
		ret = priorityInfo ();
	}
	// status
	else if (isCommand (PROTO_STATUS))
	{
		ret = status ();
	}
	// bop status update
	else if (isCommand (PROTO_BOP_STATE))
	{
		ret = bopStatus ();
	}
	// message from application
	else if (isCommand (PROTO_MESSAGE))
	{
		ret = message ();
	}
	// technical - to keep connection working
	else if (isCommand (PROTO_TECHNICAL))
	{
		char *msg;
		if (paramNextString (&msg) || !paramEnd ())
			return -1;
		if (!strcmp (msg, "ready"))
		{
			#ifdef DEBUG_EXTRA
			std::cout << "Send T OK" << std::endl;
			#endif
			sendMsg (PROTO_TECHNICAL " OK");
			return -1;
		}
		if (!strcmp (msg, "OK"))
		{
			return -1;
		}
		return -2;
	}
	// metainfo with values
	else if (isCommand (PROTO_METAINFO))
	{
		int m_type;
		char *m_name;
		char *m_descr;
		if (paramNextInteger (&m_type)
			|| paramNextString (&m_name)
			|| paramNextString (&m_descr) || !paramEnd ())
			return -2;
		return metaInfo (m_type, std::string (m_name), std::string (m_descr));
	}
	else if (isCommand (PROTO_VALUE))
	{
		char *m_name;
		if (paramNextString (&m_name))
			return -2;
		return commandValue (m_name);
	}
	else if (isCommand (PROTO_SELMETAINFO))
	{
		char *m_name;
		char *sel_name;
		if (paramNextString (&m_name)
			|| paramNextString (&sel_name) || !paramEnd ())
			return -2;
		return selMetaInfo (m_name, sel_name);
	}
	else if (isCommand (PROTO_SET_VALUE))
	{
		ret = master->setValue (this, false);
	}
	else if (isCommand (PROTO_SET_VALUE_DEF))
	{
		ret = master->setValue (this, true);
	}
	else if (isCommand (PROTO_BINARY))
	{
		int data_conn, data_type;
		long data_size;
		// we expect binary data
		if (paramNextInteger (&data_conn)
			|| paramNextLong (&data_size)
			|| paramNextInteger (&data_type)
			|| !paramEnd ())
		{
			// end connection - we cannot process this command
			activeReadData = -1;
			connectionError (-2);
			return -1;
		}

		readData[data_conn] = new Rts2DataRead (data_size, data_type);
		newDataConn (data_conn);
		return -1;
	}
	else if (isCommand (PROTO_DATA))
	{
		if (paramNextInteger (&activeReadData)
			|| readData[activeReadData]->readDataSize (this)
			|| !paramEnd ())
		{
			// end connection - bad binary data header
			activeReadData = -1;
			connectionError (-2);
			return -1;
		}
		return -1;
	}
	else if (isCommandReturn ())
	{
		ret = commandReturn ();
	}
	else
	{
		setCommandInProgress (true);
		ret = command ();
	}
	#ifdef DEBUG_ALL
	std::cerr << "Rts2Conn::processLine [" << getCentraldId ()
		<< "] command: " << getCommand () << " ret: " << ret << std::endl;
	#endif
	if (!ret)
		sendCommandEnd (DEVDEM_OK, "OK");
	else if (ret == -2)
	{
		//      logStream (MESSAGE_DEBUG) << "Rts2Conn::processLine [" <<
		//      getCentraldId () << "] command: " << getCommand () << " ret: " << ret
		//      << sendLog;
		sendCommandEnd (DEVDEM_E_COMMAND,
			"invalid parameters/invalid number of parameters");
	}
	return ret;
}


void
Rts2Conn::processBuffer ()
{
	if (full_data_end)
		return;
	full_data_end = buf_top;
	buf_top = buf;
	command_start = buf;
	while (*buf_top)
	{
		while (isspace (*buf_top) || (*buf_top && *buf_top == '\n'))
			buf_top++;
		command_start = buf_top;
		// find command end..
		while (*buf_top && *buf_top != '\n' && *buf_top != '\r')
			buf_top++;

		if (*buf_top == '\r' && *(buf_top + 1) == '\n')
		{
			*buf_top = '\0';
			buf_top++;
		}
		// weird error on when we get \r in one and \n in next read
		if (*buf_top == '\r' && !*(buf_top + 1))
		{
			// we get to 0, while will ends, and we get out, waiting for \n in next packet
			buf_top++;
			break;
		}

		if (*buf_top == '\n')
		{
			// mark end of line..
			*buf_top = '\0';
			buf_top++;
			processLine ();
			// binary read just started
			if (activeReadData >= 0)
			{
				long readSize = full_data_end - buf_top;
				readSize = readData[activeReadData]->addData (buf_top, readSize);
				dataReceived ();
				// move binary data away
				memmove (buf_top, buf_top + readSize, (full_data_end - buf_top) - readSize + 1);
				full_data_end -= readSize;
			}
			command_start = buf_top;
		}
	}
	if (buf != command_start)
	{
		memmove (buf, command_start, (full_data_end - command_start) + 1);
		// move buffer to the end..
		buf_top -= command_start - buf;
	}
	full_data_end = NULL;
}


int
Rts2Conn::receive (fd_set * readset)
{
	int data_size = 0;
	// connections market for deletion
	if (isConnState (CONN_DELETE))
		return -1;
	if ((sock >= 0) && FD_ISSET (sock, readset))
	{
		if (isConnState (CONN_CONNECTING))
		{
			return acceptConn ();
		}
		// we are receiving binary data
		if (activeReadData >= 0)
		{
			data_size = readData[activeReadData]->getData (sock);
			if (data_size == -1)
			{
				connectionError (data_size);
			}
			if (data_size == 0)
				return 0;
			dataReceived ();
			return data_size;
		}
		// increase buffer if it's too small
		if (((int) buf_size) == (buf_top - buf))
		{
			char *new_buf = new char[buf_size + MAX_DATA + 1];
			memcpy (new_buf, buf, buf_size);
			buf_top = new_buf + (buf_top - buf);
			buf_size += MAX_DATA;
			delete[]buf;
			buf = new_buf;
			// read us again..
		}
		data_size = read (sock, buf_top, buf_size - (buf_top - buf));
		// ignore EINTR
		if (data_size == -1 && errno == EINTR)
			return 0;
		if (data_size <= 0)
		{
			connectionError (data_size);
			return -1;
		}
		buf_top[data_size] = '\0';
		successfullRead ();
		#ifdef DEBUG_ALL
		std::cout << "Rts2Conn::receive name " << getName ()
			<< " reas: " << buf_top
			<< " full_buf: " << buf
			<< " size: " << data_size
			<< " commandInProgress " << commandInProgress
			<< " runningCommand " << runningCommand
			<< std::endl;
		#endif
		// move buf_top to end of readed data
		buf_top += data_size;
		// don't do anything else if command is in progress
		//if (commandInProgress)
		//{
		//	return data_size;
		//}
		// put old data size into account..
		data_size += buf_top - buf;
		processBuffer ();
	}
	return data_size;
}


int
Rts2Conn::writable (fd_set * writeset)
{
	if (sock >=0 && FD_ISSET (sock, writeset) && isConnState (CONN_INPROGRESS))
	{
		int err = 0;
		int ret;
		socklen_t len = sizeof (err);

		ret = getsockopt (sock, SOL_SOCKET, SO_ERROR, &err, &len);
		if (ret)
		{
			logStream (MESSAGE_ERROR) << "Rts2Conn::idle getsockopt " <<
				strerror (errno) << sendLog;
			connectionError (-1);
		}
		else if (err)
		{
			logStream (MESSAGE_ERROR) << "Rts2Conn::idle getsockopt " <<
				strerror (errno) << sendLog;
			connectionError (-1);
		}
		else
		{
			connConnected ();
		}
	}
	return 0;
}


int
Rts2Conn::getOurAddress (struct sockaddr_in *in_addr)
{
	// get our address and pass it to data conn
	socklen_t size;
	size = sizeof (struct sockaddr_in);

	return getsockname (sock, (struct sockaddr *) in_addr, &size);
}


void
Rts2Conn::setAddress (struct in_addr *in_address)
{
	addr = *in_address;
}


void
Rts2Conn::getAddress (char *addrBuf, int in_buf_size)
{
	char *addr_s;
	addr_s = inet_ntoa (addr);
	strncpy (addrBuf, addr_s, in_buf_size);
	addrBuf[in_buf_size - 1] = '0';
}


int
Rts2Conn::havePriority ()
{
	return have_priority;
}


void
Rts2Conn::setHavePriority (int in_have_priority)
{
	if (in_have_priority)
		sendMsg (PROTO_PRIORITY_INFO " 1");
	else
		sendMsg (PROTO_PRIORITY_INFO " 0");
	have_priority = in_have_priority;
};

void
Rts2Conn::setCentraldId (int in_centrald_id)
{
	centrald_id = in_centrald_id;
	master->checkPriority (this);
}


int
Rts2Conn::sendPriorityInfo ()
{
	char *msg;
	int ret;
	asprintf (&msg, PROTO_PRIORITY_INFO " %i", havePriority ());
	ret = sendMsg (msg);
	free (msg);
	return ret;
}


void
Rts2Conn::queCommand (Rts2Command * cmd, int notBop, Rts2Object * originator)
{
	cmd->setBopMask (notBop);
	cmd->setOriginator (originator);
	queCommand (cmd);
}


void
Rts2Conn::queCommand (Rts2Command * cmd)
{
	cmd->setConnection (this);
	if (runningCommand
		|| isConnState (CONN_CONNECTING)
		|| isConnState (CONN_INPROGRESS)
		|| isConnState (CONN_AUTH_PENDING) || isConnState (CONN_UNKNOW))
	{
		commandQue.push_back (cmd);
		return;
	}
	runningCommand = cmd;
	sendCommand ();
}


void
Rts2Conn::queSend (Rts2Command * cmd)
{
	cmd->setConnection (this);
	if (isConnState (CONN_CONNECTING)
		|| isConnState (CONN_INPROGRESS)
		|| isConnState (CONN_UNKNOW))
	{
		commandQue.push_front (cmd);
		return;
	}
	if (runningCommand)
	{
		// when the actual command was sended or if the actual command
		// is returning, push us to fron of que to be executed as first
		// command
		if (runningCommandStatus == SEND || runningCommandStatus == RETURNING)
		{
			commandQue.push_front (cmd);
			return;
		}
		commandQue.push_front (runningCommand);
	}
	runningCommand = cmd;
	sendCommand ();
}


void
Rts2Conn::commandReturn (Rts2Command * cmd, int in_status)
{
	if (otherDevice)
		otherDevice->commandReturn (cmd, in_status);
}


bool Rts2Conn::queEmptyForOriginator (Rts2Object *testOriginator)
{
	if (runningCommand && runningCommandStatus != RETURNING && runningCommand->isOriginator (testOriginator))
		return false;
	for (std::list <Rts2Command *>::iterator iter = commandQue.begin (); iter != commandQue.end (); iter++)
	{
		if ((*iter)->isOriginator (testOriginator))
			return false;
	}
	return true;
}


void
Rts2Conn::queClear ()
{
	if (runningCommand && runningCommandStatus != SEND)
	{
		delete runningCommand;
		runningCommand = NULL;
	}
	std::list < Rts2Command * >::iterator que_iter;
	for (que_iter = commandQue.begin (); que_iter != commandQue.end ();
		que_iter++)
	{
		Rts2Command *cmd;
		cmd = (*que_iter);
		delete cmd;
	}
	commandQue.clear ();
}


void
Rts2Conn::deleteConnection (Rts2Conn *conn)
{
	// look if connection isn't entered among originators or controll connections..
	if (runningCommand)
		runningCommand->deleteConnection (conn);
	for (std::list < Rts2Command * >::iterator iter = commandQue.begin ();
		iter != commandQue.end (); iter++)
	{
		(*iter)->deleteConnection (conn);
	}
}


// high-level commands, used to pass variables etc..
int
Rts2Conn::command ()
{
	if (isCommand ("device"))
	{
		int p_centrald_num;
		int p_centraldId;
		char *p_name;
		char *p_host;
		int p_port;
		int p_device_type;
		if (paramNextInteger (&p_centrald_num)
		  	|| paramNextInteger (&p_centraldId)
			|| paramNextString (&p_name)
			|| paramNextString (&p_host)
			|| paramNextInteger (&p_port)
			|| paramNextInteger (&p_device_type)
			|| !paramEnd ())
			return -2;
		master->addAddress (p_centrald_num, p_name, p_host, p_port, p_device_type);
		setCommandInProgress (false);
		return -1;
	}
	else if (isCommand ("user"))
	{
		int p_centraldId;
		int p_priority;
		char *p_priority_have;
		char *p_login;
		if (paramNextInteger (&p_centraldId)
			|| paramNextInteger (&p_priority)
			|| paramNextString (&p_priority_have)
			|| paramNextString (&p_login)
			|| !paramEnd ())
			return -2;
		master->addUser (p_centraldId, p_priority, (*p_priority_have == '*'),
			p_login);
		setCommandInProgress (false);
		return -1;
	}
	else if (isCommand ("status_info"))
	{
		if (!paramEnd ())
			return -2;
		return master->statusInfo (this);
	}
	// don't respond to values with error - otherDevice does respond to
	// values, if there is no other device, we have to take resposibility
	// as it can fails (V without value), not with else
	if (isCommand (PROTO_VALUE))
		return -1;
	logStream (MESSAGE_DEBUG) <<
		"Rts2Conn::command unknow command: getCommand " << getCommand () <<
		" state: " << conn_state << " type: " << getType () << " name: " <<
		getName () << sendLog;
	sendCommandEnd (-4, "Unknow command");
	return -4;
}


int
Rts2Conn::status ()
{
	int value;
	if (paramNextInteger (&value) || !paramEnd ())
		return -2;
	setState (value);
	return -1;
}


int
Rts2Conn::bopStatus ()
{
	int value;
	if (paramNextInteger (&value) || !paramEnd ())
		return -2;
	setBopState (value);
	return -1;
}


int
Rts2Conn::message ()
{
	struct timeval messageTime;
	char *messageOName;
	int messageType;

	if (paramNextTimeval (&messageTime)
		|| paramNextString (&messageOName) || paramNextInteger (&messageType))
		return -2;

	Rts2Message rts2Message = Rts2Message
		(messageTime, std::string (messageOName), messageType,
		std::string (paramNextWholeString ()));

	master->message (rts2Message);
	// message is always processed and do not need OK return
	return -1;
}


void
Rts2Conn::sendCommand ()
{
	Rts2CommandDeviceStatus *statInfoCall;
	Rts2Conn *conn;
	// we require some special state before command can be executed
	if (runningCommand->getBopMask ())
	{
		// we are waiting for some BOP mask and it have already occured
		if (runningCommand->getBopMask () & BOP_WHILE_STATE)
		{
			#ifdef DEBUG_ALL
			std::cout << "waiting for " << runningCommand->getText () << " "
				<< std::hex << runningCommand->getBopMask () << " "
				<< std::hex << getMaster()->getMasterStateFull ()
				<< " " << runningCommand
				<< std::endl;
			#endif
			if (getMaster()->getMasterStateFull () & runningCommand->getBopMask () & BOP_MASK)
			{
				// just wait for finish
				if (runningCommand->getStatusCallProgress () == CIP_WAIT)
				{
					runningCommandStatus = WAITING;
					return;
				}
				#ifdef DEBUG_ALL
				logStream (MESSAGE_DEBUG) << "executing " << runningCommand->getText () << " " << runningCommand << sendLog;
				#endif
				runningCommand->send ();
				runningCommandStatus = SEND;
				runningCommand->setStatusCallProgress (CIP_WAIT);
			}
			else
			{
				// signal connection we are waiting for state changed
				runningCommand->setStatusCallProgress (CIP_RUN);
			}
			return;
		}
		switch (runningCommand->getStatusCallProgress ())
		{
			case CIP_NOT_CALLED:
				statInfoCall =
					new Rts2CommandDeviceStatus (getMaster (), this);
				conn = runningCommand->getConnection ();
				// we can do that, as if we are running on same connection as is centrald, we are runningCommand, so we can send directly..
				statInfoCall->setConnection (this);
				statInfoCall->setStatusCallProgress (CIP_RETURN);
				statInfoCall->send ();
				runningCommand->setStatusCallProgress (CIP_WAIT);
				commandQue.push_front (runningCommand);
				runningCommand = statInfoCall;
				runningCommandStatus = SEND;
				break;
			case CIP_WAIT:
				// if the bock bit is still set..
				runningCommand->setStatusCallProgress (CIP_RUN);
			case CIP_RUN:
				if (getFullBopState () & runningCommand->getBopMask () & BOP_MASK)
				{
					runningCommandStatus = WAITING;
					break;
				}
				runningCommand->send ();
				runningCommandStatus = SEND;
				runningCommand->setStatusCallProgress (CIP_RETURN);
				break;
			case CIP_RETURN:
				// do nothing, it's status command, which return back, or command waiting for return
				break;
		}
	}
	else
	{
		runningCommand->send ();
		runningCommandStatus = SEND;
	}
}


int
Rts2Conn::sendNextCommand ()
{
	// pop next command
	if (commandQue.size () > 0)
	{
		runningCommand = *(commandQue.begin ());
		commandQue.erase (commandQue.begin ());
		sendCommand ();
		return 0;
	}
	runningCommand = NULL;
	return -1;
}


int
Rts2Conn::commandReturn ()
{
	int ret;
	int stat = atoi (getCommand ());
	// ignore (for the moment) retuns recieved without command
	if (!runningCommand)
	{
		logStream (MESSAGE_ERROR) << "Rts2Conn::commandReturn null on connection with '"
			<< getName ()
			<< "' (" << getCentraldId ()
			<< ") and status " << stat
			<<"!" << sendLog;
		return -1;
	}
	runningCommandStatus = RETURNING;
	commandReturn (runningCommand, stat);
	ret = runningCommand->commandReturn (stat, this);
	switch (ret)
	{
		case RTS2_COMMAND_REQUE:
			sendCommand ();
			break;
		case -1:
			delete runningCommand;
			sendNextCommand ();
			break;
	}
	return -1;
}


void
Rts2Conn::priorityChanged ()
{
}


int
Rts2Conn::priorityChange ()
{
	// we don't want any messages yet..
	return -1;
}


int
Rts2Conn::priorityInfo ()
{
	int have;
	if (paramNextInteger (&have) || !paramEnd ())
		return -2;
	have_priority = have;
	priorityChanged ();
	if (otherDevice)
		otherDevice->priorityInfo (have);
	getMaster ()->priorityChanged (this, have);
	// don't send OK
	return -1;
}


int
Rts2Conn::sendMsg (const char *msg)
{
	int len;
	int ret;
	if (sock == -1)
		return -1;
	len = strlen (msg) + 1;
	char *mbuf = new char[len + 1];
	strcpy (mbuf, msg);
	strcat (mbuf, "\n");
	// ignore EINTR
	do
	{
		ret = write (sock, mbuf, len);
	} while (ret == -1 && errno == EINTR);

	if (ret != len)
	{
		syslog (LOG_ERR, "Cannot send msg: %s to sock %i with len %i, ret %i errno %i message %m",
			msg, sock, len, ret, errno);
		#ifdef DEBUG_EXTRA
		logStream (MESSAGE_ERROR)
			<< "Rts2Conn::send [" << getCentraldId () << ":" << conn_state << "] error "
			<< sock << " state: " << ret << " sending " << msg << ":" strerror (errno)
			<< sendLog;
		#endif
		connectionError (ret);
		delete[] mbuf;
		return -1;
	}
	#ifdef DEBUG_ALL
	std::cout << "Rts2Conn::send " << getName ()
		<< " [" << getCentraldId () << ":" << sock << "] send " << ret << ": " << msg
		<< std::endl;
	#endif

	delete[] mbuf;
	successfullSend ();
	return 0;
}


int
Rts2Conn::sendMsg (std::string msg)
{
	return sendMsg (msg.c_str ());
}


int
Rts2Conn::startBinaryData (long dataSize, int dataType)
{
	char *msg;
	dataConn++;
	asprintf (&msg, PROTO_BINARY " %i %li %i", dataConn, dataSize, dataType);
	int ret;
	ret = sendMsg (msg);
	free (msg);
	if (ret == -1)
		return -1;
	writeData[dataConn] = new Rts2DataWrite (dataSize);
	return dataConn;
}


int
Rts2Conn::sendBinaryData (int data_conn, char *data, long dataSize)
{
	char *binaryWriteTop, *binaryWriteBuff;
	binaryWriteTop = binaryWriteBuff = data;
	char *binaryEnd = data + dataSize;

	char *msg;
	asprintf (&msg, PROTO_DATA " %i %li", data_conn, dataSize);
	int ret;
	ret = sendMsg (msg);
	free (msg);
	if (ret)
		return ret;

	while (binaryWriteTop < binaryEnd)
	{
		if (dataSize > getWriteBinaryDataSize (data_conn))
		{
			logStream (MESSAGE_ERROR) << "Attemp to send too much data "
				<< dataSize << " " << getWriteBinaryDataSize (data_conn) << sendLog;
			dataSize = getWriteBinaryDataSize (data_conn);
		}
		ret = send (sock, binaryWriteTop, dataSize, 0);
		if (ret == -1)
		{
			if (errno != EINTR)
			{
				connectionError (ret);
				return -1;
			}
		}
		else
		{
			binaryWriteTop += ret;
			std::map <int, Rts2DataWrite *>::iterator iter = writeData.find (data_conn);
			((*iter).second)->dataWritten (ret);
			if (writeData[data_conn]->getDataSize () <= 0)
			{
				delete ((*iter).second);
				writeData.erase (iter);
			}
		}
	}
	return 0;
}


void
Rts2Conn::successfullSend ()
{
	time (&lastGoodSend);
}


void
Rts2Conn::getSuccessSend (time_t * in_t)
{
	*in_t = lastGoodSend;
}


bool
Rts2Conn::reachedSendTimeout ()
{
	time_t now;
	time (&now);
	return now > lastGoodSend + getConnTimeout ();
}


void
Rts2Conn::successfullRead ()
{
	time (&lastData);
}


void
Rts2Conn::connConnected ()
{
}


void
Rts2Conn::connectionError (int last_data_size)
{
	activeReadData = -1;
	setConnState (CONN_DELETE);
	if (sock >= 0)
		close (sock);
	sock = -1;
	if (strlen (getName ()))
		master->deleteAddress (getCentraldNum (), getName ());
}


int
Rts2Conn::sendMessage (Rts2Message & msg)
{
	return sendMsg (msg.toConn ());
}


int
Rts2Conn::sendValue (std::string val_name, int value)
{
	char *msg;
	int ret;
	asprintf (&msg, PROTO_VALUE " %s %i", val_name.c_str (), value);
	ret = sendMsg (msg);
	free (msg);
	return ret;
}


int
Rts2Conn::sendValue (std::string val_name, int val1, double val2)
{
	char *msg;
	int ret;
	asprintf (&msg, PROTO_VALUE " %s %i %f", val_name.c_str (), val1, val2);
	ret = sendMsg (msg);
	free (msg);
	return ret;
}


int
Rts2Conn::sendValue (std::string val_name, const char *value)
{
	char *msg;
	int ret;
	asprintf (&msg, PROTO_VALUE " %s \"%s\"", val_name.c_str (), value);
	ret = sendMsg (msg);
	free (msg);
	return ret;
}


int
Rts2Conn::sendValueRaw (std::string val_name, const char *value)
{
	char *msg;
	int ret;
	asprintf (&msg, PROTO_VALUE " %s %s", val_name.c_str (), value);
	ret = sendMsg (msg);
	free (msg);
	return ret;
}


int
Rts2Conn::sendValue (std::string val_name, double value)
{
	char *msg;
	int ret;
	asprintf (&msg, PROTO_VALUE " %s %f", val_name.c_str (), value);
	ret = sendMsg (msg);
	free (msg);
	return ret;
}


int
Rts2Conn::sendValue (char *val_name, char *val1, int val2)
{
	char *msg;
	int ret;
	asprintf (&msg, PROTO_VALUE " %s \"%s\" %i", val_name, val1, val2);
	ret = sendMsg (msg);
	free (msg);
	return ret;
}


int
Rts2Conn::sendValue (char *val_name, int val1, int val2,
double val3, double val4, double val5, double val6)
{
	char *msg;
	int ret;
	asprintf (&msg, PROTO_VALUE " %s %i %i %f %f %f %f", val_name, val1, val2,
		val3, val4, val5, val6);
	ret = sendMsg (msg);
	free (msg);
	return ret;
}


int
Rts2Conn::sendValueTime (std::string val_name, time_t * value)
{
	char *msg;
	int ret;
	asprintf (&msg, PROTO_VALUE " %s %li", val_name.c_str (), *value);
	ret = sendMsg (msg);
	free (msg);
	return ret;
}


int
Rts2Conn::sendCommandEnd (int num, const char *in_msg)
{
	char *msg;
	asprintf (&msg, "%+04i \"%s\"", num, in_msg);
	sendMsg (msg);
	free (msg);
	if (commandInProgress)
	{
		setCommandInProgress (false);
		processBuffer ();
	}
	return 0;
}


void
Rts2Conn::setConnState (conn_state_t new_conn_state)
{
	if (new_conn_state == CONN_AUTH_OK || new_conn_state == CONN_CONNECTED)
	{
		if (!runningCommand)
			sendNextCommand ();
		// otherwise command will be send after command which trigerred
		// state change finished..
	}
	conn_state = new_conn_state;
	if (new_conn_state == CONN_AUTH_FAILED)
	{
		connectionError (-1);
	}
}


int
Rts2Conn::isConnState (conn_state_t in_conn_state)
{
	return (conn_state == in_conn_state);
}


bool
Rts2Conn::paramEnd ()
{
	while (isspace (*command_buf_top))
		command_buf_top++;
	return !*command_buf_top;
}


int
Rts2Conn::paramNextString (char **str, const char *enddelim)
{
	while (isspace (*command_buf_top))
		command_buf_top++;
	if (!*command_buf_top)
		return -1;
	// start of string with spaces
	if (*command_buf_top == '"')
	{
		command_buf_top++;
		*str = command_buf_top;
		while (*command_buf_top && *command_buf_top != '"')
			command_buf_top++;
	}
	else
	{
		// eat next spaces
		*str = command_buf_top;
		while (!isspace (*command_buf_top)
		  && *command_buf_top
		  && (enddelim == NULL || strchr (enddelim, *command_buf_top) == NULL))
			command_buf_top++;
	}
	if (*command_buf_top)
	{
		*command_buf_top = '\0';
		command_buf_top++;
	}
	return 0;
}


char *
Rts2Conn::paramNextWholeString ()
{
	while (isspace (*command_buf_top))
		command_buf_top++;
	return command_buf_top;
}


int
Rts2Conn::paramNextStringNull (char **str)
{
	int ret;
	ret = paramNextString (str);
	if (ret)
		return !paramEnd ();
	return ret;
}


int
Rts2Conn::paramNextInteger (int *num)
{
	char *str_num;
	char *num_end;
	if (paramNextString (&str_num, ","))
		return -1;
	*num = strtol (str_num, &num_end, 10);
	if (*num_end)
		return -1;
	return 0;
}


int
Rts2Conn::paramNextLong (long int *num)
{
	char *str_num;
	char *num_end;
	if (paramNextString (&str_num, ","))
		return -1;
	*num = strtol (str_num, &num_end, 10);
	if (*num_end)
		return -1;
	return 0;
}


int
Rts2Conn::paramNextSizeT (size_t * num)
{
	char *str_num;
	char *num_end;
	if (paramNextString (&str_num))
		return -1;
	*num = strtol (str_num, &num_end, 10);
	if (*num_end)
		return -1;
	return 0;
}


int
Rts2Conn::paramNextDouble (double *num)
{
	char *str_num;
	int ret;
	if (paramNextString (&str_num, ","))
		return -1;
	if (!strcmp (str_num, "nan"))
	{
		*num = nan ("f");
		return 0;
	}
	ret = sscanf (str_num, "%lf", num);
	if (ret != 1)
		return -1;
	return 0;
}


int
Rts2Conn::paramNextFloat (float *num)
{
	char *str_num;
	int ret;
	if (paramNextString (&str_num, ","))
		return -1;
	ret = sscanf (str_num, "%f", num);
	if (ret != 1)
		return -1;
	return 0;
}


int
Rts2Conn::paramNextTimeval (struct timeval *tv)
{
	int sec;
	int usec;
	if (paramNextInteger (&sec) || paramNextInteger (&usec))
		return -1;
	tv->tv_sec = sec;
	tv->tv_usec = usec;
	return 0;
}


void
Rts2Conn::newDataConn (int data_conn)
{
	if (otherDevice)
		otherDevice->newDataConn (data_conn);
}


void
Rts2Conn::dataReceived ()
{
	std::map <int, Rts2DataRead *>::iterator iter = readData.find (activeReadData);
	// inform device that we read some data
	if (otherDevice)
		otherDevice->dataReceived ((*iter).second);
	getMaster ()->binaryDataArrived (this);
	if (((*iter).second)->getRestSize () == 0)
	{
		if (otherDevice)
			otherDevice->fullDataReceived ((*iter).first, (*iter).second);
		delete (*iter).second;
		readData.erase (iter);
		activeReadData = -1;
	}
	else if (((*iter).second)->getChunkSize () == 0)
	{
		activeReadData = -1;
	}
}


Rts2Value *
Rts2Conn::getValue (const char *value_name)
{
	return values.getValue (value_name);
}


void
Rts2Conn::addValue (Rts2Value * value)
{
	if (value->isValue (RTS2_VALUE_INFOTIME))
		info_time = (Rts2ValueTime *) value;
	// if value exists, remove it from list
	values.removeValue (value->getName ().c_str ());
	values.push_back (value);
}


int
Rts2Conn::metaInfo (int rts2Type, std::string m_name, std::string desc)
{
	Rts2Value *new_value;
	switch (rts2Type & RTS2_EXT_TYPE)
	{
		case 0:
			new_value = newValue (rts2Type, m_name, desc);
			if (newValue == NULL)
				return -2;
			break;
		case RTS2_VALUE_STAT:
			new_value = new Rts2ValueDoubleStat (m_name, desc, rts2Type & RTS2_VALUE_FITS, rts2Type);
			break;
		case RTS2_VALUE_MMAX:
			new_value =  new Rts2ValueDoubleMinMax (m_name, desc, rts2Type & RTS2_VALUE_FITS, rts2Type);
			break;
		case RTS2_VALUE_RECTANGLE:
			new_value =  new Rts2ValueRectangle (m_name, desc, rts2Type & RTS2_VALUE_FITS, rts2Type);
			break;
		default:
			logStream (MESSAGE_ERROR) << "unknow value type: " << rts2Type <<
				sendLog;
			return -2;
	}
	addValue (new_value);
	return -1;
}


int
Rts2Conn::selMetaInfo (const char *value_name, char *sel_name)
{
	Rts2Value *val = getValue (value_name);
	if (!val || val->getValueType () != RTS2_VALUE_SELECTION)
		return -1;
	((Rts2ValueSelection *) val)->addSelVal (sel_name);
	return 0;
}


const char *
Rts2Conn::getValueChar (const char *value_name)
{
	Rts2Value *val;
	val = getValue (value_name);
	if (val)
		return val->getValue ();
	return NULL;
}


double
Rts2Conn::getValueDouble (const char *value_name)
{
	Rts2Value *val;
	val = getValue (value_name);
	if (val)
		return val->getValueDouble ();
	return nan ("f");
}


int
Rts2Conn::getValueInteger (const char *value_name)
{
	Rts2Value *val;
	val = getValue (value_name);
	if (val)
		return val->getValueInteger ();
	return -1;
}


const char *
Rts2Conn::getValueSelection (const char *value_name)
{
	Rts2Value *val;
	val = getValue (value_name);
	if (val->getValueType () != RTS2_VALUE_SELECTION)
		return "UNK";
	return ((Rts2ValueSelection *) val)->getSelName ().c_str ();
}


const char *
Rts2Conn::getValueSelection (const char *value_name, int val_num)
{
	Rts2Value *val;
	val = getValue (value_name);
	if (val->getValueType () != RTS2_VALUE_SELECTION)
		return "UNK";
	return ((Rts2ValueSelection *) val)->getSelName (val_num).c_str ();
}


int
Rts2Conn::commandValue (const char *v_name)
{
	Rts2Value *value = getValue (v_name);
	if (value)
	{
		int ret;
		ret = value->setValue (this);
		// notice other type..
		if (getOtherDevClient ())
		{
			getOtherDevClient ()->valueChanged (value);
		}
		return ret;
	}
	logStream (MESSAGE_ERROR)
		<< "Unknow value from connection '" << getName () << "' "
		<< v_name
		<< sendLog;
	return -2;
}


bool Rts2Conn::existWriteType (int w_type)
{
	for (Rts2ValueVector::iterator iter = values.begin ();
		iter != values.end (); iter++)
	{
		if ((*iter)->getValueWriteFlags () == w_type)
			return true;
	}
	return false;
}


double
Rts2Conn::getInfoTime ()
{
	if (info_time)
		return info_time->getValueDouble ();
	return getMaster ()->getNow ();
}


void
Rts2Conn::getInfoTime (struct timeval &tv)
{
	if (info_time)
	{
		info_time->getValueTime (tv);
		return;
	}
	gettimeofday (&tv, NULL);
}


bool
Rts2Conn::infoTimeChanged ()
{
	if (info_time)
	{
		if (last_info_time != info_time->getValueDouble ())
		{
			last_info_time = info_time->getValueDouble ();
			return true;
		}
	}
	return false;
}
