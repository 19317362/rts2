/* 
 * Command classes.
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

#ifndef __RTS2_COMMAND__
#define __RTS2_COMMAND__

#include "rts2block.h"

/**
 * @defgroup RTS2Command Command classes
 */

/** Keep command, do not delete it from after it return. @ingroup RTS2Command */
#define RTS2_COMMAND_KEEP       -2
/** Send command again to device. @ingroup RTS2Command */
#define RTS2_COMMAND_REQUE      -5

typedef enum
{ EXP_LIGHT, EXP_DARK }
exposureType;

/**
 * Miscelanus flag for sending command while exposure is in progress.
 */
#define BOP_WHILE_STATE         0x00001000

/**
 * Call-in-progress mask
 */
#define BOP_CIP_MASK            0x00006000

typedef enum cip_state_t
{
	CIP_NOT_CALLED = 0x00000000,
	CIP_WAIT       = 0x00002000,
	CIP_RUN        = 0x00004000,
	CIP_RETURN     = 0x00006000
};

/**
 * Base class which represents commands send over network to other component.
 * This object is usually send through Rts2Conn::queCommand to connection,
 * which process it, wait for the other side to reply, pass return code to
 * Rts2Conn::commandReturn callback and delete it.
 *
 * @see Rts2Conn
 *
 * @ingroup RTS2Block
 * @ingroup RTS2Command
 */
class Rts2Command
{
	private:
		int bopMask;
		Rts2Object *originator;
	protected:
		Rts2Block * owner;
		Rts2Conn *connection;
		char *text;
	public:
		Rts2Command (Rts2Block * in_owner);
		Rts2Command (Rts2Block * in_owner, char *in_text);
		Rts2Command (Rts2Command * in_command);
		virtual ~ Rts2Command (void);
		int setCommand (char *in_text);
		void setConnection (Rts2Conn * conn)
		{
			connection = conn;
		}
		Rts2Conn *getConnection ()
		{
			return connection;
		}
		virtual int send ();
		int commandReturn (int status, Rts2Conn * conn);
		char *getText ()
		{
			return text;
		}

		/**
		 * Set command Block of OPeration mask.
		 *
		 * @param in_bopMask New BOP mask.
		 */
		void setBopMask (int in_bopMask)
		{
			bopMask = in_bopMask;
		}

		/**
		 * Return command BOP mask.
		 *
		 * @see Rts2Command::setBopMask
		 *
		 * @return Commmand BOP mask.
		 */
		int getBopMask ()
		{
			return bopMask;
		}

		/**
		 * Set call originator.
		 *
		 * @param in_originator Call originator. Call originator is issued
		 *   EVENT_COMMAND_OK or EVENT_COMMAND_FAILED event.
		 *
		 * @see Rts2Conn::queCommand
		 *
		 * @callergraph
		 */
		void setOriginator (Rts2Object * in_originator)
		{
			originator = in_originator;
		}

		/**
		 * Returns status of info call, issued against central server.
		 *
		 * States are:
		 *  - status command was not issued
		 *  - status command was issued
		 *  - status command sucessfulle ended
		 *
		 * @return Status of info call.
		 */
		cip_state_t getStatusCallProgress ()
		{
			return (cip_state_t) (bopMask & BOP_CIP_MASK);
		}

		/**
		 * Sets status of info call.
		 *
		 * @param call_progress Call progress.
		 *
		 * @see Rts2Command::getStatusCallProgress
		 */
		void setStatusCallProgress (cip_state_t call_progress)
		{
			bopMask = (bopMask & ~BOP_CIP_MASK) | (call_progress & BOP_CIP_MASK);
		}

		/**
		 * Called when command returns without error.
		 *
		 * This function is overwritten in childrens to react on
		 * command returning OK.
		 *
		 * @param conn Connection on which command returns.
		 *
		 * @return -1, @ref RTS2_COMMAND_KEEP or @ref RTS2_COMMAND_REQUE.
		 *
		 * @callgraph
		 */
		virtual int commandReturnOK (Rts2Conn * conn);

		/**
		 * Called when command returns with status indicating that it will be
		 * executed later.
		 *
		 * Is is called when device state will allow execution of such command. This
		 * function is overwritten in childrens to react on command which will be
		 * executed later.
		 *
		 * @param conn Connection on which command returns.
		 *
		 * @return -1, @ref RTS2_COMMAND_KEEP or @ref RTS2_COMMAND_REQUE.
		 *
		 * @callgraph
		 */
		virtual int commandReturnQued (Rts2Conn * conn);

