#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
/*! 
 * @file Driver for Dynostar telescope.
 * 
 * $Id$
 * 
 * It's something similar to LX200, with some changes.
 *
 * LX200 was based on original c library for XEphem writen by Ken
 * Shouse <ken@kshouse.engine.swri.edu> and modified by Carlos Guirao
 * <cguirao@eso.org>. But most of the code was completly
 * rewriten and you will barely find any piece from XEphem.
 *
 * @author john
 */

#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <math.h>
#include <libnova/libnova.h>
#include <sys/ioctl.h>

#include "telescope.h"
#include "hms.h"
#include "status.h"

#include <termios.h>
// uncomment following line, if you want all tel_desc read logging (will
// results in about 10 30-bytes lines to syslog for every query). 
// #define DEBUG_ALL_PORT_COMM
#define DEBUG

#define RATE_SLEW	'S'
#define RATE_FIND	'M'
#define RATE_CENTER	'C'
#define RATE_GUIDE	'G'
#define DIR_NORTH	'n'
#define DIR_EAST	'e'
#define DIR_SOUTH	's'
#define DIR_WEST	'w'
#define PORT_TIMEOUT	5

#define MOTORS_ON	1
#define MOTORS_OFF	-1

#define HOME_DEC	-87

// hard-coded LONGTITUDE & LATITUDE
// BOYDEN
#define TEL_LONG 	26.4056
#define TEL_LAT		-29.0389

// dec of visible earth rotational axis pole
#define TEL_POLE	(TEL_LAT > 0 ? 90 : -90)

// we shall not move below that POS - e.g. our target is to keep ax1 in <-1 * NOT_SAFE_POS, NOT_SAFE_POS>
// it must be < 100.0 for system to work
#define NOT_SAFE_POS	100.0

#define MIN(a,b)	((a) < (b) ? (a) : (b))
#define MAX(a,b)	((a) > (b) ? (a) : (b))

class Rts2DevTelescopeMM2:public Rts2DevTelescope
{
private:
  char *device_file;
  int tel_desc;

  double lastMoveRa, lastMoveDec;

  enum
  { NOT_MOVE, MOVE_HOME, MOVE_CLOSE_HOME, MOVE_REAL }
  move_state;

  enum
  { RUNNING, STOPED }
  worm_state;

  time_t last_pos_update;
  double last_pos_ra;

  time_t move_timeout;

  // low-level functions..
  int tel_read (char *buf, int count);
  int tel_read_hash (char *buf, int count);
  int tel_write (char *buf, int count);
  int tel_write_read (char *wbuf, int wcount, char *rbuf, int rcount);
  int tel_write_read_hash (char *wbuf, int wcount, char *rbuf, int rcount);
  int tel_read_hms (double *hmsptr, char *command);

  int tel_read_ra ();
  int tel_read_dec ();
  int tel_read_localtime ();
  int tel_read_siderealtime ();
  int tel_read_latitude ();
  int tel_read_longtitude ();
  int tel_rep_write (char *command);
  void tel_normalize (double *ra, double *dec);

  int tel_write_ra (double ra);
  int tel_write_dec (double dec);

  int tel_set_rate (char new_rate);
  int telescope_start_move (char direction);
  int telescope_stop_move (char direction);

  int tel_slew_to (double ra, double dec);

  int tel_check_coords ();

  void toggle_mode (int in_togle_count);
  void set_move_timeout (time_t plus_time);
  void goodPark ();

  double homeHA;		// value on which we will home mount; ussually is equal sidereal time * 15.0 - 90
  // ax1 have values ..-180..-90..0..90..180.. 0 is when mount is CWD position, fabs == 90 <=> at right angle (CW left or right)
  // telAxis > 0 when we are observing at east, < 0 when we are observing at west
  // it's similar to hour angle, but offset by 90 deg - following is true: fabs (HA - ax1) = 90 +- few arcmin
  //     (as calculation of telAxis is not precise)
  // ax1 is in arcdeg
  enum
  { SAFE_CWD, CLOSE_CWL, CLOSE_CWR, UNKNOW }
  cw_pos;			// counterweight position

public:
    Rts2DevTelescopeMM2 (int in_argc, char **in_argv);
    virtual ~ Rts2DevTelescopeMM2 (void);
  virtual int processOption (int in_opt);
  virtual int init ();
  virtual int idle ();
  virtual int ready ();
  virtual int initValues ();
  virtual int info ();

