/* 
 * Simple script executor.
 * Copyright (C) 2007,2009 Petr Kubanek <petr@kubanek.net>
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

#ifndef __SCRIPT_EXEC__
#define __SCRIPT_EXEC__

#include "../../lib/rts2script/scripttarget.h"
#include "../../lib/rts2script/scriptinterface.h"
#include "../../lib/rts2/connfork.h"
#include "../../lib/rts2/rts2client.h"
#include "../../lib/rts2/rts2target.h"

namespace rts2script
{
	class ScriptTarget;
}

namespace rts2plan
{

/**
 * This is main client class. It takes care of supliing right
 * devclients and other things.
 */
class ScriptExec:public Rts2Client, public rts2script::ScriptInterface
{
	public:
		ScriptExec (int in_argc, char **in_argv);
		virtual ~ ScriptExec (void);

		virtual int findScript (std::string deviceName, std::string & buf);

		virtual rts2core::Rts2DevClient *createOtherType (Rts2Conn * conn, int other_device_type);

		virtual void postEvent (Rts2Event * event);

		virtual void deviceReady (Rts2Conn * conn);

		virtual int idle ();
		virtual void deviceIdle (Rts2Conn * conn);

		virtual void getPosition (struct ln_equ_posn *pos, double JD);
	protected:
		virtual int processOption (int in_opt);
		virtual void usage ();

		virtual int init ();
		virtual int doProcessing ();
	private:
		rts2core::ValueString *expandPath;
		std::vector < rts2script::ScriptForDevice* > scripts;
		char *deviceName;
		const char *defaultScript;

		int waitState;

		rts2script::ScriptTarget *currentTarget;

		time_t nextRunningQ;

		bool isScriptRunning ();

		char *configFile;

		bool callScriptEnd;

		rts2core::ConnFork *afterImage;
};

}

#endif							 /* !__SCRIPT_EXEC__ */
