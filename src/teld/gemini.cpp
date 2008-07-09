/* 
 * Driver for Gemini systems.
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

#define DEBUG_EXTRA
/**
 * @file Driver file for LOSMANDY Gemini telescope systems
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */

#include <libnova/libnova.h>

#include "telescope.h"
#include "hms.h"
#include "../utils/rts2connserial.h"

// uncomment following line, if you want all port read logging (will
// add about 10 30-bytes lines to logStream for every query).
// #define DEBUG_ALL_PORT_COMM

#define RATE_SLEW                'S'
#define RATE_FIND                'M'
#define RATE_CENTER              'C'
#define RATE_GUIDE               'G'
#define PORT_TIMEOUT             5

#define TCM_DEFAULT_RATE         32768

#define SEARCH_STEPS             16

#define MIN(a,b)                 ((a) < (b) ? (a) : (b))
#define MAX(a,b)                 ((a) > (b) ? (a) : (b))

#define GEMINI_CMD_RATE_GUIDE    150
#define GEMINI_CMD_RATE_CENTER   170

#define OPT_BOOTES               OPT_LOCAL + 10
#define OPT_CORR                 OPT_LOCAL + 11
#define OPT_EXPTYPE              OPT_LOCAL + 12
#define OPT_FORCETYPE            OPT_LOCAL + 13

int arr[] = { 0, 1, 2, 3 };

typedef struct searchDirs_t
{
	short raDiv;
	short decDiv;
};

searchDirs_t searchDirs[SEARCH_STEPS] =
{
	{0, 1},
	{0, -1},
	{0, -1},
	{0, 1},
	{1, 0},
	{-1, 0},
	{-1, 0},
	{1, 0},
	{1, 1},
	{-1, -1},
	{-1, -1},
	{1, 1},
	{-1, 1},
	{1, -1},
	{1, -1},
	{-1, 1}
};

class Rts2DevTelescopeGemini:public Rts2DevTelescope
{
	private:

		const char *device_file;

		Rts2ValueTime *telLocalTime;
		Rts2ValueFloat *telGuidingSpeed;

		Rts2ValueSelection *resetState;
		Rts2ValueInteger *featurePort;

		Rts2ValueDouble *axRa;
		Rts2ValueDouble *axDec;

		const char *geminiConfig;

		Rts2ConnSerial *tel_conn;
		int tel_write_read (const char *buf, int wcount, char *rbuf, int rcount);
		int tel_write_read_hash (const char *wbuf, int wcount, char *rbuf, int rcount);
		int tel_read_hms (double *hmsptr, const char *command);
		unsigned char tel_gemini_checksum (const char *buf);
		// higher level I/O functions
		int tel_gemini_getch (int id, char *in_buf);
		int tel_gemini_setch (int id, char *in_buf);
		int tel_gemini_set (int id, int32_t val);
		int tel_gemini_set (int id, double val);
		int tel_gemini_get (int id, int32_t & val);
		int tel_gemini_get (int id, double &val);
		int tel_gemini_get (int id, int &val1, int &val2);
		int tel_gemini_reset ();
		int tel_gemini_match_time ();
		int tel_read_ra ();
		int tel_read_dec ();
		int tel_read_localtime ();
		int tel_read_longtitude ();
		int tel_read_siderealtime ();
		int tel_read_latitude ();
		int tel_rep_write (char *command);
		void tel_normalize (double *ra, double *dec);
		int tel_write_ra (double ra);
		int tel_write_dec (double dec);

		int tel_set_rate (char new_rate);
		int telescope_start_move (char direction);
		int telescope_stop_move (char direction);

		void telescope_stop_goto ();

		int32_t raRatio;
		int32_t decRatio;

		double maxPrecGuideRa;
		double maxPrecGuideDec;

		int readRatiosInter (int startId);
		int readRatios ();
		int readLimits ();
		int setCorrection ();

		int geminiInit ();

		double fixed_ha;
		int fixed_ntries;

		//		int startMoveFixedReal ();
	#ifdef L4_GUIDE
		bool isGuiding (struct timeval *now);
		int guide (char direction, double val);
		//		int changeDec ();
	#endif
		//		int change_real (double chng_ra, double chng_dec);

		int forcedReparking;	 // used to count reparking, when move fails
		double lastMoveRa;
		double lastMoveDec;

		int tel_start_move ();

		enum
		{ TEL_OK, TEL_BLOCKED_RESET, TEL_BLOCKED_PARKING }
		telMotorState;
		int32_t lastMotorState;
		time_t moveTimeout;

		int infoCount;
		int matchCount;
		int bootesSensors;
	#ifdef L4_GUIDE
		//		struct timeval changeTime;
	#else
		//		struct timeval changeTimeRa;
		//		struct timeval changeTimeDec;

		//		int change_ra (double chng_ra);
		//		int change_dec (double chng_dec);
	#endif
		bool guideDetected;

		//		void clearSearch ();
		// execute next search step, if necessary
		//		int changeSearch ();

		void getAxis ();
		int parkBootesSensors ();

		int searchStep;
		long searchUsecTime;

		//		short lastSearchRaDiv;
		//		short lastSearchDecDiv;
		//		struct timeval nextSearchStep;

		int32_t worm;
		int worm_move_needed;	 // 1 if we need to put mount to 130 tracking (we are performing RA move)

		double nextChangeDec;
		bool decChanged;

		char gem_version;
		char gem_subver;

		int decFlipLimit;

		double haMinusLimit;
		double haPlusLimit;

		Rts2ValueInteger *centeringSpeed;
		Rts2ValueFloat *guidingSpeed;
		Rts2ValueDouble *guideLimit;

		int expectedType;
		int forceType;

		const char *getGemType (int gem_type);

	protected:
		virtual int processOption (int in_opt);
		virtual int initValues ();
		virtual int idle ();

	public:
		Rts2DevTelescopeGemini (int argc, char **argv);
		virtual ~ Rts2DevTelescopeGemini (void);
		virtual int init ();
		virtual int changeMasterState (int new_state);
		virtual int ready ();
		virtual int info ();
		virtual int startMove ();
		virtual int isMoving ();
		virtual int endMove ();
		virtual int stopMove ();
		//		virtual int startMoveFixed (double tar_ha, double tar_dec);
		//		virtual int isMovingFixed ();
		//		virtual int endMoveFixed ();
		//		virtual int startSearch ();
		//		virtual int isSearching ();
		//		virtual int stopSearch ();
		//		virtual int endSearch ();
		virtual int startPark ();
		virtual int isParking ();
		virtual int endPark ();
		int setTo (double set_ra, double set_dec, int appendModel);
		virtual int setTo (double set_ra, double set_dec);
		//		virtual int correctOffsets (double cor_ra, double cor_dec, double real_ra,
		//			double real_dec);
		//		virtual int correct (double cor_ra, double cor_dec, double real_ra,
		//			double real_dec);
		//		virtual int change (double chng_ra, double chng_dec);
		virtual int saveModel ();
		virtual int loadModel ();
		virtual int stopWorm ();
		virtual int startWorm ();
		virtual int resetMount ();
		virtual int startDir (char *dir);
		virtual int stopDir (char *dir);
		virtual int startGuide (char dir, double dir_dist);
		virtual int stopGuide (char dir);
		virtual int stopGuideAll ();
		virtual int getFlip ();
};

int
Rts2DevTelescopeGemini::tel_write_read (const char *buf, int wcount, char *rbuf,
int rcount)
{
	int ret;
	ret = tel_conn->writeRead (buf, wcount, rbuf, rcount);
	if (ret < 0)
	{
		// try rebooting
		tel_gemini_reset ();
		ret = tel_conn->writeRead (buf, wcount, rbuf, rcount);
	}
	return ret;
}


/*!
 * Combine write && read_hash together.
 *
 * @see tel_write_read for definition
 */
int
Rts2DevTelescopeGemini::tel_write_read_hash (const char *wbuf, int wcount, char *rbuf, int rcount)
{
	int tmp_rcount = tel_conn->writeRead (wbuf, wcount, rbuf, rcount, '#');
	if (tmp_rcount < 0)
	{
		tel_gemini_reset ();
		return -1;
	}
	// end hash..
	rbuf[tmp_rcount - 1] = '\0';
	return tmp_rcount;
}


/*!
 * Reads some value from lx200 in hms format.
 *
 * Utility function for all those read_ra and other.
 *
 * @param hmsptr	where hms will be stored
 *
 * @return -1 and set errno on error, otherwise 0
 */
int
Rts2DevTelescopeGemini::tel_read_hms (double *hmsptr, const char *command)
{
	char wbuf[11];
	if (tel_write_read_hash (command, strlen (command), wbuf, 10) < 6)
		return -1;
	*hmsptr = hmstod (wbuf);
	if (isnan (*hmsptr))
		return -1;
	return 0;
}


/*!
 * Computes gemini checksum
 *
 * @param buf checksum to compute
 *
 * @return computed checksum
 */
unsigned char
Rts2DevTelescopeGemini::tel_gemini_checksum (const char *buf)
{
	unsigned char checksum = 0;
	for (; *buf; buf++)
		checksum ^= *buf;
	checksum += 64;
	checksum %= 128;			 // modulo 128
	return checksum;
}


/*!
 * Write command to set gemini local parameters
 *
 * @param id	id to set
 * @param buf	value to set (char buffer; should be 15 char long)
 *
 * @return -1 on error, otherwise 0
 */