  virtual int setTo (double set_ra, double set_dec);
  virtual int correct (double cor_ra, double cor_dec, double real_ra,
		       double real_dec);

  virtual int startMove (double tar_ra, double tar_dec);
  virtual int isMoving ();
  virtual int endMove ();
  virtual int stopMove ();

  virtual int startPark ();
  virtual int isParking ();
  virtual int endPark ();

  virtual int stopWorm ();

  virtual int startDir (char *dir);
  virtual int stopDir (char *dir);
};

/*! 
 * Reads some data directly from tel_desc.
 * 
 * Log all flow as LOG_DEBUG to syslog
 * 
 * @exception EIO when there aren't data from tel_desc
 * 
 * @param buf 		buffer to read in data
 * @param count 	how much data will be readed
 * 
 * @return -1 on failure, otherwise number of read data 
 */
int
Rts2DevTelescopeMM2::tel_read (char *buf, int count)
{
  int readed;

  for (readed = 0; readed < count; readed++)
    {
      int ret = read (tel_desc, &buf[readed], 1);
      if (ret == 0)
	{
	  ret = -1;
	}
      if (ret < 0)
	{
	  logStream (MESSAGE_DEBUG) << "MM2 tel_read: tel_desc read error " <<
	    errno << sendLog;
	  return -1;
	}
#ifdef DEBUG_ALL_PORT_COMM
      logStream (MESSAGE_DEBUG) << "MM2 tel_read: readed " << buf[readed] <<
	sendLog;
#endif
    }
  return readed;
}

/*! 
 * Will read from tel_desc till it encoutered # character.
 * 
 * Read ending #, but doesn't return it.
 * 
 * @see tel_read() for description
 */
int
Rts2DevTelescopeMM2::tel_read_hash (char *buf, int count)
{
  int readed;
  buf[0] = 0;

  for (readed = 0; readed < count; readed++)
    {
      if (tel_read (&buf[readed], 1) < 0)
	return -1;
      if (buf[readed] == '#')
	break;
    }
  if (buf[readed] == '#')
    buf[readed] = 0;
  logStream (MESSAGE_DEBUG) << "MM2 tel_read_hash: Hash-readed " << buf <<
    sendLog;
  return readed;
}

/*! 
 * Will write on telescope tel_desc string.
 *
 * @exception EIO, .. common write exceptions 
 * 
 * @param buf 		buffer to write
 * @param count 	count to write
 * 
 * @return -1 on failure, count otherwise
 */

int
Rts2DevTelescopeMM2::tel_write (char *buf, int count)
{
  logStream (MESSAGE_DEBUG) << "MM2 tel_write: will write: " << buf <<
    sendLog;
  return write (tel_desc, buf, count);
}

/*! 
 * Combine write && read together.
 * 
 * Flush tel_desc to clear any gargabe.
 *
 * @exception EINVAL and other exceptions
 * 
 * @param wbuf		buffer to write on tel_desc
 * @param wcount	write count
 * @param rbuf		buffer to read from tel_desc
 * @param rcount	maximal number of characters to read
 * 
 * @return -1 and set errno on failure, rcount otherwise
 */

int
Rts2DevTelescopeMM2::tel_write_read (char *wbuf, int wcount, char *rbuf,
				     int rcount)
{
  int tmp_rcount;
  char *buf;

  if (tcflush (tel_desc, TCIOFLUSH) < 0)
    return -1;
  if (tel_write (wbuf, wcount) < 0)
    return -1;

  tmp_rcount = tel_read (rbuf, rcount);
  if (tmp_rcount > 0)
    {
      buf = (char *) malloc (rcount + 1);
      memcpy (buf, rbuf, rcount);
      buf[rcount] = 0;
      logStream (MESSAGE_DEBUG) << "MM2 tel_write_read: readed " << tmp_rcount
	<< " " << buf << sendLog;
      free (buf);
    }
  else
    {
      logStream (MESSAGE_DEBUG) << "MM2 tel_write_read: readed returns " <<
	tmp_rcount << sendLog;
    }

  return tmp_rcount;
}

/*! 
 * Combine write && read_hash together.
 *
 * @see tel_write_read for definition
 */
int
Rts2DevTelescopeMM2::tel_write_read_hash (char *wbuf, int wcount, char *rbuf,
					  int rcount)
{
  int tmp_rcount;

  if (tcflush (tel_desc, TCIOFLUSH) < 0)
    return -1;
  if (tel_write (wbuf, wcount) < 0)
    return -1;

  tmp_rcount = tel_read_hash (rbuf, rcount);

  return tmp_rcount;
}

