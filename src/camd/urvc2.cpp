/* 
 * Alternative SBIG PP CCD driver.
 * Copyright (C) 2001-2005 Martin Jelinek <mates@iaa.es>
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

#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>
#include <string.h>
#include <sys/io.h>

#include "urvc2/urvc.h"
#include "camd.h"

								 // in case geteeprom fails
#define DEFAULT_CAMERA  ST8_CAMERA

/**
 * Alternative driver for SBIG camera.
 *
 * RTS2 urvc2 interface.
 */
class Rts2DevCameraUrvc2:public Rts2DevCamera
{
	private:
		EEPROMContents eePtr;	 // global to prevent multiple EEPROM calls
		CAMERA_TYPE cameraID;
		CAMERA *C;

		void get_eeprom ();
		void init_shutter ();
		int set_fan (int fan_state);
		int setcool (int reg, int setpt, int prel, int fan, int state);

		int ad_temp;

	protected:
		virtual int initChips ();
		virtual int startExposure ();
		virtual long isExposing ();
		virtual int stopExposure ();
		virtual int readoutOneLine ();
	public:
		Rts2DevCameraUrvc2 (int argc, char **argv);
		virtual ~Rts2DevCameraUrvc2 (void);

		virtual int init ();
		virtual int ready ();
		virtual int info ();
		virtual int camChipInfo (int chip)
		{
			return 0;
		}

		virtual int camCoolMax ();
		virtual int camCoolHold ();
		int setCoolTemp ()
		{
			return setcool (1, ad_temp, 0xaf, FAN_ON, CAMERA_COOL_HOLD);
		}
		virtual int setCoolTemp (float coolpoint);
		virtual int camCoolShutdown ();
		CAMERA_TYPE getCameraID (void)
		{
			return cameraID;
		}
};

/*!
 * Filter class for URVC2 based camera.
 * It needs it as there is now filter is an extra object.
 */

class Rts2FilterUrvc2:public Rts2Filter
{
	Rts2DevCameraUrvc2 *camera;
	int filter;
	public:
		Rts2FilterUrvc2 (Rts2DevCameraUrvc2 * in_camera);
		virtual ~ Rts2FilterUrvc2 (void);
		virtual int init (void);
		virtual int getFilterNum (void);
		virtual int setFilterNum (int new_filter);
};

Rts2FilterUrvc2::Rts2FilterUrvc2 (Rts2DevCameraUrvc2 * in_camera):Rts2Filter ()
{
	camera = in_camera;
}


Rts2FilterUrvc2::~Rts2FilterUrvc2 (void)
{
	setFilterNum (0);
}


int
Rts2FilterUrvc2::init ()
{
	setFilterNum (0);
	return 0;
}


int
Rts2FilterUrvc2::getFilterNum (void)
{
	return filter;
}


int
Rts2FilterUrvc2::setFilterNum (int new_filter)
{
	PulseOutParams pop;

	if (new_filter < 0 || new_filter > 4)
	{
		return -1;
	}

	pop.pulsePeriod = 18270;
	pop.pulseWidth = 500 + 300 * new_filter;
	pop.numberPulses = 60;

	if (MicroCommand (MC_PULSE, camera->getCameraID (), &pop, NULL))
		return -1;
	filter = new_filter;

	return 0;
}


void
Rts2DevCameraUrvc2::get_eeprom ()
{
	if (tempRegulation->getValueInteger () == -1)
	{
		if (GetEEPROM (cameraID, &eePtr) != CE_NO_ERROR)
		{
			eePtr.version = 0;
								 // ST8 camera
			eePtr.model = DEFAULT_CAMERA;
			eePtr.abgType = 0;
			eePtr.badColumns = 0;
			eePtr.trackingOffset = 0;
			eePtr.trackingGain = 304;
			eePtr.imagingOffset = 0;
			eePtr.imagingGain = 560;
			bzero (eePtr.columns, 4);
			strcpy ((char *) eePtr.serialNumber, "EEEE");
		}
		else
		{
			// check serial number
			char *b = (char *) eePtr.serialNumber;
			for (; *b; b++)
				if (!isalnum (*b))
			{
				*b = '0';
				break;
			}
		}
	}
}


