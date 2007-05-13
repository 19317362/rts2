#include "target.h"
#include "target_auger.h"
#include "rts2targetplanet.h"
#include "rts2targetgrb.h"
#include "rts2targetell.h"
#include "rts2obs.h"
#include "rts2obsset.h"
#include "rts2targetset.h"

#include "../utils/infoval.h"
#include "../utils/rts2app.h"
#include "../utils/rts2config.h"
#include "../utils/libnova_cpp.h"
#include "../utils/timestamp.h"

#include <sstream>
#include <iomanip>

EXEC SQL include sqlca;

void
Target::logMsgDb (const char *message, messageType_t msgType)
{
  logStream (msgType) << "SQL error: " << sqlca.sqlcode << " " <<
  sqlca.sqlerrm.sqlerrmc << " (at " << message << ")" << sendLog;
}

void
Target::getTargetSubject (std::string &subj)
{
  std::ostringstream _os;
  _os 
    << getTargetName ()
    << " #" << getObsTargetID () << " (" << getTargetID () << ")"
    << " " << getTargetType ();
  subj = _os.str();  
}

void
Target::sendTargetMail (int eventMask, const char *subject_text, Rts2Block *master)
{
  std::string mails;

  int count;
  // send mails
  mails = getUsersEmail (eventMask, count);
  if (count > 0)
  {
    std::ostringstream os;
    std::ostringstream subject;
    std::string tars;
    getTargetSubject (tars);
    subject << "TARGET " << tars << " " 
     << subject_text << " obs #" << getObsId ();
    // lazy observation init
    if (observation == NULL)
    {
      observation = new Rts2Obs (getObsId ());
      observation->load ();
      observation->setPrintImages (DISPLAY_ALL | DISPLAY_SUMMARY);
      observation->setPrintCounts (DISPLAY_ALL | DISPLAY_SUMMARY);
    }
    os << (*this) << std::endl << *observation;
    master->sendMailTo (subject.str().c_str(), os.str().c_str(), mails.c_str());
  }
}

void
Target::printAltTable (std::ostream & _os, double jd_start, double h_start, double h_end, double h_step, bool header)
{
  double i;
  struct ln_hrz_posn hrz;
  struct ln_equ_posn pos;
  double jd;
  
  int old_precison;
  std::ios_base::fmtflags old_settings;

  if (header)
  {
    old_precison = _os.precision (0);
    old_settings = _os.flags ();
    _os.setf (std::ios_base::fixed, std::ios_base::floatfield);
  }

  jd_start += h_start / 24.0;

  if (header)
  {
    _os
      << "H   ";

    char old_fill = _os.fill ('0');
    for (i = h_start; i <= h_end; i+=h_step)
      {
	_os << "  " << std::setw (2) << i;
      }
    _os.fill (old_fill);
    _os << std::endl;
  }

  // print alt + az
  std::ostringstream _os2;

  if (header)
  {
    _os << "ALT ";
    _os2 << "AZ  ";
    _os2.precision (0);
    _os2.setf (std::ios_base::fixed, std::ios_base::floatfield);
  }
  
  jd = jd_start;
  for (i = h_start; i <= h_end; i+=h_step, jd += h_step/24.0)
    {
      getAltAz (&hrz, jd);
      _os << " " << std::setw (3) << hrz.alt;
      _os2 << " " << std::setw (3) << hrz.az;
    }
  if (header)
  {
    _os
      << std::endl
      << _os2.str ()
      << std::endl;
  }
  else
  {
    _os << _os2.str ();
    return;
  }

  // print lunar distances
  if (header)
  {
    _os << "LD  ";
  }
  jd = jd_start;
  for (i = h_start; i <= h_end; i+=h_step, jd += h_step/24.0)
    {
      _os << " " << std::setw(3) << getLunarDistance (jd);
    }
  if (header)
  {
    _os << std::endl;
  }

  // print solar distance
  if (header)
  {
    _os << "SD  ";
  }
  jd = jd_start;
  for (i = h_start; i <= h_end; i+=h_step, jd += h_step/24.0)
    {
      _os << " " << std::setw(3) << getSolarDistance (jd);
    }
  if (header)
  {
    _os << std::endl;
  }


  // print sun position
  std::ostringstream _os3;

  if (header)
  {
    _os << "SAL ";
    _os3 << "SAZ ";
    _os3.precision (0);
    _os3.setf (std::ios_base::fixed, std::ios_base::floatfield);
  }

  jd = jd_start;
  for (i = h_start; i <= h_end; i+=h_step, jd += h_step/24.0)
    {
      ln_get_solar_equ_coords (jd, &pos);
      ln_get_hrz_from_equ (&pos, getObserver(), jd, &hrz);
      _os << " " << std::setw (3) << hrz.alt;
      _os3 << " " << std::setw (3) << hrz.az;
    }

  if (header)
  {
    _os
      << std::endl
      << _os3.str ()
      << std::endl;
  }
  else
  {
    _os << _os3.str ();
  }

  if (header)
  {
    _os.setf (old_settings);
    _os.precision (old_precison);
  }
}

void
Target::printAltTable (std::ostream & _os, double JD)
{
  double jd_start = ((int) JD) - 0.5;
  _os
    << std::endl
    << "** HOUR, ALTITUDE, AZIMUTH, MOON DISTANCE, SUN POSITION table from "
    << LibnovaDate (jd_start)
    << " **"
    << std::endl
    << std::endl;
 
 printAltTable (_os, jd_start, -1, 11);
 _os << std::endl;
 printAltTable (_os, jd_start, 12, 24);
}

void
Target::printAltTableSingleCol (std::ostream & _os, double jd_start, double i, double step)
{
  printAltTable (_os, jd_start, i, i+step, step * 2.0, false);
}

Target::Target (int in_tar_id, struct ln_lnlat_posn *in_obs):
Rts2Target ()
{
  Rts2Config *config;
  config = Rts2Config::instance ();

  observer = in_obs;

  int epId = 1;
  config->getInteger ("observatory", "epoch_id", epId);
  setEpoch (epId);

  minAlt = 0;
  config->getDouble ("observatory", "min_alt", minAlt);

  observation = NULL;

  target_id = in_tar_id;
  obs_target_id = -1;
  target_type = TYPE_UNKNOW;
  target_name = NULL;
  target_comment = NULL;
  targetUsers = NULL;

  startCalledNum = 0;

  airmassScale = 750.0;

  observationStart = -1;
}