int
Rts2DevTelescopeGemini::tel_gemini_setch (int id, char *in_buf)
{
	int len;
	char *buf;
	int ret;
	len = strlen (in_buf);
	buf = (char *) malloc (len + 10);
	len = sprintf (buf, ">%i:%s", id, in_buf);
	buf[len] = tel_gemini_checksum (buf);
	len++;
	buf[len] = '#';
	len++;
	buf[len] = '\0';
	ret = tel_conn->writePort (buf, len);
	free (buf);
	return ret;
}


/*!
 * Write command to set gemini local parameters
 *
 * @param id	id to set
 * @param val	value to set
 *
 * @return -1 on error, otherwise 0
 */
int
Rts2DevTelescopeGemini::tel_gemini_set (int id, int32_t val)
{
	char buf[15];
	int len;
	len = sprintf (buf, ">%i:%i", id, val);
	buf[len] = tel_gemini_checksum (buf);
	len++;
	buf[len] = '#';
	len++;
	buf[len] = '\0';
	return tel_conn->writePort (buf, len);
}


int
Rts2DevTelescopeGemini::tel_gemini_set (int id, double val)
{
	char buf[15];
	int len;
	len = sprintf (buf, ">%i:%0.1f", id, val);
	buf[len] = tel_gemini_checksum (buf);
	len++;
	buf[len] = '#';
	len++;
	buf[len] = '\0';
	return tel_conn->writePort (buf, len);
}


/*!
 * Read gemini local parameters
 *
 * @param id	id to get
 * @param buf	pointer where to store result (char[15] buf)
 *
 * @return -1 and set errno on error, otherwise 0
 */
int
Rts2DevTelescopeGemini::tel_gemini_getch (int id, char *buf)
{
	char *ptr;
	unsigned char checksum, cal;
	int len, ret;
	len = sprintf (buf, "<%i:", id);
	buf[len] = tel_gemini_checksum (buf);
	len++;
	buf[len] = '#';
	len++;
	buf[len] = '\0';
	ret = tel_write_read_hash (buf, len, buf, 20);
	if (ret < 0)
		return ret;
	ptr = buf + ret - 1;
	checksum = *ptr;
	*ptr = '\0';
	cal = tel_gemini_checksum (buf);
	if (cal != checksum)
	{
		logStream (MESSAGE_ERROR) << "invalid gemini checksum: should be " <<
			cal << " is " << checksum << " " << ret << sendLog;
		if (*buf || checksum)
			sleep (5);
		tel_gemini_reset ();
		*buf = '\0';
		return -1;
	}
	return 0;
}


/*!
 * Read gemini local parameters
 *
 * @param id	id to get
 * @param val	pointer where to store result
 *
 * @return -1 and set errno on error, otherwise 0
 */
int
Rts2DevTelescopeGemini::tel_gemini_get (int id, int32_t & val)
{
	char buf[9], *ptr, checksum;
	int len, ret;
	len = sprintf (buf, "<%i:", id);
	buf[len] = tel_gemini_checksum (buf);
	len++;
	buf[len] = '#';
	len++;
	buf[len] = '\0';
	ret = tel_write_read_hash (buf, len, buf, 8);
	if (ret < 0)
		return ret;
	for (ptr = buf; *ptr && (isdigit (*ptr) || *ptr == '.' || *ptr == '-');
		ptr++);
	checksum = *ptr;
	*ptr = '\0';
	if (tel_gemini_checksum (buf) != checksum)
	{
		logStream (MESSAGE_ERROR) << "invalid gemini checksum: should be " <<
			tel_gemini_checksum (buf) << " is " << checksum << sendLog;
		if (*buf)
			sleep (5);
		tel_conn->flushPortIO ();
		return -1;
	}
	val = atol (buf);
	return 0;
}


int
Rts2DevTelescopeGemini::tel_gemini_get (int id, double &val)
{
	char buf[9], *ptr, checksum;
	int len, ret;
	len = sprintf (buf, "<%i:", id);
	buf[len] = tel_gemini_checksum (buf);
	len++;
	buf[len] = '#';
	len++;
	buf[len] = '\0';
	ret = tel_write_read_hash (buf, len, buf, 8);
	if (ret < 0)
		return ret;
	for (ptr = buf; *ptr && (isdigit (*ptr) || *ptr == '.' || *ptr == '-');
		ptr++);
	checksum = *ptr;
	*ptr = '\0';
	if (tel_gemini_checksum (buf) != checksum)
	{
		logStream (MESSAGE_ERROR) << "invalid gemini checksum: should be " <<
			tel_gemini_checksum (buf) << " is " << checksum << sendLog;
		if (*buf)
			sleep (5);
		tel_conn->flushPortIO ();
		return -1;
	}
	val = atof (buf);
	return 0;
}


int
Rts2DevTelescopeGemini::tel_gemini_get (int id, int &val1, int &val2)
{
	char buf[20], *ptr;
	int ret;
	ret = tel_gemini_getch (id, buf);
	if (ret)
		return ret;
	// parse returned string
	ptr = strchr (buf, ';');
	if (!ptr)
		return -1;
	*ptr = '\0';
	ptr++;
	val1 = atoi (buf);
	val2 = atoi (ptr);
	return 0;
}


/*!
 * Reset and home losmandy telescope
 */
int
Rts2DevTelescopeGemini::tel_gemini_reset ()
{
	char rbuf[50];

	// write_read_hash
	if (tel_conn->flushPortIO () < 0)
		return -1;

	if (tel_conn->writeRead ("\x06", 1, rbuf, 47, '#') < 0)
	{
		tel_conn->flushPortIO ();
		return -1;
	}

	if (*rbuf == 'b')			 // booting phase, select warm reboot
	{
		switch (resetState->getValueInteger ())
		{
			case 0:
				tel_conn->writePort ("bR#", 3);
				break;
			case 1:
				tel_conn->writePort ("bW#", 3);
				break;
			case 2:
				tel_conn->writePort ("bC#", 3);
				break;
		}
		resetState->setValueInteger (0);
		setTimeout (USEC_SEC);
	}
	else if (*rbuf != 'G')
	{
		// something is wrong, reset all comm
		sleep (10);
		tel_conn->flushPortIO ();
	}
	return -1;
}


/*!
 * Match gemini time with system time
 */
int
Rts2DevTelescopeGemini::tel_gemini_match_time ()
{
	struct tm ts;
	time_t t;
	char buf[55];
	int ret;
	char rep;
	if (matchCount)
		return 0;
	t = time (NULL);
	gmtime_r (&t, &ts);
	// set time
	ret = tel_write_read ("#:SG+00#", 8, &rep, 1);
	if (ret < 0)
		return ret;
	snprintf (buf, 14, "#:SL%02d:%02d:%02d#", ts.tm_hour, ts.tm_min, ts.tm_sec);
	ret = tel_write_read (buf, strlen (buf), &rep, 1);
	if (ret < 0)
		return ret;
	snprintf (buf, 14, "#:SC%02d/%02d/%02d#", ts.tm_mon + 1, ts.tm_mday,
		ts.tm_year - 100);
	ret = tel_write_read_hash (buf, strlen (buf), buf, 55);
	if (ret < 0)
		return ret;
	if (*buf != '1')
	{
		return -1;
	}
	// read spaces
	ret = tel_conn->readPort (buf, 26, '#');
	if (ret)
	{
		matchCount = 1;
		return ret;
	}
	logStream (MESSAGE_INFO) << "GEMINI: time match" << sendLog;
	return 0;
}


/*!
 * Reads lx200 right ascenation.
 *
 * @param raptr		where ra will be stored
 *
 * @return -1 and set errno on error, otherwise 0
 */
int
Rts2DevTelescopeGemini::tel_read_ra ()
{
	double t_telRa;
	if (tel_read_hms (&t_telRa, "#:GR#"))
		return -1;
	t_telRa *= 15.0;
	setTelRa (t_telRa);
	return 0;
}


/*!
 * Reads losmandy declination.
 *
 * @param decptr	where dec will be stored
 *
 * @return -1 and set errno on error, otherwise 0
 */
int
Rts2DevTelescopeGemini::tel_read_dec ()
{
	double t_telDec;
	if (tel_read_hms (&t_telDec, "#:GD#"))
		return -1;
	setTelDec (t_telDec);
	return 0;
}


/*!
 * Returns losmandy local time.
 *
 * @param tptr		where time will be stored
 *
 * @return -1 and errno on error, otherwise 0
 */
int
Rts2DevTelescopeGemini::tel_read_localtime ()
{
	double t_telLocalTime;
	if (tel_read_hms (&t_telLocalTime, "#:GL#"))
		return -1;
	telLocalTime->setValueDouble (t_telLocalTime);
	return 0;
}


/*!
 * Reads losmandy longtitude.
 *
 * @param latptr	where longtitude will be stored
 *
 * @return -1 and errno on error, otherwise 0
 */
int
Rts2DevTelescopeGemini::tel_read_longtitude ()
{
	int ret;
	double t_telLongitude;
	ret = tel_read_hms (&t_telLongitude, "#:Gg#");
	if (ret)
		return ret;
	telLongitude->setValueDouble (-1 * t_telLongitude);
	return ret;
}


/*!
 * Reads losmandy latitude.
 *
 * @param latptr	here latitude will be stored
 *
 * @return -1 and errno on error, otherwise 0
 */