/*! 
 * Reads some value from MM2 in hms format.
 * 
 * Utility function for all those read_ra and other.
 * 
 * @param hmsptr	where hms will be stored
 * 
 * @return -1 and set errno on error, otherwise 0
 */
int
Rts2DevTelescopeMM2::tel_read_hms (double *hmsptr, char *command)
{
  char wbuf[11];
  if (tel_write_read_hash (command, strlen (command), wbuf, 10) < 6)
    return -1;
  *hmsptr = hmstod (wbuf);
  if (errno)
    return -1;
  return 0;
}

/*! 
 * Reads MM2 right ascenation.
 * 
 * @return -1 and set errno on error, otherwise 0
 */
int
Rts2DevTelescopeMM2::tel_read_ra ()
{
  double new_ra;
  if (tel_read_hms (&new_ra, "#:GR#"))
    return -1;
  telRa->setValueDouble (new_ra * 15.0);
  return 0;
}

/*!
 * Reads MM2 declination.
 * 
 * @return -1 and set errno on error, otherwise 0
 */
int
Rts2DevTelescopeMM2::tel_read_dec ()
{
  double t_telDec;
  if (tel_read_hms (&t_telDec, "#:GD#"))
    return -1;
  telDec->setValueDouble (t_telDec);
  return 0;
}

/*! 
 * Returns MM2 local time.
 * 
 * @return -1 and errno on error, otherwise 0
 *
 * TEMPORARY
 * MY EDIT MM2 local time
 *
 */
int
Rts2DevTelescopeMM2::tel_read_localtime ()
{
  time_t curtime;
  struct tm *loctime;

  /* Get the current time. */
  time (&curtime);

  /* Convert it to local time representation. */
  loctime = localtime (&curtime);

  telLocalTime->setValueDouble (loctime->tm_hour + loctime->tm_min / 60 +
				loctime->tm_sec / 3600);

  return 0;
}

/*! 
 * Returns MM2 sidereal time.
 * 
 * @return -1 and errno on error, otherwise 0
 *
 * TEMPORARY
 * MY EDIT MM2 sidereal time
 *
 * Dynostar doesn't support reading Sidereal time, 
 * so read sidereal time from system
 */
int
Rts2DevTelescopeMM2::tel_read_siderealtime ()
{
  telSiderealTime->setValueDouble (getLocSidTime ());
  return 0;
}

/*! 
 * Reads MM2 latitude.
 * 
 * @return -1 on error, otherwise 0
 *
 */
int
Rts2DevTelescopeMM2::tel_read_latitude ()
{
  return 0;
}

/*! 
 * Reads MM2 longtitude.
 * 
 * @return -1 on error, otherwise 0
 *
 */
int
Rts2DevTelescopeMM2::tel_read_longtitude ()
{
  return 0;
}

/*! 
 * Repeat MM2 write.
 * 
 * Handy for setting ra and dec.
 * Meade tends to have problems with that, don't know about MM2.
 *
 * @param command	command to write on tel_desc
 */