Target::Target ()
{
  Rts2Config *config;
  config = Rts2Config::instance ();

  observer = config->getObserver ();

  int epId = 1;
  config->getInteger ("observatory", "epoch_id", epId);
  setEpoch (epId);

  minAlt = 0;
  config->getDouble ("observatory", "min_alt", minAlt);

  observation = NULL;

  target_id = -1;
  obs_target_id = -1;
  target_type = TYPE_UNKNOW;
  target_name = NULL;
  target_comment = NULL;
  targetUsers = NULL;

  startCalledNum = 0;

  airmassScale = 750.0;

  observationStart = -1;

  tar_priority = 0;
  tar_bonus = nan ("f");
  tar_bonus_time = 0;
  tar_next_observable = 0;
  bool n_tar_enabled = false;

  config->getFloat ("newtarget", "priority", tar_priority);
  config->getBoolean ("newtarget", "enabled", n_tar_enabled);
  setTargetEnabled (n_tar_enabled, false);
}

Target::~Target (void)
{
  endObservation (-1);
  delete[] target_name;
  delete[] target_comment;
  delete targetUsers;
  delete observation;
}

int
Target::load ()
{
  return loadTarget (getObsTargetID ());
}

int
Target::loadTarget (int in_tar_id)
{
  EXEC SQL BEGIN DECLARE SECTION;
  VARCHAR d_tar_name[TARGET_NAME_LEN];
  float d_tar_priority;
  int d_tar_priority_ind;
  float d_tar_bonus;
  int d_tar_bonus_ind;
  long d_tar_bonus_time;
  int d_tar_bonus_time_ind;
  long d_tar_next_observable;
  int d_tar_next_observable_ind;
  bool d_tar_enabled;
  int db_tar_id = in_tar_id;
  EXEC SQL END DECLARE SECTION;

  EXEC SQL
  SELECT 
    tar_name, 
    tar_priority,
    tar_bonus,
    EXTRACT (EPOCH FROM tar_bonus_time),
    EXTRACT (EPOCH FROM tar_next_observable),
    tar_enabled
  INTO
    :d_tar_name,
    :d_tar_priority :d_tar_priority_ind,
    :d_tar_bonus :d_tar_bonus_ind,
    :d_tar_bonus_time :d_tar_bonus_time_ind,
    :d_tar_next_observable :d_tar_next_observable_ind,
    :d_tar_enabled
  FROM
    targets
  WHERE
    tar_id = :db_tar_id;
  if (sqlca.sqlcode)
  {
    logMsgDb ("Target::load", MESSAGE_ERROR);
    return -1;
  }

  delete[] target_name;
  
  target_name = new char[d_tar_name.len + 1];
  strncpy (target_name, d_tar_name.arr, d_tar_name.len);
  target_name[d_tar_name.len] = '\0';

  if (d_tar_priority_ind >= 0)
    tar_priority = d_tar_priority;
  else
    tar_priority = 0;

  if (d_tar_bonus_ind >= 0)
    tar_bonus = d_tar_bonus;
  else
    tar_bonus = -1;

  if (d_tar_bonus_time_ind >= 0)
    tar_bonus_time = d_tar_bonus_time;
  else
    tar_bonus_time = 0;

  if (d_tar_next_observable_ind >= 0)
    tar_next_observable = d_tar_next_observable;
  else
    tar_next_observable = 0;

  setTargetEnabled (d_tar_enabled, false);
  
  // load target users for events..
  targetUsers = new Rts2TarUser (getTargetID (), getTargetType ());
  return 0;
}

int
Target::save (bool overwrite)
{
  EXEC SQL BEGIN DECLARE SECTION;
  int db_new_id = getTargetID ();
  EXEC SQL END DECLARE SECTION;

  // generate new id, if we don't have any 
  if (db_new_id <= 0)
  {
    EXEC SQL
    SELECT
      nextval ('tar_id')
    INTO
      :db_new_id;
    if (sqlca.sqlcode)
    {
      logMsgDb ("Target::save cannot get new tar_id", MESSAGE_ERROR);
      return -1;
    }
  }

  return save (overwrite, db_new_id);
}

int
Target::save (bool overwrite, int tar_id)
{
  // first, try an update..
  EXEC SQL BEGIN DECLARE SECTION;
  int db_tar_id = tar_id;
  char db_type_id = getTargetType ();
  VARCHAR db_tar_name[150];
  int db_tar_name_ind = 0;
  VARCHAR db_tar_comment[2000];
  int db_tar_comment_ind = 0;
  float db_tar_priority = getTargetPriority ();
  float db_tar_bonus = getTargetBonus ();
  int db_tar_bonus_ind;
  long db_tar_bonus_time = *(getTargetBonusTime ());
  int db_tar_bonus_time_ind;
  bool db_tar_enabled = getTargetEnabled ();
  EXEC SQL END DECLARE SECTION;
  // fill in name and comment..
  if (getTargetName ())
  {
    db_tar_name.len = strlen (getTargetName ());
    strcpy (db_tar_name.arr, getTargetName ());
  }
  else
  {
    db_tar_name.len = 0;
    db_tar_name_ind = -1;
  }
  
  if (getTargetComment ())
  {
    db_tar_comment.len = strlen (getTargetComment ());
    strcpy (db_tar_comment.arr, getTargetComment ());
  }
  else
  {
    db_tar_comment.len = 0;
    db_tar_comment_ind = -1;
  }

  if (isnan (db_tar_bonus))
  {
    db_tar_bonus = 0;
    db_tar_bonus_ind =-1;
    db_tar_bonus_time = 0;
    db_tar_bonus_time_ind = -1;
  }
  else
  {
    db_tar_bonus_ind = 0;
    if (db_tar_bonus_time > 0)
    {
      db_tar_bonus_time_ind = 0;
    }
    else
    {
      db_tar_bonus_time_ind = -1;
    }
  }
  
  // try inserting it..
  EXEC SQL
  INSERT INTO targets
  (
    tar_id,
    type_id,
    tar_name,
    tar_comment,
    tar_priority,
    tar_bonus,
    tar_bonus_time,
    tar_next_observable,
    tar_enabled
  )
  VALUES
  (
    :db_tar_id,
    :db_type_id,
    :db_tar_name :db_tar_name_ind,
    :db_tar_comment :db_tar_comment_ind,
    :db_tar_priority,
    :db_tar_bonus :db_tar_bonus_ind,
    abstime (:db_tar_bonus_time :db_tar_bonus_time_ind),
    null,
    :db_tar_enabled
  );
  // insert failed - try update
  if (sqlca.sqlcode)
  {
    EXEC SQL ROLLBACK;

    if (!overwrite)
    {
      return -1;
    }

    EXEC SQL
    UPDATE
      targets
    SET
      type_id = :db_type_id,
      tar_name = :db_tar_name,
      tar_comment = :db_tar_comment,
      tar_priority = :db_tar_priority,
      tar_bonus = :db_tar_bonus :db_tar_bonus_ind,
      tar_bonus_time = abstime(:db_tar_bonus_time :db_tar_bonus_time),
      tar_enabled = :db_tar_enabled
    WHERE
      tar_id = :db_tar_id;

    if (sqlca.sqlcode)
    {
      logMsgDb ("Target::save", MESSAGE_ERROR);
      EXEC SQL ROLLBACK;
      return -1;
    }
  }
  target_id = db_tar_id;

  EXEC SQL COMMIT;
  return 0;
}

