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

#include "rts2command.h"
#include "grbd.h"

using namespace rts2grbd;

#define OPT_GRB_DISABLE         OPT_LOCAL + 49
#define OPT_GCN_HOST            OPT_LOCAL + 50
#define OPT_GCN_PORT            OPT_LOCAL + 51
#define OPT_GCN_TEST            OPT_LOCAL + 52
#define OPT_GCN_FORWARD         OPT_LOCAL + 53
#define OPT_GCN_EXE             OPT_LOCAL + 54
#define OPT_GCN_FOLLOUPS        OPT_LOCAL + 55
#define OPT_QUEUE               OPT_LOCAL + 56

Grbd::Grbd (int in_argc, char **in_argv):Rts2DeviceDb (in_argc, in_argv, DEVICE_TYPE_GRB, "GRB")
{
	gcncnn = NULL;
	gcn_host = NULL;
	gcn_port = -1;
	do_hete_test = 0;
	forwardPort = -1;
	addExe = NULL;
	execFollowups = 0;
	queueName = NULL;

	createValue (grb_enabled, "enabled", "if true, GRB reception is enabled", false, RTS2_VALUE_WRITABLE);
	grb_enabled->setValueBool (true);

	createValue (last_packet, "last_packet", "time from last packet", false);

	createValue (delta, "delta", "delta time from last packet", false);

	createValue (last_target, "last_target", "id of last GRB target", false);

	createValue (last_target_time, "last_target_time", "time of last target", false);
	createValue (last_target_radec, "last_target_radec", "coordinates (J2000) of last GRB", false);

	createValue (lastSwift, "last_swift", "time of last Swift position", false);
	createValue (lastSwiftRaDec, "last_swift_position", "Swift current position", false);

	createValue (lastIntegral, "last_integral", "time of last INTEGRAL position", false);
	createValue (lastIntegralRaDec, "last_integral_position", "INTEGRAL current position", false);

	createValue (recordNotVisible, "not_visible", "record GRBs not visible from the current location", false, RTS2_VALUE_WRITABLE);
	recordNotVisible->setValueBool (true);

	createValue (recordOnlyVisibleTonight, "only_visible_tonight", "record GRBs only visible during current night from the current location", false, RTS2_VALUE_WRITABLE);
	recordOnlyVisibleTonight->setValueBool (false);

	createValue (minGrbAltitude, "min_grb_altitude", "minimal GRB altitute to be considered as visible", false, RTS2_VALUE_WRITABLE);
	minGrbAltitude->setValueDouble (0);

	addOption (OPT_GRB_DISABLE, "disable-grbs", 0, "disable GRBs TOO execution - only receive GCN packets");
	addOption (OPT_GCN_HOST, "gcn_host", 1, "GCN host name");
	addOption (OPT_GCN_PORT, "gcn_port", 1, "GCN port");
	addOption (OPT_GCN_TEST, "test", 0, "process test notices (default to off - don't process them)");
	addOption (OPT_GCN_FORWARD, "forward", 1, "forward incoming notices to that port");
	addOption (OPT_GCN_EXE, "add-exec", 1, "execute that command when new GCN packet arrives");
	addOption (OPT_GCN_FOLLOUPS, "exec_followups", 0, "execute observation and add-exec script even for follow-ups without error box (currently Swift follow-ups of INTEGRAL and HETE GRBs)");
	addOption (OPT_QUEUE, "queue-to", 1, "queue GRBs to following queue (using now command)");
}

Grbd::~Grbd (void)
{
	delete[]gcn_host;
}

int Grbd::processOption (int in_opt)
{
	switch (in_opt)
	{
		case OPT_GRB_DISABLE:
			grb_enabled->setValueBool (false);
			break;
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
		case OPT_QUEUE:
			queueName = optarg;
			break;	
		default:
			return Rts2DeviceDb::processOption (in_opt);
	}
	return 0;
}

