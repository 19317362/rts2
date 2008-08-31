/* 
 * Target classes.
 * Copyright (C) 2003-2007 Petr Kubanek <petr@kubanek.net>
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

#ifndef __RTS_TARGETDB__
#define __RTS_TARGETDB__

#include "imgdisplay.h"
#include <errno.h>
#include <libnova/libnova.h>
#include <ostream>
#include <stdio.h>
#include <time.h>

#include "status.h"

#include "../utils/infoval.h"
#include "../utils/objectcheck.h"
#include "../utils/rts2device.h"
#include "../utils/rts2target.h"

#include "rts2taruser.h"
#include "rts2targetset.h"

#include "scriptcommands.h"

#define MAX_READOUT_TIME    120
#define PHOT_TIMEOUT         10
#define EXPOSURE_TIMEOUT     50

#define TARGET_DARK           1
#define TARGET_FLAT           2
#define TARGET_FOCUSING       3
// orimary model
#define TARGET_MODEL          4
// primary terestial - based on HAM/FRAM number
#define TARGET_TERRESTIAL     5
// master calibration target
#define TARGET_CALIBRATION    6
// master plan target
#define TARGET_PLAN           7

#define TARGET_SWIFT_FOV     10
#define TARGET_INTEGRAL_FOV  11
#define TARGET_SHOWER        12

class Rts2Obs;
class Rts2TargetSet;
class Rts2Image;

/**
 * Class for one observation.
 *
 * It is created when it's possible to observer such target, and when such
 * observation have some scientific value.
 *
 * It's put on the observation que inside planc. It's then executed.
 *
 * During execution it can do centering (acquiring images to better know scope position),
 * it can compute importance of future observations.
 *
 * After execution planc select new target for execution, run image processing
 * on first observation target in the que.
 *
 * After all images are processed, new priority can be computed, some results
 * (e.g. light curves) can be put to the database.
 *
 * Target is backed by observation entry in the DB.
 */
class Target:public Rts2Target
{
	private:
		Rts2Obs * observation;

		int type;				 // light, dark, flat, flat_dark

		int startCalledNum;		 // how many times startObservation was called - good to know for targets
		double airmassScale;

		time_t observationStart;

		Rts2TarUser *targetUsers;

		// which changes behaviour based on how many times we called them before

								 // send mail to users..
		void sendTargetMail (int eventMask, const char *subject_text, Rts2Block * master);

		double minObsAlt;

		float tar_priority;
		float tar_bonus;
		time_t tar_bonus_time;
		time_t tar_next_observable;

		void writeAirmass (std::ostream & _os, double jd);

		void printAltTable (std::ostream & _os, double jd_start, double h_start,
			double h_end, double h_step = 1.0, bool header = true);
		void printAltTable (std::ostream & _os, double JD);

	protected:
		char *target_comment;
		struct ln_lnlat_posn *observer;

		virtual int getDBScript (const char *camera_name, std::string & script);

								 // get called when target was selected to update bonuses etc..
		virtual int selectedAsGood ();
	public:
		Target (int in_tar_id, struct ln_lnlat_posn *in_obs);
		// create new target. Save will save it to the database..
		Target ();
		virtual ~ Target (void);
		void logMsgDb (const char *message, messageType_t msgType);
		void getTargetSubject (std::string & subj);

		// that method is GUARANTIE to be called after target creating to load data from DB
		virtual int load ();
		// load target data from give target id
		int loadTarget (int in_tar_id);
		virtual int save (bool overwrite);
		virtual int save (bool overwrite, int tar_id);
		virtual int getScript (const char *device_name, std::string & buf);
		int setScript (const char *device_name, const char *buf);
		struct ln_lnlat_posn *getObserver ()
		{
			return observer;
		}

		// return target semi-diameter, in degrees
		double getSDiam ()
		{
			return getSDiam (ln_get_julian_from_sys ());
		}

		virtual double getSDiam (double JD)
		{
			// 30 arcsec..
			return 30.0 / 3600.0;
		}

		/**
		 * Return target horizontal coordinates for actual system time.
		 *
		 * @param hrz Returned coordinates.
		 *
		 * @return 0 if coordinates can be calculated.
		 */
		void getAltAz (struct ln_hrz_posn *hrz)
		{
			getAltAz (hrz, ln_get_julian_from_sys ());
		}