int
Target::compareWithTarget (Target *in_target, double in_sep_limit)
{
  struct ln_equ_posn other_position;
  in_target->getPosition (&other_position);

  return (getDistance (&other_position) < in_sep_limit);
}

moveType
Target::startSlew (struct ln_equ_posn *position)
{
  EXEC SQL BEGIN DECLARE SECTION;
  int d_tar_id = getObsTargetID ();
  int d_obs_id;
  double d_obs_ra;
  double d_obs_dec;
  float d_obs_alt;
  float d_obs_az;
  EXEC SQL END DECLARE SECTION;

  struct ln_hrz_posn hrz;

  getPosition (position);

  if (getObsId () > 0) // we already observe that target
    return OBS_ALREADY_STARTED;

  d_obs_ra = position->ra;
  d_obs_dec = position->dec;
  ln_get_hrz_from_equ (position, observer, ln_get_julian_from_sys (), &hrz);
  d_obs_alt = hrz.alt;
  d_obs_az = hrz.az;

  EXEC SQL
  SELECT
    nextval ('obs_id')
  INTO
    :d_obs_id;
  EXEC SQL
  INSERT INTO
    observations
  (
    tar_id,
    obs_id,
    obs_slew,
    obs_ra,
    obs_dec,
    obs_alt,
    obs_az
  )
  VALUES
  (
    :d_tar_id,
    :d_obs_id,
    now (),
    :d_obs_ra,
    :d_obs_dec,
    :d_obs_alt,
    :d_obs_az
  );
  if (sqlca.sqlcode != 0)
  {
    logMsgDb ("cannot insert observation slew start to db", MESSAGE_ERROR);
    EXEC SQL ROLLBACK;
    return OBS_MOVE_FAILED;
  }
  EXEC SQL COMMIT;
  setObsId (d_obs_id);
  return afterSlewProcessed ();
}

moveType
Target::afterSlewProcessed ()
{
  return OBS_MOVE;
}

int
Target::startObservation (Rts2Block *master)
{
  EXEC SQL BEGIN DECLARE SECTION;
  int d_obs_id = getObsId ();
  EXEC SQL END DECLARE SECTION;
  if (observationStarted ())
    return 0;
  time (&observationStart);
  if (getObsId () > 0)
  {
    EXEC SQL
    UPDATE
      observations
    SET
      obs_start = now ()
    WHERE
      obs_id = :d_obs_id;
    if (sqlca.sqlcode != 0)
    {
      logMsgDb ("cannot start observation", MESSAGE_ERROR);
      EXEC SQL ROLLBACK;
      return -1;
    }
    EXEC SQL COMMIT;
    obsStarted ();

    sendTargetMail (SEND_START_OBS, "START OBSERVATION", master);
  }
  return 0;
}

int
Target::endObservation (int in_next_id, Rts2Block *master)
{
  int old_obs_id = getObsId ();

  int ret = endObservation (in_next_id);
  sendTargetMail (SEND_END_OBS, "END OF OBSERVATION", master);

  // check if that was the last observation..
  Rts2Obs out_observation = Rts2Obs (old_obs_id);
  out_observation.checkUnprocessedImages (master);

  return ret;
}

int
Target::endObservation (int in_next_id)
{
  EXEC SQL BEGIN DECLARE SECTION;
  int d_obs_id = getObsId ();
  int d_obs_state = getObsState ();
  EXEC SQL END DECLARE SECTION;

  if (isContinues () == 1 && in_next_id == getTargetID ())
    return 1;
  if (getObsId () > 0)
  {
    EXEC SQL
    UPDATE
      observations
    SET
      obs_end = now (),
      obs_state = :d_obs_state
    WHERE
      obs_id = :d_obs_id;
    if (sqlca.sqlcode != 0)
    {
      logMsgDb ("cannot end observation", MESSAGE_ERROR);
      EXEC SQL ROLLBACK;
      setObsId (-1);
      return -1;
    }
    EXEC SQL COMMIT;

    setObsId (-1);
  }
  observationStart = -1;
  return 0;
}

int
Target::isContinues ()
{
  return 0;
}

int
Target::observationStarted ()
{
  return (observationStart > 0) ? 1 : 0;
}

int
Target::secToObjectSet (double JD)
{
  struct ln_rst_time rst;
  int ret;
  ret = getRST (&rst, JD, getMinAlt ());
  if (ret)
    return -1;  // don't rise, circumpolar etc..
  ret = int ((rst.set - JD) * 86400.0);
  if (ret < 0)
    // hope we get current set, we are interested in next set..
    return ret + ((int (ret/86400) + 1) * -86400);
  return ret;
}

int
Target::secToObjectRise (double JD)
{
  struct ln_rst_time rst;
  int ret;
  ret = getRST (&rst, JD, getMinAlt ());
  if (ret)
    return -1;  // don't rise, circumpolar etc..
  ret = int ((rst.rise - JD) * 86400.0);
  if (ret < 0)
    // hope we get current set, we are interested in next set..
    return ret + ((int (ret/86400) + 1) * -86400);
  return ret;
}

int
Target::secToObjectMeridianPass (double JD)
{
  struct ln_rst_time rst;
  int ret;
  ret = getRST (&rst, JD, getMinAlt ());
  if (ret)
    return -1;  // don't rise, circumpolar etc..
  ret = int ((rst.transit - JD) * 86400.0);
  if (ret < 0)
    // hope we get current set, we are interested in next set..
    return ret + ((int (ret/86400) + 1) * -86400);
  return ret;
}

int
Target::beforeMove ()
{
  startCalledNum++;
  return 0;
}

int
Target::postprocess ()
{
  return 0;
}

/**
 * Return script for camera exposure.
 *
 * @param target        target id
 * @param camera_name   name of the camera
 * @param script        script
 * 
 * return -1 on error, 0 on success
 */