void
Rts2DevCameraUrvc2::init_shutter ()
{
	MiscellaneousControlParams ctrl;
	StatusResults sr;

	if (cameraID == ST237_CAMERA || cameraID == ST237A_CAMERA)
		return;

	if (MicroCommand (MC_STATUS, cameraID, NULL, &sr) == CE_NO_ERROR)
	{
		ctrl.fanEnable = sr.fanEnabled;
		ctrl.shutterCommand = 3;
		ctrl.ledState = sr.ledStatus;
		MicroCommand (MC_MISC_CONTROL, cameraID, &ctrl, NULL);
		sleep (2);
	}
}


int
Rts2DevCameraUrvc2::set_fan (int fan_state)
{
	MiscellaneousControlParams ctrl;
	StatusResults sr;

	ctrl.fanEnable = fan_state;
	ctrl.shutterCommand = 0;

	if ((MicroCommand (MC_STATUS, cameraID, NULL, &sr)))
		return -1;
	ctrl.ledState = sr.ledStatus;
	if (MicroCommand (MC_MISC_CONTROL, cameraID, &ctrl, NULL))
		return -1;

	return 0;
}


int
Rts2DevCameraUrvc2::setcool (int reg, int setpt, int prel, int in_fan,
int coolstate)
{
	MicroTemperatureRegulationParams cool;

	cool.regulation = reg;
	cool.ccdSetpoint = setpt;
	cool.preload = prel;

	set_fan (in_fan);

	if (MicroCommand (MC_REGULATE_TEMP, cameraID, &cool, NULL))
		return -1;
	tempRegulation->setValueInteger (coolstate);

	return 0;
}


int
Rts2DevCameraUrvc2::initChips ()
{
	OpenCCD (0, &C);
	return Rts2DevCamera::initChips ();
}


int
Rts2DevCameraUrvc2::startExposure ()
{
	#ifdef INIT_SHUTTER
	init_shutter ();
	#else
	if (getExpType () == 1)		 // init shutter only for dark images
		init_shutter ();
	#endif
	return (CCDExpose (C, (int) (100 * (getExposure ()) + 0.5), getExpType () ? 0 : 1));
}


long
Rts2DevCameraUrvc2::isExposing ()
{
	int ret;
	ret = Rts2DevCamera::isExposing ();
	if (ret > 0)
		return ret;
	int imstate;
	imstate = CCDImagingState (C);
	if (imstate == -1)
		return -1;
	if (imstate)
	{
		return 100;				 // recheck in 100 msec
	}
	return -2;
}


int
Rts2DevCameraUrvc2::readoutOneLine ()
{
	if (CCDReadout ((short unsigned int *) dataBuffer, C, chipUsedReadout->getXInt () / binningVertical (),
		chipUsedReadout->getYInt () / binningHorizontal (),
		getUsedWidthBinned (), getUsedHeightBinned (), binningVertical ()))
	{
		logStream (MESSAGE_DEBUG)
			<< "urvc2 chip readoutOneLine readout return not-null" << sendLog;
		return -1;
	}
	int ret;
	ret = sendReadoutData (dataBuffer, getWriteBinaryDataSize ());
	if (ret < 0)
		return -1;
	return -2;
}


Rts2DevCameraUrvc2::Rts2DevCameraUrvc2 (int in_argc, char **in_argv):
Rts2DevCamera (in_argc, in_argv)
{
	createTempAir ();
	createTempCCD ();
	createTempSet ();
	createTempRegulation ();
	createCoolingPower ();
	createCamFan ();

	createExpType ();

	cameraID = DEFAULT_CAMERA;
	tempRegulation->setValueInteger (-1);
}


Rts2DevCameraUrvc2::~Rts2DevCameraUrvc2 (void)
{
	CloseCCD (C);
}