		/**
		 * Return target horizontal coordinates for given date.
		 *
		 * @param hrz Returned coordinates.
		 * @param JD  Julian data for which target coordinates will be calculated.
		 */
		virtual void getAltAz (struct ln_hrz_posn *hrz, double JD);

		/**
		 * Returns target minimal and maximal altitude during
		 * given time period. This method may return negative values
		 * for both minimal and maximal altitude - those indicate that
		 * target remain bellow horizont for the whole period.
		 *
		 * @param _start JD of start period.
		 * @param _end   JD of end period.
		 * @param _min   Minimal altitude during period.
		 * @param _max   Maximal altitude during period.
		 */
		void getMinMaxAlt (double _start, double _end, double &_min, double &_max);

		/**
		 * Return target minimal observational altitude.
		 *
		 * @return Minimal target observational altitude in degrees.
		 */
		virtual double getMinObsAlt ()
		{
			return minObsAlt;
		}

		int getGalLng (struct ln_gal_posn *gal)
		{
			return getGalLng (gal, ln_get_julian_from_sys ());
		}
		virtual int getGalLng (struct ln_gal_posn *gal, double JD);

		double getGalCenterDist ()
		{
			return getGalCenterDist (ln_get_julian_from_sys ());
		}
		double getGalCenterDist (double JD);

		double getAirmass ()
		{
			return getAirmass (ln_get_julian_from_sys ());
		}
		virtual double getAirmass (double JD);

		double getZenitDistance ()
		{
			return getZenitDistance (ln_get_julian_from_sys ());
		}

		double getZenitDistance (double JD);

		/**
		 * Returns target HA in arcdeg, e.g. in same value as target RA is given.
		 */
		double getHourAngle ()
		{
			return getHourAngle (ln_get_julian_from_sys ());
		}

		double getHourAngle (double JD);

		double getDistance (struct ln_equ_posn *in_pos)
		{
			return getDistance (in_pos, ln_get_julian_from_sys ());
		}

		double getDistance (struct ln_equ_posn *in_pos, double JD);

		double getRaDistance (struct ln_equ_posn *in_pos)
		{
			return getRaDistance (in_pos, ln_get_julian_from_sys ());
		}

		double getRaDistance (struct ln_equ_posn *in_pos, double JD);

		double getSolarDistance ()
		{
			return getSolarDistance (ln_get_julian_from_sys ());
		}

		double getSolarDistance (double JD);

		double getSolarRaDistance ()
		{
			return getSolarRaDistance (ln_get_julian_from_sys ());
		}

		double getSolarRaDistance (double JD);

		double getLunarDistance ()
		{
			return getLunarDistance (ln_get_julian_from_sys ());
		}

		double getLunarDistance (double JD);

		double getLunarRaDistance ()
		{
			return getLunarRaDistance (ln_get_julian_from_sys ());
		}

		double getLunarRaDistance (double JD);

		double getMeridianDistance ()
		{
			return fabs (getHourAngle ());
		}

		double getMeridianDistance (double JD)
		{
			return fabs (getHourAngle (JD));
		}

		// time in seconds to object set/rise/meridian pass (if it's visible, otherwise -1 (for
		// circumpolar & not visible objects)
		int secToObjectSet ()
		{
			return secToObjectSet (ln_get_julian_from_sys ());
		}

		int secToObjectSet (double JD);

		int secToObjectRise ()
		{
			return secToObjectRise (ln_get_julian_from_sys ());
		}

		int secToObjectRise (double JD);

		int secToObjectMeridianPass ()
		{
			return secToObjectMeridianPass (ln_get_julian_from_sys ());
		}

		int secToObjectMeridianPass (double JD);

		const char *getTargetComment ()
		{
			return target_comment;
		}
		void setTargetComment (const char *in_target_comment)
		{
			delete target_comment;
			target_comment = new char[strlen (in_target_comment) + 1];
			strcpy (target_comment, in_target_comment);
		}

		float getTargetPriority ()
		{
			return tar_priority;
		}