int
Target::getDBScript (const char *camera_name, char *script)
{
  EXEC SQL BEGIN DECLARE SECTION;
  int tar_id = getTargetID ();
  VARCHAR d_camera_name[8];
  VARCHAR sc_script[2000];
  int sc_indicator;
  EXEC SQL END DECLARE SECTION;
  
  d_camera_name.len = strlen (camera_name);
  strncpy (d_camera_name.arr, camera_name, d_camera_name.len);
  
  EXEC SQL 
  SELECT
    script
  INTO
    :sc_script :sc_indicator
  FROM
    scripts
  WHERE
    tar_id = :tar_id
    AND camera_name = :d_camera_name;
  if (sqlca.sqlcode == ECPG_NOT_FOUND)
    return -1;
  if (sqlca.sqlcode)
    goto err;
  if (sc_indicator < 0)
    goto err;
  strncpy (script, sc_script.arr, sc_script.len);
  script[sc_script.len] = '\0';
  return 0;
err:
  logMsgDb ("err db_get_script", MESSAGE_DEBUG);
  script[0] = '\0';
  return -1;
}


/**
 * Return script for camera exposure.
 *
 * @param device_name	camera device for script
 * @param buf		buffer for script
 *
 * @return 0 on success, < 0 on error
 */
int
Target::getScript (const char *device_name, char *buf)
{
  int ret;
  Rts2Config *config;
  config = Rts2Config::instance ();

  ret = getDBScript (device_name, buf);
  if (!ret)
    return 0;

  ret = config->getString (device_name, "script", buf, MAX_COMMAND_LENGTH);
  if (!ret)
    return 0;

  // default is empty script
  *buf = '\0';
  return -1;
}

int
Target::setScript (const char *device_name, const char *buf)
{
  EXEC SQL BEGIN DECLARE SECTION;
  VARCHAR d_camera_name[8];
  VARCHAR d_script[2000];
  int d_tar_id = getTargetID ();
  EXEC SQL END DECLARE SECTION;

  d_camera_name.len = strlen (device_name);
  if (d_camera_name.len > 8)
    d_camera_name.len = 8;
  strncpy (d_camera_name.arr, device_name, 8);

  d_script.len = strlen (buf);
  if (d_script.len > 2000)
    d_script.len = 2000;
  strncpy (d_script.arr, buf, d_script.len);

  EXEC SQL
  INSERT INTO scripts
  (
    camera_name,
    tar_id,
    script
  )
  VALUES
  (
    :d_camera_name,
    :d_tar_id,
    :d_script
  );
  // insert failed - try update
  if (sqlca.sqlcode)
  {
    EXEC SQL ROLLBACK;
    EXEC SQL UPDATE
      scripts
    SET
      script = :d_script
    WHERE
        camera_name = :d_camera_name
      AND tar_id = :d_tar_id;
    if (sqlca.sqlcode)
    {
      logMsgDb ("Target::setScript", MESSAGE_ERROR);
      EXEC SQL ROLLBACK;
      return -1;
    }
  }
  EXEC SQL COMMIT;
  return 0;
}

int
Target::getAltAz (struct ln_hrz_posn *hrz, double JD)
{
  int ret;
  struct ln_equ_posn object;

  ret = getPosition (&object, JD);
  if (ret)
    return ret;
  ln_get_hrz_from_equ (&object, observer, JD, hrz);
  return 0;
}

int
Target::getGalLng (struct ln_gal_posn *gal, double JD)
{
  struct ln_equ_posn curr;
  getPosition (&curr, JD);
  ln_get_gal_from_equ (&curr, gal);
  return 0;
}

double
Target::getGalCenterDist (double JD)
{
  static struct ln_equ_posn cntr = { 265.610844, -28.916790 };
  struct ln_equ_posn curr;
  getPosition (&curr, JD);
  return ln_get_angular_separation (&curr, &cntr); 
}

double
Target::getAirmass (double JD)
{
  struct ln_hrz_posn hrz;
  getAltAz (&hrz, JD);
  return ln_get_airmass (hrz.alt, airmassScale);
}

double
Target::getZenitDistance (double JD)
{
  struct ln_hrz_posn hrz;
  getAltAz (&hrz, JD);
  return 90.0 - hrz.alt;
}

double
Target::getHourAngle (double JD)
{
  double lst;
  double ha;
  struct ln_equ_posn pos;
  int ret;
  lst = ln_get_mean_sidereal_time (JD) * 15.0 + observer->lng;
  ret = getPosition (&pos);
  if (ret)
    return nan ("f");
  ha = ln_range_degrees (lst - pos.ra);
  return ha;
}

double
Target::getDistance (struct ln_equ_posn *in_pos, double JD)
{
  struct ln_equ_posn object;
  getPosition (&object, JD);
  return ln_get_angular_separation (&object, in_pos);
}

double
Target::getRaDistance (struct ln_equ_posn *in_pos, double JD)
{
  struct ln_equ_posn object;
  getPosition (&object, JD);
  return ln_range_degrees (object.ra - in_pos->ra);
}

double
Target::getSolarDistance (double JD)
{
  struct ln_equ_posn sun;
  ln_get_solar_equ_coords (JD, &sun);
  return getDistance (&sun, JD);
}

double
Target::getSolarRaDistance (double JD)
{
  struct ln_equ_posn sun;
  ln_get_solar_equ_coords (JD, &sun);
  return getRaDistance (&sun, JD);
}

double
Target::getLunarDistance (double JD)
{
  struct ln_equ_posn moon;
  ln_get_lunar_equ_coords (JD, &moon);
  return getDistance (&moon, JD);
}

double
Target::getLunarRaDistance (double JD)
{
  struct ln_equ_posn moon;
  ln_get_lunar_equ_coords (JD, &moon);
  return getRaDistance (&moon, JD);
}

/**
 * This method is called to check that target which was selected as good is
 * still the best.
 * 
 * It should reload from DB values, which are important for selection process,
 * and if they indicate that target should not be observed, it should return -1.
 */