int
Rts2DevCameraUrvc2::init ()
{
	short base;
	int i;
	//StatusResults sr;
	QueryTemperatureStatusResults qtsr;
	GetVersionResults gvr;
	int ret;

	ret = Rts2DevCamera::init ();
	if (ret)
		return ret;

	base = getbaseaddr (0);		 // sbig_port...?

	// This is a standard driver init procedure
	begin_realtime ();
	CameraInit (base);

	logStream (MESSAGE_DEBUG) << "urvc2 init" << sendLog;

	if ((i = MicroCommand (MC_GET_VERSION, ST7_CAMERA, NULL, &gvr)))
	{
		logStream (MESSAGE_DEBUG) << "urvc2 GET_VERSION ret " << i << sendLog;
		return -1;
	}

	cameraID = (CAMERA_TYPE) gvr.cameraID;

	if ((i = MicroCommand (MC_TEMP_STATUS, cameraID, NULL, &qtsr)))
	{
		logStream (MESSAGE_DEBUG) << "urvc2 TEMP_STATUS ret " << i << sendLog;
		return -1;
	}

	logStream (MESSAGE_DEBUG) << "urvc2 get eeprom" << sendLog;

	get_eeprom ();

	setSize (Cams[eePtr.model].horzImage, Cams[eePtr.model].vertImage, 0, 0);
	pixelX = Cams[eePtr.model].pixelX;
	pixelY = Cams[eePtr.model].pixelY;

	// determine temperature regulation state
	switch (qtsr.enabled)
	{
		case 0:
			// a bit strange: may have different
			// meanings... (freeze)
			tempRegulation->setValueInteger ((qtsr.power > 250) ? CAMERA_COOL_MAX : CAMERA_COOL_OFF);
			break;
		case 1:
			tempRegulation->setValueInteger (CAMERA_COOL_HOLD);
			break;
		default:
			tempRegulation->setValueInteger (CAMERA_COOL_OFF);
	}

	logStream (MESSAGE_DEBUG) << "urvc2 init return " << Cams[eePtr.model].
		horzImage << sendLog;

	filter = new Rts2FilterUrvc2 (this);

	strcpy (ccdType, (char *) Cams[eePtr.model].fullName);
	strcpy (serialNumber, (char *) eePtr.serialNumber);

	return initChips ();
}


int
Rts2DevCameraUrvc2::ready ()
{
	StatusResults gvr;
	if (MicroCommand (MC_STATUS, cameraID, NULL, &gvr))
		return -1;
	return 0;
}


int
Rts2DevCameraUrvc2::info ()
{
	StatusResults gvr;
	QueryTemperatureStatusResults qtsr;

	if (MicroCommand (MC_STATUS, cameraID, NULL, &gvr))
		return -1;
	if (MicroCommand (MC_TEMP_STATUS, cameraID, NULL, &qtsr))
		return -1;

	tempSet->setValueDouble (ccd_ad2c (qtsr.ccdSetpoint));
	coolingPower->
		setValueInteger ((int) ((qtsr.power == 255) ? 1000 : qtsr.power * 3.906));
	tempAir->setValueDouble (ambient_ad2c (qtsr.ambientThermistor));
	tempCCD->setValueDouble (ccd_ad2c (qtsr.ccdThermistor));
	fan->setValueInteger (gvr.fanEnabled);
	return Rts2DevCamera::info ();
}


int
Rts2DevCameraUrvc2::stopExposure ()
{
	EndExposureParams eep;
	eep.ccd = 0;

	if (MicroCommand (MC_END_EXPOSURE, cameraID, &eep, NULL))
		return -1;

	return 0;
};

int
Rts2DevCameraUrvc2::camCoolMax ()/* try to max temperature */
{
	return setcool (2, 255, 0, FAN_ON, CAMERA_COOL_MAX);
}


int
								 /* hold on that temperature */
Rts2DevCameraUrvc2::camCoolHold ()
{
	QueryTemperatureStatusResults qtsr;
	float ot;					 // optimal temperature

	if (tempRegulation->getValueInteger () == CAMERA_COOL_HOLD)
		return 0;				 // already cooled

	if (isnan (nightCoolTemp))
	{
		if (MicroCommand (MC_TEMP_STATUS, cameraID, NULL, &qtsr))
			return -1;

		ot = ccd_ad2c (qtsr.ccdThermistor);
		ot = ((int) (ot + 5) / 5) * 5;
	}
	else
	{
		ot = nightCoolTemp;
	}

	set_fan (1);

	return setCoolTemp (ot);
}


int
								 /* set direct setpoint */
Rts2DevCameraUrvc2::setCoolTemp (float coolpoint)
{
								 // zaokrohlovat a neorezavat!
	ad_temp = ccd_c2ad (coolpoint) + 0x7;
	return setCoolTemp ();
}


int
								 /* ramp to ambient */
Rts2DevCameraUrvc2::camCoolShutdown ()
{
	return setcool (0, 0, 0, FAN_OFF, CAMERA_COOL_OFF);
}


int
main (int argc, char **argv)
{
	Rts2DevCameraUrvc2 device = Rts2DevCameraUrvc2 (argc, argv);
	return device.run ();
}