		/**
		 * Called when command returns with error.
		 *
		 * This function is overwritten in childrens to react on
		 * command returning OK.
		 *
		 * @param conn Connection on which command returns.
		 *
		 * @return -1, @ref RTS2_COMMAND_KEEP or @ref RTS2_COMMAND_REQUE.
		 *
		 * @callgraph
		 */
		virtual int commandReturnFailed (int status, Rts2Conn * conn);

};

/**
 * Command send to central daemon.
 *
 * @ingroup RTS2Command
 */
class Rts2CentraldCommand:public Rts2Command
{

	public:
		Rts2CentraldCommand (Rts2Block * in_owner,
			char *in_text):Rts2Command (in_owner, in_text)
		{
		}
};

/**
 * Send and process authorization request.
 *
 * @ingroup RTS2Command
 */
class Rts2CommandSendKey:public Rts2Command
{
	private:
		int key;
	public:
		Rts2CommandSendKey (Rts2Block * in_master, int in_key);
		virtual int send ();

		virtual int commandReturnOK (Rts2Conn * conn)
		{
			connection->setConnState (CONN_AUTH_OK);
			return -1;
		}
		virtual int commandReturnFailed (int status, Rts2Conn * conn)
		{
			connection->setConnState (CONN_AUTH_FAILED);
			return -1;
		}
};

/**
 * Receive authorization reply from centrald daemon.
 *
 * @ingroup RTS2Command
 */
class Rts2CommandAuthorize:public Rts2Command
{
	public:
		Rts2CommandAuthorize (Rts2Block * in_master, const char *device_name);
};

/**
 * Common class for all command, which changed camera settings.
 *
 * @ingroup RTS2Command
 */
class Rts2CommandCameraSettings:public Rts2Command
{
	public:
		Rts2CommandCameraSettings (Rts2DevClientCamera * in_camera);
};

/**
 * Start exposure on camera.
 *
 * @ingroup RTS2Command
 */
class Rts2CommandExposure:public Rts2Command
{
	private:
		Rts2DevClientCamera * camera;
	public:
		/**
		 * Send exposure command to device.
		 *
		 * @param in_master Master block which controls exposure
		 * @param in_camera Camera client for exposure
		 * @param in_bopMask BOP mask for exposure command
		 */
		Rts2CommandExposure (Rts2Block * in_master, Rts2DevClientCamera * in_camera, int in_bopMask);

		virtual int commandReturnFailed (int status, Rts2Conn * conn);
};

/**
 * Start data readout.
 *
 * @ingourp RTS2Command
 */
class Rts2CommandReadout:public Rts2Command
{
	public:
		Rts2CommandReadout (Rts2Block * in_master);
};

/**
 * Set filter.
 *
 * @ingroup RTS2Command
 */
class Rts2CommandFilter:public Rts2Command
{
	private:
		Rts2DevClientCamera * camera;
		Rts2DevClientPhot *phot;
		Rts2DevClientFilter *filterCli;
		void setCommandFilter (int filter);
	public:
		Rts2CommandFilter (Rts2DevClientCamera * in_camera, int filter);
		Rts2CommandFilter (Rts2DevClientPhot * in_phot, int filter);
		Rts2CommandFilter (Rts2DevClientFilter * in_filter, int filter);

		virtual int commandReturnOK (Rts2Conn * conn);
		virtual int commandReturnFailed (int status, Rts2Conn * conn);
};

class Rts2CommandBox:public Rts2CommandCameraSettings
{
	public:
		Rts2CommandBox (Rts2DevClientCamera * in_camera, int x, int y,
			int w, int h);
};

class Rts2CommandCenter:public Rts2CommandCameraSettings
{
	public:
		Rts2CommandCenter (Rts2DevClientCamera * in_camera, int width,
			int height);
};

/**
 * Issue command to change value, but do not send return status.
 *
 * @ingroup RTS2Command
 */
class Rts2CommandChangeValueDontReturn:public Rts2Command
{
	public:
		Rts2CommandChangeValueDontReturn (Rts2DevClient * in_client,
			std::string in_valName, char op,
			int in_operand);
		Rts2CommandChangeValueDontReturn (Rts2DevClient * in_client,
			std::string in_valName, char op,
			float in_operand);
		Rts2CommandChangeValueDontReturn (Rts2DevClient * in_client,
			std::string in_valName, char op,
			double in_operand);
		Rts2CommandChangeValueDontReturn (Rts2DevClient * in_client,
			std::string in_valName, char op,
			bool in_operand);
		Rts2CommandChangeValueDontReturn (Rts2DevClient * in_client,
			std::string in_valName, char op,
			std::string in_operand);
};