int Grbd::reloadConfig ()
{
	int ret;
	Rts2Config *config;
	ret = Rts2DeviceDb::reloadConfig ();
	if (ret)
		return ret;

	config = Rts2Config::instance ();

	observer = config->getObserver ();

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

	recordNotVisible->setValueBool (config->getBoolean ("grbd", "notvisible", recordNotVisible->getValueBool ()));
	recordOnlyVisibleTonight->setValueBool (config->getBoolean ("grbd", "onlyvisibletonight", recordOnlyVisibleTonight->getValueBool ()));
	minGrbAltitude->setValueDouble (config->getDoubleDefault ("observatory", "min_alt", minGrbAltitude->getValueDouble ()));

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
	gcncnn = new ConnGrb (gcn_host, gcn_port, do_hete_test, addExe, execFollowups, this);
	gcncnn->setGbmError (config->getDoubleDefault ("grbd", "gbm_error_limit", 0.25));
	gcncnn->setGbmRecordAboveError (config->getBoolean ("grbd", "gbm_record_above_error", true));
	gcncnn->setGbmEnabledAboveError (config->getBoolean ("grbd", "gbm_enabled_above_error", false));
	// setup..
	// wait till grb connection init..
	ret = gcncnn->init ();
	if (ret)
	{
		logStream (MESSAGE_ERROR) << "Grbd::init cannot init conngrb, waiting for 60 sec" << sendLog;
		addTimer (60, new Rts2Event (EVENT_TIMER_GCNCNN_INIT, this));
	}
	else
	{
		addConnection (gcncnn);
	}

	return 0;
}

int Grbd::init ()
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
				<< "Grbd::init cannot create forward connection, ignoring ("
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

void Grbd::help ()
{
	Rts2DeviceDb::help ();
	std::cout << std::endl << " Execution script, specified with --add-exec option, receives following parameters as arguments:"
		" target-id grb-id grb-seqn grb-type grb-ra grb-dec grb-is-grb grb-date grb-errorbox." << std::endl
		<< " Please see man page for meaning of that arguments." << std::endl;
}

int Grbd::info ()
{
	last_packet->setValueDouble (gcncnn->lastPacket ());
	delta->setValueDouble (gcncnn->delta ());
	last_target->setValueCharArr (gcncnn->lastTarget ());
	last_target_time->setValueDouble (gcncnn->lastTargetTime ());
	last_target_radec->setValueRaDec (gcncnn->lastRa (), gcncnn->lastDec ());
	return Rts2DeviceDb::info ();
}

void Grbd::postEvent (Rts2Event * event)
{
	switch (event->getType ())
	{
		case RTS2_EVENT_GRB_PACKET:
			infoAll ();
			break;
		case EVENT_TIMER_GCNCNN_INIT:
			if (gcncnn->init () != 0)
			{
				logStream (MESSAGE_ERROR) << "Cannot init GCN connection, waiting for 60 seconds" << sendLog;
				addTimer (60, new Rts2Event (EVENT_TIMER_GCNCNN_INIT, this));
			}
			break;
	}
	Rts2DeviceDb::postEvent (event);
}

// that method is called when somebody want to immediatelly observe GRB
int Grbd::newGcnGrb (int tar_id)
{
	if (grb_enabled->getValueBool () != true)
	{
		logStream (MESSAGE_WARNING) << "GRB was not passed to executor, as this feature is disabled" << sendLog;
		return -1;
	}
	Rts2Conn *exec;
	exec = getOpenConnection ("EXEC");
	if (exec)
	{
		exec->queCommand (new rts2core::Rts2CommandExecGrb (this, tar_id));
	}
	else if (!queueName)
	{
		logStream (MESSAGE_ERROR) << "FATAL! No executor running to post grb ID " << tar_id << sendLog;
		return -1;
	}
	if (queueName)
	{
		int num = 0;
		rts2core::Rts2CommandQueueNow cmd (this, queueName, tar_id);
		queueCommandForType (DEVICE_TYPE_SELECTOR, cmd, &num);
		if (num == 0)
		{
			logStream (MESSAGE_ERROR) << "FATAL! Cannot find any running selector" << sendLog;
			return -2;
		}
	}
	return 0;
}

int Grbd::commandAuthorized (Rts2Conn * conn)
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

void Grbd::updateSwift (double lastTime, double ra, double dec)
{
	lastSwift->setValueDouble (lastTime);
	sendValueAll (lastSwift);
	lastSwiftRaDec->setValueRaDec (ra, dec);
	sendValueAll (lastSwiftRaDec);
}

void Grbd::updateIntegral (double lastTime, double ra, double dec)
{
	lastIntegral->setValueDouble (lastTime);
	sendValueAll (lastIntegral);
	lastIntegralRaDec->setValueRaDec (ra, dec);
	sendValueAll (lastIntegralRaDec);
}

int main (int argc, char **argv)
{
	Grbd grb (argc, argv);
	return grb.run ();
}
