/* 
 * Forking connection handling.
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

#include "connfork.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

using namespace rts2core;

ConnFork::ConnFork (Rts2Block *_master, const char *_exe, bool _fillConnEnvVars, bool _openin, int _timeout):Rts2ConnNoSend (_master)
{
	childPid = -1;
	sockerr = -1;
	if (_openin)
		sockwrite = -2;
	else
		sockwrite = -1;

	exePath = new char[strlen (_exe) + 1];
	strcpy (exePath, _exe);
	if (_timeout > 0)
	{
		time (&forkedTimeout);
		forkedTimeout += _timeout;
	}
	else
	{
		forkedTimeout = 0;
	}
	fillConnEnvVars = _fillConnEnvVars;
}

ConnFork::~ConnFork ()
{
	if (childPid > 0)
		kill (-childPid, SIGINT);
	if (sockerr > 0)
		close (sockerr);
	if (sockwrite > 0)
		close (sockwrite);
	delete[]exePath;
}

int ConnFork::writeToProcess (const char *msg)
{
	if (sockwrite == -1)
		return 0;

	std::string completeMessage = input + msg + "\n";

	int l = completeMessage.length ();
	int ret = write (sockwrite, completeMessage.c_str (), l);

	if (ret < 0)
	{
		logStream (MESSAGE_ERROR) << "Cannot write to process " << getExePath () << ": " << strerror (errno) << sendLog;
		connectionError (-1);
		return -1;
	}
	setInput (completeMessage.substr (ret));
	return 0;	
}

int ConnFork::add (fd_set * readset, fd_set * writeset, fd_set * expset)
{
	if (sockerr > 0)
		FD_SET (sockerr, readset);
	if (input.length () > 0)
	{
		if (sockwrite < 0)
		{
			logStream (MESSAGE_ERROR) << "asked to write data after write sock was closed" << sendLog;
			input = std::string ("");
		}
		else
		{
			FD_SET (sockwrite, writeset);
		}
	}
	return Rts2ConnNoSend::add (readset, writeset, expset);
}

int ConnFork::receive (fd_set * readset)
{
	if (sockerr > 0 && FD_ISSET (sockerr, readset))
	{
		int data_size;
		char errbuf[5001];
		data_size = read (sockerr, errbuf, 5000);
		if (data_size > 0)
		{
			errbuf[data_size] = '\0';
			processErrorLine (errbuf);	
		}
		else if (data_size == 0)
		{
			close (sockerr);
			sockerr = -1;
		}
		else
		{
			logStream (MESSAGE_ERROR) << "From error pipe read error " << strerror (errno) << "." << sendLog;
		}
	}
	return Rts2ConnNoSend::receive (readset);
}

int ConnFork::writable (fd_set * writeset)
{
	if (input.length () > 0)
	{
		if (sockwrite < 0)
		{
			connectionError (-1);
			return -1;
		}
	 	if (FD_ISSET (sockwrite, writeset))
		{
			int write_size = write (sockwrite, input.c_str (), input.length ());
			if (write_size < 0 && errno != EINTR)
			{
				logStream (MESSAGE_ERROR) << "rts2core::ConnFork while writing to sockwrite: " << strerror (errno) << sendLog;
				close (sockwrite);
				sockwrite = -1;
				return -1;
			}
			input = input.substr (write_size);
			if (input.length () == 0)
			{
				write_size = close (sockwrite);
				if (write_size < 0)
					logStream (MESSAGE_ERROR) << "rts2core::ConnFork error while closing write descriptor: " << strerror (errno) << sendLog;
				sockwrite = -1;
			}
		}
	}
	return Rts2ConnNoSend::writable (writeset);
}

void ConnFork::connectionError (int last_data_size)
{
	if (last_data_size < 0 && errno == EAGAIN)
	{
		logStream (MESSAGE_DEBUG) << "ConnFork::connectionError reported EAGAIN - that should not happen, ignoring it" << sendLog;
		return;
	}
	Rts2Conn::connectionError (last_data_size);
}

void ConnFork::beforeFork ()
{
}

void ConnFork::initFailed ()
{
}

void ConnFork::processErrorLine (char *errbuf)
{
	logStream (MESSAGE_ERROR) << exePath << " error stream: " << errbuf << sendLog;
}

int ConnFork::newProcess ()
{
	char * args[argv.size () + 2];
	args[0] = exePath;
	for (size_t i = 0; i < argv.size (); i++)
		args[i + 1] = (char *) argv[i].c_str ();
	args[argv.size () + 1] = NULL;
	return execv (exePath, args);
}

void ConnFork::fillConnectionEnv (Rts2Conn *conn, const char *prefix)
{
	std::ostringstream _os;
	_os << conn->getState ();

	// put to environment device state
	char *envV = new char [strlen (prefix) + _os.str ().length () + 8];
	sprintf (envV, "%s_state=%s", prefix, _os.str ().c_str ());
	putenv (envV);

	for (Rts2ValueVector::iterator viter = conn->valueBegin (); viter != conn->valueEnd (); viter++)
	{
		Rts2Value *val = (*viter);
		// replace non-alpha characters
		std::string valn = val->getName ();
		for (std::string::iterator siter = valn.begin (); siter != valn.end (); siter++)
		{
			if (!isalnum (*siter))
				(*siter) = '_';
		}

		envV = new char [strlen (prefix) + valn.length () + strlen (val->getDisplayValue ()) + 3];
		sprintf (envV, "%s_%s=%s", prefix, valn.c_str (), val->getDisplayValue ());
		putenv (envV);
	}
}

int ConnFork::init ()
{
	int ret;
	if (childPid > 0)
	{
		// continue
		kill (childPid, SIGCONT);
		initFailed ();
		return 1;
	}
	// first check if exePath exist and we have run permissions..
	if (access (exePath, X_OK))
	{
		logStream (MESSAGE_ERROR) << "execution of " << exePath << " failed with error: " << strerror (errno) << sendLog;
		return -1;
	}
	int filedes[2];
	int filedeserr[2];
	int filedeswrite[2];
	ret = pipe (filedes);
	if (ret)
	{
		logStream (MESSAGE_ERROR) << "ConnFork::init cannot create pipe for process: " << strerror (errno) << sendLog;
		initFailed ();
		return -1;
	}
	ret = pipe (filedeserr);
	if (ret)
	{
		logStream (MESSAGE_ERROR) << "ConnFork::init cannot create error pipe for process: " << strerror (errno) << sendLog;
		initFailed ();
		return -1;
	}

	if (sockwrite == -2)
	{
		ret = pipe (filedeswrite);
		if (ret)
		{
		  	logStream (MESSAGE_ERROR) << "ConnFork::init cannot create write pipe for process: " << strerror (errno) << sendLog;
			initFailed ();
			return -1;
		}
	}

	// do everything that will be needed to done before forking
	beforeFork ();
	childPid = fork ();
	if (childPid == -1)
	{
		logStream (MESSAGE_ERROR) << "ConnFork::init cannot fork: " << strerror (errno) << sendLog;
		initFailed ();
		return -1;
	}
	else if (childPid)			 // parent
	{
		sock = filedes[0];
		close (filedes[1]);
		sockerr = filedeserr[0];
		close (filedeserr[1]);

		if (sockwrite == -2)
		{
			sockwrite = filedeswrite[1];
			close (filedeswrite[0]);
			fcntl (sockwrite, F_SETFL, O_NONBLOCK);
		}

		fcntl (sock, F_SETFL, O_NONBLOCK);
		fcntl (sockerr, F_SETFL, O_NONBLOCK);
		return 0;
	}
	// child
	close (0);
	close (1);
	close (2);

	if (sockwrite == -2)
	{
		close (filedeswrite[1]);
		dup2 (filedeswrite[0], 0);
	}

	close (filedes[0]);
	dup2 (filedes[1], 1);
	close (filedeserr[0]);
	dup2 (filedeserr[1], 2);

	// if required, pass environemnt values about connections..
	if (fillConnEnvVars)
	{
		connections_t *conns;
		connections_t::iterator citer;
		// fill centrald variables..
		conns = getMaster ()->getCentraldConns ();
		int i = 1;
		for (citer = conns->begin (); citer != conns->end (); citer++, i++)
		{
	  		std::ostringstream _os;
			_os << "centrald" << i;
			fillConnectionEnv (*citer, _os.str ().c_str ());
		}

		conns = getMaster () ->getConnections ();
		for (citer = conns->begin (); citer != conns->end (); citer++)
			fillConnectionEnv (*citer, (*citer)->getName ());
	}

	// close all sockets so when we crash, we don't get any dailing
	// sockets
	master->forkedInstance ();
	// start new group, so SIGTERM to group will kill all children
	setpgrp ();

	ret = newProcess ();
	if (ret)
		logStream (MESSAGE_ERROR) << "ConnFork::init " << exePath << " newProcess return : " <<
			ret << " " << strerror (errno) << sendLog;
	exit (0);
}

int ConnFork::run ()
{
	int ret;
	ret = init ();
	if (ret)
		return ret;
	
	fd_set read_set, write_set, exp_set;
	struct timeval read_tout;
	read_tout.tv_sec = 10;
	read_tout.tv_usec = 0;

	while (sock > 0)
	{
		FD_ZERO (&read_set);
		FD_ZERO (&write_set);
		FD_ZERO (&exp_set);

		add (&read_set, &write_set, &exp_set);
		ret = select (FD_SETSIZE, &read_set, &write_set, &exp_set, &read_tout);
		if (ret == -1)
			return -1;
		if (receive (&read_set) == -1 || writable (&write_set) == -1)
			return 0;
		idle ();
	}

	return 0;
}

void ConnFork::stop ()
{
	if (childPid > 0)
		kill (-childPid, SIGSTOP);
}

void ConnFork::term ()
{
	if (childPid > 0)
		kill (-childPid, SIGKILL);
	childPid = -1;
}

int ConnFork::idle ()
{
	if (forkedTimeout > 0)
	{
		time_t now;
		time (&now);
		if (now > forkedTimeout)
		{
			term ();
			endConnection ();
		}
	}
	return 0;
}

void ConnFork::childReturned (pid_t in_child_pid)
{
	if (childPid == in_child_pid)
	{
		childEnd ();
		childPid = -1;
		// endConnection will be called after read from pipe fails
	}
}