int
Target::selectedAsGood ()
{
  EXEC SQL BEGIN DECLARE SECTION;
  bool d_tar_enabled;
  int d_tar_id = getTargetID ();
  float d_tar_priority;
  int d_tar_priority_ind;
  float d_tar_bonus;
  int d_tar_bonus_ind;
  long d_tar_next_observable;
  int d_tar_next_observable_ind;
  EXEC SQL END DECLARE SECTION;

  // check if we are still enabled..
  EXEC SQL
  SELECT
    tar_enabled,
    tar_priority,
    tar_bonus,
    EXTRACT (EPOCH FROM tar_next_observable)
  INTO
    :d_tar_enabled,
    :d_tar_priority :d_tar_priority_ind,
    :d_tar_bonus :d_tar_bonus_ind,
    :d_tar_next_observable :d_tar_next_observable_ind
  FROM
    targets
  WHERE
    tar_id = :d_tar_id;
  if (sqlca.sqlcode)
  {
    logMsgDb ("Target::selectedAsGood", MESSAGE_ERROR);
    return -1;
  }
  setTargetEnabled (d_tar_enabled, false);
  if (d_tar_priority_ind >= 0)
    tar_priority = d_tar_priority;
  else
    tar_priority = 0;
  if (d_tar_bonus_ind >= 0)
    tar_bonus = d_tar_bonus;
  else
    tar_bonus = 0;

  if (getTargetEnabled () && tar_priority + tar_bonus >= 0)
  {
    if (d_tar_next_observable_ind >= 0)
    {
      time_t now;
      time (&now);
      if (now < d_tar_next_observable)
	return -1;
    }
    return 0;
  }
  return -1;
}

/**
 * return 0 if we cannot observe that target, 1 if it's above horizon.
 */
int
Target::isGood (double lst, double JD, struct ln_equ_posn * pos)
{
  struct ln_hrz_posn hrz;
  getAltAz (&hrz, JD);
  if (hrz.alt < getMinAlt ())
    return 0;
  return Rts2Config::instance ()->getObjectChecker ()->is_good (lst, pos, &hrz);
}

int
Target::isGood (double JD)
{
  struct ln_equ_posn pos;
  getPosition (&pos, JD);
  return isGood (ln_get_mean_sidereal_time (JD) + observer->lng / 15.0, JD, &pos);
}

/****
 * 
 *   Return -1 if target is not suitable for observing,
 *   othrwise return 0.
 */
int
Target::considerForObserving (double JD)
{
  // horizon constrain..
  struct ln_equ_posn curr_position;
  double lst = ln_get_mean_sidereal_time (JD) + observer->lng / 15.0;
  int ret;
  if (getPosition (&curr_position, JD))
  {
    setNextObservable (JD + 1);
    return -1;
  }
  ret = isGood (lst, JD, &curr_position);
  if (!ret)
  {
    struct ln_rst_time rst;
    ret = getRST (&rst, JD, getMinAlt ());
    if (ret == -1)
    {
      // object doesn't rise, let's hope tomorrow it will rise
      logStream (MESSAGE_DEBUG)
        << "Target::considerForObserving tar " << getTargetID ()
	<< " obstarid " << getObsTargetID ()
	<< " don't rise"
	<< sendLog;
      setNextObservable (JD + 1);
      return -1;
    }
    // handle circumpolar objects..
    if (ret == 1)
    {
      logStream (MESSAGE_DEBUG) << "Target::considerForObserving tar "
        << getTargetID ()
	<< " obstarid " << getObsTargetID ()
	<< " is circumpolar, but is not good, scheduling after 10 minutes"
	<< sendLog;
      setNextObservable (JD + 10.0 / (24.0 * 60.0));
      return -1;
    }
    // object is above horizon, but checker reject it..let's see what
    // will hapens in 12 minutes 
    if (rst.transit < rst.set && rst.set < rst.rise)
    {
      // object rose, but is not above horizon, let's hope in 12 minutes it will get above horizon
      logStream (MESSAGE_DEBUG) << "Target::considerForObserving " 
        << getTargetID () 
	<< " obstarid " << getObsTargetID () 
	<< " will rise tommorow: " << LibnovaDate (rst.rise)
	<< " JD:" << JD
	<< sendLog;
      setNextObservable (JD + 12*(1.0/1440.0));
      return -1;
    }
    // object is setting, let's target it for next rise..
    logStream (MESSAGE_DEBUG)
      << "Target::considerForObserving " << getTargetID ()
      << " obstarid " << getObsTargetID ()
      << " will rise at: " << LibnovaDate (rst.rise)
      << sendLog;
    setNextObservable (rst.rise);
    return -1;
  }
  // target was selected for observation
  return selectedAsGood (); 
}

int
Target::dropBonus ()
{
  EXEC SQL BEGIN DECLARE SECTION;
  int db_tar_id;
  EXEC SQL END DECLARE SECTION;

  EXEC SQL UPDATE 
    targets 
  SET
    tar_bonus = NULL,
    tar_bonus_time = NULL
  WHERE
    tar_id = :db_tar_id;
  if (sqlca.sqlcode)
  {
    logMsgDb ("Target::dropBonus", MESSAGE_ERROR);
    EXEC SQL ROLLBACK;
    return -1;
  }
  EXEC SQL COMMIT;
  return 0;
}

float
Target::getBonus (double JD)
{
  return tar_priority + tar_bonus;
}

int
Target::changePriority (int pri_change, time_t *time_ch)
{
  EXEC SQL BEGIN DECLARE SECTION;
  int db_tar_id = getObsTargetID ();
  int db_priority_change = pri_change;
  int db_next_t = (int) *time_ch;
  EXEC SQL END DECLARE SECTION;
  
  EXEC SQL UPDATE 
    targets
  SET
    tar_bonus = tar_bonus + :db_priority_change,
    tar_bonus_time = abstime(:db_next_t)
  WHERE
    tar_id = :db_tar_id;
  if (sqlca.sqlcode)
  {
    logMsgDb ("Target::changePriority", MESSAGE_ERROR);
    EXEC SQL ROLLBACK;
    return -1;
  }
  EXEC SQL COMMIT;
  return 0;
}

int
Target::changePriority (int pri_change, double validJD)
{
  time_t next;
  ln_get_timet_from_julian (validJD, &next);
  return changePriority (pri_change, &next);
}

int
Target::setNextObservable (time_t *time_ch)
{
  EXEC SQL BEGIN DECLARE SECTION;
  int db_tar_id = getObsTargetID ();
  int db_next_observable;
  int db_next_observable_ind;
  EXEC SQL END DECLARE SECTION;

  if (time_ch)
  {
    db_next_observable = (int) *time_ch;
    db_next_observable_ind = 0;
    tar_next_observable = *time_ch;
  }
  else
  {
    db_next_observable = 0;
    db_next_observable_ind = -1;
    tar_next_observable = 0;
  }
  
  EXEC SQL UPDATE 
    targets
  SET
    tar_next_observable = abstime(:db_next_observable :db_next_observable_ind)
  WHERE
    tar_id = :db_tar_id;
  if (sqlca.sqlcode)
  {
    logMsgDb ("Target::setNextObservable", MESSAGE_ERROR);
    EXEC SQL ROLLBACK;
    return -1;
  }
  EXEC SQL COMMIT;
  return 0;
}