int
Rts2DevTelescopeGemini::tel_read_latitude ()
{
	double t_telLatitude;
	if (tel_read_hms (&t_telLatitude, "#:Gt#"))
		return -1;
	telLatitude->setValueDouble (t_telLatitude);
	return 0;
}


/*!
 * Repeat losmandy write.
 *
 * Handy for setting ra and dec.
 * Meade tends to have problems with that.
 *
 * @param command	command to write on port
 */
int
Rts2DevTelescopeGemini::tel_rep_write (char *command)
{
	int count;
	char retstr;
	for (count = 0; count < 3; count++)
	{
		if (tel_write_read (command, strlen (command), &retstr, 1) < 0)
			return -1;
		if (retstr == '1')
			break;
		sleep (1);
		logStream (MESSAGE_DEBUG) << "Losmandy tel_rep_write - for " << count <<
			" time" << sendLog;
	}
	if (count == 200)
	{
		logStream (MESSAGE_ERROR) <<
			"losmandy tel_rep_write unsucessful due to incorrect return." <<
			sendLog;
		return -1;
	}
	return 0;
}


/*!
 * Normalize ra and dec,
 *
 * @param ra		rigth ascenation to normalize in decimal hours
 * @param dec		rigth declination to normalize in decimal degrees
 *
 * @return 0
 */
void
Rts2DevTelescopeGemini::tel_normalize (double *ra, double *dec)
{
	*ra = ln_range_degrees (*ra);
	if (*dec < -90)
								 //normalize dec
		*dec = floor (*dec / 90) * -90 + *dec;
	if (*dec > 90)
		*dec = *dec - floor (*dec / 90) * 90;
}


/*!
 * Set losmandy right ascenation.
 *
 * @param ra		right ascenation to set in decimal degrees
 *
 * @return -1 and errno on error, otherwise 0
 */
int
Rts2DevTelescopeGemini::tel_write_ra (double ra)
{
	char command[14];
	int h, m, s;
	ra = ra / 15;
	dtoints (ra, &h, &m, &s);
	if (snprintf (command, 14, "#:Sr%02d:%02d:%02d#", h, m, s) < 0)
		return -1;
	return tel_rep_write (command);
}


/*!
 * Set losmandy declination.
 *
 * @param dec		declination to set in decimal degrees
 *
 * @return -1 and errno on error, otherwise 0
 */
int
Rts2DevTelescopeGemini::tel_write_dec (double dec)
{
	char command[15];
	struct ln_dms dh;
	char sign = '+';
	if (dec < 0)
	{
		sign = '-';
		dec = -1 * dec;
	}
	ln_deg_to_dms (dec, &dh);
	if (snprintf
		(command, 15, "#:Sd%c%02d*%02d:%02.0f#", sign, dh.degrees, dh.minutes,
		dh.seconds) < 0)
		return -1;
	return tel_rep_write (command);
}


Rts2DevTelescopeGemini::Rts2DevTelescopeGemini (int in_argc, char **in_argv):Rts2DevTelescope (in_argc,
in_argv)
{
	createValue (telLocalTime, "localtime", "telescope local time", false);
	createValue (telGuidingSpeed, "guiding_speed", "telescope guiding speed", false);

	createValue (resetState, "next_reset", "next reset state", false);
	resetState->addSelVal ("RESTART");
	resetState->addSelVal ("WARM_START");
	resetState->addSelVal ("COLD_START");

	createValue (featurePort, "feature_port", "state of feature port", false);

	createValue (axRa, "CNT_RA", "RA axis ticks", true);
	createValue (axDec, "CNT_DEC", "DEC axis ticks", true);

	device_file = "/dev/ttyS0";
	geminiConfig = "/etc/rts2/gemini.ini";
	bootesSensors = 0;
	expectedType = 0;
	forceType = 0;

	addOption ('f', NULL, 1, "device file (ussualy /dev/ttySx");
	addOption ('c', NULL, 1, "config file (with model parameters)");
	addOption (OPT_BOOTES, "bootes", 0,
		"BOOTES G-11 (with 1/0 pointing sensor)");
	addOption (OPT_CORR, "corrections", 1,
		"level of correction done in Gemini - 0 none, 3 all");
	addOption (OPT_EXPTYPE, "expected_type", 1,
		"expected Gemini type (1 GM8, 2 G11, 3 HGM-200, 4 CI700, 5 Titan, 6 Titan50)");
	addOption (OPT_FORCETYPE, "force_type", 1,
		"expected Gemini type (1 GM8, 2 G11, 3 HGM-200, 4 CI700, 5 Titan, 6 Titan50)");

	lastMotorState = 0;
	telMotorState = TEL_OK;
	infoCount = 0;
	matchCount = 0;

	tel_conn = NULL;

	worm = 0;
	worm_move_needed = 0;

	/*
	#ifdef L4_GUIDE
	timerclear (&changeTime);
	#else
	timerclear (&changeTimeRa);
	timerclear (&changeTimeDec);
	#endif
	*/

	guideDetected = false;

	nextChangeDec = 0;
	decChanged = false;
	// default guiding speed
	telGuidingSpeed->setValueDouble (0.2);

	fixed_ha = nan ("f");

	decFlipLimit = 4000;

	//clearSearch ();

	createValue (centeringSpeed, "centeringSpeed",
		"speed used for centering mount on target", false);
	centeringSpeed->setValueInteger (2);

	createValue (guidingSpeed, "guidingSpeed",
		"speed used for guiding mount on target", false);
	guidingSpeed->setValueFloat (0.8);

	createValue (guideLimit, "guideLimit", "limit to to change between ",
		false);
	guideLimit->setValueDouble (5.0 / 60.0);
}


Rts2DevTelescopeGemini::~Rts2DevTelescopeGemini ()
{
}


int
Rts2DevTelescopeGemini::processOption (int in_opt)
{
	switch (in_opt)
	{
		case 'f':
			device_file = optarg;
			break;
		case 'c':
			geminiConfig = optarg;
			break;
		case OPT_BOOTES:
			bootesSensors = 1;
			break;
		case OPT_CORR:
			switch (*optarg)
			{
				case '0':
					correctionsMask->
						setValueInteger (COR_ABERATION | COR_PRECESSION | COR_REFRACTION);
					break;
				case '1':
					correctionsMask->setValueInteger (COR_REFRACTION);
					break;
				case '2':
					correctionsMask->setValueInteger (COR_ABERATION | COR_PRECESSION);
					break;
				case '3':
					correctionsMask->setValueInteger (0);
					break;
				default:
					std::cerr << "Invalid correction option " << optarg << std::endl;
					return -1;
			}
		case OPT_EXPTYPE:
			expectedType = atoi (optarg);
			break;
		case OPT_FORCETYPE:
			forceType = atoi (optarg);
			break;
		default:
			return Rts2DevTelescope::processOption (in_opt);
	}
	return 0;
}


int
Rts2DevTelescopeGemini::geminiInit ()
{
	char rbuf[10];
	int ret;

	tel_gemini_reset ();

	// 12/24 hours trick..
	if (tel_write_read ("#:Gc#", 5, rbuf, 5) < 0)
		return -1;
	rbuf[5] = 0;
	if (strncmp (rbuf, "(24)#", 5))
		return -1;

	// we get 12:34:4# while we're in short mode
	// and 12:34:45 while we're in long mode
	if (tel_write_read_hash ("#:GR#", 5, rbuf, 9) < 0)
		return -1;
	if (rbuf[7] == '\0')
	{
		// that could be used to distinguish between long
		// short mode
		// we are in short mode, set the long on
		if (tel_write_read (":U#", 5, rbuf, 0) < 0)
			return -1;
	}
	tel_write_read_hash (":Ml#", 5, rbuf, 1);
	if (bootesSensors)
		tel_gemini_set (311, 15);// reset feature port

	ret = tel_write_read_hash ("#:GV#", 5, rbuf, 4);
	if (ret <= 0)
		return -1;

	gem_version = rbuf[0] - '0';
	gem_subver = (rbuf[1] - '0') * 10 + (rbuf[2] - '0');

	return 0;
}


int
Rts2DevTelescopeGemini::readRatiosInter (int startId)
{
	int32_t t;
	int res = 1;
	int id;
	int ret;
	for (id = startId; id < startId + 5; id += 2)
	{
		ret = tel_gemini_get (id, t);
		if (ret)
			return -1;
		res *= t;
	}
	return res;
}


int
Rts2DevTelescopeGemini::readRatios ()
{
	raRatio = 0;
	decRatio = 0;

	if (gem_version < 4)
	{
		maxPrecGuideRa = 5 / 60.0;
		maxPrecGuideDec = 5 / 60.0;
		return 0;
	}

	raRatio = readRatiosInter (21);
	decRatio = readRatiosInter (22);

	if (raRatio == -1 || decRatio == -1)
		return -1;

	maxPrecGuideRa = raRatio;
	maxPrecGuideRa = (255 * 360.0 * 3600.0) / maxPrecGuideRa;
	maxPrecGuideRa /= 3600.0;

	maxPrecGuideDec = decRatio;
	maxPrecGuideDec = (255 * 360.0 * 3600.0) / maxPrecGuideDec;
	maxPrecGuideDec /= 3600.0;

	return 0;
}


int
Rts2DevTelescopeGemini::readLimits ()
{
	// TODO read from 221 and 222
	haMinusLimit = -94;
	haPlusLimit = 97;
	return 0;
}


