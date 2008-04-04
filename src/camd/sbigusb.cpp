/* 
 * (USB) Sbig driver.
 * Copyright (C) 2004-2007 Petr Kubanek <petr@kubanek.net>
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camd.h"
#include "../utils/rts2device.h"
#include "../utils/rts2block.h"

#include "sbigudrv.h"
#include "csbigcam.h"

unsigned int
ccd_c2ad (double t)
{
	double r = 3.0 * exp (0.94390589891 * (25.0 - t) / 25.0);
	unsigned int ret = (unsigned int) (4096.0 / (10.0 / r + 1.0));
	if (ret > 4096)
		ret = 4096;
	return ret;
}


double
ccd_ad2c (unsigned int ad)
{
	double r;
	if (ad == 0)
		return 1000;
	r = 10.0 / (4096.0 / (double) ad - 1.0);
	return 25.0 - 25.0 * (log (r / 3.0) / 0.94390589891);
}


double
ambient_ad2c (unsigned int ad)
{
	double r;
	r = 3.0 / (4096.0 / (double) ad - 1.0);
	return 25.0 - 45.0 * (log (r / 3.0) / 2.0529692213);
}


class Rts2DevCameraSbig:public Rts2DevCamera
{
	private:
		CSBIGCam * pcam;
		int checkSbigHw (PAR_ERROR ret)
		{
			if (ret == CE_NO_ERROR)
				return 0;
			logStream (MESSAGE_ERROR) << "Rts2DevCameraSbig::checkSbigHw ret: " << ret
				<< sendLog;
			return -1;
		}
		int fanState (int newFanState);
		int usb_port;
		char *reqSerialNumber;

		SBIG_DEVICE_TYPE getDevType ();

		GetCCDInfoResults0 chip_info;
		ReadoutLineParams rlp;
		int sbig_readout_mode;
	protected:
		virtual int processOption (int in_opt);
		virtual int initChips ();
		virtual void initBinnings ()
		{
			Rts2DevCamera::initBinnings ();

			addBinning2D (2, 2);
			addBinning2D (3, 3);
		}

		virtual int setBinning (int in_vert, int in_hori)
		{
			if (in_vert != in_hori || in_vert < 1 || in_vert > 3)
				return -1;
			// use only modes 0, 1, 2, which equals to binning 1x1, 2x2, 3x3
			// it doesn't seems that Sbig offers more binning :(
			sbig_readout_mode = in_vert - 1;
			return Rts2DevCamera::setBinning (in_vert, in_hori);
		}
		virtual int startExposure ();
		virtual long isExposing ();
		virtual int readoutStart ();
		virtual int endReadout ();
		virtual int readoutOneLine ();

	public:
		Rts2DevCameraSbig (int argc, char **argv);
		virtual ~ Rts2DevCameraSbig ();

		virtual int init ();

		// callback functions for Camera alone
		virtual int ready ();
		virtual int info ();
		virtual int camChipInfo ();
		virtual long camWaitExpose ();
		virtual int camStopExpose ();
		virtual int camBox (int x, int y, int width, int height);
		virtual int camStopRead ();
		virtual int camCoolMax ();
		virtual int camCoolHold ();
		virtual int setCoolTemp (float new_temp);
		virtual int camCoolShutdown ();
};

int
Rts2DevCameraSbig::initChips ()
{
	GetCCDInfoParams req;
	req.request = CCD_INFO_IMAGING;
	sbig_readout_mode = 0;
	SBIGUnivDrvCommand (CC_GET_CCD_INFO, &req, &chip_info);
	return Rts2DevCamera::initChips ();
};

int
Rts2DevCameraSbig::startExposure ()
{
	PAR_ERROR ret;
	if (!pcam->CheckLink ())
		return -1;

	StartExposureParams sep;
	sep.ccd = 0;
	sep.exposureTime = (unsigned long) (getExposure () * 100.0 + 0.5);
	if (sep.exposureTime < 1)
		sep.exposureTime = 1;
	sep.abgState = ABG_LOW7;
	sep.openShutter = getExpType () ? SC_CLOSE_SHUTTER : SC_OPEN_SHUTTER;

	ret = pcam->SBIGUnivDrvCommand (CC_START_EXPOSURE, &sep, NULL);
	return checkSbigHw (ret);
}


long
Rts2DevCameraSbig::isExposing ()
{
	long ret;
	ret = Rts2DevCamera::isExposing ();
	if (ret > 0)
		return ret;

	QueryCommandStatusParams qcsp;
	QueryCommandStatusResults qcsr;

	qcsp.command = CC_START_EXPOSURE;
	SBIGUnivDrvCommand (CC_QUERY_COMMAND_STATUS, &qcsp, &qcsr);
	if ((qcsr.status & 0x03) != 0x02)
		return -2;
	return 1;
}


int
Rts2DevCameraSbig::readoutStart ()
{
	StartReadoutParams srp;
	srp.ccd = 0;
	srp.left = srp.top = 0;
	srp.height = chipUsedReadout->getHeightInt ();
	srp.width = chipUsedReadout->getWidthInt ();
	srp.readoutMode = sbig_readout_mode;
	SBIGUnivDrvCommand (CC_START_READOUT, &srp, NULL);
	rlp.ccd = 0;
	rlp.pixelStart = chipUsedReadout->getXInt () / binningVertical ();
	rlp.pixelLength = chipUsedReadout->getWidthInt () / binningVertical ();
	rlp.readoutMode = sbig_readout_mode;
	return Rts2DevCamera::readoutStart ();
}


int
Rts2DevCameraSbig::endReadout ()
{
	EndReadoutParams erp;
	erp.ccd = 0;
	SBIGUnivDrvCommand (CC_END_READOUT, &erp, NULL);
	return Rts2DevCamera::endReadout ();
}


int
Rts2DevCameraSbig::readoutOneLine ()
{
	int ret;

	char *dest_top = dataBuffer;

	DumpLinesParams dlp;
	dlp.ccd = 0;
	dlp.lineLength = (chipUsedReadout->getYInt () / binningVertical ());
	dlp.readoutMode = sbig_readout_mode;
	SBIGUnivDrvCommand (CC_DUMP_LINES, &dlp, NULL);

	for (int readoutLine = 0; readoutLine < getUsedHeightBinned (); readoutLine++)
	{
		SBIGUnivDrvCommand (CC_READOUT_LINE, &rlp, dest_top);
		dest_top += rlp.pixelLength * usedPixelByteSize ();
	}
	ret = sendReadoutData (dataBuffer, getWriteBinaryDataSize ());
	if (ret < 0)
		return -1;
	return -2;
}


Rts2DevCameraSbig::Rts2DevCameraSbig (int in_argc, char **in_argv):
Rts2DevCamera (in_argc, in_argv)
{
	createTempAir ();
	createTempSet ();
	createCoolingPower ();
	createTempCCD ();
	createCamFan ();

	createExpType ();

	pcam = NULL;
	usb_port = 0;
	reqSerialNumber = NULL;
	addOption ('u', "usb_port", 1, "USB port number - defaults to 0");
	addOption ('n', "serial_number", 1,
		"SBIG serial number to accept for that camera");
}


Rts2DevCameraSbig::~Rts2DevCameraSbig ()
{
	delete pcam;
}


int
Rts2DevCameraSbig::processOption (int in_opt)
{
	switch (in_opt)
	{
		case 'u':
			usb_port = atoi (optarg);
			if (usb_port <= 0 || usb_port > 3)
			{
				usb_port = 0;
				return -1;
			}
			break;
		case 'n':
			if (usb_port)
				return -1;
			reqSerialNumber = optarg;
			break;
		default:
			return Rts2DevCamera::processOption (in_opt);
	}
	return 0;
}


SBIG_DEVICE_TYPE
Rts2DevCameraSbig::getDevType ()
{
	switch (usb_port)
	{
		case 1:
			return DEV_USB1;
			break;
		case 2:
			return DEV_USB2;
			break;
		case 3:
			return DEV_USB3;
			break;
		case 4:
			return DEV_USB4;
		default:
			return DEV_USB;
	}
}


int
Rts2DevCameraSbig::init ()
{
	int ret_c_init;
	OpenDeviceParams odp;

	ret_c_init = Rts2DevCamera::init ();
	if (ret_c_init)
		return ret_c_init;

	pcam = new CSBIGCam ();
	if (pcam->OpenDriver () != CE_NO_ERROR)
	{
		delete pcam;
		return -1;
	}

	// find camera by serial number..
	if (reqSerialNumber)
	{
		QueryUSBResults qusbres;
		if (pcam->SBIGUnivDrvCommand (CC_QUERY_USB, NULL, &qusbres) !=
			CE_NO_ERROR)
		{
			delete pcam;
			return -1;
		}
		// search for serial number..
		for (usb_port = 0; usb_port < 4; usb_port++)
		{
			if (qusbres.usbInfo[usb_port].cameraFound == TRUE
				&& !strncmp (qusbres.usbInfo[usb_port].serialNumber,
				reqSerialNumber, 10))
				break;
		}
		if (usb_port == 4)
		{
			delete pcam;
			return -1;
		}
		usb_port++;				 //cause it's 1 based..
	}

	odp.deviceType = getDevType ();
	if (pcam->OpenDevice (odp) != CE_NO_ERROR)
	{
		delete pcam;
		return -1;
	}

	if (pcam->GetError () != CE_NO_ERROR)
	{
		delete pcam;
		return -1;
	}
	pcam->EstablishLink ();
	if (pcam->GetError () != CE_NO_ERROR)
	{
		delete pcam;
		return -1;
	}

	// init chips
	GetCCDInfoParams req;
	GetCCDInfoResults0 res;

	PAR_ERROR ret;

	req.request = CCD_INFO_IMAGING;
	ret = pcam->SBIGUnivDrvCommand (CC_GET_CCD_INFO, &req, &res);
	if (ret != CE_NO_ERROR)
	{
		return -1;
	}
	setSize (res.readoutInfo[0].width, res.readoutInfo[0].height, 0, 0);
	//	res.readoutInfo[0].pixelWidth,
	//		res.readoutInfo[0].pixelHeight);

	GetDriverInfoResults0 gccdir0;
	pcam->GetDriverInfo (DRIVER_STD, gccdir0);
	if (pcam->GetError () != CE_NO_ERROR)
		return -1;
	sprintf (ccdType, "SBIG_%i", pcam->GetCameraType ());
	// get serial number

	GetCCDInfoParams reqI;
	GetCCDInfoResults2 resI;

	reqI.request = CCD_INFO_EXTENDED;
	ret = pcam->SBIGUnivDrvCommand (CC_GET_CCD_INFO, &reqI, &resI);
	if (ret != CE_NO_ERROR)
		return -1;
	strcpy (serialNumber, resI.serialNumber);

	return initChips ();
}


int
Rts2DevCameraSbig::ready ()
{
	double ccdTemp;
	PAR_ERROR ret;
	ret = pcam->GetCCDTemperature (ccdTemp);
	return checkSbigHw (ret);
}


int
Rts2DevCameraSbig::info ()
{
	QueryTemperatureStatusResults qtsr;
	QueryCommandStatusParams qcsp;
	QueryCommandStatusResults qcsr;
	if (pcam->SBIGUnivDrvCommand (CC_QUERY_TEMPERATURE_STATUS, NULL, &qtsr) !=
		CE_NO_ERROR)
		return -1;
	qcsp.command = CC_MISCELLANEOUS_CONTROL;
	if (pcam->SBIGUnivDrvCommand (CC_QUERY_COMMAND_STATUS, &qcsp, &qcsr) !=
		CE_NO_ERROR)
		return -1;
	fan->setValueInteger (qcsr.status & 0x100);
	tempAir->setValueFloat (pcam->ADToDegreesC (qtsr.ambientThermistor, FALSE));
	tempSet->setValueFloat (pcam->ADToDegreesC (qtsr.ccdSetpoint, TRUE));
	coolingPower->setValueInteger ((int) ((qtsr.power / 255.0) * 1000));
	tempCCD->setValueFloat (pcam->ADToDegreesC (qtsr.ccdThermistor, TRUE));
	return Rts2DevCamera::info ();
}


int
Rts2DevCameraSbig::camChipInfo ()
{
	return 0;
}


long
Rts2DevCameraSbig::camWaitExpose ()
{
	long ret;
	ret = Rts2DevCamera::camWaitExpose ();
	if (ret == -2)
	{
		camStopExpose ();		 // SBIG devices are strange, there is same command for forced stop and normal stop
		return -2;
	}
	return ret;
}


int
Rts2DevCameraSbig::camStopExpose ()
{
	PAR_ERROR ret;
	if (!pcam->CheckLink ())
	{
		return DEVDEM_E_HW;
	}

	EndExposureParams eep;
	eep.ccd = 0;

	ret = pcam->SBIGUnivDrvCommand (CC_END_EXPOSURE, &eep, NULL);
	return checkSbigHw (ret);
}


int
Rts2DevCameraSbig::camBox (int x, int y, int width, int height)
{
	return -1;
}


int
Rts2DevCameraSbig::camStopRead ()
{
	return -1;
}


int
Rts2DevCameraSbig::fanState (int newFanState)
{
	PAR_ERROR ret;
	MiscellaneousControlParams mcp;
	mcp.fanEnable = newFanState;
	mcp.shutterCommand = 0;
	mcp.ledState = 0;
	ret = pcam->SBIGUnivDrvCommand (CC_MISCELLANEOUS_CONTROL, &mcp, NULL);
	return checkSbigHw (ret);
}


int
Rts2DevCameraSbig::camCoolMax ()
{
	SetTemperatureRegulationParams temp;
	PAR_ERROR ret;
	temp.regulation = REGULATION_OVERRIDE;
	temp.ccdSetpoint = 255;
	if (fanState (TRUE))
		return -1;
	ret = pcam->SBIGUnivDrvCommand (CC_SET_TEMPERATURE_REGULATION, &temp, NULL);
	return checkSbigHw (ret);
}


int
Rts2DevCameraSbig::camCoolHold ()
{
	int ret;
	ret = fanState (TRUE);
	if (ret)
		return -1;
	if (isnan (nightCoolTemp))
		ret = setCoolTemp (-5);
	else
		ret = setCoolTemp (nightCoolTemp);
	if (ret)
		return -1;
	return fanState (TRUE);
}


int
Rts2DevCameraSbig::setCoolTemp (float new_temp)
{
	SetTemperatureRegulationParams temp;
	PAR_ERROR ret;
	temp.regulation = REGULATION_ON;
	logStream (MESSAGE_DEBUG) << "sbig setCoolTemp setTemp " << new_temp <<
		sendLog;
	if (fanState (TRUE))
		return -1;
	temp.ccdSetpoint = ccd_c2ad (new_temp);
	ret = pcam->SBIGUnivDrvCommand (CC_SET_TEMPERATURE_REGULATION, &temp, NULL);
	return checkSbigHw (ret);
}


int
Rts2DevCameraSbig::camCoolShutdown ()
{
	SetTemperatureRegulationParams temp;
	PAR_ERROR ret;
	temp.regulation = REGULATION_OFF;
	temp.ccdSetpoint = 0;
	if (fanState (FALSE))
		return -1;
	ret = pcam->SBIGUnivDrvCommand (CC_SET_TEMPERATURE_REGULATION, &temp, NULL);
	return checkSbigHw (ret);
}


int
main (int argc, char **argv)
{
	Rts2DevCameraSbig device = Rts2DevCameraSbig (argc, argv);
	return device.run ();
}