/**
 * Issue command to change value, send return status and handle it.
 *
 * @ingroup RTS2Command
 */
class Rts2CommandChangeValue:public Rts2CommandChangeValueDontReturn
{
	private:
		Rts2DevClient * client;
	public:
		Rts2CommandChangeValue (Rts2DevClient * in_client, std::string in_valName,
			char op, int in_operand);
		Rts2CommandChangeValue (Rts2DevClient * in_client, std::string in_valName,
			char op, float in_operand);
		Rts2CommandChangeValue (Rts2DevClient * in_client, std::string in_valName,
			char op, double in_operand);
		Rts2CommandChangeValue (Rts2DevClient * in_client, std::string in_valName,
			char op, bool in_operand);
		Rts2CommandChangeValue (Rts2DevClient * in_client, std::string in_valName,
			char op, std::string in_operand);
};

/**
 * Command for telescope movement.
 *
 * @ingroup RTS2Command
 */
class Rts2CommandMove:public Rts2Command
{
	Rts2DevClientTelescope *tel;
	protected:
		Rts2CommandMove (Rts2Block * in_master, Rts2DevClientTelescope * in_tel);
	public:
		Rts2CommandMove (Rts2Block * in_master, Rts2DevClientTelescope * in_tel,
			double ra, double dec);
		virtual int commandReturnFailed (int status, Rts2Conn * conn);
};

/**
 * Move telescope without modelling corrections.
 *
 * @ingroup RTS2Command
 */
class Rts2CommandMoveUnmodelled:public Rts2CommandMove
{
	public:
		Rts2CommandMoveUnmodelled (Rts2Block * in_master,
			Rts2DevClientTelescope * in_tel, double ra,
			double dec);
};

/**
 * Move telescope to fixed location.
 *
 * @ingroup RTS2Command
 */
class Rts2CommandMoveFixed:public Rts2Command
{
	Rts2DevClientTelescope *tel;
	public:
		Rts2CommandMoveFixed (Rts2Block * in_master,
			Rts2DevClientTelescope * in_tel, double ra,
			double dec);
		virtual int commandReturnFailed (int status, Rts2Conn * conn);
};

class Rts2CommandResyncMove:public Rts2Command
{
	Rts2DevClientTelescope *tel;
	public:
		Rts2CommandResyncMove (Rts2Block * in_master,
			Rts2DevClientTelescope * in_tel, double ra,
			double dec);
		virtual int commandReturnFailed (int status, Rts2Conn * conn);
};

class Rts2CommandSearch:public Rts2Command
{
	Rts2DevClientTelescope *tel;
	public:
		Rts2CommandSearch (Rts2DevClientTelescope * in_tel, double searchRadius,
			double searchSpeed);
		virtual int commandReturnFailed (int status, Rts2Conn * conn);
};

class Rts2CommandSearchStop:public Rts2Command
{
	public:
		Rts2CommandSearchStop (Rts2DevClientTelescope * in_tel);
};

class Rts2CommandChange:public Rts2Command
{
	Rts2DevClientTelescope *tel;
	public:
		Rts2CommandChange (Rts2Block * in_master, double ra, double dec);
		Rts2CommandChange (Rts2DevClientTelescope * in_tel, double ra,
			double dec);
		Rts2CommandChange (Rts2CommandChange * in_command,
			Rts2DevClientTelescope * in_tel);
		virtual int commandReturnFailed (int status, Rts2Conn * conn);
};

class Rts2CommandCorrect:public Rts2Command
{
	public:
		Rts2CommandCorrect (Rts2Block * in_master, int corr_mark, double ra,
			double dec, double ra_err, double dec_err);
};

class Rts2CommandStartGuide:public Rts2Command
{
	public:
		Rts2CommandStartGuide (Rts2Block * in_master, char dir, double dir_dist);
};

class Rts2CommandStopGuide:public Rts2Command
{
	public:
		Rts2CommandStopGuide (Rts2Block * in_master, char dir);
};

class Rts2CommandStopGuideAll:public Rts2Command
{
	public:
		Rts2CommandStopGuideAll (Rts2Block * in_master):Rts2Command (in_master)
		{
			setCommand ("stop_guide_all");
		}
};

class Rts2CommandCupolaMove:public Rts2Command
{
	Rts2DevClientCupola *copula;
	public:
		Rts2CommandCupolaMove (Rts2DevClientCupola * in_copula, double ra,
			double dec);
		virtual int commandReturnFailed (int status, Rts2Conn * conn);
};

