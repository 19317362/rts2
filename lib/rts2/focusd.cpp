/**
 * Copyright (C) 2005-2009 Petr Kubanek <petr@kubanek.net>
 * Copyright (C) 2005-2007 Stanislav Vitek
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "focusd.h"

#define OPT_START  OPT_LOCAL + 235

using namespace rts2focusd;

Focusd::Focusd (int in_argc, char **in_argv):rts2core::Device (in_argc, in_argv, DEVICE_TYPE_FOCUS, "F0")
{
	temperature = NULL;

	createValue (position, "FOC_POS", "focuser position", true); // reported by focuser, use FOC_TAR to set the target position
	createValue (target, "FOC_TAR", "focuser target position", true, RTS2_VALUE_WRITABLE);

	createValue (defaultPosition, "FOC_DEF", "default target value", true, RTS2_VALUE_WRITABLE);
	createValue (focusingOffset, "FOC_FOFF", "offset from focusing routine", true, RTS2_VALUE_WRITABLE);
	createValue (tempOffset, "FOC_TOFF", "temporary offset for focusing", true, RTS2_VALUE_WRITABLE);

	focusingOffset->setValueFloat (0);
	tempOffset->setValueFloat (0);

	addOption (OPT_START, "start-position", 1, "focuser start position (focuser will be set to this one, if initial position is detected");
}

int Focusd::processOption (int in_opt)
{
	switch (in_opt)
	{
		case OPT_START:
			defaultPosition->setValueCharArr (optarg);
			break;
		default:
			return rts2core::Device::processOption (in_opt);
	}
	return 0;
}

void Focusd::checkState ()
{
	if ((getState () & FOC_MASK_FOCUSING) == FOC_FOCUSING)
	{
		int ret;
		ret = isFocusing ();

		if (ret >= 0)
		{
			setTimeout (ret);
			sendValueAll (position);
		}
		else
		{
			ret = endFocusing ();
			infoAll ();
			setTimeout (USEC_SEC);
			if (ret)
			{
				maskState (DEVICE_ERROR_MASK | FOC_MASK_FOCUSING | BOP_EXPOSURE, DEVICE_ERROR_HW | FOC_SLEEPING, "focusing finished with error");
			}
			else
			{
				maskState (FOC_MASK_FOCUSING | BOP_EXPOSURE, FOC_SLEEPING, "focusing finished without error");
				logStream (MESSAGE_INFO) << "focuser moved to " << position->getValueFloat () << sendLog;
			}
		}
	}
}

int Focusd::initValues ()
{
	addConstValue ("FOC_TYPE", "focuser type", focType);

	if (isnan (defaultPosition->getValueFloat ()))
	{
		// refresh position values
		if (info ())
			return -1;
		target->setValueFloat (getPosition ());
		defaultPosition->setValueFloat (getPosition ());
	}
	else if (isAtStartPosition () == false)
	{
		setPosition (defaultPosition->getValueFloat ());
	}
	else
	{
		target->setValueFloat (getPosition ());
	}

	return rts2core::Device::initValues ();
}

int Focusd::idle ()
{
	checkState ();
	return rts2core::Device::idle ();
}

int Focusd::setPosition (float num)
{
	int ret;
	target->setValueFloat (num);
	sendValueAll (target);
	maskState (FOC_MASK_FOCUSING | BOP_EXPOSURE, FOC_FOCUSING | BOP_EXPOSURE, "focus change started");
	logStream (MESSAGE_INFO) << "changing focuser position to " << num << sendLog;
	ret = setTo (num);
	if (ret)
	{
		logStream (MESSAGE_ERROR) << "cannot set focuser to " << num << sendLog;
		maskState (FOC_MASK_FOCUSING | BOP_EXPOSURE | DEVICE_ERROR_HW, DEVICE_ERROR_HW, "focus change started");
		return ret;
	}
	return ret;
}

int Focusd::autoFocus (Rts2Conn * conn)
{
	/* ask for priority */

	maskState (FOC_MASK_FOCUSING, FOC_FOCUSING, "autofocus started");

	// command ("priority 50");

	return 0;
}

int Focusd::isFocusing ()
{
	int ret;
	time_t now;
	time (&now);
	if (now > focusTimeout)
		return -1;
	ret = info ();
	if (ret)
		return -1;
	if (getPosition () != getTarget ())
		return USEC_SEC;
	return -2;
}

int Focusd::endFocusing ()
{
	return 0;
}

int Focusd::setValue (rts2core::Value * old_value, rts2core::Value * new_value)
{
        float tco= tcOffset();

	if (old_value == target)
	{
		return setPosition (new_value->getValueFloat ())? -2 : 0;
	}
	if (old_value == defaultPosition)
	{
		return setPosition (new_value->getValueFloat () + focusingOffset->getValueFloat () + tempOffset->getValueFloat ()+tco )? -2 : 0;
	}
	if (old_value == focusingOffset)
	{
		return setPosition (defaultPosition->getValueFloat () + new_value->getValueFloat () + tempOffset->getValueFloat ()+tco )? -2 : 0;
	}  
	if (old_value == tempOffset)
	{
		return setPosition (defaultPosition->getValueFloat () + focusingOffset->getValueFloat () + new_value->getValueFloat ()+tco )? -2 : 0;
	}
	return rts2core::Device::setValue (old_value, new_value);
}

int Focusd::scriptEnds ()
{
	tempOffset->setValueFloat (0);
	setPosition (defaultPosition->getValueFloat () + focusingOffset->getValueFloat () + tempOffset->getValueFloat () + tcOffset());
	sendValueAll (tempOffset);
	return rts2core::Device::scriptEnds ();
}

int Focusd::commandAuthorized (Rts2Conn * conn)
{
	if (conn->isCommand ("help"))
	{
		conn->sendMsg ("info  - information about focuser");
		conn->sendMsg ("exit  - exit from connection");
		conn->sendMsg ("help  - print, what you are reading just now");
		return 0;
	}
	return rts2core::Device::commandAuthorized (conn);
}
