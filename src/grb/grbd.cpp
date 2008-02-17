/* 
 * Receives informations from GCN via socket, put them to database.
 * Copyright (C) 2003-2008 Petr Kubanek <petr@kubanek.net>
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

#include "../utils/rts2command.h"
#include "grbd.h"

#define OPT_GCN_HOST            OPT_LOCAL + 50
#define OPT_GCN_PORT            OPT_LOCAL + 51
#define OPT_GCN_TEST            OPT_LOCAL + 52
#define OPT_GCN_FORWARD         OPT_LOCAL + 53
#define OPT_GCN_EXE             OPT_LOCAL + 54
#define OPT_GCN_FOLLOUPS        OPT_LOCAL + 55

Rts2DevGrb::Rts2DevGrb (int in_argc, char **in_argv):
Rts2DeviceDb (in_argc, in_argv, DEVICE_TYPE_GRB, "GRB")
{
	gcncnn = NULL;
	gcn_host = NULL;
	gcn_port = -1;
	do_hete_test = 0;
	forwardPort = -1;
	addExe = NULL;
	execFollowups = 0;

	createValue (last_packet, "last_packet", "time from last packet", false);

	createValue (delta, "delta", "delta time from last packet", false);

	createValue (last_target, "last_target", "id of last GRB target", false);

	createValue (last_target_time, "last_target_time", "time of last target",
		false);

	createValue (execConnection, "exec", "exec connection", false);

	addOption (OPT_GCN_HOST, "gcn_host", 1, "GCN host name");
	addOption (OPT_GCN_PORT, "gcn_port", 1, "GCN port");
	addOption (OPT_GCN_TEST, "test", 0, "process test notices (default to off - don't process them)");
	addOption (OPT_GCN_FORWARD, "forward", 1, "forward incoming notices to that port");
	addOption (OPT_GCN_EXE, "add_exe", 1, "execute that command when new GCN packet arrives");
	addOption (OPT_GCN_FOLLOUPS, "exec_followups", 0,
		"execute observation and add_exe script even for follow-ups without error box (currently Swift follow-ups of INTEGRAL and HETE GRBs)");
}


Rts2DevGrb::~Rts2DevGrb (void)
{
	delete[]gcn_host;
	delete[]addExe;
}


int
Rts2DevGrb::processOption (int in_opt)
{
	switch (in_opt)
	{
		case OPT_GCN_HOST:
			gcn_host = new char[strlen (optarg) + 1];
			strcpy (gcn_host, optarg);
			break;
		case OPT_GCN_PORT:
			gcn_port = atoi (optarg);
			break;
		case OPT_GCN_TEST:
			do_hete_test = 1;
			break;
		case OPT_GCN_FORWARD:
			forwardPort = atoi (optarg);
			break;
		case OPT_GCN_EXE:
			addExe = optarg;
			break;
		case OPT_GCN_FOLLOUPS:
			execFollowups = 1;
			break;
		default:
			return Rts2DeviceDb::processOption (in_opt);
	}
	return 0;
}


int
Rts2DevGrb::reloadConfig ()
{
	int ret;
	Rts2Config *config;
	ret = Rts2DeviceDb::reloadConfig ();
	if (ret)
		return ret;

	config = Rts2Config::instance ();

	// get some default, if we cannot get them from command line
	if (!gcn_host)
	{
		std::string gcn_h;
		ret = config->getString ("grbd", "server", gcn_h);
		if (ret)
			return -1;
		gcn_host = new char[gcn_h.length () + 1];
		strcpy (gcn_host, gcn_h.c_str ());
	}

	if (gcn_port < 0)
	{
		ret = config->getInteger ("grbd", "port", gcn_port);
		if (ret)
			return -1;
	}

	// try to get exe from config
	if (!addExe)
	{
		std::string conf_addExe;
		ret = config->getString ("grbd", "add_exe", conf_addExe);
		if (!ret)
		{
			addExe = new char[conf_addExe.length () + 1];
			strcpy (addExe, conf_addExe.c_str ());
		}
	}
	if (gcncnn)
	{
		deleteConnection (gcncnn);
		delete gcncnn;
	}
	// add connection..
	gcncnn =
		new Rts2ConnGrb (gcn_host, gcn_port, do_hete_test, addExe, execFollowups,
		this);
	// wait till grb connection init..
	while (1)
	{
		ret = gcncnn->init ();
		if (!ret)
			break;
		logStream (MESSAGE_ERROR)
			<< "Rts2DevGrb::init cannot init conngrb, sleeping for 60 sec" <<
			sendLog;
		sleep (60);
		if (getEndLoop ())
			return -1;
	}
	addConnection (gcncnn);

	return ret;
}


int
Rts2DevGrb::init ()
{
	int ret;
	ret = Rts2DeviceDb::init ();
	if (ret)
		return ret;

	// add forward connection
	if (forwardPort > 0)
	{
		int ret2;
		Rts2GrbForwardConnection *forwardConnection;
		forwardConnection = new Rts2GrbForwardConnection (this, forwardPort);
		ret2 = forwardConnection->init ();
		if (ret2)
		{
			logStream (MESSAGE_ERROR)
				<< "Rts2DevGrb::init cannot create forward connection, ignoring ("
				<< ret2 << ")" << sendLog;
			delete forwardConnection;
			forwardConnection = NULL;
		}
		else
		{
			addConnection (forwardConnection);
		}
	}
	return ret;
}


void
Rts2DevGrb::help ()
{
	Rts2DeviceDb::help ();
	std::cout << std::endl << " Execution script, specified with --add_exec option, receives following parameters as arguments:"
		" target-id grb-id grb-seqn grb-type grb-ra grb-dec grb-is-grb grb-date grb-errorbox." << std::endl
		<< " Please see man page for meaning of that arguments." << std::endl;
}


int
Rts2DevGrb::info ()
{
	last_packet->setValueDouble (gcncnn->lastPacket ());
	delta->setValueDouble (gcncnn->delta ());
	last_target->setValueString (gcncnn->lastTarget ());
	last_target_time->setValueDouble (gcncnn->lastTargetTime ());
	execConnection->setValueInteger (getOpenConnection ("EXEC") ? 1 : 0);
	return Rts2DeviceDb::info ();
}


void
Rts2DevGrb::postEvent (Rts2Event * event)
{
	switch (event->getType ())
	{
		case RTS2_EVENT_GRB_PACKET:
			infoAll ();
			break;
	}
	Rts2DeviceDb::postEvent (event);
}


// that method is called when somebody want to immediatelly observe GRB
int
Rts2DevGrb::newGcnGrb (int tar_id)
{
	Rts2Conn *exec;
	exec = getOpenConnection ("EXEC");
	if (exec)
	{
		exec->queCommand (new Rts2CommandExecGrb (this, tar_id));
	}
	else
	{
		logStream (MESSAGE_ERROR) <<
			"FATAL! No executor running to post grb ID " << tar_id << sendLog;
		return -1;
	}
	return 0;
}


int
Rts2DevGrb::commandAuthorized (Rts2Conn * conn)
{
	if (conn->isCommand ("test"))
	{
		int tar_id;
		if (conn->paramNextInteger (&tar_id) || !conn->paramEnd ())
			return -2;
		return newGcnGrb (tar_id);
	}
	return Rts2DeviceDb::commandAuthorized (conn);
}


int
main (int argc, char **argv)
{
	Rts2DevGrb grb = Rts2DevGrb (argc, argv);
	return grb.run ();
}