int
Rts2DevTelescopeGemini::setCorrection ()
{
	if (gem_version < 4)
		return 0;
	switch (correctionsMask->getValueInteger ())
	{
		case COR_ABERATION | COR_PRECESSION | COR_REFRACTION:
			return tel_conn->writePort (":p0#", 4);
		case COR_REFRACTION:
			return tel_conn->writePort (":p1#", 4);
		case COR_ABERATION | COR_PRECESSION:
			return tel_conn->writePort (":p2#", 4);
		case 0:
			return tel_conn->writePort (":p3#", 4);
	}
	return -1;
}


/*!
 * Init telescope, connect on given port.
 *
 * @return 0 on succes, -1 & set errno otherwise
 */
int
Rts2DevTelescopeGemini::init ()
{
	int ret;

	ret = Rts2DevTelescope::init ();
	if (ret)
		return ret;

	tel_conn = new Rts2ConnSerial (device_file, this, BS9600, C8, NONE, 40);
	tel_conn->setDebug ();

	ret = tel_conn->init ();
	if (ret)
		return ret;

	while (1)
	{
		tel_conn->flushPortIO ();

		ret = geminiInit ();
		if (!ret)
		{
			ret = readRatios ();
			if (ret)
				return ret;
			ret = readLimits ();
			if (ret)
				return ret;
			setCorrection ();
			return ret;
		}

		sleep (60);
	}
	return 0;
}


const char *
Rts2DevTelescopeGemini::getGemType (int gem_type)
{
	switch (gem_type)
	{
		case 1:
			return "GM8";
		case 2:
			return "G-11";
		case 3:
			return "HGM-200";
		case 4:
			return "CI700";
		case 5:
			return "Titan";
		case 6:
			return "Titan50";
	}
	return "UNK";
}


int
Rts2DevTelescopeGemini::initValues ()
{
	int32_t gem_type;
	char buf[5];
	int ret;
	if (tel_read_longtitude () || tel_read_latitude ())
		return -1;
	if (forceType > 0)
	{
		ret = tel_gemini_set (forceType, forceType);
		if (ret)
			return ret;
	}
	ret = tel_gemini_get (0, gem_type);
	if (ret)
		return ret;
	strcpy (telType, getGemType (gem_type));
	switch (gem_type)
	{
		case 6:
			decFlipLimit = 6750;
	}
	if (expectedType > 0 && gem_type != expectedType)
	{
		logStream (MESSAGE_ERROR) <<
			"Cannot init teld, because, it's type is not expected. Expected " <<
			expectedType << " (" << getGemType (expectedType) << "), get " <<
			gem_type << " (" << telType << ")." << sendLog;
		return -1;
	}
	ret = tel_write_read_hash ("#:GV#", 5, buf, 4);
	if (ret <= 0)
		return -1;
	buf[4] = '\0';
	strcat (telType, "_");
	strcat (telType, buf);
	telAltitude->setValueDouble (600);

	return Rts2DevTelescope::initValues ();
}


int
Rts2DevTelescopeGemini::idle ()
{
	int ret;
	tel_gemini_get (130, worm);
	ret = tel_gemini_get (99, lastMotorState);
	// if moving, not on limits & need info..
	if ((lastMotorState & 8) && (!(lastMotorState & 16)) && infoCount % 25 == 0)
	{
		info ();
	}
	infoCount++;
	if (telMotorState == TEL_OK
		&& ((ret && (getState () & TEL_MASK_MOVING) == TEL_MASK_MOVING)
		|| (lastMotorState & 16)))
	{
		stopMove ();
		resetState->setValueInteger (0);
		resetMount ();
		telMotorState = TEL_BLOCKED_RESET;
	}
	else if (!ret && telMotorState == TEL_BLOCKED_RESET)
	{
		// we get telescope back from reset state
		telMotorState = TEL_BLOCKED_PARKING;
	}
	if (telMotorState == TEL_BLOCKED_PARKING)
	{
		if (forcedReparking < 10)
		{
			// ignore for the moment tel state, pretends, that everything
			// is OK
			telMotorState = TEL_OK;
			if (forcedReparking % 2 == 0)
			{
				startPark ();
				forcedReparking++;
			}
			else
			{
				ret = isParking ();
				switch (ret)
				{
					case -2:
						// parked, let's move us to location where we belong
						forcedReparking = 0;
						if ((getState () & TEL_MASK_MOVING) == TEL_MOVING)
						{
							// not ideal; lastMoveRa contains (possibly corrected) move values
							// but we don't care much about that as we have reparked..
							setTarget (lastMoveRa, lastMoveDec);
							tel_start_move ();
							tel_gemini_get (130, worm);
							ret = tel_gemini_get (99, lastMotorState);
							if (ret)
							{
								// still not responding, repark
								resetState->setValueInteger (0);
								resetMount ();
								telMotorState = TEL_BLOCKED_RESET;
								forcedReparking++;
							}
						}
						break;
					case -1:
						forcedReparking++;
						break;
				}
			}
			if (forcedReparking > 0)
				telMotorState = TEL_BLOCKED_PARKING;
			setTimeout (USEC_SEC);
		}
		else
		{
			stopWorm ();
			stopMove ();
			ret = -1;
		}
	}
	return Rts2DevTelescope::idle ();
}


int
Rts2DevTelescopeGemini::changeMasterState (int new_state)
{
	matchCount = 0;
	return Rts2DevTelescope::changeMasterState (new_state);
}


int
Rts2DevTelescopeGemini::ready ()
{
	return 0;
}


void
Rts2DevTelescopeGemini::getAxis ()
{
	int feature;
	tel_gemini_get (311, feature);
	featurePort->setValueInteger (feature);
	telFlip->setValueInteger ((feature & 2) ? 1 : 0);
}


int
Rts2DevTelescopeGemini::info ()
{
	telFlip->setValueInteger (0);

	if (tel_read_ra () || tel_read_dec () || tel_read_localtime ())
		return -1;
	if (bootesSensors)
	{
		getAxis ();
	}
	else
	{
		telFlip->setValueInteger (getFlip ());
	}

	return Rts2DevTelescope::info ();
}


/*!
 * Set slew rate.
 *
 * @param new_rate	new rate to set. Uses RATE_<SPEED> constant.
 *
 * @return -1 on failure & set errno, 5 (>=0) otherwise
 */
int
Rts2DevTelescopeGemini::tel_set_rate (char new_rate)
{
	char command[6];
	sprintf (command, "#:R%c#", new_rate);
	return tel_conn->writePort (command, 5);
}


/*!
 * Start slew.
 *
 * @see tel_set_rate() for speed.
 *
 * @param direction 	direction
 *
 * @return -1 on failure, 0 on sucess.
 */
int
Rts2DevTelescopeGemini::telescope_start_move (char direction)
{
	char command[6];
	// start worm if moving in RA DEC..
	/*  if (worm == 135 && (direction == DIR_EAST || direction == DIR_WEST))
		{
		  // G11 (version 3.x) have some problems with starting worm..give it some time
		  // to react
		  startWorm ();
		  usleep (USEC_SEC / 10);
		  startWorm ();
		  usleep (USEC_SEC / 10);
		  worm_move_needed = 1;
		}
	  else
		{
		  worm_move_needed = 0;
		} */
	sprintf (command, "#:M%c#", direction);
	return tel_conn->writePort (command, 5);
	// workaround suggested by Rene Goerlich
	//if (worm_move_needed == 1)
	//  stopWorm ();
}


/*!
 * Stop sleew.
 *
 * @see tel_start_slew for direction.
 */
int
Rts2DevTelescopeGemini::telescope_stop_move (char direction)
{
	char command[6];
	sprintf (command, "#:Q%c#", direction);
	if (worm_move_needed && (direction == DIR_EAST || direction == DIR_WEST))
	{
		worm_move_needed = 0;
		stopWorm ();
	}
	return tel_conn->writePort (command, 5);
}


void
Rts2DevTelescopeGemini::telescope_stop_goto ()
{
	tel_gemini_get (99, lastMotorState);
	tel_conn->writePort ("#:Q#", 4);
	if (lastMotorState & 8)
	{
		lastMotorState &= ~8;
		tel_gemini_set (99, lastMotorState);
	}
	worm_move_needed = 0;
}


int
Rts2DevTelescopeGemini::tel_start_move ()
{
	char retstr;
	char buf[55];

	telescope_stop_goto ();

	if ((tel_write_ra (lastMoveRa) < 0) || (tel_write_dec (lastMoveDec) < 0))
		return -1;
	if (tel_write_read ("#:MS#", 5, &retstr, 1) < 0)
		return -1;

	if (retstr == '0')
	{
		sleep (5);
		return 0;
	}
	// otherwise read reply..
	tel_conn->readPort (buf, 53, '#');
	if (retstr == '3')			 // manual control..
		return 0;
	return -1;
}


