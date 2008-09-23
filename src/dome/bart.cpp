/* 
 * Driver for Ford boards.
 * Copyright (C) 2007-2008 Petr Kubanek <petr@kubanek.net>
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

#include <termios.h>
#include <fcntl.h>

#include <sys/ioctl.h>

#include <vector>

#include "ford.h"

#define CAS_NA_OTEVRENI              30

typedef enum
{
	ZASUVKA_1, ZASUVKA_2, ZASUVKA_3, ZASUVKA_4, ZASUVKA_5, ZASUVKA_6, MOTOR,
	SMER, SVETLO, KONCAK_ZAVRENI_JIH, KONCAK_OTEVRENI_JIH,
	KONCAK_OTEVRENI_SEVER, KONCAK_ZAVRENI_SEVER
} vystupy;

#define NUM_ZAS   5

#define OFF   0
#define STANDBY   1
#define OBSERVING 2

// zasuvka c.1 kamera, c.3 kamera, c.5 montaz. c.6 topeni /Ford 21.10.04
int zasuvky_index[NUM_ZAS] =
{ ZASUVKA_1, ZASUVKA_2, ZASUVKA_3, ZASUVKA_5, ZASUVKA_6 };

enum stavy
{ ZAS_VYP, ZAS_ZAP };

// prvni index - cislo stavu (0 - off, 1 - standby, 2 - observing), druhy je stav zasuvky cislo j)
enum stavy zasuvky_stavy[3][NUM_ZAS] =
{
	// off
	{ZAS_ZAP, ZAS_ZAP, ZAS_VYP, ZAS_ZAP, ZAS_VYP},
	// standby
	{ZAS_ZAP, ZAS_ZAP, ZAS_ZAP, ZAS_ZAP, ZAS_ZAP},
	// observnig
	{ZAS_ZAP, ZAS_ZAP, ZAS_ZAP, ZAS_ZAP, ZAS_ZAP}
};

using namespace rts2dome;


namespace rts2dome
{

/**
 * Class for BART dome control.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class Bart:public Ford
{
	private:
		unsigned char spinac[2];

		int handle_zasuvky (int zas);

		// if true, after we open close dome - used as safety measurement as
		// BART roof needs to be fully opened when commanded close to close properly
		bool closeAfterOpen;

		Rts2ValueInteger *sw_state;

	protected:
		virtual int init ();

		virtual int info ();

		virtual int startOpen ();
		virtual long isOpened ();
		virtual int endOpen ();
		virtual int startClose ();
		virtual long isClosed ();
		virtual int endClose ();

		virtual int observing ();
		virtual int standby ();
		virtual int off ();

	public:
		Bart (int argc, char **argv);
		virtual ~Bart ();

		virtual int changeMasterState (int new_state);
};

}

Bart::Bart (int argc, char **argv)
:Ford (argc, argv)
{
	createValue (sw_state, "sw_state", "state of dome switches", false, RTS2_DT_HEX);
	closeAfterOpen = false;
}


Bart::~Bart ()
{
}


int
Bart::startOpen ()
{
	if (!isOn (KONCAK_OTEVRENI_JIH))
		return endOpen ();
	VYP (MOTOR);
	sleep (1);
	VYP (SMER);
	sleep (1);
	ZAP (MOTOR);
	logStream (MESSAGE_DEBUG) << "oteviram strechu" << sendLog;
	return 0;
}


long
Bart::isOpened ()
{
	int ret;
	ret = zjisti_stav_portu ();
	if (ret)
		return ret;
	if (isOn (KONCAK_OTEVRENI_JIH))
		return USEC_SEC;
	return -2;
}


int
Bart::endOpen ()
{
	VYP (MOTOR);
	zjisti_stav_portu ();		 //kdyz se to vynecha, neposle to posledni prikaz nebo znak
	setTimeout (USEC_SEC);
	if (closeAfterOpen)
	{
		sleep (2);
		return domeCloseStart ();
	}
	return 0;
}


int
Bart::startClose ()
{
	int motor;
	int smer;
	if (!isOn (KONCAK_ZAVRENI_JIH))
		return endClose ();
	motor = isOn (MOTOR);
	smer = isOn (SMER);
	if (motor == -1 || smer == -1)
	{
		// errror
		return -1;
	}
	if (!motor)
	{
		// closing in progress
		if (!smer)
			return 0;
		// let it go to open state and then close it..
		if (closeAfterOpen == false)
		{
			closeAfterOpen = true;
			logStream (MESSAGE_DEBUG) << "Commanded to close just afer dome finished opening." << sendLog;
		}
		return 0;
	}
	ZAP (SMER);
	sleep (1);
	ZAP (MOTOR);
	logStream (MESSAGE_DEBUG) << "zaviram strechu" << sendLog;

	return 0;
}


long
Bart::isClosed ()
{
	int ret;
	ret = zjisti_stav_portu ();
	if (ret)
		return ret;
	if (isOn (KONCAK_ZAVRENI_JIH))
		return USEC_SEC;
	return -2;
}


int
Bart::endClose ()
{
	int motor;
	motor = isOn (MOTOR);
	closeAfterOpen = false;
	if (motor == -1)
		return -1;
	if (motor)
		return 0;
	VYP (MOTOR);
	sleep (1);
	VYP (SMER);
	zjisti_stav_portu ();
	return 0;
}


int
Bart::init ()
{
	int ret = Ford::init ();
	if (ret)
		return ret;


	// get state
	ret = zjisti_stav_portu ();
	if (ret)
		return -1;

	if (isOn (KONCAK_OTEVRENI_JIH) && !isOn (KONCAK_ZAVRENI_JIH))
		setState (DOME_CLOSED, "dome is closed");
	else if (!isOn (KONCAK_OTEVRENI_JIH) && isOn (KONCAK_ZAVRENI_JIH))
		setState (DOME_OPENED, "dome is opened");

	return 0;
}


int
Bart::handle_zasuvky (int zas)
{
	int i;
	for (i = 0; i < NUM_ZAS; i++)
	{
		int zasuvka_num = zasuvky_index[i];
		if (zasuvky_stavy[zas][i] == ZAS_VYP)
		{
			ZAP (zasuvka_num);
		}
		else
		{
			VYP (zasuvka_num);
		}
		sleep (1);				 // doplnil Ford
	}
	return 0;
}


int
Bart::info ()
{
	int ret;
	ret = zjisti_stav_portu ();
	if (ret)
		return -1;
	sw_state->setValueInteger (!getPortState (KONCAK_OTEVRENI_JIH));
	sw_state->setValueInteger (sw_state->getValueInteger () | (!getPortState (SMER) << 1));
	sw_state->setValueInteger (sw_state->getValueInteger () | (!getPortState (KONCAK_ZAVRENI_JIH) << 2));
	sw_state->setValueInteger (sw_state->getValueInteger () | (!getPortState (MOTOR) << 3));
	return Ford::info ();
}


int
Bart::off ()
{
	Ford::off ();
	return handle_zasuvky (OFF);
}


int
Bart::standby ()
{
	Ford::standby ();
	return handle_zasuvky (STANDBY);
}


int
Bart::observing ()
{
	handle_zasuvky (OBSERVING);
	return Ford::observing ();
}


int
Bart::changeMasterState (int new_state)
{
	if ((new_state & SERVERD_STANDBY_MASK) == SERVERD_STANDBY)
	{
		switch (new_state & SERVERD_STATUS_MASK)
		{
			case SERVERD_EVENING:
			case SERVERD_MORNING:
			case SERVERD_DUSK:
			case SERVERD_NIGHT:
			case SERVERD_DAWN:
				standby ();
				break;
			default:
				off ();
		}
	}
	else
	{
		switch (new_state & SERVERD_STATUS_MASK)
		{
			case SERVERD_EVENING:
			case SERVERD_NIGHT:
			case SERVERD_DUSK:
			case SERVERD_DAWN:
				observing ();
				break;
			case SERVERD_MORNING:
				standby ();
				break;
			default:
				off ();
		}
	}
	return Ford::changeMasterState (new_state);
}


int
main (int argc, char **argv)
{
	Bart device = Bart (argc, argv);
	return device.run ();
}