int
Target::setNextObservable (double validJD)
{
  time_t next;
  ln_get_timet_from_julian (validJD, &next);
  return setNextObservable (&next);
}

int
Target::getNumObs (time_t *start_time, time_t *end_time)
{
  EXEC SQL BEGIN DECLARE SECTION;
  int d_start_time = (int) *start_time;
  int d_end_time = (int) *end_time;
  int d_count;
  int d_tar_id = getTargetID ();
  EXEC SQL END DECLARE SECTION;

  EXEC SQL
  SELECT
    count (*)
  INTO
    :d_count
  FROM
    observations
  WHERE
      tar_id = :d_tar_id
    AND obs_slew >= abstime (:d_start_time)
    AND (obs_end is null 
      OR obs_end <= abstime (:d_end_time)
    );
  // runnign observations counts as well - hence obs_end is null

  return d_count;
}

double
Target::getLastObsTime ()
{
  EXEC SQL BEGIN DECLARE SECTION;
  int d_tar_id = getTargetID ();
  double d_time_diff;
  EXEC SQL END DECLARE SECTION;

  EXEC SQL
  SELECT
    min (EXTRACT (EPOCH FROM (now () - obs_slew)))
  INTO
    :d_time_diff
  FROM
    observations
  WHERE
    tar_id = :d_tar_id
  GROUP BY
    tar_id;

  if (sqlca.sqlcode)
    {
      if (sqlca.sqlcode == ECPG_NOT_FOUND)
        {
           // 1 year was the last observation..
           return 356 * 86400.0;
        }
      else
        logMsgDb ("Target::getLastObsTime", MESSAGE_ERROR);
    }
  return d_time_diff;
}

std::string
Target::getUsersEmail (int in_event_mask, int &count)
{
  if (targetUsers)
    return targetUsers->getUsers (in_event_mask, count);
  count = 0;
  return std::string ("");
}

double
Target::getFirstObs ()
{
  EXEC SQL BEGIN DECLARE SECTION;
  int db_tar_id = getTargetID ();
  double ret;
  EXEC SQL END DECLARE SECTION;
  EXEC SQL
  SELECT
    MIN (EXTRACT (EPOCH FROM obs_start))
  INTO
    :ret
  FROM
    observations
  WHERE
    tar_id = :db_tar_id;
  if (sqlca.sqlcode)
  {
    EXEC SQL ROLLBACK;
    return nan("f");
  }
  EXEC SQL ROLLBACK;
  return ret;
}

double
Target::getLastObs ()
{
  EXEC SQL BEGIN DECLARE SECTION;
  int db_tar_id = getTargetID ();
  double ret;
  EXEC SQL END DECLARE SECTION;
  EXEC SQL
  SELECT
    MAX (EXTRACT (EPOCH FROM obs_start))
  INTO
    :ret
  FROM
    observations
  WHERE
    tar_id = :db_tar_id;
  if (sqlca.sqlcode)
  {
    EXEC SQL ROLLBACK;
    return nan("f");
  }
  EXEC SQL ROLLBACK;
  return ret;
}

void
Target::printExtra (std::ostream &_os, double JD)
{
  if (getTargetEnabled ())
  {
    _os << "Target is enabled" << std::endl;
  }
  else
  {
    _os << "Target is disabled" << std::endl;
  }
  _os 
    << std::endl
    << InfoVal<double> ("TARGET PRIORITY", tar_priority)
    << InfoVal<double> ("TARGET BONUS", tar_bonus)
    << InfoVal<Timestamp> ("TARGET BONUS TIME", Timestamp(tar_bonus_time))
    << InfoVal<Timestamp> ("TARGET NEXT OBS.", Timestamp(tar_next_observable))
    << std::endl;
}

void
Target::printShortInfo (std::ostream & _os, double JD)
{
  struct ln_equ_posn pos;
  struct ln_hrz_posn hrz;
  const char * name = getTargetName ();
  int old_prec = _os.precision (2);
  getPosition (&pos, JD);
  getAltAz (&hrz, JD);
  LibnovaRaDec raDec (&pos);
  LibnovaHrz hrzP (&hrz);
  _os
   << std::setw (5) << getTargetID () << SEP
   << getTargetType () << SEP
   << std::left << std::setw (40) << (name ? name :  "null") << std::right << SEP
   << raDec << SEP
   << std::setw (5) << getAirmass (JD) << SEP
   << hrzP;
  _os.precision (old_prec);
}

void
Target::printDS9Reg (std::ostream & _os, double JD)
{
  struct ln_equ_posn pos;
  getPosition (&pos, JD);
  _os << "circle(" << pos.ra << "," << pos.dec << "," << getSDiam (JD) << ")" << std::endl;
}

void
Target::printShortBonusInfo (std::ostream & _os, double JD)
{
  struct ln_equ_posn pos;
  struct ln_hrz_posn hrz;
  const char * name = getTargetName ();
  int old_prec = _os.precision (2);
  getPosition (&pos, JD);
  getAltAz (&hrz, JD);
  LibnovaRaDec raDec (&pos);
  LibnovaHrz hrzP (&hrz);
  _os
   << std::setw (5) << getTargetID () << SEP
   << std::setw (7) << getBonus (JD) << SEP
   << getTargetType () << SEP
   << std::left << std::setw (40) << (name ? name :  "null") << std::right << SEP
   << raDec << SEP
   << std::setw (5) << getAirmass (JD) << SEP
   << hrzP;
  _os.precision (old_prec);
}


int
Target::printObservations (double radius, double JD, std::ostream &_os)
{
  struct ln_equ_posn tar_pos;
  getPosition (&tar_pos, JD);
  
  Rts2ObsSet obsset = Rts2ObsSet (&tar_pos, radius);
  _os << obsset;

  return obsset.size ();
}

Rts2TargetSet Target::getTargets (double radius)
{
  return getTargets (radius, ln_get_julian_from_sys ());
}

Rts2TargetSet Target::getTargets (double radius, double JD)
{
  struct ln_equ_posn tar_pos;
  getPosition (&tar_pos, JD);
  
  Rts2TargetSet tarset = Rts2TargetSet (&tar_pos, radius);
  return tarset;
}

int
Target::printTargets (double radius, double JD, std::ostream &_os)
{
  Rts2TargetSet tarset = getTargets (radius, JD);
  _os << tarset;

  return tarset.size ();
}