int
Rts2DevTelescopeGemini::startMove ()
{
	int newFlip = telFlip->getValueInteger ();
	bool willFlip = false;
	double ra_diff, ra_diff_flip;
	double dec_diff, dec_diff_flip;
	double ha;

	double max_not_flip, max_flip;

	struct ln_equ_posn pos;
	struct ln_equ_posn model_change;

	getTarget (&pos);

	tel_normalize (&pos.ra, &pos.dec);

	ra_diff = ln_range_degrees (getTelRa () - pos.ra);
	if (ra_diff > 180.0)
		ra_diff -= 360.0;

	dec_diff = getTelDec () - pos.dec;

	// get diff when we flip..
	ra_diff_flip =
		ln_range_degrees (180 + getTelRa () - pos.ra);
	if (ra_diff_flip > 180.0)
		ra_diff_flip -= 360.0;

	if (telLatitude->getValueDouble () > 0)
		dec_diff_flip = (90 - getTelDec () + 90 - pos.dec);
	else
		dec_diff_flip = (getTelDec () - 90 + pos.dec - 90);

	// decide which path is closer

	max_not_flip = MAX (fabs (ra_diff), fabs (dec_diff));
	max_flip = MAX (fabs (ra_diff_flip), fabs (dec_diff_flip));

	if (max_flip < max_not_flip)
		willFlip = true;

	logStream (MESSAGE_DEBUG) << "Losmandy start move ra_diff " << ra_diff <<
		" dec_diff " << dec_diff << " ra_diff_flip " << ra_diff_flip <<
		" dec_diff_flip  " << dec_diff_flip << " max_not_flip " << max_not_flip <<
		" max_flip " << max_flip << " willFlip " << willFlip << sendLog;

	// "do" flip
	if (willFlip)
		newFlip = !newFlip;

	// calculate current HA
	ha = getLocSidTime () - lastMoveRa;

	lastMoveRa = pos.ra;
	lastMoveDec = pos.dec;

	// normalize HA to meridian angle
	if (newFlip)
		ha = 90.0 - ha;
	else
		ha = ha + 90.0;

	ha = ln_range_degrees (ha);

	if (ha > 180.0)
		ha = 360.0 - ha;

	if (ha > 90.0)
		ha = ha - 180.0;

	logStream (MESSAGE_DEBUG) << "Losmandy start move newFlip " << newFlip <<
		" ha " << ha << sendLog;

	if (willFlip)
		ha += ra_diff_flip;
	else
		ha += ra_diff;

	logStream (MESSAGE_DEBUG) << "Losmandy start move newFlip " << newFlip <<
		" ha " << ha << sendLog;

	if (ha < haMinusLimit || ha > haPlusLimit)
	{
		willFlip = !willFlip;
		newFlip = !newFlip;
	}

	logStream (MESSAGE_DEBUG) << "Losmandy start move ha " << ha
		<< " ra_diff " << ra_diff
		<< " lastMoveRa " << lastMoveRa
		<< " telRa " << getTelRa ()
		<< " newFlip  " << newFlip
		<< " willFlip " << willFlip
		<< sendLog;

	// we fit to limit, aply model

	pos.ra = lastMoveRa;
	pos.dec = lastMoveDec;

	applyModel (&pos, &model_change, newFlip, ln_get_julian_from_sys ());

	lastMoveRa = pos.ra;
	lastMoveDec = pos.dec;

	fixed_ha = nan ("f");

	if (telMotorState != TEL_OK) // lastMoveRa && lastMoveDec will bring us to correct location after we finish rebooting/reparking
		return 0;

	startWorm ();

	struct ln_equ_posn telPos;
	getTelRaDec (&telPos);

	double sep = ln_get_angular_separation (&pos, &telPos);
	#ifdef DEBUG_EXTRA
	logStream (MESSAGE_DEBUG) << "Losmandy change sep " << sep << sendLog;
	#endif
	if (sep > 1)
	{
		tel_set_rate (RATE_SLEW);
		tel_gemini_set (GEMINI_CMD_RATE_CENTER, 8);
	}
	else
	{
		tel_set_rate (RATE_CENTER);
		tel_gemini_set (GEMINI_CMD_RATE_CENTER,
			centeringSpeed->getValueInteger ());
	}
	worm_move_needed = 0;

	forcedReparking = 0;

	time (&moveTimeout);
	moveTimeout += 300;

	return tel_start_move ();
}


/*!
 * Check, if telescope is still moving
 *
 * @return -1 on error, 0 if telescope isn't moving, 100 if telescope is moving
 */
int
Rts2DevTelescopeGemini::isMoving ()
{
	struct timeval now;
	if (telMotorState != TEL_OK)
		return USEC_SEC;
	gettimeofday (&now, NULL);
	// change move..
	#ifdef L4_GUIDE
	if (changeTime.tv_sec > 0)
	{
		if (isGuiding (&now))
			return 0;
		if (nextChangeDec != 0)
		{
			// worm need to be started - most probably due to error in Gemini
			startWorm ();
			// initiate dec change
			if (changeDec ())
				return -1;
			return 0;
		}
		return -2;
	}
	#else
	/*
	int ret;
	if (changeTimeRa.tv_sec > 0 || changeTimeRa.tv_usec > 0
		|| changeTimeDec.tv_sec > 0 || changeTimeDec.tv_usec > 0)
	{
		if (changeTimeRa.tv_sec > 0 || changeTimeRa.tv_usec > 0)
		{
			if (timercmp (&changeTimeRa, &now, <))
			{
				ret = telescope_stop_move (DIR_EAST);
				if (ret == -1)
					return ret;
				ret = telescope_stop_move (DIR_WEST);
				if (ret == -1)
					return ret;
				timerclear (&changeTimeRa);
				if (nextChangeDec != 0)
				{
					ret = change_dec (nextChangeDec);
					if (ret)
						return ret;
				}
			}
		}
		if (changeTimeDec.tv_sec > 0 || changeTimeDec.tv_usec > 0)
		{
			if (timercmp (&changeTimeDec, &now, <))
			{
				nextChangeDec = 0;
				ret = telescope_stop_move (DIR_NORTH);
				if (ret == -1)
					return ret;
				ret = telescope_stop_move (DIR_SOUTH);
				if (ret == -1)
					return ret;
				timerclear (&changeTimeDec);
			}
		}
		if (changeTimeRa.tv_sec == 0 && changeTimeRa.tv_usec == 0
			&& changeTimeDec.tv_sec == 0 && changeTimeDec.tv_usec == 0)
			return -2;
		return 0;
	}
	*/
	#endif
	if (now.tv_sec > moveTimeout)
	{
		stopMove ();
		return -2;
	}
	if (lastMotorState & 8)
		return USEC_SEC / 10;
	return -2;
}


int
Rts2DevTelescopeGemini::endMove ()
{
	int32_t track;
	#ifdef L4_GUIDE
	// don't start tracking while we are performing only change - it seems to
	// disturb RA tracking
	if (changeTime.tv_sec > 0)
	{
		if (!decChanged)
			startWorm ();
		decChanged = false;
		timerclear (&changeTime);
		return Rts2DevTelescope::endMove ();
	}
	#endif
	tel_gemini_get (130, track);
	setTimeout (USEC_SEC);
	if (tel_conn->writePort ("#:ONtest#", 9) < 0)
		return -1;
	return Rts2DevTelescope::endMove ();
}


int
Rts2DevTelescopeGemini::stopMove ()
{
	telescope_stop_goto ();
	#ifdef L4_GUIDE
	//	timerclear (&changeTime);
	#else
	//	timerclear (&changeTimeRa);
	//	timerclear (&changeTimeDec);
	#endif
	return 0;
}


/*
int
Rts2DevTelescopeGemini::startMoveFixedReal ()
{
	int ret;

	// compute ra
	lastMoveRa = getLocSidTime () * 15.0 - fixed_ha;

	tel_normalize (&lastMoveRa, &lastMoveDec);

	if (telMotorState != TEL_OK) // same as for startMove - after repark. we will get to correct location
		return 0;

	stopWorm ();

	time (&moveTimeout);
	moveTimeout += 300;

	ret = tel_start_move ();
	if (!ret)
	{
		setTarget (lastMoveRa, lastMoveDec);
		fixed_ntries++;
	}
	return ret;
}
*/

