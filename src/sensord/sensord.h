/* 
 * Basic sensor class.
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

#ifndef __RTS2_SENSORD__
#define __RTS2_SENSORD__

#include "../utils/device.h"

/**
 * Abstract sensors, SensorWeather with functions to set weather state, and various other sensors.
 */
namespace rts2sensord
{

/**
 * Class for a sensor. Sensor can be any device which produce some information
 * which RTS2 can use.
 *
 * For special devices, which are ussually to be found in an observatory,
 * please see special classes (Dome, Camera, Telescope etc..).
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class Sensor:public rts2core::Device
{
	public:
		Sensor (int argc, char **argv);
		virtual ~ Sensor (void);
};

/**
 * Sensor for weather devices. It provides timeout variable. If timeout is set
 * to some time in future, the device will set WEATHER_BAD flag up to this
 * time.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class SensorWeather:public Sensor
{
	public:
		/**
		 * Construct new SensorWeather instance.
		 *
		 * @param _timeout Weather timeout when device is started. Shall be set to value above
		 * readout of first data from device.
		 */
		SensorWeather (int argc, char **argv, int _timeout = 120);

		double getNextGoodWeather ()
		{
			return nextGoodWeather->getValueDouble ();
		}

		void setWeatherTimeout (time_t wait_time, const char *msg);
	protected:
		virtual int idle ();

		virtual bool isGoodWeather ();
	private:
		Rts2ValueTime *nextGoodWeather;
};

};

#endif							 /* !__RTS2_SENSORD__ */