int
Target::printImages (double JD, std::ostream &_os, int flags)
{
  struct ln_equ_posn tar_pos;
  int ret;
  
  ret = getPosition (&tar_pos, JD);
  if (ret)
    return ret;

  Rts2ImgSetPosition img_set = Rts2ImgSetPosition (&tar_pos);
  ret = img_set.load ();
  if (ret)
    return ret;

  img_set.print (std::cout, flags);

  return img_set.size ();
}

Target *createTarget (int in_tar_id)
{
  return createTarget (in_tar_id, Rts2Config::instance()->getObserver ());
}

Target *createTarget (int in_tar_id, struct ln_lnlat_posn *in_obs)
{
  EXEC SQL BEGIN DECLARE SECTION;
  int db_tar_id = in_tar_id;
  char db_type_id;
  EXEC SQL END DECLARE SECTION;

  Target *retTarget;
  int ret;

  EXEC SQL
  SELECT
    type_id
  INTO
    :db_type_id
  FROM
    targets
  WHERE
    tar_id = :db_tar_id;

  if (sqlca.sqlcode)
  {
    logStream (MESSAGE_ERROR) << "createTarget cannot get entry from targets table for target with ID " << db_tar_id << sendLog;
    return NULL;
  }

  // get more informations about target..
  switch (db_type_id)
  {
    // calibration targets..
    case TYPE_DARK:
      retTarget = new DarkTarget (in_tar_id, in_obs);
      break;
    case TYPE_FLAT:
      retTarget = new FlatTarget (in_tar_id, in_obs);
      break;
    case TYPE_FOCUSING:
      retTarget = new FocusingTarget (in_tar_id, in_obs);
      break;
    case TYPE_CALIBRATION:
      retTarget = new CalibrationTarget (in_tar_id, in_obs);
      break;
    case TYPE_MODEL:
      retTarget = new ModelTarget (in_tar_id, in_obs);
      break;
    case TYPE_OPORTUNITY:
      retTarget = new OportunityTarget (in_tar_id, in_obs);
      break;
    case TYPE_ELLIPTICAL:
      retTarget = new EllTarget (in_tar_id, in_obs);
      break;
    case TYPE_GRB:
      retTarget = new TargetGRB (in_tar_id, in_obs, 3600, 86400, 5 * 86400);
      break;
    case TYPE_SWIFT_FOV:
      retTarget = new TargetSwiftFOV (in_tar_id, in_obs);
      break;
    case TYPE_INTEGRAL_FOV:
      retTarget = new TargetIntegralFOV (in_tar_id, in_obs);
      break;
    case TYPE_GPS:
      retTarget = new TargetGps (in_tar_id, in_obs);
      break;
    case TYPE_SKY_SURVEY:
      retTarget = new TargetSkySurvey (in_tar_id, in_obs);
      break;
    case TYPE_TERESTIAL:
      retTarget = new TargetTerestial (in_tar_id, in_obs);
      break;
    case TYPE_PLAN:
      retTarget = new TargetPlan (in_tar_id, in_obs);
      break;
    case TYPE_AUGER:
      retTarget = new TargetAuger (in_tar_id, in_obs, 1800);
      break;
    case TYPE_PLANET:
      retTarget = new TargetPlanet (in_tar_id, in_obs);
      break;
    default:
      retTarget = new ConstTarget (in_tar_id, in_obs);
      break;
  }

  retTarget->setTargetType (db_type_id);
  ret = retTarget->load ();
  if (ret)
  {
    logStream (MESSAGE_ERROR) << "Cannot create target: " << db_tar_id << " error code " <<
      sqlca.sqlcode << " message " << sqlca.sqlerrm.sqlerrmc << sendLog;
    EXEC SQL ROLLBACK;
    delete retTarget;
    return NULL;
  }
  EXEC SQL COMMIT;
  return retTarget;
}

void
sendEndMails (const time_t *t_from, const time_t *t_to, int printImages, int printCounts,
  struct ln_lnlat_posn *in_obs, Rts2App *master)
{
  EXEC SQL BEGIN DECLARE SECTION;
  long db_from = (long) *t_from;
  long db_end = (long) *t_to;
  int db_tar_id;
  EXEC SQL END DECLARE SECTION;

  double JD;
  JD = ln_get_julian_from_sys ();

  EXEC SQL DECLARE tar_obs_cur CURSOR FOR
  SELECT
    targets.tar_id
  FROM
    targets,
    observations
  WHERE
      targets.tar_id = observations.tar_id
    AND observations.obs_slew >= abstime (:db_from)
    AND observations.obs_end <= abstime (:db_end);
    
  EXEC SQL OPEN tar_obs_cur;

  while (1)
  {
    Target *tar;
    EXEC SQL FETCH next FROM tar_obs_cur INTO
      :db_tar_id;
    if (sqlca.sqlcode)
      break;
    tar = createTarget (db_tar_id, in_obs);
    if (tar)
    {
      int count;
      std::string mails = tar->getUsersEmail (SEND_END_NIGHT, count);
      if (count > 0)
      {
        std::string subject_text = std::string ("END OF NIGHT, TARGET #");
        Rts2ObsSet obsset = Rts2ObsSet (db_tar_id, t_from, t_to);
        tar->getTargetSubject (subject_text);
        std::ostringstream os;
        obsset.printImages (printImages);
        obsset.printCounts (printCounts);
        tar->sendInfo (os, JD);
        tar->printExtra (os, JD);
        os << obsset;
        master->sendMailTo (subject_text.c_str(), os.str().c_str(), mails.c_str());
      }
      delete tar;
    }
  }
  EXEC SQL CLOSE tar_obs_cur;
  EXEC SQL COMMIT;
}