/*
int
Rts2DevTelescopeGemini::startMoveFixed (double tar_ha, double tar_dec)
{
	int32_t ra_ind;
	int32_t dec_ind;
	int ret;

	double ha_diff;
	double dec_diff;

	ret = tel_gemini_get (205, ra_ind);
	if (ret)
		return ret;
	ret = tel_gemini_get (206, dec_ind);
	if (ret)
		return ret;

	// apply offsets
	//tar_ha += ((double) ra_ind) / 3600;
	//tar_dec += ((double) dec_ind) / 3600.0;

	// we moved to fixed ra before..
	if (!isnan (fixed_ha))
	{
		// check if we can only change..
		// if we want to go to W, diff have to be negative, as RA is decreasing to W,
		// so we need to substract tar_ha from last one (smaller - bigger number)
		ha_diff = ln_range_degrees (fixed_ha - tar_ha);
		dec_diff = tar_dec - lastMoveDec;
		if (ha_diff > 180)
			ha_diff = ha_diff - 360;
		// do changes smaller then max change arc min using precision guide command
		#ifdef L4_GUIDE
		if (fabs (ha_diff) < maxPrecGuideRa
			&& fabs (dec_diff) < maxPrecGuideDec)
		#else
			if (fabs (ha_diff) < 5 / 60.0 && fabs (dec_diff) < 5 / 60.0)
		#endif
		{
			ret = change_real (ha_diff, dec_diff);
			if (!ret)
			{
				fixed_ha = tar_ha;
				lastMoveDec += dec_diff;
				// move_fixed = 0;        // we are performing change, not moveFixed
				maskState (TEL_MASK_MOVING, TEL_MOVING, "change started");
				return ret;
			}
		}
	}

	fixed_ha = tar_ha;
	lastMoveDec = tar_dec;

	fixed_ntries = 0;

	ret = startMoveFixedReal ();
	// move OK
	if (!ret)
		return Rts2DevTelescope::startMoveFixed (tar_ha, tar_dec);
	// try to do small change..
	#ifndef L4_GUIDE
	if (!isnan (fixed_ha) && fabs (ha_diff) < 5 && fabs (dec_diff) < 5)
	{
		ret = change_real (ha_diff, dec_diff);
		if (!ret)
		{
			fixed_ha = tar_ha;
			lastMoveDec += dec_diff;
			// move_fixed = 0;    // we are performing change, not moveFixed
			maskState (TEL_MASK_MOVING, TEL_MOVING, "change started");
			return Rts2DevTelescope::startMoveFixed (tar_ha, tar_dec);
		}
	}
	#endif
	return ret;
}

int
Rts2DevTelescopeGemini::isMovingFixed ()
{
	int ret;
	ret = isMoving ();
	// move ended
	if (ret == -2 && fixed_ntries < 3)
	{
		struct ln_equ_posn pos1, pos2;
		double sep;
		// check that we reach destination..
		info ();
		pos1.ra = getLocSidTime () * 15.0 - fixed_ha;
		pos1.dec = lastMoveDec;

		pos2.ra = telRaDec->getRa ();
		pos2.dec = telRaDec->getDec ();
		sep = ln_get_angular_separation (&pos1, &pos2);
								 // 15 seconds..
		if (sep > 15.0 / 60.0 / 4.0)
		{
			#ifdef DEBUG_EXTRA
			logStream (MESSAGE_DEBUG) << "Losmandy isMovingFixed sep " << sep *
				3600 << " arcsec" << sendLog;
			#endif
			// reque move..
			ret = startMoveFixedReal ();
			if (ret)			 // end in case of error
				return -2;
			return USEC_SEC;
		}
	}
	return ret;
}

int
Rts2DevTelescopeGemini::endMoveFixed ()
{
	int32_t track;
	stopWorm ();
	tel_gemini_get (130, track);
	setTimeout (USEC_SEC);
	if (tel_conn->writePort ("#:ONfixed#", 10) == 0)
		return Rts2DevTelescope::endMoveFixed ();
	return -1;
}
*/

/*
void
Rts2DevTelescopeGemini::clearSearch ()
{
	searchStep = 0;
	searchUsecTime = 0;
	lastSearchRaDiv = 0;
	lastSearchDecDiv = 0;
	timerclear (&nextSearchStep);
	worm_move_needed = 0;
}

int
Rts2DevTelescopeGemini::startSearch ()
{
	int ret;
	char *searchSpeedCh;
	clearSearch ();
	if (searchSpeed > 0.8)
		searchSpeed = 0.8;
	else if (searchSpeed < 0.2)
		searchSpeed = 0.2;
	// maximal guiding speed
	asprintf (&searchSpeedCh, "%lf", searchSpeed);
	ret = tel_gemini_setch (GEMINI_CMD_RATE_GUIDE, searchSpeedCh);
	free (searchSpeedCh);
	if (ret)
		return -1;
	ret = tel_gemini_set (GEMINI_CMD_RATE_CENTER, 1);
	if (ret)
		return -1;
	ret = tel_set_rate (RATE_GUIDE);
	searchUsecTime =
		(long int) (USEC_SEC * (searchRadius * 3600 / (searchSpeed * 15.0)));
	return changeSearch ();
}

int
Rts2DevTelescopeGemini::changeSearch ()
{
	int ret;
	struct timeval add;
	struct timeval now;
	if (telMotorState != TEL_OK)
		return -1;
	if (searchStep >= SEARCH_STEPS)
	{
		// that will stop move in all directions
		telescope_stop_goto ();
		return -2;
	}
	ret = 0;
	gettimeofday (&now, NULL);
	// change in RaDiv..
	if (lastSearchRaDiv != searchDirs[searchStep].raDiv)
	{
		// stop old in RA
		if (lastSearchRaDiv == 1)
		{
			ret = telescope_stop_move (DIR_WEST);
		}
		else if (lastSearchRaDiv == -1)
		{
			ret = telescope_stop_move (DIR_EAST);
		}
		if (ret)
			return -1;
	}
	// change in DecDiv..
	if (lastSearchDecDiv != searchDirs[searchStep].decDiv)
	{
		// stop old in DEC
		if (lastSearchDecDiv == 1)
		{
			ret = telescope_stop_move (DIR_NORTH);
		}
		else if (lastSearchDecDiv == -1)
		{
			ret = telescope_stop_move (DIR_SOUTH);
		}
		if (ret)
			return -1;
	}
	// send current location
	infoAll ();

	// change to new..
	searchStep++;
	add.tv_sec = searchUsecTime / USEC_SEC;
	add.tv_usec = searchUsecTime % USEC_SEC;
	timeradd (&now, &add, &nextSearchStep);

	if (lastSearchRaDiv != searchDirs[searchStep].raDiv)
	{
		lastSearchRaDiv = searchDirs[searchStep].raDiv;
		// start new in RA
		if (lastSearchRaDiv == 1)
		{
			ret = telescope_start_move (DIR_WEST);
		}
		else if (lastSearchRaDiv == -1)
		{
			ret = telescope_start_move (DIR_EAST);
		}
		if (ret)
			return -1;
	}
	if (lastSearchDecDiv != searchDirs[searchStep].decDiv)
	{
		lastSearchDecDiv = searchDirs[searchStep].decDiv;
		// start new in DEC
		if (lastSearchDecDiv == 1)
		{
			ret = telescope_start_move (DIR_NORTH);
		}
		else if (lastSearchDecDiv == -1)
		{
			ret = telescope_start_move (DIR_SOUTH);
		}
		if (ret)
			return -1;
	}
	return 0;
}

int
Rts2DevTelescopeGemini::isSearching ()
{
	struct timeval now;
	gettimeofday (&now, NULL);
	if (timercmp (&now, &nextSearchStep, >))
	{
		return changeSearch ();
	}
	return 100;
}

int
Rts2DevTelescopeGemini::stopSearch ()
{
	clearSearch ();
	telescope_stop_goto ();
	return Rts2DevTelescope::stopSearch ();
}

int
Rts2DevTelescopeGemini::endSearch ()
{
	clearSearch ();
	telescope_stop_goto ();
	return Rts2DevTelescope::endSearch ();
}
*/

int
Rts2DevTelescopeGemini::isParking ()
{
	char buf = '3';
	if (telMotorState != TEL_OK)
		return USEC_SEC;
	if (lastMotorState & 8)
		return USEC_SEC;
	tel_write_read ("#:h?#", 5, &buf, 1);
	switch (buf)
	{
		case '2':
		case ' ':
			return USEC_SEC;
		case '1':
			return -2;
		case '0':
			logStream (MESSAGE_ERROR) <<
				"Losmandy isParking called without park command" << sendLog;
		default:
			return -1;
	}
}


int
Rts2DevTelescopeGemini::endPark ()
{
	if (getMasterState () != SERVERD_NIGHT)
		tel_gemini_match_time ();
	setTimeout (USEC_SEC * 10);
	return stopWorm ();
}


/*!
 * Set telescope to match given coordinates
 *
 * This function is mainly used to tell the telescope, where it
 * actually is at the beggining of observation
 *
 * @param ra		setting right ascennation
 * @param dec		setting declination
 *
 * @return -1 and set errno on error, otherwise 0
 */
int
Rts2DevTelescopeGemini::setTo (double set_ra, double set_dec, int appendModel)
{
	char readback[101];

	tel_normalize (&set_ra, &set_dec);

	if ((tel_write_ra (set_ra) < 0) || (tel_write_dec (set_dec) < 0))
		return -1;

	if (appendModel)
	{
		if (tel_write_read_hash ("#:Cm#", 5, readback, 100) < 0)
			return -1;
	}
	else
	{
		int32_t v205;
		int32_t v206;
		int32_t v205_new;
		int32_t v206_new;
		tel_gemini_get (205, v205);
		tel_gemini_get (206, v206);
		if (tel_write_read_hash ("#:CM#", 5, readback, 100) < 0)
			return -1;
		tel_gemini_get (205, v205_new);
		tel_gemini_get (206, v206_new);
	}
	return 0;
}


int
Rts2DevTelescopeGemini::setTo (double set_ra, double set_dec)
{
	return setTo (set_ra, set_dec, 0);
}


/*
int
Rts2DevTelescopeGemini::correctOffsets (double cor_ra, double cor_dec,
double real_ra, double real_dec)
{
	int32_t v205;
	int32_t v206;
	int ret;
	// change allign parameters..
	tel_gemini_get (205, v205);
	tel_gemini_get (206, v206);
								 // convert from degrees to arcminutes
	v205 += (int) (cor_ra * 3600);
	v206 += (int) (cor_dec * 3600);
	tel_gemini_set (205, v205);
	ret = tel_gemini_set (206, v206);
	if (ret)
		return -1;
	return 0;
}
*/

/*!
 * Correct telescope coordinates.
 *
 * Used for closed loop coordinates correction based on astronometry
 * of obtained images.
 *
 * @param ra		ra correction
 * @param dec		dec correction
 *
 * This routine can be called from teld.cpp only when we haven't moved
 * from last location. However, we recalculate angular separation and
 * check if we are closer then 5 degrees to be 100% sure we will not
 * load mess to the telescope.
 *
 * @return -1 and set errno on error, 0 otherwise.
 */
