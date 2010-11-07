/* 
 * Daemon class.
 * Copyright (C) 2005-2010 Petr Kubanek <petr@kubanek.net>
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

#ifndef __RTS2_DAEMON__
#define __RTS2_DAEMON__

#include "rts2block.h"
#include "rts2logstream.h"
#include "rts2value.h"
#include "rts2valuelist.h"
#include "rts2valuestat.h"
#include "rts2valueminmax.h"
#include "rts2valuerectangle.h"
#include "valuearray.h"

#include <vector>

/**
 * Abstract class for centrald and all devices.
 *
 * This class contains functions which are common to components with one listening socket.
 *
 * @ingroup RTS2Block
 */
class Rts2Daemon:public Rts2Block
{
	public:
		/**
		 * Construct daemon.
		 *
		 * @param _argc         Number of arguments.
		 * @param _argv         Arguments values.
		 * @param _init_state   Initial state.
		 */
		Rts2Daemon (int in_argc, char **in_argv, int _init_state = 0);

		virtual ~ Rts2Daemon (void);
		virtual int run ();

		/**
		 * Init daemon.
		 * This is call to init daemon. It calls @see Rts2Daemon::init and @see Rts2Daemon::initValues
		 * functions to complete daemon initialization.
		 */
		void initDaemon ();
		
		/**
		 * Set timer to send updates every interval seconds. This method is designed to keep user 
		 * infromed about progress of actions which rapidly changes values read by info call.
		 *
		 * @param interval If > 0, set idle info interval to this value. Daemon will then call info method every interval 
		 *   seconds and distribute updates to all connected clients. If <= 0, disables automatic info.
		 *
		 * @see info()
		 */  
		void setIdleInfoInterval (double interval);

		/**
		 * Updates info_time to current time.
		 */
		void updateInfoTime ()
		{
			info_time->setValueDouble (getNow ());
		}

		virtual void postEvent (Rts2Event *event);

		virtual void forkedInstance ();
		virtual void sendMessage (messageType_t in_messageType,
			const char *in_messageString);
		virtual void centraldConnRunning (Rts2Conn *conn);
		virtual void centraldConnBroken (Rts2Conn *conn);

		virtual int baseInfo ();
		int baseInfo (Rts2Conn * conn);
		int sendBaseInfo (Rts2Conn * conn);

		virtual int info ();
		int info (Rts2Conn * conn);
		int infoAll ();
		void constInfoAll ();
		int sendInfo (Rts2Conn * conn, bool forceSend = false);

		int sendMetaInfo (Rts2Conn * conn);

		virtual int setValue (Rts2Conn * conn);

		/**
		 * Return full device state. You are then responsible
		 * to use approproate mask defined in status.h to extract
		 * from state information that you want.
		 *
		 * @return Block state.
		 */
		int getState () { return state; }

		/**
		 * Retrieve value with given name.
		 *
		 * @return NULL if value with specifed name does not exists
		 */
		Rts2Value *getOwnValue (const char *v_name);

		/**
		 * Create new value, and return pointer to it.
		 * It also saves value pointer to the internal values list.
		 *
		 * @param value          Rts2Value which will be created.
		 * @param in_val_name    Value name.
		 * @param in_description Value description.
		 * @param writeToFits    When true, value will be writen to FITS.
		 * @param valueFlags     Value display type, one of the RTS2_DT_xxx constant.
		 * @param queCondition   Conditions in which the change will be put to que.
		 */
		template < typename T > void createValue (
			T * &val,
			const char *in_val_name,
			std::string in_description,
			bool writeToFits = true,
			int32_t valueFlags = 0,
			int queCondition = 0)
		{
			val = new T (in_val_name, in_description, writeToFits, valueFlags);
			addValue (val, queCondition);
		}

		/**
		 * Send new value over the wire to all connections.
		 */
		void sendValueAll (Rts2Value * value);

	protected:
		/**
		 * Delete all saved reference of given value.
		 *
		 * Used to clear saved values when the default value shall be changed.
		 * 
		 * @param val Value which references will be deleted.
		 */
		void deleteSaveValue (Rts2CondValue * val);

		/**
		 * Send progress parameters.
		 */
		void sendProgressAll (double start, double end);

		int checkLockFile (const char *_lock_fname);
		void setNotDaemonize ()
		{
			daemonize = DONT_DAEMONIZE;
		}
		/**
		 * Returns true if daemon should stream debug messages to standart output.
		 *
		 * @return True when debug messages can be printed.
		 */
		bool printDebug ()
		{
			return daemonize == DONT_DAEMONIZE;
		}

		/**
		 * Called before main daemon loop.
		 */
		virtual void beforeRun () {}

		/**
		 * Returns true if connection is running.
		 */
		virtual bool isRunning (Rts2Conn *conn) = 0;

		virtual int checkNotNulls ();

		/**
		 * Call fork to start true daemon. This method must be called
		 * before any threads are created.
		 */
		int doDaemonize ();

