/* 
 * Dummy telescope for tests.
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <libnova/libnova.h>

#include "telescope.h"
#include "../utils/rts2config.h"

/*!
 * Dummy teld for testing purposes.
 */

class Rts2DevTelescopeDummy:public Rts2DevTelescope
{
	private:
		struct ln_equ_posn dummyPos;
		long countLong;
	protected:

		virtual int isMoving ()
		{
			if (countLong > 12)
				return -2;
			countLong++;
			return USEC_SEC;
		}

		virtual int isMovingFixed ()
		{
			return isMoving ();
		}

		virtual int isSearching ()
		{
			return USEC_SEC;
		}

		virtual int isParking ()
		{
			return isMoving ();
		}
	public:
		Rts2DevTelescopeDummy (int in_argc, char **in_argv):Rts2DevTelescope (in_argc,
			in_argv)
		{
			dummyPos.ra = 0;
			dummyPos.dec = 0;
		}

		virtual int initValues ()
		{
			Rts2Config *config;
			config = Rts2Config::instance ();
			config->loadFile ();
			telLatitude->setValueDouble (config->getObserver ()->lat);
			telLongitude->setValueDouble (config->getObserver ()->lng);
			strcpy (telType, "Dummy");
			return Rts2DevTelescope::initValues ();
		}

		virtual int ready ()
		{
			return 0;
		}

		virtual int info ()
		{
			setTelRaDec (dummyPos.ra, dummyPos.dec);
			return Rts2DevTelescope::info ();
		}

		virtual int startMove ()
		{
			getTarget (&dummyPos);
			countLong = 0;
			return 0;
		}

		virtual int startMoveFixed (double tar_az, double tar_alt)
		{
			getTarget (&dummyPos);
			countLong = 0;
			return 0;
		}

		virtual int startSearch ()
		{
			return 0;
		}

		virtual int stopMove ()
		{
			return 0;
		}

		virtual int startPark ()
		{
			dummyPos.ra = 2;
			dummyPos.dec = 2;
			countLong = 0;
			return 0;
		}

		virtual int endPark ()
		{
			return 0;
		}
};

int
main (int argc, char **argv)
{
	Rts2DevTelescopeDummy device = Rts2DevTelescopeDummy (argc, argv);
	return device.run ();
}