		void setTargetPriority (float new_priority)
		{
			tar_priority = new_priority;
		}

		float getTargetBonus ()
		{
			return tar_bonus;
		}

		const time_t *getTargetBonusTime ()
		{
			return &tar_bonus_time;
		}

		virtual void setTargetBonus (float new_bonus, time_t * new_time = NULL)
		{
			tar_bonus = new_bonus;
			if (new_time)
				setTargetBonusTime (new_time);
		}

		void setTargetBonusTime (time_t * new_time)
		{
			tar_bonus_time = *new_time;
		}

		int getRST (struct ln_rst_time *rst)
		{
			return getRST (rst, ln_get_julian_from_sys (), LN_STAR_STANDART_HORIZON);
		}
		virtual int getRST (struct ln_rst_time *rst, double jd, double horizon) = 0;

		// returns 1 when we are almost the same target, so
		// interruption of this target is not necessary
		// otherwise (when interruption is necessary) returns 0
		virtual int compareWithTarget (Target * in_target, double in_sep_limit);
		virtual moveType startSlew (struct ln_equ_posn *position);
		virtual moveType afterSlewProcessed ();
		// return 1 if observation is already in progress, 0 if observation started, -1 on error
		// 2 if we don't need to move
		virtual int startObservation (Rts2Block * master);

		// similar to startSlew - return 0 if observation ends, 1 if
		// it doesn't ends (ussually in case when in_next_id == target_id),
		// -1 on errror

		int endObservation (int in_next_id, Rts2Block * master);

		virtual int endObservation (int in_next_id);

		// returns 1 if target is continuus - in case next target is same | next
		// targer doesn't exists, we keep exposing and we will not move mount between
		// exposures. Good for darks observation, partial good for GRB (when we solve
		// problem with moving mount in exposures - position updates)
		// returns 2 when target need change - usefull for plan target and other targets,
		// which set obs_target_id
		virtual int isContinues ();

		int observationStarted ();

								 // called when we can move to next observation - good to generate next target in mosaic observation etc..
		virtual int beforeMove ();
		int postprocess ();
		virtual bool isGood (double lst, double JD, struct ln_equ_posn *pos);
		virtual bool isGood (double JD);
		bool isAboveHorizon (struct ln_hrz_posn *hrz);

		/**
		 *   Return -1 if target is not suitable for observing,
		 *   otherwise return 0.
		 */
		virtual int considerForObserving (double JD);
		virtual int dropBonus ();
		float getBonus ()
		{
			return getBonus (ln_get_julian_from_sys ());
		}
		virtual float getBonus (double JD);
		virtual int changePriority (int pri_change, time_t * time_ch);
		int changePriority (int pri_change, double validJD);

		virtual int setNextObservable (time_t * time_ch);
		int setNextObservable (double validJD);

		int getNumObs (time_t * start_time, time_t * end_time);
		double getLastObsTime ();// return time in seconds to last observation of same target

		int getCalledNum ()
		{
			return startCalledNum;
		}

		double getAirmassScale ()
		{
			return airmassScale;
		}

		/**
		 * Returns time of first observation, or nan if no observation.
		 */
		double getFirstObs ();

		/**
		 * Returns time of last observation, or nan if no observation.
		 */
		double getLastObs ();

		/**
		 * Prints extra informations about target, which can be interested to user.
		 *
		 * @param _os stream to print that
		 */
		virtual void printExtra (Rts2InfoValStream & _os, double JD);

		/**
		 * print short target info
		 */
		void printShortInfo (std::ostream & _os)
		{
			printShortInfo (_os, ln_get_julian_from_sys ());
		}

		virtual void printShortInfo (std::ostream & _os, double JD);

		/**
		 * Prints one-line info with bonus inforamtions
		 */

		void printShortBonusInfo (std::ostream & _os)
		{
			printShortBonusInfo (_os, ln_get_julian_from_sys ());
		}

		virtual void printShortBonusInfo (std::ostream & _os, double JD);

		void printDS9Reg (std::ostream & _os)
		{
			printDS9Reg (_os, ln_get_julian_from_sys ());
		}

		virtual void printDS9Reg (std::ostream & _os, double JD);