		/**
		 * Set lock prefix.
		 *
		 * @param _lockPrefix New lock prefix.
		 */
		void setLockPrefix (const char *_lockPrefix)
		{
			lockPrefix = _lockPrefix;
		}
	
		/**
		 * Return prefix (directory) for lock files.
		 *
		 * @return Prefix for lock files.
		 */
		const char *getLockPrefix ();

		/**
		 * Write to lock file process PID, and lock it.
		 *
		 * @return 0 on success, -1 on error.
		 */
		int lockFile ();

		virtual void addSelectSocks ();
		virtual void selectSuccess ();

		Rts2ValueQueVector queValues;

		/**
		 * Return conditional value entry for given value name.
		 *
		 * @param v_name Value name.
		 *
		 * @return Rts2CondValue for given value, if this exists. NULL if it does not exist.
		 */
		Rts2CondValue *getCondValue (const char *v_name);

		/**
		 * Return conditional value entry for given value.
		 *
		 * @param val Value entry.
		 *
		 * @return Rts2CondValue for given value, if this exists. NULL if it does not exist.
		 */
		Rts2CondValue *getCondValue (const Rts2Value *val);

		/**
		 * Duplicate variable.
		 *
		 * @param old_value Old value of variable.
		 * @param withVal When set to true, variable will be duplicated with value.
		 *
		 * @return Duplicate of old_value.
		 */
		Rts2Value *duplicateValue (Rts2Value * old_value, bool withVal = false);

		/**
		 * Create new constant value, and return pointer to it.
		 *
		 * @param value          Rts2Value which will be created
		 * @param in_val_name    value name
		 * @param in_description value description
		 * @param writeToFits    when true, value will be writen to FITS
		 */
		template < typename T > void createConstValue (T * &val, const char *in_val_name,
			std::string in_description,
			bool writeToFits =
			true, int32_t valueFlags = 0)
		{
			val = new T (in_val_name, in_description, writeToFits, valueFlags);
			addConstValue (val);
		}
		void addConstValue (Rts2Value * value);
		void addConstValue (const char *in_name, const char *in_desc, const char *in_value);
		void addConstValue (const char *in_name, const char *in_desc, std::string in_value);
		void addConstValue (const char *in_name, const char *in_desc, double in_value);
		void addConstValue (const char *in_name, const char *in_desc, int in_value);
		void addConstValue (const char *in_name, const char *in_value);
		void addConstValue (const char *in_name, double in_value);
		void addConstValue (const char *in_name, int in_value);

		/**
		 * Set value. This is the function that get called when user want to change some value, either interactively through
		 * rts2-mon, XML-RPC or from the script. You can overwrite this function in descendants to allow additional variables 
		 * beiing overwritten. If variable has flag RTS2_VALUE_WRITABLE, default implemetation returns sucess. If setting variable
		 * involves some special commands being send to the device, you most probably want to overwrite setValue, and provides
		 * set action for your values in it.
		 *
		 * Suppose you have variables var1 and var2, which you would like to be settable by user. When user set var1, system will just change
		 * value and pick it up next time it will use it. If user set integer value var2, method setVar2 should be called to communicate
		 * the change to the underliing hardware. Then your setValueMethod should looks like:
		 *
		 * @code
		 * class MyClass:public MyParent
		 * {
		 *   ....
		 *   protected:
		 *       virtual int setValue (Rts2Value * old_value, Rts2Value *new_value)
		 *       {
		 *             if (old_value == var1)
		 *                   return 0;
		 *             if (old_value == var2)
		 *                   return setVar2 (new_value->getValueInteger ()) == 0 ? 0 : -2;
		 *             return MyParent::setValue (old_value, new_value);
	         *       }
		 *   ...
		 * };
		 *
		 * @endcode
		 *
		 * @param  old_value	Old value (pointer), can be directly
		 *        accesed with the pointer stored in object.
		 * @param new_value	New value.
		 *
		 * @return 1 when value can be set, but it will take longer
		 * time to perform, 0 when value can be se immediately, -1 when
		 * value set was queued and -2 on an error.
		 */
		virtual int setValue (Rts2Value * old_value, Rts2Value * new_value);

		/**
		 * Perform value changes. Check if value can be changed before performing change.
		 *
		 * @return 0 when value change can be performed, -2 on error, -1 when value change is qued.
		 */
		int setCondValue (Rts2CondValue * old_value_cond, char op, Rts2Value * new_value);

		/**
		 * Really perform value change.
		 *
		 * @param old_cond_value
		 * @param op Operation string. Permited values depends on value type
		 *   (numerical or string), but "=", "+=" and "-=" are ussualy supported.
		 * @param new_value
		 */
		int doSetValue (Rts2CondValue * old_cond_value, char op, Rts2Value * new_value);

		/**
		 * Called after value was changed.
		 *
		 * @param changed_value Pointer to changed value.
		 */
		virtual void valueChanged (Rts2Value *changed_value);

