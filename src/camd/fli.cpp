/*
 * Driver for FLI CCDs.
 * Copyright (C) 2005-2007 Petr Kubanek <petr@kubanek.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * @file FLI camera driver
 *
 * @author Petr Kubanek <petr@kubanek.net>
 *
 * You will need FLIlib and option to ./configure (--with-fli=<llibflidir>)
 * to get that running. You can get libfli on http://www.moronski.com/fli
 */

#include "camd.h"

#include "libfli.h"

/**
 * Class for control of FLI CCD.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class Rts2DevCameraFli:public Rts2DevCamera
{
	private:
		flidomain_t deviceDomain;

		flidev_t dev;

		long hwRev;
		int camNum;

		int fliDebug;
		int nflush;
	protected:
		virtual int initChips ();

		virtual void initBinnings ()
		{
			Rts2DevCamera::initBinnings ();

			addBinning2D (2, 2);
			addBinning2D (4, 4);
			addBinning2D (8, 8);
			addBinning2D (16, 16);
			addBinning2D (2, 1);
			addBinning2D (4, 1);
			addBinning2D (8, 1);
			addBinning2D (16, 1);
			addBinning2D (1, 2);
			addBinning2D (1, 4);
			addBinning2D (1, 8);
			addBinning2D (1, 16);

		}

		virtual int startExposure ();
		virtual long isExposing ();
		virtual int stopExposure ();
		virtual int readoutOneLine ();
	public:
		Rts2DevCameraFli (int in_argc, char **in_argv);
		virtual ~ Rts2DevCameraFli (void);

		virtual int processOption (int in_opt);

		virtual int init ();

		virtual int ready ();
		virtual int info ();

		virtual int camChipInfo (int chip);

		virtual int camCoolMax ();
		virtual int camCoolHold ();
		virtual int camCoolTemp (float new_temp);
		virtual int camCoolShutdown ();
};

int
Rts2DevCameraFli::initChips ()
{
	LIBFLIAPI ret;
	long x, y, w, h;

	ret = FLIGetVisibleArea (dev, &x, &y, &w, &h);
	if (ret)
		return -1;
	// put true width & height
	w -= x;
	h -= y;
	setSize ((int) w, (int) h, (int) x, (int) y);

	ret = FLIGetPixelSize (dev, &pixelX, &pixelY);
	if (ret)
		return -1;

	return 0;
}


int
Rts2DevCameraFli::startExposure ()
{
	LIBFLIAPI ret;

	ret = FLISetVBin (dev, binningVertical ());
	if (ret)
		return -1;
	ret = FLISetHBin (dev, binningHorizontal ());
	if (ret)
		return -1;

	ret = FLISetImageArea (dev,
		chipTopX(), chipTopY(),
		chipTopX () + getUsedWidthBinned (), chipTopY () + getUsedHeightBinned ());
	if (ret)
		return -1;

	ret = FLISetExposureTime (dev, (long) (getExposure () * 1000));
	if (ret)
		return -1;

	ret = FLISetFrameType (dev, (getExpType () == 0) ? FLI_FRAME_TYPE_NORMAL : FLI_FRAME_TYPE_DARK);
	if (ret)
		return -1;

	ret = FLIExposeFrame (dev);
	if (ret)
		return -1;
	return 0;
}


long
Rts2DevCameraFli::isExposing ()
{
	LIBFLIAPI ret;
	long tl;
	ret = FLIGetExposureStatus (dev, &tl);
	if (ret)
		return -1;

	return tl * 1000;			 // we get tl in msec, needs to return usec
}


int
Rts2DevCameraFli::stopExposure ()
{
	LIBFLIAPI ret;
	long timer;
	ret = FLIGetExposureStatus (dev, &timer);
	if (ret == 0 && timer > 0)
	{
		ret = FLICancelExposure (dev);
		if (ret)
			return ret;
		ret = FLIFlushRow (dev, getHeight (), 1);
		if (ret)
			return ret;
	}
	return Rts2DevCamera::stopExposure ();
}


int
Rts2DevCameraFli::readoutOneLine ()
{
	LIBFLIAPI ret;
	char *bufferTop = dataBuffer;
	for (int line = 0; line < getUsedHeightBinned (); line++, bufferTop += usedPixelByteSize () * getUsedWidthBinned ())
	{
		ret = FLIGrabRow (dev, bufferTop, getUsedWidthBinned ());
		if (ret)
			return -1;
	}
	ret = sendReadoutData (dataBuffer, getWriteBinaryDataSize ());
	if (ret < 0)
	{
		return ret;
	}
	return -2;
}


Rts2DevCameraFli::Rts2DevCameraFli (int in_argc, char **in_argv):
Rts2DevCamera (in_argc, in_argv)
{
	createTempSet ();
	createTempCCD ();
	createExpType ();

	deviceDomain = FLIDEVICE_CAMERA | FLIDOMAIN_USB;
	fliDebug = FLIDEBUG_NONE;
	hwRev = -1;
	camNum = -1;
	nflush = -1;
	addOption ('D', "domain", 1,
		"CCD Domain (default to USB; possible values: USB|LPT|SERIAL|INET)");
	addOption ('R', "HW revision", 1, "find camera by HW revision");
	addOption ('b', "fli_debug", 1,
		"FLI debug level (1, 2 or 3; 3 will print most error message to stdout)");
	addOption ('n', "number", 1, "Camera number (in FLI list)");
	addOption ('l', "flush", 1, "Number of CCD flushes");
}


Rts2DevCameraFli::~Rts2DevCameraFli (void)
{
	FLIClose (dev);
}


int
Rts2DevCameraFli::processOption (int in_opt)
{
	switch (in_opt)
	{
		case 'D':
			deviceDomain = FLIDEVICE_CAMERA;
			if (!strcasecmp ("USB", optarg))
				deviceDomain |= FLIDOMAIN_USB;
			else if (!strcasecmp ("LPT", optarg))
				deviceDomain |= FLIDOMAIN_PARALLEL_PORT;
			else if (!strcasecmp ("SERIAL", optarg))
				deviceDomain |= FLIDOMAIN_SERIAL;
			else if (!strcasecmp ("INET", optarg))
				deviceDomain |= FLIDOMAIN_INET;
			else
				return -1;
			break;
		case 'R':
			hwRev = atol (optarg);
			break;
		case 'b':
			switch (atoi (optarg))
			{
				case 1:
					fliDebug = FLIDEBUG_FAIL;
					break;
				case 2:
					fliDebug = FLIDEBUG_FAIL | FLIDEBUG_WARN;
					break;
				case 3:
					fliDebug = FLIDEBUG_ALL;
					break;
				default:
					return -1;
			}
			break;
		case 'n':
			camNum = atoi (optarg);
			break;
		case 'l':
			nflush = atoi (optarg);
			break;
		default:
			return Rts2DevCamera::processOption (in_opt);
	}
	return 0;
}


int
Rts2DevCameraFli::init ()
{
	LIBFLIAPI ret;

	int ret_c;
	char **names;
	char *nam_sep;
	char **nam;					 // current name

	//long serno = 0;

	ret_c = Rts2DevCamera::init ();
	if (ret_c)
		return ret_c;

	if (fliDebug)
		FLISetDebugLevel (NULL, fliDebug);

	ret = FLIList (deviceDomain, &names);
	if (ret)
		return -1;

	if (names[0] == NULL)
	{
		logStream (MESSAGE_ERROR) << "Rts2DevCameraFli::init No device found!"
			<< sendLog;
		return -1;
	}

	// find device based on hw revision..
	if (hwRev > 0)
	{
		nam = names;
		while (*nam)
		{
			// separate semicolon
			long cam_hwrev;
			nam_sep = strchr (*nam, ';');
			if (nam_sep)
				*nam_sep = '\0';
			ret = FLIOpen (&dev, *nam, deviceDomain);
			if (ret)
			{
				nam++;
				continue;
			}
			ret = FLIGetHWRevision (dev, &cam_hwrev);
			if (!ret && cam_hwrev == hwRev)
			{
				break;
			}
			// skip to next camera
			FLIClose (dev);
			nam++;
		}
	}
	else if (camNum > 0)
	{
		nam = names;
		int i = 1;
		while (*nam && i != camNum)
		{
			nam++;
			i++;
		}
		if (!*nam)
			return -1;
		i--;
		nam_sep = strchr (names[i], ';');
		*nam_sep = '\0';
		ret = FLIOpen (&dev, names[i], deviceDomain);
	}
	else
	{
		nam_sep = strchr (names[0], ';');
		if (nam_sep)
			*nam_sep = '\0';
		ret = FLIOpen (&dev, names[0], deviceDomain);
	}

	FLIFreeList (names);

	if (ret)
		return -1;

	if (nflush >= 0)
	{
		ret = FLISetNFlushes (dev, nflush);
		if (ret)
		{
			logStream (MESSAGE_ERROR) << "fli init FLISetNFlushes ret " << ret
				<< sendLog;
			return -1;
		}
		logStream (MESSAGE_DEBUG) << "fli init set Nflush to " << nflush <<
			sendLog;
	}

	// FLIGetSerialNum (dev, &serno);

	// snprintf (serialNumber, 64, "%li", serno);

	long hwrev;
	long fwrev;
	ret = FLIGetModel (dev, ccdType, 64);
	if (ret)
		return -1;
	ret = FLIGetHWRevision (dev, &hwrev);
	if (ret)
		return -1;
	ret = FLIGetFWRevision (dev, &fwrev);
	if (ret)
		return -1;
	sprintf (ccdType, "FLI %li.%li", hwrev, fwrev);

	return initChips ();
}


int
Rts2DevCameraFli::ready ()
{
	long fwrev;
	LIBFLIAPI ret;
	ret = FLIGetFWRevision (dev, &fwrev);
	if (ret)
		return -1;
	return 0;
}


int
Rts2DevCameraFli::info ()
{
	LIBFLIAPI ret;
	double fliTemp;
	ret = FLIGetTemperature (dev, &fliTemp);
	if (ret)
		return -1;
	tempCCD->setValueDouble (fliTemp);
	return Rts2DevCamera::info ();
}


int
Rts2DevCameraFli::camChipInfo (int chip)
{
	return 0;
}


int
Rts2DevCameraFli::camCoolMax ()
{
	return camCoolTemp (nightCoolTemp);
}


int
Rts2DevCameraFli::camCoolHold ()
{
	return camCoolTemp (nightCoolTemp);
}


int
Rts2DevCameraFli::camCoolTemp (float new_temp)
{
	LIBFLIAPI ret;
	ret = FLISetTemperature (dev, new_temp);
	if (ret)
		return -1;
	tempSet->setValueDouble (new_temp);
	return 0;
}


int
Rts2DevCameraFli::camCoolShutdown ()
{
	return camCoolTemp (40);
}


int
main (int argc, char **argv)
{
	Rts2DevCameraFli device = Rts2DevCameraFli (argc, argv);
	return device.run ();
}