		/**
		 * Prints position info for given JD.
		 *
		 * @param _os stream to print that
		 * @param JD date for which to print info
		 */
		virtual void sendPositionInfo (Rts2InfoValStream & _os, double JD);
		void sendInfo (Rts2InfoValStream & _os)
		{
			sendInfo (_os, ln_get_julian_from_sys ());
		}
		virtual void sendInfo (Rts2InfoValStream & _os, double JD);

		void printAltTableSingleCol (std::ostream & _os, double jd_start, double i,
			double step);

		std::string getUsersEmail (int in_event_mask, int &count);

		/**
		 * Print observations at current target postion around given position.
		 *
		 */
		int printObservations (double radius, std::ostream & _os)
		{
			return printObservations (radius, ln_get_julian_from_sys (), _os);
		}

		int printObservations (double radius, double JD, std::ostream & _os);

		Rts2TargetSet getTargets (double radius);
		Rts2TargetSet getTargets (double radius, double JD);

		int printTargets (double radius, std::ostream & _os)
		{
			return printTargets (radius, ln_get_julian_from_sys (), _os);
		}

		int printTargets (double radius, double JD, std::ostream & _os);

		int printImages (std::ostream & _os, int flags = DISPLAY_ALL)
		{
			return printImages (ln_get_julian_from_sys (), _os, flags);
		}

		int printImages (double JD, std::ostream & _os, int flags = DISPLAY_ALL);

		/**
		 * Return calibration targets for given target
		 */
		Rts2TargetSet *getCalTargets ()
		{
			return getCalTargets (ln_get_julian_from_sys ());
		}

		virtual Rts2TargetSet *getCalTargets (double JD);

		virtual void writeToImage (Rts2Image * image, double JD);
};

class ConstTarget:public Target
{
	private:
		struct ln_equ_posn position;
	protected:

								 // get called when target was selected to update bonuses, target position etc..
		virtual int selectedAsGood ();
	public:
		ConstTarget ();
		ConstTarget (int in_tar_id, struct ln_lnlat_posn *in_obs);
		ConstTarget (int in_tar_id, struct ln_lnlat_posn *in_obs,
			struct ln_equ_posn *pos);
		virtual int load ();
		virtual int save (bool overwrite, int tar_id);
		virtual void getPosition (struct ln_equ_posn *pos, double JD);
		virtual int getRST (struct ln_rst_time *rst, double jd, double horizon);
		virtual int compareWithTarget (Target * in_target, double grb_sep_limit);
		virtual void printExtra (Rts2InfoValStream & _os, double JD);

		void setPosition (double ra, double dec)
		{
			position.ra = ra;
			position.dec = dec;
		}
};

class DarkTarget;

class PossibleDarks
{
	private:
		char *deviceName;
		//
		void addDarkExposure (float exp);
		int defaultDark ();
		int dbDark ();
		// we will need temperature as well
		std::list < float >dark_exposures;
		DarkTarget *target;
	public:
		PossibleDarks (DarkTarget * in_target, const char *in_deviceName);
		virtual ~ PossibleDarks (void);
		int getScript (std::string & buf);
		int isName (const char *in_deviceName);
};

class DarkTarget:public Target
{
	private:
		struct ln_equ_posn currPos;
		std::list < PossibleDarks * >darkList;
	public:
		DarkTarget (int in_tar_id, struct ln_lnlat_posn *in_obs);
		virtual ~ DarkTarget (void);
		virtual int getScript (const char *deviceName, std::string & buf);
		virtual void getPosition (struct ln_equ_posn *pos, double JD);
		virtual int getRST (struct ln_rst_time *rst, double JD, double horizon)
		{
			return 1;
		}
		virtual moveType startSlew (struct ln_equ_posn * position);
		virtual int isContinues ()
		{
			return 1;
		}
};

class FlatTarget:public ConstTarget
{
	private:
		void getAntiSolarPos (struct ln_equ_posn *pos, double JD);
	public:
		FlatTarget (int in_tar_id, struct ln_lnlat_posn *in_obs);
		virtual int getScript (const char *deviceName, std::string & buf);
		virtual int load ();
		virtual void getPosition (struct ln_equ_posn *pos, double JD);
		virtual int considerForObserving (double JD);
		virtual int isContinues ()
		{
			return 1;
		}
		virtual void printExtra (Rts2InfoValStream & _os, double JD);
};

