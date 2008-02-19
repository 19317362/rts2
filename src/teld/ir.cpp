/* 
 * Driver for OpemTPL mounts.
 * Copyright (C) 2005-2007 Petr Kubanek <petr@kubanek.net>
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

#include <sstream>
#include <fstream>

#include "../utils/rts2config.h"
#include "ir.h"

#define DEBUG_EXTRA

using namespace OpenTPL;

#define LONGITUDE -3.3847222
#define LATITUDE 37.064167
#define ALTITUDE 2896

int
Rts2TelescopeIr::coverClose ()
{
	int status = TPL_OK;
	double targetPos;
	if (cover_state == CLOSED)
		return 0;
	status = irConn->tpl_set ("COVER.TARGETPOS", 0, &status);
	status = irConn->tpl_set ("COVER.POWER", 1, &status);
	status = irConn->tpl_get ("COVER.TARGETPOS", targetPos, &status);
	cover_state = CLOSING;
	logStream (MESSAGE_INFO) << "closing cover, status" << status <<
		" target position " << targetPos << sendLog;
	return status;
}


int
Rts2TelescopeIr::coverOpen ()
{
	int status = TPL_OK;
	if (cover_state == OPENED)
		return 0;
	status = irConn->tpl_set ("COVER.TARGETPOS", 1, &status);
	status = irConn->tpl_set ("COVER.POWER", 1, &status);
	cover_state = OPENING;
	logStream (MESSAGE_INFO) << "opening cover, status" << status << sendLog;
	return status;
}


int
Rts2TelescopeIr::domeOpen ()
{
	int status = TPL_OK;
	dome_state = D_OPENING;
	status = irConn->tpl_set ("DOME[1].TARGETPOS", 1, &status);
	status = irConn->tpl_set ("DOME[2].TARGETPOS", 1, &status);
	logStream (MESSAGE_INFO) << "opening dome, status " << status << sendLog;
	return status;
}


int
Rts2TelescopeIr::domeClose ()
{
	int status = TPL_OK;
	dome_state = D_CLOSING;
	status = irConn->tpl_set ("DOME[1].TARGETPOS", 0, &status);
	status = irConn->tpl_set ("DOME[2].TARGETPOS", 0, &status);
	logStream (MESSAGE_INFO) << "closing dome, status " << status << sendLog;
	return status;
}


int
Rts2TelescopeIr::setTrack (int new_track)
{
	int status = TPL_OK;
	status = irConn->tpl_set ("POINTING.TRACK", new_track, &status);
	if (status != TPL_OK)
	{
		logStream (MESSAGE_ERROR) << "Cannot setTrack" << sendLog;
	}
	return status;
}


int
Rts2TelescopeIr::setTrack (int new_track, bool autoEn)
{
	return setTrack ((new_track & ~128) | (autoEn ? 128 : 0));
}


int
Rts2TelescopeIr::setValue (Rts2Value * old_value, Rts2Value * new_value)
{
	int status = TPL_OK;
	if (old_value == cabinetPower)
	{
		status =
			irConn->tpl_set ("CABINET.POWER",
			((Rts2ValueBool *) new_value)->getValueBool ()? 1 : 0,
			&status);
		if (status != TPL_OK)
			return -2;
		return 0;
	}
	if (old_value == derotatorOffset)
	{
		status =
			irConn->tpl_set ("DEROTATOR[3].OFFSET", -1 * new_value->getValueDouble (),
			&status);
		if (status != TPL_OK)
			return -2;
		return 0;
	}
	if (old_value == derotatorCurrpos)
	{
		status =
			irConn->tpl_set ("DEROTATOR[3].TARGETPOS", new_value->getValueDouble (),
			&status);
		if (status != TPL_OK)
			return -2;
		return 0;
	}
	if (old_value == derotatorPower)
	{
		status =
			irConn->tpl_set ("DEROTATOR[3].POWER",
			((Rts2ValueBool *) new_value)->getValueBool ()? 1 : 0,
			&status);
		if (status != TPL_OK)
			return -2;
		return 0;
	}
	if (old_value == cover)
	{
		switch (new_value->getValueInteger ())
		{
			case 1:
				status = coverOpen ();
				break;
			case 0:
				status = coverClose ();
				break;
			default:
				return -2;
		}
		if (status != TPL_OK)
			return -2;
		return 0;
	}
	if (old_value == mountTrack)
	{
		status = setTrack (new_value->getValueInteger ());
		if (status != TPL_OK)
			return -2;
		return 0;
	}
	if (old_value == domeAutotrack)
	{
		status =
			setTrack (mountTrack->getValueInteger (),
			((Rts2ValueBool *) new_value)->getValueBool ());
		if (status != TPL_OK)
			return -2;
		return 0;
	}
	if (old_value == domeUp)
	{
		status =
			irConn->tpl_set ("DOME[1].TARGETPOS", new_value->getValueFloat (), &status);
		if (status != TPL_OK)
			return -2;
		return 0;
	}
	if (old_value == domeDown)
	{
		status =
			irConn->tpl_set ("DOME[2].TARGETPOS", new_value->getValueFloat (), &status);
		if (status != TPL_OK)
			return -2;
		return 0;
	}
	if (old_value == domeTargetAz)
	{
		status =
			irConn->tpl_set ("DOME[0].TARGETPOS",
			ln_range_degrees (new_value->getValueDouble () - 180.0),
			&status);
		if (status != TPL_OK)
			return -2;
		return 0;

	}
	if (old_value == domePower)
	{
		status =
			irConn->tpl_set ("DOME[0].POWER",
			((Rts2ValueBool *) new_value)->getValueBool ()? 1 : 0,
			&status);
		if (status != TPL_OK)
			return -2;
		return 0;

	}
	if (old_value == model_aoff)
	{
		status =
			irConn->tpl_set ("POINTING.POINTINGPARAMS.AOFF", new_value->getValueDouble (),
			&status);
		if (status != TPL_OK)
			return -2;
		return 0;
	}
	if (old_value == model_zoff)
	{
		status =
			irConn->tpl_set ("POINTING.POINTINGPARAMS.ZOFF", new_value->getValueDouble (),
			&status);
		if (status != TPL_OK)
			return -2;
		return 0;
	}
	if (old_value == model_ae)
	{
		status =
			irConn->tpl_set ("POINTING.POINTINGPARAMS.AE", new_value->getValueDouble (),
			&status);
		if (status != TPL_OK)
			return -2;
		return 0;
	}
	if (old_value == model_an)
	{
		status =
			irConn->tpl_set ("POINTING.POINTINGPARAMS.AN", new_value->getValueDouble (),
			&status);
		if (status != TPL_OK)
			return -2;
		return 0;
	}
	if (old_value == model_npae)
	{
		status =
			irConn->tpl_set ("POINTING.POINTINGPARAMS.NPAE", new_value->getValueDouble (),
			&status);
		if (status != TPL_OK)
			return -2;
		return 0;
	}
	if (old_value == model_ca)
	{
		status =
			irConn->tpl_set ("POINTING.POINTINGPARAMS.CA", new_value->getValueDouble (),
			&status);
		if (status != TPL_OK)
			return -2;
		return 0;
	}
	if (old_value == model_flex)
	{
		status =
			irConn->tpl_set ("POINTING.POINTINGPARAMS.FLEX", new_value->getValueDouble (),
			&status);
		if (status != TPL_OK)
			return -2;
		return 0;
	}
	if (old_value == model_aoff)
	{
		status =
			irConn->tpl_set ("POINTING.POINTINGPARAMS.AOFF", new_value->getValueDouble (),
			&status);
		if (status != TPL_OK)
			return -2;
		return 0;
	}
	if (old_value == model_zoff)
	{
		status =
			irConn->tpl_set ("POINTING.POINTINGPARAMS.ZOFF", new_value->getValueDouble (),
			&status);
		if (status != TPL_OK)
			return -2;
		return 0;
	}
	if (old_value == model_ae)
	{
		status =
			irConn->tpl_set ("POINTING.POINTINGPARAMS.AE", new_value->getValueDouble (),
			&status);
		if (status != TPL_OK)
			return -2;
		return 0;
	}
	if (old_value == model_an)
	{
		status =
			irConn->tpl_set ("POINTING.POINTINGPARAMS.AN", new_value->getValueDouble (),
			&status);
		if (status != TPL_OK)
			return -2;
		return 0;
	}
	if (old_value == model_npae)
	{
		status =
			irConn->tpl_set ("POINTING.POINTINGPARAMS.NPAE", new_value->getValueDouble (),
			&status);
		if (status != TPL_OK)
			return -2;
		return 0;
	}
	if (old_value == model_ca)
	{
		status =
			irConn->tpl_set ("POINTING.POINTINGPARAMS.CA", new_value->getValueDouble (),
			&status);
		if (status != TPL_OK)
			return -2;
		return 0;
	}
	if (old_value == model_flex)
	{
		status =
			irConn->tpl_set ("POINTING.POINTINGPARAMS.FLEX", new_value->getValueDouble (),
			&status);
		if (status != TPL_OK)
			return -2;
		return 0;
	}

	return Rts2DevTelescope::setValue (old_value, new_value);
}


Rts2TelescopeIr::Rts2TelescopeIr (int in_argc, char **in_argv):Rts2DevTelescope (in_argc,
in_argv)
{
	ir_port = 0;
	irConn = NULL;

	doCheckPower = false;

	createValue (cabinetPower, "cabinet_power", "power of cabinet", false);
	createValue (cabinetPowerState, "cabinet_power_state",
		"power state of cabinet", false);

	createValue (derotatorOffset, "DER_OFF", "derotator offset", true,
		RTS2_DT_ROTANG, 0, true);
	createValue (derotatorCurrpos, "DER_CUR", "derotator current position",
		true, RTS2_DT_DEGREES);

	createValue (targetDist, "target_dist", "distance in degrees to target",
		false, RTS2_DT_DEG_DIST);
	createValue (targetTime, "target_time", "reach target time in seconds",
		false);

	createValue (derotatorPower, "derotatorPower", "derotator power setting",
		false);

	createValue (mountTrack, "TRACK", "mount track");
	createValue (domeAutotrack, "dome_auto_track", "dome auto tracking", false);
	domeAutotrack->setValueBool (true);

	createValue (domeUp, "dome_up", "upper dome cover", false);
	createValue (domeDown, "dome_down", "dome down cover", false);

	createValue (domeCurrAz, "dome_curr_az", "dome current azimunt", false,
		RTS2_DT_DEGREES);
	createValue (domeTargetAz, "dome_target_az", "dome targer azimut", false,
		RTS2_DT_DEGREES);
	createValue (domePower, "dome_power", "if dome have power", false);
	createValue (domeTarDist, "dome_tar_dist", "dome target distance", false,
		RTS2_DT_DEG_DIST);

	createValue (cover, "cover", "cover state (1 = opened)", false);

	createValue (model_dumpFile, "dump_file", "model dump file", false);
	createValue (model_aoff, "aoff", "model azimuth offset", false,
		RTS2_DT_DEG_DIST);
	createValue (model_zoff, "zoff", "model zenith offset", false,
		RTS2_DT_DEG_DIST);
	createValue (model_ae, "ae", "azimuth equator? offset", false,
		RTS2_DT_DEG_DIST);
	createValue (model_an, "an", "azimuth nadir? offset", false,
		RTS2_DT_DEG_DIST);
	createValue (model_npae, "npae", "not polar adjusted equator?", false,
		RTS2_DT_DEG_DIST);
	createValue (model_ca, "ca", "model ca parameter", false, RTS2_DT_DEG_DIST);
	createValue (model_flex, "flex", "model flex parameter", false,
		RTS2_DT_DEG_DIST);

	addOption ('I', "ir_ip", 1, "IR TCP/IP address");
	addOption ('N', "ir_port", 1, "IR TCP/IP port number");
	addOption ('p', "check power", 0,
		"whenever to check for power state != 0 (currently depreciated)");

	strcpy (telType, "BOOTES_IR");

	cover_state = CLOSED;
	dome_state = D_OPENING;
}


Rts2TelescopeIr::~Rts2TelescopeIr (void)
{
	delete irConn;
}


int
Rts2TelescopeIr::processOption (int in_opt)
{
	switch (in_opt)
	{
		case 'I':
			ir_ip = std::string (optarg);
			break;
		case 'N':
			ir_port = atoi (optarg);
			break;
		case 'p':
			doCheckPower = true;
			break;
		default:
			return Rts2DevTelescope::processOption (in_opt);
	}
	return 0;
}


int
Rts2TelescopeIr::initIrDevice ()
{
	Rts2Config *config = Rts2Config::instance ();
	config->loadFile (NULL);
	// try to get default from config file
	if (ir_ip.length () == 0)
	{
		config->getString ("ir", "ip", ir_ip);
	}
	if (!ir_port)
	{
		config->getInteger ("ir", "port", ir_port);
	}
	if (ir_ip.length () == 0 || !ir_port)
	{
		std::cerr << "Invalid port or IP address of mount controller PC"
			<< std::endl;
		return -1;
	}

	irConn = new IrConn (ir_ip, ir_port);

	// are we connected ?
	if (!irConn->isOK ())
	{
		std::cerr << "Connection to server failed" << std::endl;
		return -1;
	}

	// infoModel
	int ret = infoModel ();
	if (ret)
		return ret;

	return 0;
}


int
Rts2TelescopeIr::init ()
{
	int ret;
	ret = Rts2DevTelescope::init ();
	if (ret)
		return ret;

	ret = initIrDevice ();
	if (ret)
		return ret;

	initCoverState ();

	return 0;
}


int
Rts2TelescopeIr::initValues ()
{
	int status = TPL_OK;
	std::string serial;

	telLongitude->setValueDouble (LONGITUDE);
	telLatitude->setValueDouble (LATITUDE);
	telAltitude->setValueDouble (ALTITUDE);
	irConn->tpl_get ("CABINET.SETUP.HW_ID", serial, &status);
	addConstValue ("IR_HWID", "serial number", (char *) serial.c_str ());

	return Rts2DevTelescope::initValues ();
}


void
Rts2TelescopeIr::checkErrors ()
{
	int status = TPL_OK;
	std::string list;

	status = irConn->tpl_get ("CABINET.STATUS.LIST", list, &status);
	if (status == 0 && list.length () > 2 && list != errorList)
	{
		// print errors to log & ends..
		logStream (MESSAGE_ERROR) << "IR checkErrors Telescope errors " <<
			list << sendLog;
		errorList = list;
		irConn->tpl_set ("CABINET.STATUS.CLEAR", 1, &status);
		if (list == "\"ERR_Soft_Limit_max:4:ZD\"")
		{
			double zd;
			status = irConn->tpl_get ("ZD.CURRPOS", zd, &status);
			if (!status)
			{
				if (zd < -80)
					zd = -85;
				if (zd > 80)
					zd = 85;
				status = setTrack (0);
				logStream (MESSAGE_DEBUG) <<
					"IR checkErrors set pointing status " << status << sendLog;
				sleep (1);
				status = TPL_OK;
				status = irConn->tpl_set ("ZD.TARGETPOS", zd, &status);
				logStream (MESSAGE_ERROR) <<
					"IR checkErrors zd soft limit reset " <<
					zd << " (" << status << ")" << sendLog;
			}
		}
	}
}


void
Rts2TelescopeIr::checkCover ()
{
	int status = TPL_OK;
	switch (cover_state)
	{
		case OPENING:
			getCover ();
			if (cover->getValueDouble () == 1.0)
			{
				irConn->tpl_set ("COVER.POWER", 0, &status);
			#ifdef DEBUG_EXTRA
				logStream (MESSAGE_DEBUG) << "IR checkCover opened " << status <<
					sendLog;
			#endif
				cover_state = OPENED;
				break;
			}
			setTimeout (USEC_SEC);
			break;
		case CLOSING:
			getCover ();
			if (cover->getValueDouble () == 0.0)
			{
				irConn->tpl_set ("COVER.POWER", 0, &status);
			#ifdef DEBUG_EXTRA
				logStream (MESSAGE_DEBUG) << "IR checkCover closed " << status <<
					sendLog;
			#endif
				cover_state = CLOSED;
				break;
			}
			setTimeout (USEC_SEC);
			break;
		case OPENED:
		case CLOSED:
			break;
	}
}


void
Rts2TelescopeIr::checkPower ()
{
	int status = TPL_OK;
	double power_state;
	double referenced;
	status = irConn->tpl_get ("CABINET.POWER_STATE", power_state, &status);
	if (status)
	{
		logStream (MESSAGE_ERROR) << "IR checkPower tpl_ret " << status <<
			sendLog;
		return;
	}

	if (!doCheckPower)
		return;

	if (power_state == 0)
	{
		status = irConn->tpl_setw ("CABINET.POWER", 1, &status);
		status = irConn->tpl_get ("CABINET.POWER_STATE", power_state, &status);
		if (status)
		{
			logStream (MESSAGE_ERROR) << "IR checkPower set power ot 1 ret " <<
				status << sendLog;
			return;
		}
		while (power_state == 0.5)
		{
			logStream (MESSAGE_DEBUG) << "IR checkPower waiting for power up" <<
				sendLog;
			sleep (5);
			status = irConn->tpl_get ("CABINET.POWER_STATE", power_state, &status);
			if (status)
			{
				logStream (MESSAGE_ERROR) << "IR checkPower power_state ret " <<
					status << sendLog;
				return;
			}
		}
	}
	while (true)
	{
		status = irConn->tpl_get ("CABINET.REFERENCED", referenced, &status);
		if (status)
		{
			logStream (MESSAGE_ERROR) << "IR checkPower get referenced " <<
				status << sendLog;
			return;
		}
		if (referenced == 1)
			break;
		logStream (MESSAGE_DEBUG) << "IR checkPower referenced " << referenced
			<< sendLog;
		if (referenced == 0)
		{
			status = irConn->tpl_set ("CABINET.REINIT", 1, &status);
			if (status)
			{
				logStream (MESSAGE_ERROR) << "IR checkPower reinit " <<
					status << sendLog;
				return;
			}
		}
		sleep (1);
	}
	// force close of cover..
	initCoverState ();
	switch (getMasterState ())
	{
		case SERVERD_DUSK:
		case SERVERD_NIGHT:
		case SERVERD_DAWN:
			coverOpen ();
			break;
		default:
			coverClose ();
			break;
	}
}


void
Rts2TelescopeIr::getCover ()
{
	double cor_tmp;
	int status = TPL_OK;
	irConn->tpl_get ("COVER.REALPOS", cor_tmp, &status);
	if (status)
		return;
	cover->setValueDouble (cor_tmp);
}


void
Rts2TelescopeIr::getDome ()
{
	if (dome_state != D_CLOSING && dome_state != D_OPENING)
		return;
	int status = TPL_OK;
	double dome_up, dome_down;
	status = irConn->tpl_get ("DOME[1].CURRPOS", dome_up, &status);
	status = irConn->tpl_get ("DOME[2].CURRPOS", dome_down, &status);
	if (status != TPL_OK)
	{
		logStream (MESSAGE_ERROR) << "unknow dome state" << sendLog;
		return;
	}
	domeUp->setValueFloat (dome_up);
	domeDown->setValueFloat (dome_down);
	if (dome_up == 1.0 && dome_down == 1.0)
		dome_state = D_OPENED;
	if (dome_up == 0.0 && dome_down == 0.0)
		dome_state = D_CLOSED;
}


void
Rts2TelescopeIr::initCoverState ()
{
	getCover ();
	if (cover->getValueDouble () == 0)
		cover_state = CLOSED;
	else if (cover->getValueDouble () == 1)
		cover_state = OPENED;
	else
		cover_state = CLOSING;
}


int
Rts2TelescopeIr::infoModel ()
{
	int status = TPL_OK;

	std::string dumpfile;
	double aoff, zoff, ae, an, npae, ca, flex;

	status = irConn->tpl_get ("POINTING.POINTINGPARAMS.DUMPFILE", dumpfile, &status);
	status = irConn->tpl_get ("POINTING.POINTINGPARAMS.AOFF", aoff, &status);
	status = irConn->tpl_get ("POINTING.POINTINGPARAMS.ZOFF", zoff, &status);
	status = irConn->tpl_get ("POINTING.POINTINGPARAMS.AE", ae, &status);
	status = irConn->tpl_get ("POINTING.POINTINGPARAMS.AN", an, &status);
	status = irConn->tpl_get ("POINTING.POINTINGPARAMS.NPAE", npae, &status);
	status = irConn->tpl_get ("POINTING.POINTINGPARAMS.CA", ca, &status);
	status = irConn->tpl_get ("POINTING.POINTINGPARAMS.FLEX", flex, &status);
	if (status != TPL_OK)
		return -1;

	//  model_dumpFile->setValueString (dumpfile);
	model_aoff->setValueDouble (aoff);
	model_zoff->setValueDouble (zoff);
	model_ae->setValueDouble (ae);
	model_an->setValueDouble (an);
	model_npae->setValueDouble (npae);
	model_ca->setValueDouble (ca);
	model_flex->setValueDouble (flex);
	return 0;
}


int
Rts2TelescopeIr::idle ()
{
	// check for power..
	checkPower ();
	// check for errors..
	checkErrors ();
	checkCover ();
	return Rts2DevTelescope::idle ();
}


int
Rts2TelescopeIr::ready ()
{
	return !irConn->isOK ();
}


int
Rts2TelescopeIr::getAltAz ()
{
	int status = TPL_OK;
	double zd, az;

	status = irConn->tpl_get ("ZD.REALPOS", zd, &status);
	status = irConn->tpl_get ("AZ.REALPOS", az, &status);

	if (status != TPL_OK)
		return -1;

	telAlt->setValueDouble (90 - fabs (zd));
	telAz->setValueDouble (ln_range_degrees (az + 180));

	return 0;
}


int
Rts2TelescopeIr::info ()
{
	double zd, az;
	#ifdef DEBUG_EXTRA
	double zd_speed, az_speed;
	#endif						 // DEBUG_EXTRA
	double t_telRa, t_telDec;
	int track = 0;
	int derPower = 0;
	int status = TPL_OK;

	int cab_power;
	double cab_power_state;

	if (!irConn->isOK ())
		return -1;

	status = irConn->tpl_get ("CABINET.POWER", cab_power, &status);
	status = irConn->tpl_get ("CABINET.POWER_STATE", cab_power_state, &status);
	if (status != TPL_OK)
		return -1;
	cabinetPower->setValueBool (cab_power == 1);
	cabinetPowerState->setValueFloat (cab_power_state);

	status = irConn->tpl_get ("POINTING.CURRENT.RA", t_telRa, &status);
	t_telRa *= 15.0;
	status = irConn->tpl_get ("POINTING.CURRENT.DEC", t_telDec, &status);
	status = irConn->tpl_get ("POINTING.TRACK", track, &status);
	if (status != TPL_OK)
		return -1;

	setTelRaDec (t_telRa, t_telDec);

	#ifdef DEBUG_EXTRA
	status = irConn->tpl_get ("ZD.CURRSPEED", zd_speed, &status);
	status = irConn->tpl_get ("AZ.CURRSPEED", az_speed, &status);

	logStream (MESSAGE_DEBUG) << "IR info ra " << getTelRa ()
		<< " dec " << getTelDec ()
		<< " az_speed " << az_speed
		<< " zd_speed " << zd_speed
		<< " track " << track
		<< sendLog;
	#endif						 // DEBUG_EXTRA

	if (!track)
	{
		// telRA, telDec are invalid: compute them from ZD/AZ
		struct ln_hrz_posn hrz;
		struct ln_lnlat_posn observer;
		struct ln_equ_posn curr;
		hrz.az = az;
		hrz.alt = 90 - fabs (zd);
		observer.lng = telLongitude->getValueDouble ();
		observer.lat = telLatitude->getValueDouble ();
		ln_get_equ_from_hrz (&hrz, &observer, ln_get_julian_from_sys (), &curr);
		setTelRa (curr.ra);
		setTelDec (curr.dec);
		telFlip->setValueInteger (0);
	}
	else
	{
		telFlip->setValueInteger (0);
	}

	if (status)
		return -1;

	double tmp_derOff;
	double tmp_derCur;
	irConn->tpl_get ("DEROTATOR[3].OFFSET", tmp_derOff, &status);
	irConn->tpl_get ("DEROTATOR[3].CURRPOS", tmp_derCur, &status);
	irConn->tpl_get ("DEROTATOR[3].POWER", derPower, &status);
	if (status == TPL_OK)
	{
		derotatorOffset->setValueDouble (-1 * tmp_derOff);
		derotatorCurrpos->setValueDouble (tmp_derCur);
		derotatorPower->setValueBool (derPower == 1);
	}

	getCover ();
	getDome ();

	mountTrack->setValueInteger (track);

	status = TPL_OK;
	double point_dist;
	double point_time;
	status = irConn->tpl_get ("POINTING.TARGETDISTANCE", point_dist, &status);
	status = irConn->tpl_get ("POINTING.SLEWINGTIME", point_time, &status);
	if (status == TPL_OK)
	{
		targetDist->setValueDouble (point_dist);
		targetTime->setValueDouble (point_time);
	}

	double dome_curr_az, dome_target_az, dome_tar_dist, dome_power;
	status = TPL_OK;
	status = irConn->tpl_get ("DOME[0].CURRPOS", dome_curr_az, &status);
	status = irConn->tpl_get ("DOME[0].TARGETPOS", dome_target_az, &status);
	status = irConn->tpl_get ("DOME[0].TARGETDISTANCE", dome_tar_dist, &status);
	status = irConn->tpl_get ("DOME[0].POWER", dome_power, &status);
	if (status == TPL_OK)
	{
		domeCurrAz->setValueDouble (ln_range_degrees (dome_curr_az + 180));
		domeTargetAz->setValueDouble (ln_range_degrees (dome_target_az + 180));
		domeTarDist->setValueDouble (dome_tar_dist);
		domePower->setValueBool (dome_power == 1);
	}
	return Rts2DevTelescope::info ();
}


/*
int
Rts2TelescopeIr::correct (double cor_ra, double cor_dec, double real_ra,
double real_dec)
{
	// idea - convert current & astrometry position to alt & az, get
	// offset in alt & az, apply offset
	struct ln_equ_posn eq_astr;
	struct ln_equ_posn eq_target;
	struct ln_hrz_posn hrz_astr;
	struct ln_hrz_posn hrz_target;
	struct ln_lnlat_posn observer;
	double az_off = 0;
	double alt_off = 0;
	double sep;
	double jd = ln_get_julian_from_sys ();
	double quality;
	double zd;
	int sample = 1;
	int status = TPL_OK;

	eq_astr.ra = real_ra;
	eq_astr.dec = real_dec;
	eq_target.ra = real_ra + cor_ra;
	eq_target.dec = real_dec + cor_dec;

	applyLocCorr (&eq_target);

	observer.lng = telLongitude->getValueDouble ();
	observer.lat = telLatitude->getValueDouble ();
	ln_get_hrz_from_equ (&eq_astr, &observer, jd, &hrz_astr);
	ln_get_hrz_from_equ (&eq_target, &observer, jd, &hrz_target);
	// calculate alt & az diff
	az_off = hrz_target.az - hrz_astr.az;
	alt_off = hrz_target.alt - hrz_astr.alt;

	status = irConn->tpl_get ("ZD.CURRPOS", zd, &status);
	if (status)
	{
		logStream (MESSAGE_ERROR) << "IR correct cannot get ZD.CURRPOS" <<
			sendLog;
		return -1;
	}
	if (zd > 0)
		alt_off *= -1;			 // get ZD offset - when ZD < 0, it's actuall alt offset
	sep = ln_get_angular_separation (&eq_astr, &eq_target);
	#ifdef DEBUG_EXTRA
	logStream (MESSAGE_DEBUG) << "IR correct az_off " << az_off << " zd_off " <<
		alt_off << " sep " << sep << sendLog;
	#endif
	if (sep > 2)
		return -1;
	status = irConn->tpl_set ("AZ.OFFSET", az_off, &status);
	status = irConn->tpl_set ("ZD.OFFSET", alt_off, &status);
	//  if (isModelOn ())
	//    return (status ? -1 : 1);
	// sample..
	status = irConn->tpl_set ("POINTING.POINTINGPARAMS.SAMPLE", sample, &status);
	status = irConn->tpl_get ("POINTING.POINTINGPARAMS.CALCULATE", quality, &status);
	logStream (MESSAGE_DEBUG) << "IR correct quality: " << quality << " status "
		<< status << sendLog;
	return (status ? -1 : 1);
}
*/

