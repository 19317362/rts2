/* 
 * Observation entry.
 * Copyright (C) 2005-2007 Petr Kubanek <petr@kubanek.net>
 * Copyright (C) 2007 Stanislav Vitek <standa@iaa.es>
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

#ifndef __RTS2_OBS_DB__
#define __RTS2_OBS_DB__

#include <ostream>
#include <time.h>
#include <vector>

#include "imgdisplay.h"

#include "../utils/timestamp.h"

#include "rts2count.h"
#include "rts2imgset.h"

/**
 * Observation class.
 *
 * This class contains observation. They are (optionaly) linked to images.
 * It can calculate (and display) various statistic on images it contains.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class Rts2Obs
{
	private:
		//! list of images for that observation
		Rts2ImgSet * imgset;
		std::vector < Rts2Count > counts;

		int displayImages;
		int displayCounts;

		bool printHeader;

		int tar_id;
		std::string tar_name;
		char tar_type;
		int obs_id;
		double obs_ra;
		double obs_dec;
		double obs_alt;
		double obs_az;
		double obs_slew;
		double obs_start;
		int obs_state;
		// nan when observations wasn't ended
		double obs_end;

	public:
		/**
		 * Create class from DB
		 */
		Rts2Obs (int in_obs_id);
		/**
		 * Create class from informations provided.
		 *
		 * Handy when creating class from cursor.
		 */
		Rts2Obs (int in_tar_id, const char *in_tar_name, char in_tar_type,
			int in_obs_id, double in_obs_ra, double in_obs_dec,
			double in_obs_alt, double in_obs_az, double in_obs_slew,
			double in_obs_start, int in_obs_state, double in_obs_end);
		virtual ~ Rts2Obs (void);
		int load ();
		int loadImages ();
		int loadCounts ();

		void printObsHeader (std::ostream & _os);

		void printCountsShort (std::ostream & _os);
		void printCounts (std::ostream & _os);
		void printCountsSummary (std::ostream & _os);

		void setPrintImages (int in_printImages)
		{
			displayImages = in_printImages;
		}

		void setPrintCounts (int in_printCounts)
		{
			if (in_printCounts == DISPLAY_SHORT)
				printHeader = false;
			displayCounts = in_printCounts;
		}

		int getTargetId ()
		{
			return tar_id;
		}

		char getTargetType ()
		{
			return tar_type;
		}

		int getObsId ()
		{
			return obs_id;
		}

		double getObsStart ()
		{
			return obs_start;
		}

		double getObsEnd ()
		{
			return obs_end;
		}

		double getObsRa ()
		{
			return obs_ra;
		}

		double getObsDec ()
		{
			return obs_dec;
		}

		Rts2ImgSet *getImageSet ()
		{
			return imgset;
		}

		int getUnprocessedCount ();

		/**
		 * Called to check if we have any unprocessed images
		 * if we don't have any unprocessed images and observation was ended, then
		 * call appopriate mails.
		 *
		 * @return 0 when we don't have more images to process and observation
		 * was already finished, -1 when observation is still ongoing, > 0 when there are
		 * return images to process, but observation was ended.
		 */
		int checkUnprocessedImages (Rts2Block * master);

		int getNumberOfImages ();

		int getNumberOfGoodImages ();

		int getFirstErrors (double &eRa, double &eDec, double &eRad);
		int getAverageErrors (double &eRa, double &eDec, double &eRad);

		/**
		 * Return -1 when previous position cannot
		 */
		int getPrevPosition (struct ln_equ_posn &prevPoz,
			struct ln_hrz_posn &hrzPoz);

		void getEqu (struct ln_equ_posn &equ)
		{
			equ.ra = obs_ra;
			equ.dec = obs_dec;
		}

		void getHrz (struct ln_hrz_posn &hrz)
		{
			hrz.az = obs_az;
			hrz.alt = obs_alt;
		}

		/**
		 * Return average slew speed in arc deg/sec. Return nan when slew speed cannot be determined.
		 */
		double getPrevSeparation ();
		double getSlewSpeed ();

		void maskState (int newBits);
		void unmaskState (int newBits);

		friend std::ostream & operator << (std::ostream & _os, Rts2Obs & obs);
};

std::ostream & operator << (std::ostream & _os, Rts2Obs & obs);

class Rts2ObsState
{
	private:
		int state;
	public:
		Rts2ObsState (int in_state)
		{
			state = in_state;
		}

		friend std::ostream & operator << (std::ostream & _os,
			Rts2ObsState obs_state);
};

std::ostream & operator << (std::ostream & _os, Rts2ObsState obs_state);
#endif							 /* !__RTS2_OBS_DB__ */
