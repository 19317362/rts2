/*
 * Centrald - RTS2 coordinator
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

/*!
 * @file Server deamon source.
 *
 * Source for server deamon - a something between client and device,
 * what takes care of priorities and authentification.
 *
 * Contains list of clients with their id's and with their access rights.
 */

#ifndef __RTS2_CENTRALD__
#define __RTS2_CENTRALD__

#include "../utils/riseset.h"

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <libnova/libnova.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

#include <config.h>
#include "../utils/rts2daemon.h"
#include "../utils/rts2config.h"
#include "status.h"

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX   255
#endif

class Rts2ConnCentrald;

/**
 * Class for central server.
 *
 * Central server supports following functions:
 *
 * - holds list of active system components
 * - authorizes new connections to the system
 * - activates pool of system BOP mask via "status_info" (Rts2CommandStatusInfo) command
 * - holds and manage connection priorities
 *
 * Centrald is written to be as simple as possible. Commands are ussually
 * passed directly to devices on separate connections, they do not go through
 * centrald.
 *
 * @see Rts2Device
 * @see Rts2DevConnMaster
 * @see Rts2Client
 * @see Rts2ConnCentraldClient
 */
class Rts2Centrald:public Rts2Daemon
{
	private:
		int priority_client;

		int next_event_type;
		time_t next_event_time;
		struct ln_lnlat_posn *observer;

		Rts2ValueBool *morning_off;
		Rts2ValueBool *morning_standby;

		char *configFile;
		std::string logFile;
		// which sets logfile
		enum { LOGFILE_ARG, LOGFILE_DEF, LOGFILE_CNF }
		logFileSource;

		std::ofstream * fileLog;

		void openLog ();
		int reloadConfig ();

		int connNum;

		Rts2ValueString *priorityClient;
		Rts2ValueInteger *priority;

		Rts2ValueTime *nextStateChange;
		Rts2ValueSelection *nextState;
		Rts2ValueDouble *observerLng;
		Rts2ValueDouble *observerLat;

		Rts2ValueDouble *nightHorizon;
		Rts2ValueDouble *dayHorizon;

		Rts2ValueInteger *eveningTime;
		Rts2ValueInteger *morningTime;

	protected:
		/**
		 * @param new_state	new state, if -1 -> 3
		 */
		int changeState (int new_state, const char *user);

		virtual int processOption (int in_opt);

		/**
		 * Those callbacks are for current centrald implementation empty and returns
		 * NULL. They can be used in future to link two centrald to enable
		 * cooperative observation.
		 */
		virtual Rts2Conn *createClientConnection (char *in_deviceName)
		{
			return NULL;
		}

		virtual Rts2Conn *createClientConnection (Rts2Address * in_addr)
		{
			return NULL;
		}

		virtual int init ();
		virtual int initValues ();

		virtual int setValue (Rts2Value *old_value, Rts2Value *new_value);

		virtual void connectionRemoved (Rts2Conn * conn);

	public:
		Rts2Centrald (int argc, char **argv);
		virtual ~ Rts2Centrald (void);

		virtual int idle ();

		/**
		 * Made priority update, distribute messages to devices
		 * about priority update.
		 *
		 * @param timeout	time to wait for priority change..
		 *
		 * @return 0 on success, -1 and set errno otherwise
		 */
		int changePriority (time_t timeout);

		/**
		 * Switch centrald state to ON.
		 *
		 * @param user Name of user who initiated state change.
		 */
		int changeStateOn (const char *user)
		{
			return changeState ((next_event_type + 5) % 6, user);
		}

		/**
		 * Switch centrald to standby.
		 *
		 * @param user Name of user who initiated state change.
		 */
		int changeStateStandby (const char *user)
		{
			return changeState (SERVERD_STANDBY | ((next_event_type + 5) % 6), user);
		}

		/**
		 * Switch centrald to off.
		 *
		 * @param user Name of user who initiated state change.
		 */
		int changeStateOff (const char *user)
		{
			return changeState (SERVERD_OFF, user);
		}
		inline int getPriorityClient ()
		{
			return priority_client;
		}

		virtual Rts2Conn *createConnection (int in_sock);
		void connAdded (Rts2ConnCentrald * added);

		/**
		 * Return connection based on conn_num.
		 *
		 * @param conn_num Connection number, which uniquely identified
		 * every connection.
		 *
		 * @return Connection with given connection number.
		 */
		Rts2Conn *getConnection (int conn_num);

		void sendMessage (messageType_t in_messageType,
			const char *in_messageString);

		virtual void message (Rts2Message & msg);
		void processMessage (Rts2Message & msg)
		{
			sendMessageAll (msg);
		}

		virtual void signaledHUP ();

		void bopMaskChanged ();

		virtual int statusInfo (Rts2Conn * conn);

		/**
		 * Return state of system, as seen from device identified by connection.
		 *
		 * This command return state. It is similar to Rts2Daemon::getState() call.
		 * It result only differ when connection which is asking for state is a
		 * device connection. In this case, BOP mask is composed only from devices
		 * which can block querying device.
		 *
		 * The blocking devices are specified by blocking_by parameter in rts2.ini
		 * file.
		 *
		 * @param conn Connection which is asking for state.
		 */
		int getStateForConnection (Rts2Conn * conn);
};

/**
 * Represents connection from device or client to centrald.
 *
 * It is used in Rts2Centrald.
 */
class Rts2ConnCentrald:public Rts2Conn
{
	private:
		int authorized;
		char login[CLIENT_LOGIN_SIZE];
		Rts2Centrald *master;
		char hostname[HOST_NAME_MAX];
		int port;
		int device_type;

		// number of status commands device have running
		int statusCommandRunning;

		/**
		 * Handle serverd commands.
		 *
		 * @return -2 on exit, -1 and set errno on HW failure, 0 otherwise
		 */
		int command ();
		int commandDevice ();
		int commandClient ();
		// command handling functions
		int priorityCommand ();
		int sendDeviceKey ();
		int sendInfo ();

		/**
		 * Prints standard status header.
		 *
		 * It needs to be called after establishing of every new connection.
		 */
		int sendStatusInfo ();
		int sendAValue (char *name, int value);
		int messageMask;

	protected:
		virtual void setState (int in_value);
	public:
		Rts2ConnCentrald (int in_sock, Rts2Centrald * in_master,
			int in_centrald_id);
		/**
		 * Called on connection exit.
		 *
		 * Delete client|device login|name, updates priorities, detach shared
		 * memory.
		 */
		virtual ~ Rts2ConnCentrald (void);
		virtual int sendMessage (Rts2Message & msg);
		virtual int sendInfo (Rts2Conn * conn);

		virtual void updateStatusWait (Rts2Conn * conn);

		void statusCommandSend ()
		{
			statusCommandRunning++;
		}
};
#endif							 /*! __RTS2_CENTRALD__ */
