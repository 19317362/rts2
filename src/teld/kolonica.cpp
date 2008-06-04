/* 
 * Driver for Chermelin Telescope Mounting at the Kolonica Observatory, Slovak Republic
 * Copyright (C) 2008 Petr Kubanek <petr@kubanek.net>
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

#include "fork.h"

#include "../utils/rts2connserial.h"

/**
 * Driver for Kolonica telescope.
 *
 * @author Petr Kubanek <peter@kubanek.net>
 * @author Marek Chrastina
 */
class TelKolonica:public TelFork
{
	private:
		const char *telDev;
		Rts2ConnSerial *telConn;

		/**
		 * Convert long integer to binary. Most important byte is at
		 * first place, e.g. it's low endian.
		 * 
		 * @param num Number which will be converted. 
		 * @param buf Buffer where converted string will be printed.
		 * @param len Length of expected binarry buffer.
		 */
		void intToBin (unsigned long num, char *buf, int len);

		/**
		 * Wait for given number of (msec?).
		 *
		 * @param num  Number of units to wait.
		 *
		 * @return -1 on failure, 0 on success.
		 */
		int telWait (long num);

		/**
		 * Change commanding axis. Issue G command to change
		 * commanding axis.
		 *
		 * @param axId -1 for global (all axis) commanding, 1-16 are then
		 * valid axis numbers.
		 *
		 * @return -1 on failure, 0 on success.
		 */
		int changeAxis (int axId);

		/**
		 * Set axis target position to given value.
		 *
		 * @param axId   Id of the axis, -1 for global.
		 * @param value  Value to which axis will be set.
		 * 
		 * @return -1 on error, 0 on sucess.
		 */
		int setAltAxis (long value);

		/**
		 * Set axis target position to given value.
		 *
		 * @param axId   Id of the axis, -1 for global.
		 * @param value  Value to which axis will be set.
		 * 
		 * @return -1 on error, 0 on sucess.
		 */
		int setAzAxis (long value);

		/**
		 * Switch motor on/off.
		 *
		 * @param axId Axis ID.
		 * @param on True if motor shall be set to on.
		 *
		 * @return -1 on error, 0 on success.
		 */
		int setMotor (int axId, bool on);

		Rts2ValueLong *axAlt;
		Rts2ValueLong *axAz;

		Rts2ValueBool *motorAlt;
		Rts2ValueBool *motorAz;

	protected:
		virtual int processOption (int in_opt);
		virtual int init ();


		virtual int startMove();
		virtual int stopMove();
		virtual int startPark();
		virtual int endPark();
		virtual int updateLimits();
		virtual int getHomeOffset(int32_t&);

		virtual int setValue (Rts2Value *old_value, Rts2Value *new_value);

	public:
		TelKolonica (int in_argc, char **in_argv);
		virtual ~TelKolonica (void);
};


void
TelKolonica::intToBin (unsigned long num, char *buf, int len)
{
	int i;
      
        for(i = len - 1 ; i >= 0 ; i--)  {
		buf[i] = num & 0xff;
		num >>= 8;
	}
}


int
TelKolonica::telWait (long num)
{
	char buf[5];
	buf[0] = 'W';
	intToBin (num, buf + 1, 3);
	buf[4] = '\r';
	return telConn->writePort (buf, 3);
}


int
TelKolonica::changeAxis (int axId)
{
	char buf[3];
	buf[0] = 'X';
	if (axId < 0)
	{
		buf[1] = 'G';
	}
	else
	{
		intToBin ((unsigned long) axId, buf + 1, 1);
	}
	buf[2] = '\r';
	if (telConn->writePort (buf, 3))
		return -1;
	return telWait (150);	
}


int
TelKolonica::setAltAxis (long value)
{
	char buf[5];

	if (changeAxis (1))
		return -1;

	long diff = value - axAlt->getValueLong ();
	if (diff < 0)
	{
		buf[0] = 'B';
		diff *= -1;
	}
	else
	{
		buf[0] = 'F';
	}
	intToBin ((unsigned long) diff, buf + 1, 3);
	buf[4] = '\r';
	return telConn->writePort (buf, 5);
}


int
TelKolonica::setAzAxis (long value)
{
 	char buf[5];

	if (changeAxis (2))
		return -1;

	long diff = value - axAz->getValueLong ();
	if (diff < 0)
	{
		buf[0] = 'B';
		diff *= -1;
	}
	else
	{
		buf[0] = 'F';
	}
	intToBin ((unsigned long) diff, buf + 1, 3);
	buf[4] = '\r';
	return telConn->writePort (buf, 5);
}


int
TelKolonica::setMotor (int axId, bool on)
{
	char buf[3];

	if (changeAxis (axId))
		return -1;

	buf[0] = on ? 'C' : 'T';
        intToBin (16, buf + 1, 1);
	buf[2] = '\r';
	return telConn->writePort (buf, 3);
}


int
TelKolonica::processOption (int in_opt)
{
	switch (in_opt)
	{
		case 'f':
			telDev = optarg;
			break;
		default:
			return TelFork::processOption (in_opt);
	}
	return 0;
}


int
TelKolonica::init ()
{
	int ret;
	ret = TelFork::init ();
	if (ret)
		return ret;
	
	telConn = new Rts2ConnSerial (telDev, this, BS4800, C8, NONE, 50);
	ret = telConn->init ();
	if (ret)
		return ret;

	telConn->setDebug ();
	telConn->flushPortIO ();
	
	// check that telescope is working
	return info ();
}


int
TelKolonica::startMove()
{
	return -1;
}


int
TelKolonica::stopMove()
{
	return -1;
}


int
TelKolonica::startPark()
{
	return -1;
}


int
TelKolonica::endPark()
{
	return -1;
}


int
TelKolonica::updateLimits()
{
	return -1;
}


int
TelKolonica::getHomeOffset(int32_t&)
{
	return 0;
}


int
TelKolonica::setValue (Rts2Value *old_value, Rts2Value *new_value)
{
	if (old_value == axAlt)
	{
		return setAltAxis (new_value->getValueLong ()) == 0 ? 0 : -2;
	}
	if (old_value == axAz)
	{
		return setAzAxis (new_value->getValueLong ()) == 0 ? 0 : -2;
	}
	if (old_value == motorAlt)
	{
		return setMotor (1, ((Rts2ValueBool *) new_value)->getValueBool ()) == 0 ? 0 : -2;
	}
	if (old_value == motorAz)
	{
		return setMotor (2, ((Rts2ValueBool *) new_value)->getValueBool ()) == 0 ? 0 : -2;
	}
	return TelFork::setValue (old_value, new_value);
}


TelKolonica::TelKolonica (int in_argc, char **in_argv)
:TelFork (in_argc, in_argv)
{
	telDev = "/dev/ttyS0";
	telConn = NULL;

	createValue (axAlt, "AXALT", "altitude axis", true);
	createValue (axAz, "AXAZ", "azimuth axis", true);

	createValue (motorAlt, "motor_alt", "altitude motor", false);
	createValue (motorAz, "motor_az", "azimuth motor", false);

	addOption ('f', NULL, 1, "telescope device (default to /dev/ttyS0)");
}


TelKolonica::~TelKolonica (void)
{
	delete telConn;
}

int
main (int argc, char **argv)
{
	TelKolonica device = TelKolonica (argc, argv);
	return device.run ();
}