/**
 * OpenTCI/Bootes IR - POINTING.POINTINGPARAMS.xx:
 * AOFF, ZOFF, AE, AN, NPAE, CA, FLEX
 * AOFF, ZOFF = az / zd offset
 * AE = azimut tilt east
 * AN = az tilt north
 * NPAE = az / zd not perpendicular
 * CA = M1 tilt with respect to optical axis
 * FLEX = sagging of tube
 */
int
Rts2TelescopeIr::saveModel ()
{
	std::ofstream of;
	int status = TPL_OK;
	double aoff, zoff, ae, an, npae, ca, flex;
	status = irConn->tpl_get ("POINTING.POINTINGPARAMS.AOFF", aoff, &status);
	status = irConn->tpl_get ("POINTING.POINTINGPARAMS.ZOFF", zoff, &status);
	status = irConn->tpl_get ("POINTING.POINTINGPARAMS.AE", ae, &status);
	status = irConn->tpl_get ("POINTING.POINTINGPARAMS.AN", an, &status);
	status = irConn->tpl_get ("POINTING.POINTINGPARAMS.NPAE", npae, &status);
	status = irConn->tpl_get ("POINTING.POINTINGPARAMS.CA", ca, &status);
	status = irConn->tpl_get ("POINTING.POINTINGPARAMS.FLEX", flex, &status);
	if (status)
	{
		logStream (MESSAGE_ERROR) << "IR saveModel status: " << status <<
			sendLog;
		return -1;
	}
	of.open ("/etc/rts2/ir.model", std::ios_base::out | std::ios_base::trunc);
	of.precision (20);
	of << aoff << " "
		<< zoff << " "
		<< ae << " "
		<< an << " " << npae << " " << ca << " " << flex << " " << std::endl;
	of.close ();
	if (of.fail ())
	{
		logStream (MESSAGE_ERROR) << "IR saveModel file" << sendLog;
		return -1;
	}
	return 0;
}