/*
int
Rts2DevTelescopeGemini::correct (double cor_ra, double cor_dec,
double real_ra, double real_dec)
{
	struct ln_equ_posn pos1;
	struct ln_equ_posn pos2;
	double sep;
	int ret;

	if (tel_read_ra () || tel_read_dec ())
		return -1;

	// do not change if we are too close to poles
	if (fabs (telRaDec->getDec ()) > 85)
		return -1;

	setTelRa (getTelRa () - cor_ra);
	setTelDec (getTelDec () - cor_dec);

	// do not change if we are too close to poles
	if (fabs (getTelDec ()) > 85)
		return -1;

	pos1.ra = getTelRa ();
	pos1.dec = getTelDec ();
	pos2.ra = real_ra;
	pos2.dec = real_dec;

	sep = ln_get_angular_separation (&pos1, &pos2);
	#ifdef DEBUG_EXTRA
	logStream (MESSAGE_DEBUG) << "Losmandy correct separation " << sep <<
		sendLog;
	#endif
	if (sep > 5)
		return -1;

	ret = setTo (real_ra, real_dec, getNumCorr () < 2 ? 0 : 1);
	if (ret)
	{
		// mount correction failed - do local one
		return Rts2DevTelescope::correct (cor_ra, cor_dec, real_ra, real_dec);
	}
	return ret;
}
*/

#ifdef L4_GUIDE
/*
bool Rts2DevTelescopeGemini::isGuiding (struct timeval * now)
{
	int
		ret;
	char
		guiding;
	ret = tel_write_read (":Gv#", 4, &guiding, 1);
	if (guiding == 'G')
		guideDetected = true;
	if ((guideDetected && guiding != 'G')
		|| (!guideDetected && timercmp (&changeTime, now, <)))
		return false;
	return true;
}

int
Rts2DevTelescopeGemini::guide (char direction, unsigned int val)
{
	// convert to arcsec
	char buf[20];
	int len;
	int ret;
	len = sprintf (buf, ":Mi%c%i#", direction, val);
	return tel_conn->writePort (buf, len);
}

int
Rts2DevTelescopeGemini::changeDec ()
{
	char direction;
	struct timeval chng_time;
	int ret;
	direction = nextChangeDec > 0 ? DIR_NORTH : DIR_SOUTH;
	ret = guide (direction,
		(unsigned int) (fabs (nextChangeDec) * 255.0 /
		maxPrecGuideDec));
	if (ret)
	{
		nextChangeDec = 0;
		return ret;
	}
	decChanged = true;
	chng_time.tv_sec = (int) (ceil (fabs (nextChangeDec) * 240.0));
	gettimeofday (&changeTime, NULL);
	timeradd (&changeTime, &chng_time, &changeTime);
	guideDetected = false;
	nextChangeDec = 0;
	return 0;
}

int
Rts2DevTelescopeGemini::change_real (double chng_ra, double chng_dec)
{
	char direction;
	struct timeval chng_time;
	int ret = 0;

	 // smaller then 30 arcsec
	  if (chng_dec < 60.0 / 3600.0 && chng_ra < 60.0 / 3600.0)
		{
		  // set smallest rate..
		  tel_gemini_set (GEMINI_CMD_RATE_GUIDE, 0.2);
		  if (chng_ra < 30.0 / 3600.0)
	  chng_ra = 0;
		}
	  else
		{ *
	tel_gemini_set (GEMINI_CMD_RATE_GUIDE, guidingSpeed->getValueFloat ());
	if (!getFlip ())
	{
		chng_dec *= -1;
	}
	if (chng_ra != 0)
	{
		// first - RA direction
		// slew speed to 1 - 0.25 arcmin / sec
		if (chng_ra > 0)
		{
			direction = DIR_EAST;
		}
		else
		{
			direction = DIR_WEST;
		}
		ret =
			guide (direction,
			(unsigned int) (fabs (chng_ra) * 255.0 / maxPrecGuideRa));
		if (ret)
			return ret;

								 // * 0.8 * some constand, but that will left enought margin..
		chng_time.tv_sec = (int) (ceil (fabs (chng_ra) * 240.0));
		nextChangeDec = chng_dec;
		gettimeofday (&changeTime, NULL);
		timeradd (&changeTime, &chng_time, &changeTime);
		guideDetected = false;
		decChanged = false;
		return 0;
	}
	else if (chng_dec != 0)
	{
		nextChangeDec = chng_dec;
		// don't update guide,,
		// that will be chandled in changeDec
		return 0;
	}
	return -1;
}

*/
#else
/*
int
Rts2DevTelescopeGemini::change_ra (double chng_ra)
{
	char direction;
	long u_sleep;
	struct timeval now;
	int ret;
	// slew speed to 1 - 0.25 arcmin / sec
	direction = (chng_ra > 0) ? DIR_EAST : DIR_WEST;
	if (!isnan (fixed_ha))
	{
		if (fabs (chng_ra) > guideLimit->getValueDouble ())
			centeringSpeed->setValueInteger (20);
		else
			centeringSpeed->setValueInteger (2);
		// divide by sidereal speed we will use..
		u_sleep =
			(long) (((fabs (chng_ra) * 60.0) *
			(4.0 / ((float) centeringSpeed->getValueInteger ()))) *
			USEC_SEC);
	}
	else
	{
		u_sleep =
			(long) (((fabs (chng_ra) * 60.0) *
			(4.0 / guidingSpeed->getValueFloat ())) * USEC_SEC);
	}
	changeTimeRa.tv_sec = (long) (u_sleep / USEC_SEC);
	changeTimeRa.tv_usec = (long) (u_sleep - changeTimeRa.tv_sec);
	if (!isnan (fixed_ha))
	{
		if (direction == DIR_EAST)
		{
			ret = tel_set_rate (RATE_CENTER);
			if (ret)
				return ret;
			ret =
				tel_gemini_set (GEMINI_CMD_RATE_CENTER,
				centeringSpeed->getValueInteger ());
			if (ret)
				return ret;
			worm_move_needed = 1;
			ret = telescope_start_move (direction);
		}
		// west..only turn worm on
		else
		{
			ret = tel_set_rate (RATE_CENTER);
			if (ret)
				return ret;
			ret =
				tel_gemini_set (GEMINI_CMD_RATE_CENTER,
				centeringSpeed->getValueInteger ());
			if (ret)
				return ret;
			worm_move_needed = 1;
			ret = telescope_start_move (direction);
		}
	}
	else
	{
		ret = tel_set_rate (RATE_GUIDE);
		if (ret)
			return ret;
		ret =
			tel_gemini_set (GEMINI_CMD_RATE_GUIDE,
			guidingSpeed->getValueFloat ());
		if (ret)
			return ret;
		ret = telescope_start_move (direction);
	}
	gettimeofday (&now, NULL);
	timeradd (&changeTimeRa, &now, &changeTimeRa);
	return ret;
}

int
Rts2DevTelescopeGemini::change_dec (double chng_dec)
{
	char direction;
	long u_sleep;
	struct timeval now;
	int ret;
	// slew speed to 20 - 5 arcmin / sec
	direction = chng_dec > 0 ? DIR_NORTH : DIR_SOUTH;
	if (fabs (chng_dec) > guideLimit->getValueDouble ())
	{
		centeringSpeed->setValueInteger (20);
		ret = tel_set_rate (RATE_CENTER);
		if (ret == -1)
			return ret;
		ret =
			tel_gemini_set (GEMINI_CMD_RATE_CENTER,
			centeringSpeed->getValueInteger ());
		if (ret)
			return ret;
		u_sleep =
			(long) ((fabs (chng_dec) * 60.0) *
			(4.0 / ((float) centeringSpeed->getValueInteger ())) *
			USEC_SEC);
	}
	else
	{
		ret = tel_set_rate (RATE_GUIDE);
		if (ret == -1)
			return ret;
		ret =
			tel_gemini_set (GEMINI_CMD_RATE_GUIDE,
			guidingSpeed->getValueFloat ());
		if (ret)
			return ret;
		u_sleep =
			(long) ((fabs (chng_dec) * 60.0) *
			(4.0 / guidingSpeed->getValueFloat ()) * USEC_SEC);
	}
	changeTimeDec.tv_sec = (long) (u_sleep / USEC_SEC);
	changeTimeDec.tv_usec = (long) (u_sleep - changeTimeDec.tv_sec);
	ret = telescope_start_move (direction);
	if (ret)
		return ret;
	gettimeofday (&now, NULL);
	timeradd (&changeTimeDec, &now, &changeTimeDec);
	return ret;
}

int
Rts2DevTelescopeGemini::change_real (double chng_ra, double chng_dec)
{
	int ret;
	// center rate
	nextChangeDec = 0;
	if (!getFlip ())
	{
		chng_dec *= -1;
	}
	if (chng_ra != 0)
	{
		ret = change_ra (chng_ra);
		nextChangeDec = chng_dec;
		return ret;
	}
	else if (chng_dec != 0)
	{
		return change_dec (chng_dec);
	}
	return -1;
}
*/
#endif

