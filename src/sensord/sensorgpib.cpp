/* 
 * Class for GPIB sensors.
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

#include "sensorgpib.h"

#include "conngpiblinux.h"
#include "conngpibenet.h"

using namespace rts2sensord;


int
Gpib::processOption (int _opt)
{
	switch (_opt)
	{
		case 'm':
			minor = atoi (optarg);
			break;
		case 'p':
			pad = atoi (optarg);
			break;
		case 'n':
			enet_addr = new HostString (optarg, "5000");
			break;
		default:
			return Sensor::processOption (_opt);
	}
	return 0;
}


int
Gpib::init ()
{
	int ret;
	ret = Sensor::init ();
	if (ret)
		return ret;

	// create connGpin
	if (enet_addr != NULL)
	{
		if (pad < 0)
		{
			std::cerr << "unknown pad number" << std::endl;
			return -1;
		}
		connGpib = new ConnGpibEnet (this, enet_addr->getHostname (), enet_addr->getPort (), pad);
	}
	else if (pad >= 0)
	{
		connGpib = new ConnGpibLinux (minor, pad);
	}
	// enet
	else
	{
		std::cerr << "Device connection was not specified, exiting" << std::endl;
	}

	try
	{
		connGpib->initGpib ();
	}
	catch (rts2core::Error er)
	{
		logStream (MESSAGE_ERROR) << er << sendLog;
		return -1;
	}
	return 0;
}


Gpib::Gpib (int argc, char **argv):Sensor (argc, argv)
{
	minor = 0;
	pad = -1;
	enet_addr = NULL;

	connGpib = NULL;

	addOption ('m', NULL, 1, "board number (default to 0)");
	addOption ('p', NULL, 1, "device number (counted from 0, not from 1)");
	addOption ('n', NULL, 1, "network adress (and port) of NI GPIB-ENET interface");
}


Gpib::~Gpib (void)
{
	delete connGpib;
}