// possible calibration target
class PosCalibration:public Target
{
	private:
		double currAirmass;
		char type_id;
		struct ln_equ_posn object;
	public:
		PosCalibration (int in_tar_id, double ra, double dec, char in_type_id,
			char *in_tar_name, struct ln_lnlat_posn *in_observer,
			double JD):Target (in_tar_id, in_observer)
		{
			struct ln_hrz_posn hrz;

			object.ra = ra;
			object.dec = dec;
			ln_get_hrz_from_equ (&object, observer, JD, &hrz);
			currAirmass = ln_get_airmass (hrz.alt, getAirmassScale ());

			setTargetType (in_type_id);
			setTargetName (in_tar_name);
		}
		double getCurrAirmass ()
		{
			return currAirmass;
		}

		virtual void getPosition (struct ln_equ_posn *in_pos, double JD)
		{
			in_pos->ra = object.ra;
			in_pos->dec = object.dec;
		}

		virtual int getRST (struct ln_rst_time *rst, double JD, double horizon)
		{
			struct ln_equ_posn pos;
			getPosition (&pos, JD);
			ln_get_object_next_rst_horizon (JD, observer, &pos, horizon, rst);
			return 0;
		}
};

class CalibrationTarget:public ConstTarget
{
	private:
		struct ln_equ_posn airmassPosition;
		time_t lastImage;
		int needUpdate;
	public:
		CalibrationTarget (int in_tar_id, struct ln_lnlat_posn *in_obs);
		virtual int load ();
		virtual int beforeMove ();
		virtual int endObservation (int in_next_id);
		virtual void getPosition (struct ln_equ_posn *pos, double JD);
		virtual int considerForObserving (double JD);
		virtual int changePriority (int pri_change, time_t * time_ch)
		{
			return 0;
		}
		virtual float getBonus (double JD);
};

class FocusingTarget:public ConstTarget
{
	public:
		FocusingTarget (int in_tar_id,
			struct ln_lnlat_posn *in_obs):ConstTarget (in_tar_id,
			in_obs)
		{
		};
		virtual int getScript (const char *deviceName, std::string & buf);
};

class ModelTarget:public ConstTarget
{
	private:
		struct ln_hrz_posn hrz_poz;
		struct ln_equ_posn equ_poz;
		double ra_noise;
		double dec_noise;
		float alt_start;
		float alt_stop;
		float alt_step;
		float az_start;
		float az_stop;
		float az_step;
		float noise;
		int step;

		int alt_size;

		int modelStepType;

		int writeStep ();
		int getNextPosition ();
		int calPosition ();
	public:
		ModelTarget (int in_tar_id, struct ln_lnlat_posn *in_obs);
		virtual ~ ModelTarget (void);
		virtual int load ();
		virtual int beforeMove ();
		virtual moveType afterSlewProcessed ();
		virtual int endObservation (int in_next_id);
		virtual void getPosition (struct ln_equ_posn *pos, double JD);
		virtual double getMinObsAlt ()
		{
			return -1;
		}
};

class OportunityTarget:public ConstTarget
{
	public:
		OportunityTarget (int in_tar_id, struct ln_lnlat_posn * in_obs);
		virtual float getBonus (double JD);
		virtual int isContinues ()
		{
			return 1;
		}
};

class LunarTarget:public Target
{
	public:
		LunarTarget (int in_tar_id, struct ln_lnlat_posn * in_obs);
		virtual int getScript (const char *deviceName, std::string & buf);
		virtual void getPosition (struct ln_equ_posn *pos, double JD);
		virtual int getRST (struct ln_rst_time *rst, double jd, double horizon);
};

/**
 * Swift Field of View - generate observations on Swift Field of View, based on
 * data received from GCN
 *
 */
class TargetSwiftFOV:public Target
{
	private:
		int oldSwiftId;
		int swiftId;
		struct ln_equ_posn swiftFovCenter;
		time_t swiftTimeStart;
		time_t swiftTimeEnd;
		double swiftOnBonus;
		char *swiftName;
		double swiftRoll;