int
Rts2TelescopeIr::loadModel ()
{
	std::ifstream ifs;
	int status = TPL_OK;
	double aoff, zoff, ae, an, npae, ca, flex;
	try
	{
		ifs.open ("/etc/rts2/ir.model");
		ifs >> aoff >> zoff >> ae >> an >> npae >> ca >> flex;
	}
	catch (std::exception & e)
	{
		logStream (MESSAGE_DEBUG) << "IR loadModel error" << sendLog;
		return -1;
	}
	status = irConn->tpl_set ("POINTING.POINTINGPARAMS.AOFF", aoff, &status);
	status = irConn->tpl_set ("POINTING.POINTINGPARAMS.ZOFF", zoff, &status);
	status = irConn->tpl_set ("POINTING.POINTINGPARAMS.AE", ae, &status);
	status = irConn->tpl_set ("POINTING.POINTINGPARAMS.AN", an, &status);
	status = irConn->tpl_set ("POINTING.POINTINGPARAMS.NPAE", npae, &status);
	status = irConn->tpl_set ("POINTING.POINTINGPARAMS.CA", ca, &status);
	status = irConn->tpl_set ("POINTING.POINTINGPARAMS.FLEX", flex, &status);
	if (status)
	{
		logStream (MESSAGE_ERROR) << "IR saveModel status: " << status <<
			sendLog;
		return -1;
	}
	return 0;
}


int
Rts2TelescopeIr::resetMount ()
{
	int status = TPL_OK;
	int power = 0;
	double power_state;
	status = irConn->tpl_setw ("CABINET.POWER", power, &status);
	if (status)
	{
		logStream (MESSAGE_ERROR) << "IR resetMount powering off: " << status <<
			sendLog;
		return -1;
	}
	while (true)
	{
		logStream (MESSAGE_DEBUG) << "IR resetMount waiting for power down" <<
			sendLog;
		sleep (5);
		status = irConn->tpl_get ("CABINET.POWER_STATE", power_state, &status);
		if (status)
		{
			logStream (MESSAGE_ERROR) << "IR resetMount power_state ret: " <<
				status << sendLog;
			return -1;
		}
		if (power_state == 0 || power_state == -1)
		{
			logStream (MESSAGE_DEBUG) << "IR resetMount final power_state: " <<
				power_state << sendLog;
			break;
		}
	}
	return Rts2DevTelescope::resetMount ();
}