/*
int
Rts2DevTelescopeGemini::change (double chng_ra, double chng_dec)
{
	int ret;
	ret = info ();
	if (ret)
		return ret;
	#ifdef DEBUG_EXTRA
	logStream (MESSAGE_INFO) << "Losmandy change ra " << telRaDec->getRa ()
		<< " dec " << telRaDec->getDec ()
		<< " move_fixed " << move_fixed
		<< sendLog;
	#endif
	// decide, if we make change, or move using move command
	#ifdef L4_GUIDE
	if (fabs (chng_ra) > maxPrecGuideRa || fabs (chng_dec) > maxPrecGuideDec)
	#else
		if (fabs (chng_ra) > 20 / 60.0 || fabs (chng_dec) > 20 / 60.0)
	#endif
	{
		if (move_fixed && !isnan (fixed_ha))
		{
			ret =
				startMoveFixed (fixed_ha - chng_ra,
				getTelDec () + chng_dec);
		}
		else
		{
			ret =
				startMove (getTelRa () + chng_ra,
				getTelDec () + chng_dec);
		}
		if (ret)
			return ret;
		// move wait ended .. log results
		#ifdef DEBUG_EXTRA
		logStream (MESSAGE_DEBUG) << "Losmandy change move " << ret << sendLog;
		#endif
	}
	else
	{
		return change_real (chng_ra, chng_dec);
	}
	if (ret == -1)
		return -1;
	return 0;
}
*/

/*!
 * Park telescope to neutral location.
 *
 * @return -1 and errno on error, 0 otherwise
 */
int
Rts2DevTelescopeGemini::startPark ()
{
	int ret;
	fixed_ha = nan ("f");
	if (telMotorState != TEL_OK)
		return -1;
	stopMove ();
	ret = tel_conn->writePort ("#:hP#", 5);
	if (ret < 0)
		return -1;
	sleep (1);
	return 0;
}


static int save_registers[] =
{
	0,							 // mount type
	120,						 // manual slewing speed
	140,						 // goto slewing speed
	GEMINI_CMD_RATE_GUIDE,		 // guiding speed
	GEMINI_CMD_RATE_CENTER,		 // centering speed
	200,						 // TVC stepout
	201,						 // modeling parameter A (polar axis misalignment in azimuth) in seconds of arc
	202,						 // modeling parameter E (polar axis misalignment in elevation) in seconds of arc
	203,						 // modeling parameter NP (axes non-perpendiculiraty at the pole) in seconds of arc
	204,						 // modeling parameter NE (axes non-perpendiculiraty at the Equator) in seconds of arc
	205,						 // modeling parameter IH (index error in hour angle) in seconds of arc
	206,						 // modeling parameter ID (index error in declination) in seconds of arc
	207,						 // modeling parameter FR (mirror flop/gear play in RE) in seconds of arc
	208,						 // modeling parameter FD (mirror flop/gear play in declination) in seconds of arc
	209,						 // modeling paremeter CF (counterweight & RA axis flexure) in seconds of arc
	211,						 // modeling parameter TF (tube flexure) in seconds of arc
	221,						 // ra limit 1
	222,						 // ra limit 2
	223,						 // ra limit 3
	411,						 // RA track divisor
	412,						 // DEC tracking divisor
	-1
};
int
Rts2DevTelescopeGemini::saveModel ()
{
	int *reg = save_registers;
	char buf[20];
	FILE *config_file;
	config_file = fopen (geminiConfig, "w");
	if (!config_file)
	{
		logStream (MESSAGE_ERROR) << "Gemini saveModel cannot open file " <<
			geminiConfig << sendLog;
		return -1;
	}

	while (*reg >= 0)
	{
		int ret;
		ret = tel_gemini_getch (*reg, buf);
		if (ret)
		{
			fprintf (config_file, "%i:error", *reg);
		}
		else
		{
			fprintf (config_file, "%i:%s\n", *reg, buf);
		}
		reg++;
	}
	fclose (config_file);
	return 0;
}


extern int
Rts2DevTelescopeGemini::loadModel ()
{
	FILE *config_file;
	char *line;
	size_t numchar;
	int id;
	int ret;
	config_file = fopen (geminiConfig, "r");
	if (!config_file)
	{
		logStream (MESSAGE_ERROR) << "Gemini loadModel cannot open file " <<
			geminiConfig << sendLog;
		return -1;
	}

	numchar = 100;
	line = (char *) malloc (numchar);
	while (getline (&line, &numchar, config_file) != -1)
	{
		char *buf;
		ret = sscanf (line, "%i:%as", &id, &buf);
		if (ret == 2)
		{
			ret = tel_gemini_setch (id, buf);
			if (ret)
				logStream (MESSAGE_ERROR) << "Gemini loadModel setch return " <<
					ret << " on " << buf << sendLog;

			free (buf);
		}
		else
		{
			logStream (MESSAGE_ERROR) << "Gemini loadModel invalid line " <<
				line << sendLog;
		}
	}
	free (line);
	return 0;
}


int
Rts2DevTelescopeGemini::stopWorm ()
{
	worm = 135;
	return tel_gemini_set (135, 1);
}


int
Rts2DevTelescopeGemini::startWorm ()
{
	worm = 131;
	return tel_gemini_set (131, 1);
}


int
Rts2DevTelescopeGemini::parkBootesSensors ()
{
	int ret;
	char direction;
	time_t timeout;
	time_t now;
	double old_tel_axis;
	ret = info ();
	startWorm ();
	// first park in RA
	old_tel_axis = featurePort->getValueInteger () & 1;
	direction = old_tel_axis ? DIR_EAST : DIR_WEST;
	time (&timeout);
	now = timeout;
	timeout += 20;
	ret = tel_set_rate (RATE_SLEW);
	if (ret)
		return ret;
	ret = telescope_start_move (direction);
	if (ret)
	{
		telescope_stop_move (direction);
		return ret;
	}
	while ((featurePort->getValueInteger () & 1) == old_tel_axis && now < timeout)
	{
		getAxis ();
		time (&now);
	}
	telescope_stop_move (direction);
	// then in dec
	//
	old_tel_axis = featurePort->getValueInteger () & 2;
	direction = old_tel_axis ? DIR_NORTH : DIR_SOUTH;
	time (&timeout);
	now = timeout;
	timeout += 20;
	ret = telescope_start_move (direction);
	if (ret)
	{
		telescope_stop_move (direction);
		return ret;
	}
	while ((featurePort->getValueInteger () & 2) == old_tel_axis && now < timeout)
	{
		getAxis ();
		time (&now);
	}
	telescope_stop_move (direction);
	tel_gemini_set (205, 0);
	tel_gemini_set (206, 0);
	return 0;
}


int
Rts2DevTelescopeGemini::resetMount ()
{
	int ret;
	if ((resetState->getValueInteger () == 1 || resetState->getValueInteger () == 2)
		&& bootesSensors)
	{
		// park using optical things
		parkBootesSensors ();
	}
	ret = tel_gemini_set (65535, 65535);
	if (ret)
		return ret;
	return Rts2DevTelescope::resetMount ();
}


int
Rts2DevTelescopeGemini::startDir (char *dir)
{
	switch (*dir)
	{
		case DIR_EAST:
		case DIR_WEST:
		case DIR_NORTH:
		case DIR_SOUTH:
			tel_set_rate (RATE_FIND);
			return telescope_start_move (*dir);
	}
	return -2;
}


int
Rts2DevTelescopeGemini::stopDir (char *dir)
{
	switch (*dir)
	{
		case DIR_EAST:
		case DIR_WEST:
		case DIR_NORTH:
		case DIR_SOUTH:
			return telescope_stop_move (*dir);
	}
	return -2;
}


int
Rts2DevTelescopeGemini::startGuide (char dir, double dir_dist)
{
	int ret;
	switch (dir)
	{
		case DIR_EAST:
		case DIR_WEST:
		case DIR_NORTH:
		case DIR_SOUTH:
			tel_set_rate (RATE_GUIDE);
			// set smallest rate..
			tel_gemini_set (GEMINI_CMD_RATE_GUIDE, 0.2);
			telGuidingSpeed->setValueDouble (0.2);
			ret = telescope_start_move (dir);
			if (ret)
				return ret;
			return Rts2DevTelescope::startGuide (dir, dir_dist);
	}
	return -2;
}


int
Rts2DevTelescopeGemini::stopGuide (char dir)
{
	int ret;
	switch (dir)
	{
		case DIR_EAST:
		case DIR_WEST:
		case DIR_NORTH:
		case DIR_SOUTH:
			ret = telescope_stop_move (dir);
			if (ret)
				return ret;
			return Rts2DevTelescope::stopGuide (dir);
	}
	return -2;
}


int
Rts2DevTelescopeGemini::stopGuideAll ()
{
	telescope_stop_goto ();
	return Rts2DevTelescope::stopGuideAll ();
}


int
Rts2DevTelescopeGemini::getFlip ()
{
	int32_t raTick, decTick;
	int ret;
	if (gem_version < 4)
	{
		double ha;
		ha = ln_range_degrees (getLocSidTime () * 15.0 - getTelRa ());

		return ha < 180.0 ? 1 : 0;
	}
	ret = tel_gemini_get (235, raTick, decTick);
	if (ret)
		return -1;
	axRa->setValueDouble (raTick);
	axDec->setValueDouble (decTick);
	if (decTick > decFlipLimit)
		return 1;
	return 0;
}


int
main (int argc, char **argv)
{
	Rts2DevTelescopeGemini device = Rts2DevTelescopeGemini (argc, argv);
	return device.run ();
}