		char *swiftLastTarName;
		int swiftLastTar;
		time_t swiftLastTarTimeStart;
		time_t swiftLastTarTimeEnd;
		struct ln_equ_posn swiftLastTarPos;
	public:
		TargetSwiftFOV (int in_tar_id, struct ln_lnlat_posn *in_obs);
		virtual ~ TargetSwiftFOV (void);

		virtual int load ();	 // find Swift pointing for observation
		virtual void getPosition (struct ln_equ_posn *pos, double JD);
		virtual int getRST (struct ln_rst_time *rst, double JD, double horizon);
		virtual moveType afterSlewProcessed ();
								 // return 0, when target can be observed, otherwise modify tar_bonus..
		virtual int considerForObserving (double JD);
		virtual int beforeMove ();
		virtual float getBonus (double JD);
		virtual int isContinues ();

		virtual void printExtra (Rts2InfoValStream & _os, double JD);
};

class TargetIntegralFOV:public Target
{
	private:
		int oldIntegralId;
		int integralId;
		struct ln_equ_posn integralFovCenter;
		time_t integralTimeStart;
		double integralOnBonus;
	public:
		TargetIntegralFOV (int in_tar_id, struct ln_lnlat_posn *in_obs);
		virtual ~ TargetIntegralFOV (void);

		virtual int load ();	 // find Swift pointing for observation
		virtual void getPosition (struct ln_equ_posn *pos, double JD);
		virtual int getRST (struct ln_rst_time *rst, double JD, double horizon);
		virtual moveType afterSlewProcessed ();
								 // return 0, when target can be observed, otherwise modify tar_bonus..
		virtual int considerForObserving (double JD);
		virtual int beforeMove ();
		virtual float getBonus (double JD);
		virtual int isContinues ();

		virtual void printExtra (Rts2InfoValStream & _os, double JD);
};

class TargetGps:public ConstTarget
{
	public:
		TargetGps (int in_tar_id, struct ln_lnlat_posn *in_obs);
		virtual float getBonus (double JD);
};

class TargetSkySurvey:public ConstTarget
{
	public:
		TargetSkySurvey (int in_tar_id, struct ln_lnlat_posn *in_obs);
		virtual float getBonus (double JD);
};

class TargetTerestial:public ConstTarget
{
	public:
		TargetTerestial (int in_tar_id, struct ln_lnlat_posn *in_obs);
		virtual int considerForObserving (double JD);
		virtual float getBonus (double JD);
		virtual moveType afterSlewProcessed ();
};

class Rts2Plan;

class TargetPlan:public Target
{
	private:
		Rts2Plan * selectedPlan;
		Rts2Plan *nextPlan;

		// how long to look back for previous plan
		float hourLastSearch;
		float hourConsiderPlans;
		time_t nextTargetRefresh;
		void refreshNext ();
	protected:
		virtual int getDBScript (const char *camera_name, std::string & script);
	public:
		TargetPlan (int in_tar_id, struct ln_lnlat_posn *in_obs);
		virtual ~ TargetPlan (void);

		virtual int load ();
		virtual int load (double JD);
		virtual void getPosition (struct ln_equ_posn *pos, double JD);
		virtual int getRST (struct ln_rst_time *rst, double JD, double horizon);
		virtual int getObsTargetID ();
		virtual int considerForObserving (double JD);
		virtual float getBonus (double JD);
		virtual int isContinues ();
		virtual int beforeMove ();
		virtual moveType startSlew (struct ln_equ_posn *position);

		virtual void printExtra (Rts2InfoValStream & _os, double JD);
};

// load target from DB
Target *createTarget (int in_tar_id);

Target *createTarget (int in_tar_id, struct ln_lnlat_posn *in_obs);

// send end mails
void sendEndMails (const time_t * t_from, const time_t * t_to,
int printImages, int printCounts,
struct ln_lnlat_posn *in_obs, Rts2App * master);

// print target information to stdout..
std::ostream & operator << (std::ostream & _os, Target & target);
Rts2InfoValStream & operator << (Rts2InfoValStream & _os, Target & target);
#endif							 /*! __RTS_TARGETDB__ */