class Rts2CommandChangeFocus:public Rts2Command
{
	private:
		Rts2DevClientFocus * focuser;
		Rts2DevClientCamera *camera;
		void change (int in_steps);
	public:
		Rts2CommandChangeFocus (Rts2DevClientFocus * in_focuser, int in_steps);
		Rts2CommandChangeFocus (Rts2DevClientCamera * in_camera, int in_steps);
		virtual int commandReturnFailed (int status, Rts2Conn * conn);
};

class Rts2CommandSetFocus:public Rts2Command
{
	private:
		Rts2DevClientFocus * focuser;
		Rts2DevClientCamera *camera;
		void set (int in_steps);
	public:
		Rts2CommandSetFocus (Rts2DevClientFocus * in_focuser, int in_steps);
		Rts2CommandSetFocus (Rts2DevClientCamera * in_camera, int in_steps);
		virtual int commandReturnFailed (int status, Rts2Conn * conn);
};

class Rts2CommandMirror:public Rts2Command
{
	private:
		Rts2DevClientMirror * mirror;
	public:
		Rts2CommandMirror (Rts2DevClientMirror * in_mirror, int in_pos);
		virtual int commandReturnFailed (int status, Rts2Conn * conn);
};

class Rts2CommandIntegrate:public Rts2Command
{
	private:
		Rts2DevClientPhot * phot;
	public:
		Rts2CommandIntegrate (Rts2DevClientPhot * in_phot, int in_filter,
			float in_exp, int in_count);
		Rts2CommandIntegrate (Rts2DevClientPhot * in_phot, float in_exp,
			int in_count);
		virtual int commandReturnFailed (int status, Rts2Conn * conn);
};

class Rts2CommandExecNext:public Rts2Command
{
	public:
		Rts2CommandExecNext (Rts2Block * in_master, int next_id);
};

class Rts2CommandExecNow:public Rts2Command
{
	public:
		Rts2CommandExecNow (Rts2Block * in_master, int now_id);
};

/**
 * Execute GRB observation.
 *
 * @ingroup RTS2Command
 */
class Rts2CommandExecGrb:public Rts2Command
{
	public:
		Rts2CommandExecGrb (Rts2Block * in_master, int grb_id);
};

class Rts2CommandExecShower:public Rts2Command
{
	public:
		Rts2CommandExecShower (Rts2Block * in_master);
};

class Rts2CommandKillAll:public Rts2Command
{
	public:
		Rts2CommandKillAll (Rts2Block * in_master);
};

/**
 * Sends script end command.
 *
 * @ingroup RTS2Command
 */
class Rts2CommandScriptEnds:public Rts2Command
{
	public:
		Rts2CommandScriptEnds (Rts2Block * in_master);
};

class Rts2CommandMessageMask:public Rts2Command
{
	public:
		Rts2CommandMessageMask (Rts2Block * in_master, int in_mask);
};

/**
 * Send info command to central server.
 *
 * @ingroup RTS2Command
 */
class Rts2CommandInfo:public Rts2Command
{
	public:
		Rts2CommandInfo (Rts2Block * in_master);
		virtual int commandReturnOK (Rts2Conn * conn);
		virtual int commandReturnFailed (int status, Rts2Conn * conn);
};

/**
 * Send status info command.
 *
 * When this command return, device status is updated, so updateStatusWait from
 * master is called.
 *
 * @msc
 *
 * Block, Centrald, Devices;
 *
 * Block->Centrald [label="status_info"];
 * Centrald->Devices [label="status_info"];
 * Devices->Centrald [label="S xxxx"];
 * Centrald->Block [label="S xxxx"];
 * Centrald->Block [label="OK"];
 * Devices->Centrald [label="OK"];
 *
 * @endmsc
 *
 * @ingroup RTS2Command
 */
class Rts2CommandStatusInfo:public Rts2Command
{
	private:
		Rts2Conn * control_conn;
		bool keep;
	public:
		Rts2CommandStatusInfo (Rts2Block * master, Rts2Conn * in_control_conn,
			bool in_keep);
		virtual int commandReturnOK (Rts2Conn * conn);
		virtual int commandReturnFailed (Rts2Conn * conn);
};

/**
 * Send device_status command instead of status_info command.
 *
 * @ingroup RTS2Command
 */
class Rts2CommandDeviceStatus:public Rts2CommandStatusInfo
{
	public:
		Rts2CommandDeviceStatus (Rts2Block * master, Rts2Conn * in_control_conn,
			bool in_keep);
};
#endif							 /* !__RTS2_COMMAND__ */