int
Rts2DevTelescopeMM2::tel_rep_write (char *command)
{
  int count;
  char retstr;
  for (count = 0; count < 200; count++)
    {
      if (tel_write_read (command, strlen (command), &retstr, 1) < 0)
	return -1;
      if (retstr == '1')
	break;
      sleep (1);
      logStream (MESSAGE_DEBUG) << "MM2 tel_rep_write - for " << count <<
	" time." << sendLog;
    }
  if (count == 200)
    {
      logStream (MESSAGE_ERROR) <<
	"MM2 tel_rep_write unsucessful due to incorrect return." << sendLog;
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
Rts2DevTelescopeMM2::tel_normalize (double *ra, double *dec)
{
  if (*ra < 0)
    *ra = floor (*ra / 360) * -360 + *ra;	//normalize ra
  if (*ra > 360)
    *ra = *ra - floor (*ra / 360) * 360;

  if (*dec < -90)
    *dec = floor (*dec / 90) * -90 + *dec;	//normalize dec
  if (*dec > 90)
    *dec = *dec - floor (*dec / 90) * 90;
}

/*! 
 * Set MM2 right ascenation.
 *
 * @param ra		right ascenation to set in decimal degrees
 *
 * @return -1 and errno on error, otherwise 0
 */
int
Rts2DevTelescopeMM2::tel_write_ra (double ra)
{
  char command[14];
  int h, m, s;
  if (ra < 0 || ra > 360.0)
    {
      return -1;
    }
  ra = ra / 15;
  dtoints (ra, &h, &m, &s);
  if (snprintf (command, 14, "#:Sr%02d:%02d:%02d#", h, m, s) < 0)
    return -1;
  return tel_rep_write (command);
}

/*! 
 * Set MM2 declination.
 *
 * @param dec		declination to set in decimal degrees
 * 
 * @return -1 and errno on error, otherwise 0
 */
int
Rts2DevTelescopeMM2::tel_write_dec (double dec)
{
  char command[15];
  int h, m, s;
  if (dec < -90.0 || dec > 90.0)
    {
      return -1;
    }
  dtoints (dec, &h, &m, &s);
  if (snprintf (command, 15, "#:Sd%+02d*%02d:%02d#", h, m, s) < 0)
    return -1;
  return tel_rep_write (command);
}

Rts2DevTelescopeMM2::Rts2DevTelescopeMM2 (int in_argc, char **in_argv):Rts2DevTelescope (in_argc,
		  in_argv)
{
  device_file = "/dev/ttyS0";

  addOption ('f', "device_file", 1, "device file (ussualy /dev/ttySx");

  tel_desc = -1;

  move_state = NOT_MOVE;
  worm_state = RUNNING;

  telLongtitude->setValueDouble (TEL_LONG);
  telLatitude->setValueDouble (TEL_LAT);
}

Rts2DevTelescopeMM2::~Rts2DevTelescopeMM2 (void)
{
  int ret;
  // park us before deleting us
  startPark ();
  while ((ret = isParking ()) >= 0)
    {
      usleep (ret);
    }
  endPark ();
  close (tel_desc);
}

int
Rts2DevTelescopeMM2::processOption (int in_opt)
{
  switch (in_opt)
    {
    case 'f':
      device_file = optarg;
      break;
    default:
      return Rts2DevTelescope::processOption (in_opt);
    }
  return 0;
}

/*!
* Init telescope, connect on given tel_desc.
* 
* @param device_name		pointer to device name
* @param telescope_id		id of telescope, for MM2 ignored
* 
* @return 0 on succes, -1 & set errno otherwise
*/
int
Rts2DevTelescopeMM2::init ()
{
  struct termios tel_termios;
  char rbuf[10];

  int status;

  status = Rts2DevTelescope::init ();
  if (status)
    return status;

  tel_desc = open (device_file, O_RDWR);

  if (tel_desc < 0)
    return -1;

  if (tcgetattr (tel_desc, &tel_termios) < 0)
    return -1;

  if (cfsetospeed (&tel_termios, B9600) < 0 ||
      cfsetispeed (&tel_termios, B9600) < 0)
    return -1;

  tel_termios.c_iflag = IGNBRK & ~(IXON | IXOFF | IXANY);
  tel_termios.c_oflag = 0;
  tel_termios.c_cflag =
    ((tel_termios.c_cflag & ~(CSIZE)) | CS8) & ~(PARENB | PARODD);
  tel_termios.c_lflag = 0;
  tel_termios.c_cc[VMIN] = 0;
  tel_termios.c_cc[VTIME] = 5;

  if (tcsetattr (tel_desc, TCSANOW, &tel_termios) < 0)
    {
      logStream (MESSAGE_ERROR) << "MM2 init tcsetattr: " << strerror (errno)
	<< sendLog;
      return -1;
    }

// get current state of control signals 
  ioctl (tel_desc, TIOCMGET, &status);

// Drop DTR  
  status &= ~TIOCM_DTR;
  ioctl (tel_desc, TIOCMSET, &status);

  logStream (MESSAGE_DEBUG) << "MM2 init initialization complete on port " <<
    device_file << sendLog;

// we get 12:34:4# while we're in short mode
// and 12:34:45 while we're in long mode
  if (tel_write_read_hash ("#:Gr#", 5, rbuf, 9) < 0)
    return -1;

  if (rbuf[7] == '\0')
    {
      // that could be used to distinguish between long
      // short mode
      // we are in short mode, set the long on
      if (tel_write_read ("#:U#", 5, rbuf, 0) < 0)
	return -1;
      return 0;
    }
  stopWorm ();

  // try to gues telAxis initial state
  status = tel_read_ra ();
  if (status)
    return -1;

  telSiderealTime->setValueDouble (getLocSidTime ());

  // hour angle - 90
  ax1->
    setValueDouble (ln_range_degrees
		    (telSiderealTime->getValueDouble () * 15.0 -
		     telRa->getValueDouble () - 90.0));
  if (ax1->getValueDouble () > 180.0)
    ax1->setValueDouble (ax1->getValueDouble () - 360.0);

  // we suppose that we were before at safe range..
  // 90 is black magic:(, as we don't have more precise way how to measure that
  if (ax1->getValueDouble () < -90.0)
    {
      ax1->setValueDouble (ax1->getValueDouble () + 180);
    }
  if (ax1->getValueDouble () > NOT_SAFE_POS)
    {
      ax1->setValueDouble (ax1->getValueDouble () - 180);
    }

  time (&last_pos_update);
  last_pos_ra = telRa->getValueDouble ();

  status = startPark ();
  if (status)
    return -1;
  move_fixed = 0;
  maskState (TEL_MASK_MOVING, TEL_PARKING, "initial parking started");
  return 0;
}

int
Rts2DevTelescopeMM2::idle ()
{
  time_t now;
  // if we don't update pos for more then 5 seconds..
  time (&now);
  if (now > (last_pos_update + 2))
    {
      // let's update 
      // position by elapsed sidereal time
      // we are moving to west
      ax1->setValueDouble (ax1->getValueDouble () -
			   360.0 * (now -
				    last_pos_update) / LN_SIDEREAL_DAY_SEC);
      // there was big change in ra, which we should keep track of - it can
      // be result of manual move of the telescope or performed goto
      double ha_change;
      if (tel_read_ra () == 0)
	{
	  ha_change = telRa->getValueDouble () - last_pos_ra;
	  ha_change = ln_range_degrees (ha_change);
	  if (ha_change > 1.0 && ha_change < 359.0)
	    {
	      // take care of changes which are due to flip in dec..
	      // in 5 (change timeout) seconds we cannot move be more then few arcdeg
	      if (ha_change > 180.0)
		ha_change -= 360.0;
	      if (ha_change > 90.0)
		ha_change = ha_change - 180.0;
	      if (ha_change < -90.0)
		ha_change = ha_change + 180.0;
	      ax1->setValueDouble (ax1->getValueDouble () + ha_change);
	      last_pos_ra = telRa->getValueDouble ();
	    }
	  // too much on west..park and resync
	  if (ax1->getValueDouble () < -100.0)
	    {
	      int ret;
	      telSiderealTime->setValueDouble (getLocSidTime ());
	      homeHA = telSiderealTime->getValueDouble () * 15.0 + 90.0;
	      double ha_diff =
		ln_range_degrees (homeHA - telRa->getValueDouble ());
	      // keep on same side as current telRa
	      if (ha_diff < 90 || ha_diff > 270)
		homeHA = telSiderealTime->getValueDouble () * 15.0 - 90;
	      // make sure we will initiate move to part closer to our side..
	      homeHA -= 30.0;
	      homeHA = ln_range_degrees (homeHA);
	      // recalculate homeHA to correct half
	      ret = tel_slew_to (homeHA, telDec->getValueDouble ());
	      if (ret == 0)
		{
		  move_state = MOVE_CLOSE_HOME;
		  maskState (TEL_MASK_MOVING, TEL_MOVING,
			     "forced move to avoid pilar crash");
		}
	    }
	}
      logStream (MESSAGE_DEBUG) << "MM2 idle new pos " << ax1->
	getValueDouble () << " last_pos_ra " << last_pos_ra << sendLog;
      time (&last_pos_update);
    }
  return Rts2DevTelescope::idle ();
}

int
Rts2DevTelescopeMM2::ready ()
{
  return tel_desc > 0 ? 0 : -1;
}

/*!
* Reads information about telescope.
*
*/
int
Rts2DevTelescopeMM2::initValues ()
{
  if (tel_read_longtitude () || tel_read_latitude ())
    return -1;

  strcpy (telType, "MM2");
  telAltitude->setValueDouble (600);

  telFlip->setValueInteger (0);

  return Rts2DevTelescope::initValues ();
}

int
Rts2DevTelescopeMM2::info ()
{
  if (tel_read_ra () || tel_read_dec () || tel_read_localtime ())
    return -1;

  return Rts2DevTelescope::info ();
}

/*! 
* Set slew rate. For completness?
* 
* This functions are there IMHO mainly for historical reasons. They
* don't have any use, since if you start move and then die, telescope
* will move forewer till it doesn't hurt itself. So it's quite
* dangerous to use them for automatic observation. Better use quide
* commands from attached CCD, since it defines timeout, which rules
* CCD.
*
* @param new_rate	new rate to set. Uses RATE_<SPEED> constant.
* 
* @return -1 on failure & set errno, 5 (>=0) otherwise
*/
int
Rts2DevTelescopeMM2::tel_set_rate (char new_rate)
{
  char command[6];
  sprintf (command, "#:R%c#", new_rate);
  return tel_write (command, 5);
}

int
Rts2DevTelescopeMM2::telescope_start_move (char direction)
{
  char command[6];
  tel_set_rate (RATE_FIND);
  sprintf (command, "#:M%c#", direction);
  return tel_write (command, 5) == 1 ? -1 : 0;
}

int
Rts2DevTelescopeMM2::telescope_stop_move (char direction)
{
  char command[6];
  sprintf (command, "#:Q%c#", direction);
  return tel_write (command, 5) < 0 ? -1 : 0;
}

/*! 
* Slew (=set) MM2 to new coordinates.
*
* @param ra 		new right ascenation
* @param dec 		new declination
*
* @return -1 on error, otherwise 0
*/
int
Rts2DevTelescopeMM2::tel_slew_to (double ra, double dec)
{
  char retstr;

  tel_normalize (&ra, &dec);

  worm_state = RUNNING;

  if (tel_read_ra () < 0 || tel_read_dec () < 0
      || tel_write_ra (ra) < 0 || tel_write_dec (dec) < 0)
    return -1;
  if (tel_write_read ("#:MS#", 5, &retstr, 1) < 0)
    return -1;
  if (retstr == '0')
    {
      setTarget (ra, dec);
      set_move_timeout (100);
      return 0;
    }
  return -1;
}

/*! 
* Check, if telescope match target coordinates (set during tel_slew_to with Rts2DevTelescope::setTarget)
*
* @return -1 on error, 0 if not matched, 1 if matched, 2 if timeouted
*/
int
Rts2DevTelescopeMM2::tel_check_coords ()
{
  // ADDED BY JF
  double JD;

  double sep;
  time_t now;

  struct ln_lnlat_posn observer;
  struct ln_equ_posn object;
  struct ln_hrz_posn hrz;

  time (&now);
  if (now > move_timeout)
    return 2;

  if ((tel_read_ra () < 0) || (tel_read_dec () < 0))
    return -1;

  // ADDED BY JF
  // CALCULATE & PRINT ALT/AZ & HOUR ANGLE TO LOG
  object.ra = telRa->getValueDouble ();
  object.dec = telDec->getValueDouble ();

  observer.lng = telLongtitude->getValueDouble ();
  observer.lat = telLatitude->getValueDouble ();

  JD = ln_get_julian_from_sys ();

  ln_get_hrz_from_equ (&object, &observer, JD, &hrz);

  logStream (MESSAGE_DEBUG) << "MM2 tel_check_coords TELESCOPE ALT " << hrz.
    alt << " AZ " << hrz.az << sendLog;

  sep = getMoveTargetSep ();

  if (sep > 0.1)
    return 0;

  return 1;
}


void
Rts2DevTelescopeMM2::toggle_mode (int in_togle_count)
{
  int status;
  for (int i = 0; i < in_togle_count; i++)
    {
      // get current state of control signals 
      ioctl (tel_desc, TIOCMGET, &status);

      // DTR high
      status |= TIOCM_DTR;
      ioctl (tel_desc, TIOCMSET, &status);

      // get current state of control signals 
      ioctl (tel_desc, TIOCMGET, &status);

      sleep (4);

      // DTR low 
      status &= ~TIOCM_DTR;
      ioctl (tel_desc, TIOCMSET, &status);

      sleep (2);
      logStream (MESSAGE_DEBUG) << "MM2 toggle_mode toggle ends" << sendLog;
    }
}



void
Rts2DevTelescopeMM2::set_move_timeout (time_t plus_time)
{
  time_t now;
  time (&now);

  move_timeout = now + plus_time;
}

int
Rts2DevTelescopeMM2::startMove (double tar_ra, double tar_dec)
{
  int ret;
  if (cw_pos == UNKNOW)
    return -1;

  stopMove ();
  // test if we needed to park befor move..
  if (tel_read_ra () < 0 || tel_read_dec () < 0)
    return -1;

  // help variable - current pole distance and target pole distance
  double pole_dist_act = telDec->getValueDouble ();
  double pole_dist_tar = tar_dec;
  if (TEL_LAT < 0)
    {
      pole_dist_act = -1 * telDec->getValueDouble ();
      pole_dist_tar = -1 * tar_dec;
    }

  // if in -, substract from 90 (90 - (-50) = 140)
  if (pole_dist_act < 0)
    pole_dist_act = 90 - pole_dist_act;
  if (pole_dist_tar < 0)
    pole_dist_tar = 90 - pole_dist_tar;

  // calculate which path will telescope choose - if fliping in DEC, or moving in RA. 
  // That assumes Dynostar chooses closest path, which is observed result
  double step_diff_flip, step_diff;

  // we assume tar_ra = telRa + ra_diff
  double ra_diff = ln_range_degrees (tar_ra - telRa->getValueDouble ());

  step_diff_flip = MAX (pole_dist_act + pole_dist_tar, fabs (ra_diff - 180));

  step_diff =
    MAX (fabs (pole_dist_act - pole_dist_tar),
	 (ra_diff > 180) ? (360.0 - ra_diff) : ra_diff);

  // that's to calculate new ax1 value; it measure mount movement in RA unit, which dynostar will choose to perform
  if (step_diff_flip < step_diff)
    {
      // we will flip
      ra_diff = ra_diff - 180.0;
    }
  // else ra_diff will remain at calculated value

  // Dynostar will choose closer path, regaredes if it will hit the pilar or not
  // so put ra_diff to -180,180 range
  if (ra_diff > 180.0)
    ra_diff = ra_diff - 360.0;

  // calculate new pos..
  double new_pos = ax1->getValueDouble () + ra_diff;

  logStream (MESSAGE_DEBUG) << "MM2 startMove new_pos " << new_pos << sendLog;

  if ((new_pos < -90) || (new_pos > NOT_SAFE_POS))
    {
      // go throught parking..
      tel_read_siderealtime ();
      homeHA = telSiderealTime->getValueDouble () * 15.0 + 90;
      double ha_diff = ln_range_degrees (homeHA - telRa->getValueDouble ());
      // keep on same side as current telRa
      if (ha_diff < 90 || ha_diff > 270)
	homeHA = telSiderealTime->getValueDouble () * 15.0 - 90;
      // make sure we will initiate move to part closer to our side..
      if (new_pos < 0)
	homeHA -= 30.0;
      else
	homeHA += (NOT_SAFE_POS - 90);
      homeHA = ln_range_degrees (homeHA);
      // recalculate homeHA to correct half
      ret = tel_slew_to (homeHA, telDec->getValueDouble ());
      if (ret)
	{
	  move_state = NOT_MOVE;
	  return -1;
	}
      move_state = MOVE_CLOSE_HOME;
    }
  else
    {
      // just move..
      ret = tel_slew_to (tar_ra, tar_dec);
      if (ret)
	return -1;
      move_state = MOVE_REAL;
    }
  lastMoveRa = tar_ra;
  lastMoveDec = tar_dec;
  return 0;
}

int
Rts2DevTelescopeMM2::isMoving ()
{
  int ret;

  switch (move_state)
    {
    case MOVE_HOME:
    case MOVE_CLOSE_HOME:
      ret = tel_check_coords ();
      switch (ret)
	{
	case -1:
	  return -1;
	case 0:
	  return USEC_SEC / 10;
	case 1:
	  if (move_state == MOVE_CLOSE_HOME)
	    {
	      tel_read_siderealtime ();
	      // repark mount after having it parked just a bit..
	      homeHA = telSiderealTime->getValueDouble () * 15.0 + 90;
	      // keep on same side as current telRa
	      if (ln_range_degrees (homeHA - telRa->getValueDouble ()) > 180)
		homeHA = telSiderealTime->getValueDouble () * 15.0 - 90;
	      homeHA = ln_range_degrees (homeHA);
	      ret = tel_slew_to (homeHA, telDec->getValueDouble ());
	      if (ret)
		return -1;
	      move_state = MOVE_HOME;
	      return USEC_SEC / 10;
	    }
	  stopMove ();
	  goodPark ();
	  ret = tel_slew_to (lastMoveRa, lastMoveDec);
	  if (ret)
	    return -1;
	  move_state = MOVE_REAL;
	  return USEC_SEC / 10;
	case 2:
	  // timeout should not happen
	  cw_pos = UNKNOW;
	  return -2;
	}
      break;
    case MOVE_REAL:
      ret = tel_check_coords ();
      switch (ret)
	{
	case -1:
	  return -1;
	case 0:
	  return USEC_SEC / 10;
	case 1:
	case 2:
	  // check for new coordinates, recalculate pos & cw_pos
	  move_state = NOT_MOVE;
	  return -2;
	}
      break;
    case NOT_MOVE:
      break;
    }
  return -1;
}

int
Rts2DevTelescopeMM2::endMove ()
{
  // wait for mount to settle down after move
  sleep (2);
  return Rts2DevTelescope::endMove ();
}

int
Rts2DevTelescopeMM2::stopMove ()
{
  char dirs[] = { 'e', 'w', 'n', 's' };
  int i;
  for (i = 0; i < 4; i++)
    {
      if (telescope_stop_move (dirs[i]) < 0)
	return -1;
    }
  return 0;
}

/*! 
 * Set telescope to match given coordinates
 *
 * This function is mainly used to tell the telescope, where it
 * actually is at the beggining of observation (remember, that MM2
 * doesn't have absolute position sensors)
 * 
 * @param ra		setting right ascennation
 * @param dec		setting declination
 * 
 * @return -1 and set errno on error, otherwise 0
 */

int
Rts2DevTelescopeMM2::setTo (double ra, double dec)
{
  char readback[101];
  int ret;

  tel_normalize (&ra, &dec);

  if ((tel_write_ra (ra) < 0) || (tel_write_dec (dec) < 0))
    return -1;
  if (tel_write_read_hash ("#:CM#", 5, readback, 100) < 0)
    return -1;
  // since we are carring operation critical for next movements of telescope, 
  // we are obliged to check its correctness 
  set_move_timeout (10);
  ret = tel_check_coords ();
  return ret == 1;
}

/*!
 * Correct telescope coordinates.
 *
 * Used for closed loop coordinates correction based on astronometry
 * of obtained images.
 *
 * @param ra		ra correction
 * @param dec		dec correction
 *
 * @return -1 and set errno on error, 0 otherwise.
 */
int
Rts2DevTelescopeMM2::correct (double cor_ra, double cor_dec, double real_ra,
			      double real_dec)
{
  if (setTo (real_ra, real_dec))
    return -1;
  return 0;
}

/*! 
 * Park telescope to neutral location.
 *
 * @return -1 and errno on error, 0 otherwise
 */
int
Rts2DevTelescopeMM2::startPark ()
{
  int ret;
  tel_read_siderealtime ();
  homeHA = telSiderealTime->getValueDouble () * 15.0 + 90;
  homeHA = ln_range_degrees (homeHA);
  ret = tel_slew_to (homeHA, HOME_DEC);
  cw_pos = UNKNOW;
  return ret;
}

int
Rts2DevTelescopeMM2::isParking ()
{
  int ret;

  ret = tel_check_coords ();
  switch (ret)
    {
    case -1:
      return -1;
    case 0:
      return USEC_SEC / 10;
    case 1:
      goodPark ();
      break;
    case 2:
      cw_pos = UNKNOW;
      break;			// timeout
    }
  return -2;
}

int
Rts2DevTelescopeMM2::endPark ()
{
  stopWorm ();
  return 0;
}

int
Rts2DevTelescopeMM2::stopWorm ()
{
  toggle_mode (2);
  worm_state = STOPED;
  return 0;
}

void
Rts2DevTelescopeMM2::goodPark ()
{
  tel_read_siderealtime ();
  last_pos_ra = telRa->getValueDouble ();
  ax1->
    setValueDouble (ln_range_degrees
		    (telRa->getValueDouble () -
		     telSiderealTime->getValueDouble () * 15.0 - 90));
  if (ax1->getValueDouble () > 180.0)
    {
      ax1->setValueDouble (360 - ax1->getValueDouble ());
    }
  if (ax1->getValueDouble () > 90.0)
    {
      ax1->setValueDouble (180.0 - ax1->getValueDouble ());
    }
  cw_pos = SAFE_CWD;
  logStream (MESSAGE_DEBUG) << "MM2 isParking reset pos " << ax1->
    getValueDouble () << sendLog;
}

int
Rts2DevTelescopeMM2::startDir (char *dir)
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
Rts2DevTelescopeMM2::stopDir (char *dir)
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
main (int argc, char **argv)
{
  Rts2DevTelescopeMM2 device = Rts2DevTelescopeMM2 (argc, argv);
  return device.run ();
}