void
Target::sendPositionInfo (std::ostream &_os, double JD)
{
  struct ln_hrz_posn hrz;
  struct ln_gal_posn gal;
  struct ln_rst_time rst;
  double hourangle;
  time_t now;
  int ret;

  ln_get_timet_from_julian (JD, &now);

  getAltAz (&hrz, JD);
  hourangle = getHourAngle (JD);
  _os 
    << InfoVal<LibnovaDeg90> ("ALTITUDE", LibnovaDeg90 (hrz.alt))
    << InfoVal<LibnovaDeg90> ("ZENITH DISTANCE", LibnovaDeg90 (getZenitDistance (JD)))
    << InfoVal<LibnovaDeg360> ("AZIMUTH", LibnovaDeg360 (hrz.az))
    << InfoVal<LibnovaRa> ("HOUR ANGLE", LibnovaRa (hourangle))
    << InfoVal<double> ("AIRMASS", getAirmass (JD))
    << std::endl;
  ret = getRST (&rst, JD, LN_STAR_STANDART_HORIZON);
  switch (ret)
  {
    case 1:
      _os << " - CIRCUMPOLAR - " << std::endl;
      rst.transit = JD + ((360 - hourangle) / 15.0 / 24.0);
      _os << InfoVal<TimeJDDiff> ("TRANSIT", TimeJDDiff (rst.transit, now));
      break;
    case -1:
      _os << " - DON'T RISE - " << std::endl;
      break;
    default:
      if (rst.set < rst.rise && rst.rise < rst.transit)
      {
	_os 
	  << InfoVal<TimeJDDiff> ("SET", TimeJDDiff (rst.set, now)) 
	  << InfoVal<TimeJDDiff> ("RISE", TimeJDDiff (rst.rise, now))
	  << InfoVal<TimeJDDiff> ("TRANSIT", TimeJDDiff (rst.transit, now));
      }
      else if (rst.transit < rst.set && rst.set < rst.rise)
      {
	_os 
	  << InfoVal<TimeJDDiff> ("TRANSIT", TimeJDDiff (rst.transit, now))
	  << InfoVal<TimeJDDiff> ("SET", TimeJDDiff (rst.set, now))
	  << InfoVal<TimeJDDiff> ("RISE", TimeJDDiff (rst.rise, now));
      }
      else
      {
	_os 
	  << InfoVal<TimeJDDiff> ("RISE", TimeJDDiff (rst.rise, now))
	  << InfoVal<TimeJDDiff> ("TRANSIT", TimeJDDiff (rst.transit, now))
	  << InfoVal<TimeJDDiff> ("SET", TimeJDDiff (rst.set, now));
      }
  }
  _os << std::endl
    << InfoVal<double> ("MIN_ALT", getMinAlt ())
    << std::endl
    << "RISE, SET AND TRANSIT ABOVE MIN_ALT"
    << std::endl
    << std::endl;
  ret = getRST (&rst, JD, getMinAlt ());
  switch (ret)
  {
    case 1:
      _os << " - CIRCUMPOLAR - " << std::endl;
      rst.transit = JD + ((360 - hourangle) / 15.0 / 24.0);
      _os << InfoVal<TimeJDDiff> ("TRANSIT", TimeJDDiff (rst.transit, now));
      break;
    case -1:
      _os << " - DON'T RISE - " << std::endl;
      break;
    default:
      if (rst.set < rst.rise && rst.rise < rst.transit)
      {
	_os 
	  << InfoVal<TimeJDDiff> ("SET", TimeJDDiff (rst.set, now)) 
	  << InfoVal<TimeJDDiff> ("RISE", TimeJDDiff (rst.rise, now))
	  << InfoVal<TimeJDDiff> ("TRANSIT", TimeJDDiff (rst.transit, now));
      }
      else if (rst.transit < rst.set && rst.set < rst.rise)
      {
	_os 
	  << InfoVal<TimeJDDiff> ("TRANSIT", TimeJDDiff (rst.transit, now))
	  << InfoVal<TimeJDDiff> ("SET", TimeJDDiff (rst.set, now))
	  << InfoVal<TimeJDDiff> ("RISE", TimeJDDiff (rst.rise, now));
      }
      else
      {
	_os 
	  << InfoVal<TimeJDDiff> ("RISE", TimeJDDiff (rst.rise, now))
	  << InfoVal<TimeJDDiff> ("TRANSIT", TimeJDDiff (rst.transit, now))
	  << InfoVal<TimeJDDiff> ("SET", TimeJDDiff (rst.set, now));
      }
  }

  printAltTable (_os, JD);

  getGalLng (&gal, JD);
  _os 
    << std::endl
    << InfoVal<LibnovaDeg360> ("GAL. LONGITUDE", LibnovaDeg360 (gal.l))
    << InfoVal<LibnovaDeg90> ("GAL. LATITUDE", LibnovaDeg90 (gal.b))
    << InfoVal<LibnovaDeg180> ("GAL. CENTER DIST.", LibnovaDeg180 (getGalCenterDist (JD)))
    << InfoVal<LibnovaDeg360> ("SOLAR DIST.", LibnovaDeg360 (getSolarDistance (JD)))
    << InfoVal<LibnovaDeg180> ("SOLAR RA DIST.", LibnovaDeg180 (getSolarRaDistance (JD)))
    << InfoVal<LibnovaDeg360> ("LUNAR DIST.", LibnovaDeg360 (getLunarDistance (JD)))
    << InfoVal<LibnovaDeg180> ("LUNAR RA DIST.", LibnovaDeg180 (getLunarRaDistance (JD)))
    << InfoVal<LibnovaDeg360> ("LUNAR PHASE", LibnovaDeg360 (ln_get_lunar_phase (JD)))
    << std::endl;
}

void
Target::sendInfo (std::ostream & _os, double JD)
{
  struct ln_equ_posn pos;
  double gst;
  double lst;
  time_t now, last;

  const char *name = getTargetName ();

  ln_get_timet_from_julian (JD, &now);

  getPosition (&pos, JD);

  _os 
    << InfoVal<int> ("ID", getTargetID ())
    << InfoVal<int> ("SEL_ID", getObsTargetID ())
    << InfoVal<const char *> ("NAME", (name ? name : "null name"))
    << InfoVal<char> ("TYPE", getTargetType ())
    << InfoVal<LibnovaRaJ2000> ("RA", LibnovaRaJ2000 (pos.ra))
    << InfoVal<LibnovaDecJ2000> ("DEC", LibnovaDecJ2000 (pos.dec))
    << std::endl;
    
  sendPositionInfo (_os, JD);
  
  last = now - 86400;
  _os 
    << InfoVal<int> ("24 HOURS OBS", getNumObs (&last, &now));
  last = now - 7 * 86400;  
  _os
    << InfoVal<int> ("7 DAYS OBS", getNumObs (&last, &now))
    << InfoVal<double> ("BONUS", getBonus (JD));

  // is above horizon?
  gst = ln_get_mean_sidereal_time (JD);
  lst = gst + getObserver()->lng / 15.0;
  _os << (isGood (lst, JD, & pos)
   ? "Target is above local horizon." 
   : "Target is below local horizon, it's not possible to observe it.")
   << std::endl;
  printExtra (_os, JD);
}

Rts2TargetSet *
Target::getCalTargets (double JD)
{
  return new Rts2TargetSetCalibration (this, JD);
}

void
Target::writeToImage (Rts2Image * image)
{
}

std::ostream &
operator << (std::ostream &_os, Target &target)
{
  target.sendInfo (_os);
  return _os;
}
