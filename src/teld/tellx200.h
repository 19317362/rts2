/* 
 * LX200 protocol abstract class.
 * Copyright (C) 2009-2010 Markus Wildi
 * Copyright (C) 2010 Petr Kubanek, Institute of Physics
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

#include "teld.h"
#include "../../lib/rts2/connserial.h"

namespace rts2teld
{
/**
 * Abstract class, providing LX200 protocol services.
 *
 * If command is passed as parameter, it must be a full LX200 command,
 * including end #.
 *
 * @author Markus Wildi @author Petr Kubanek
 */
class TelLX200:public Telescope
{
	public:
		TelLX200 (int argc, char **argv);
		virtual ~TelLX200 (void);

	protected:
		virtual int processOption (int in_opt);
		virtual int init ();

		rts2core::ConnSerial *serConn;

		rts2core::ValueDouble *localTime;

		/**
		 * Reads some value from LX200 in hms format.
 		 *
		 * Utility function for all those read_ra and other.
		 *
		 * @param hmsptr   where hms will be stored
		 * @param command  command
		 * @param allowZ   if time can start with Z for Zulu time
		 *
		 * @return -1 and set errno on error, otherwise 0
		 */
		int tel_read_hms (double *hmsptr, const char *command, bool allowZ = false);

		/**
		 * Reads control unit local time.
		 *
		 * @return -1 and set errno on error, otherwise 0
		 */
		int tel_read_local_time ();

		/**
		 * Reads TelLX200 local sidereal time.
		 *
		 * @return -1 and set errno on error, otherwise 0
		*/
		int tel_read_sidereal_time ();

		/**
		 * Reads current telescope right ascenation.
		 *
 		 * @return -1 and set errno on error, otherwise 0
		 */
		int tel_read_ra ();

		/**
		 * Reads current telescope declination.
		 *
		 * @return -1 and set errno on error, otherwise 0
 		*/
		int tel_read_dec ();

		/**
		 * Reads current telescope altitude.
		 *
		 * @return -1 and set errno on error, otherwise 0
		 */
		int tel_read_altitude ();

		/**
		 * Reads current telescope azimuth.
		 *
		 * @return -1 and set errno on error, otherwise 0
		 */
		int tel_read_azimuth ();

		/**
		 * Reads telescope control system latitude.
		 *
		 * @return -1 on error, otherwise 0
		 */
		int tel_read_latitude ();

		/**
		 * Reads telescope control system longitude.
		 *
		 * @return -1 on error, otherwise 0
		 */
		int tel_read_longitude ();

		/**
		 * Repeats write command, until 1 is retrieved.
		 *
		 * Handy for setting ra and dec.
		 *
		 * @param command	command to write on apgto_fd
		 */
		int tel_rep_write (char *command);

		/**
		 * Set command target right ascenation.
		 *
		 * @param ra	right ascenation to set in decimal degrees
		 *
		 * @return -1 and errno on error, otherwise 0
		 */
		int tel_write_ra (double ra);

		/**
		 * Set TelLX200 declination.
		 *
		 * @param dec	declination to set in decimal degrees
		 *
		 * @return -1 and errno on error, otherwise 0
		 */
		int tel_write_dec (double dec);

		int tel_write_altitude(double alt); 
		int tel_write_azimuth(double az); 

		/**
		 * Set slew rate.
		 *
		 * @param new_rate	new slew speed to set.
		 *
		 * @return -1 on failure & set errno, 5 (>=0) otherwise
		 */
		int tel_set_slew_rate (char new_rate);

		int tel_start_move (char direction);
		int tel_stop_move (char direction);

	private:
		const char *device_file;
		bool connDebug;
};

}