		/**
		 * Returns whenever value change with old_value needs to be qued or
		 * not.
		 *
		 * @param old_value Rts2CondValue object describing the old_value
		 * @param fakeState Server state agains which value change will be checked.
		 */
		bool queValueChange (Rts2CondValue * old_value, int fakeState)
		{
			return old_value->queValueChange (fakeState);
		}

		virtual int processOption (int in_opt);
		virtual int init ();
		virtual int initValues ();
		virtual int idle ();

		/**
		 * Set info time to supplied date. Please note that if you use this function,
		 * you should consider not calling standard info () routine, which updates
		 * info time - just overwrite its implementation with empty body.
		 *
		 * @param _date  Date of the infotime.
		 */
		void setInfoTime (struct tm *_date);

		/**
		 * Set infotime from time_t structure.
		 *
		 * @param _time Time_t holding time (in seconds from 1-1-1970) to which set info time.
		 */
		void setInfoTime (time_t _time)
		{
			info_time->setValueInteger (_time);
		}

		/**
		 * Get time from last info time in seconds (and second fractions).
		 *
		 * @return Difference from last info time in seconds. 86400 (one day) if info time was not defined.
		 */
		double getLastInfoTime ()
		{
			if (isnan (info_time->getValueDouble ()))
				return 86400;
			return getNow () - info_time->getValueDouble ();	
		}


		/**
		 * Called to set new state value
		 */
		void setState (int new_state, const char *description);

		/**
		 * Called when state of the device is changed.
		 *
		 * @param new_state   New device state.
		 * @param old_state   Old device state.
		 * @param description Text description of state change.
		 */
		virtual void stateChanged (int new_state, int old_state, const char *description);

		/**
		 * Called from idle loop after HUP signal occured.
		 *
		 * This is most probably callback you needed for handling HUP signal.
		 * Handling HUP signal when it occurs can be rather dangerous, as it might
		 * reloacte memory location - if you read pointer before HUP signal and use
		 * it after HUP signal, RTS2 does not guarantee that it will be still valid.
		 */
		virtual void signaledHUP ();

		/**
		 * Called when state is changed.
		 */
		void maskState (int state_mask, int new_state, const char *description = NULL, double start = rts2_nan ("f"), double end = rts2_nan ("f"));

		/**
		 * Return daemon state without ERROR information.
		 */
		int getDaemonState ()
		{
			return state & ~DEVICE_ERROR_MASK;
		}

		/**
		 * Call external script on trigger. External script gets in
		 * enviroment values current RTS2 values as
		 * RTS2_{device_name}_{value}, and trigger reason as first
		 * argument. This call creates new connection. Everything
		 * outputed to standart error will be logged as WARN message.
		 * Everything outputed to standart output will be logged as
		 * DEBUG message.
		 *
		 * @param reason Trigger name.
		 */
		void execTrigger (const char *reason);

		/**
		 * Get daemon local weather state. Please use isGoodWeather()
		 * to test for system weather state.
		 *
		 * @ returns true if weather state is good.
		 */
		bool getWeatherState ()
		{
			return (getState () & WEATHER_MASK) == GOOD_WEATHER;
		}

		/**
		 * Set weather state.
		 *
		 * @param good_weather If true, weather is good.
		 * @param msg Text message for state transition - in case of bad weather, it will be recorded in centrald
		 */
		void setWeatherState (bool good_weather, const char *msg)
		{
			if (good_weather)
				maskState (WEATHER_MASK, GOOD_WEATHER, msg);
			else
				maskState (WEATHER_MASK, BAD_WEATHER, msg);
		}

		virtual void sigHUP (int sig);

		/**
		 * Called to verify that the infoAll call can be called.
		 *
		 * @return True if infoAll can be called. When returns False, infoAll will not be called from timer.
		 *
		 * @see setIdleInfoInterval()
		 */
		virtual bool canCallInfoFromTimer () { return true; }

	private:
		// 0 - don't daemonize, 1 - do daemonize, 2 - is already daemonized, 3 - daemonized & centrald is running, don't print to stdout
		enum { DONT_DAEMONIZE, DO_DAEMONIZE, IS_DAEMONIZED, CENTRALD_OK } daemonize;
		int listen_sock;
		void addConnectionSock (int in_sock);
		const char * lock_fname;
		int lock_file;

		// daemon state
		int state;

		Rts2CondValueVector values;
		// values which do not change, they are send only once at connection
		// initialization
		Rts2ValueVector constValues;

		Rts2ValueTime *info_time;

		double idleInfoInterval;

		/**
		 * Adds value to list of values supported by daemon.
		 *
		 * @param value Rts2Value which will be added.
		 */
		void addValue (Rts2Value * value, int queCondition = 0);

		bool doHupIdleLoop;

		/**
		 * Prefix (directory) for lock file.
		 */
		const char *lockPrefix;
};
#endif							 /* ! __RTS2_DAEMON__ */
