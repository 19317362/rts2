/* 
 * (USB) SBIG driver.
 * Copyright (C) 2004-2008 Petr Kubanek <petr@kubanek.net>
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
#include "device.h"
#include "block.h"

#include "sbigudrv.h"
#include "csbigcam.h"

#define OPT_USBPORT     OPT_LOCAL + 230
#define OPT_SERIAL      OPT_LOCAL + 231

unsigned int ccd_c2ad (double t)
{
	double r = 3.0 * exp (0.94390589891 * (25.0 - t) / 25.0);
	unsigned int ret = (unsigned int) (4096.0 / (10.0 / r + 1.0));
	if (ret > 4096)
		ret = 4096;
	return ret;
}

double ccd_ad2c (unsigned int ad)
{
	double r;
	if (ad == 0)
		return 1000;
	r = 10.0 / (4096.0 / (double) ad - 1.0);
	return 25.0 - 25.0 * (log (r / 3.0) / 0.94390589891);
}

double ambient_ad2c (unsigned int ad)
{
	double r;
	r = 3.0 / (4096.0 / (double) ad - 1.0);
	return 25.0 - 45.0 * (log (r / 3.0) / 2.0529692213);
}

namespace rts2camd
{

/**
 * Class for USB SBIG camera.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class Sbig:public Camera
{
	private:
		CSBIGCam * pcam;
		int checkSbigHw (PAR_ERROR ret)
		{
			if (ret == CE_NO_ERROR)
				return 0;
			logStream (MESSAGE_ERROR) << "Sbig::checkSbigHw ret: " << ret << sendLog;
			return -1;
		}
		int fanState (int newFanState);
		int usb_port;
		char *reqSerialNumber;

		SBIG_DEVICE_TYPE getDevType ();

		GetCCDInfoResults0 chip_info;
		ReadoutLineParams rlp;
		int sbig_readout_mode;

		rts2core::ValueInteger *coolingPower;
		rts2core::ValueSelection *tempRegulation;
		rts2core::ValueBool *fan;

	protected:
		virtual int processOption (int in_opt);
		virtual int initChips ();
		virtual void initBinnings ()
		{
			Camera::initBinnings ();

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
			return Camera::setBinning (in_vert, in_hori);
		}
		virtual int startExposure ();
		virtual long isExposing ();
		virtual int readoutStart ();
		virtual int endReadout ();
		virtual int doReadout ();

		virtual int setValue (rts2core::Value * old_value, rts2core::Value * new_value);

	public:
		Sbig (int argc, char **argv);
		virtual ~ Sbig ();

		virtual int init ();

		// callback functions for Camera alone
		virtual int info ();
		virtual long camWaitExpose ();
		virtual int camStopExpose ();
		virtual int camBox (int x, int y, int width, int height);
		virtual int camCoolMax ();
		virtual int camCoolHold ();
		virtual int setCoolTemp (float new_temp);
		virtual int tempOff ();
		virtual void afterNight ();
};

}

using namespace rts2camd;

int Sbig::initChips ()
{
	GetCCDInfoParams req;
	req.request = CCD_INFO_IMAGING;
	sbig_readout_mode = 0;
	SBIGUnivDrvCommand (CC_GET_CCD_INFO, &req, &chip_info);
	return Camera::initChips ();
};

int Sbig::startExposure ()
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

long Sbig::isExposing ()
{
	long ret;
	ret = Camera::isExposing ();
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

int Sbig::readoutStart ()
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
	return Camera::readoutStart ();
}

int Sbig::endReadout ()
{
	EndReadoutParams erp;
	erp.ccd = 0;
	SBIGUnivDrvCommand (CC_END_READOUT, &erp, NULL);
	return Camera::endReadout ();
}

int Sbig::doReadout ()
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

int Sbig::setValue (rts2core::Value * old_value, rts2core::Value * new_value)
{
	if (old_value == tempRegulation)
	{
		switch (new_value->getValueInteger ())
		{
			case 0:
				return tempOff () == 0 ? 0 : -2;
			case 1:
				return setCoolTemp (tempSet->getValueFloat ()) == 0 ? 0 : -2;
			default:
				return -2;
		}
	}
	if (old_value == coolingPower)
	{
		return -1; //setcool (2, new_value->getValueInteger (), 0) == 0 ? 0 : -2;
	}
	if (old_value == fan)
	{
		return -1; //set_fan (((rts2core::ValueBool *) new_value)->getValueBool ()) == 0 ? 0 : -2;
	}
	return Camera::setValue (old_value, new_value);
}

Sbig::Sbig (int in_argc, char **in_argv):Camera (in_argc, in_argv)
{
	createTempAir ();
	createTempSet ();
	createTempCCD ();

	createExpType ();

	createValue (tempRegulation, "TEMP_REG", "temperature regulation", true, RTS2_VALUE_WRITABLE);
	tempRegulation->addSelVal ("OFF");
	tempRegulation->addSelVal ("TEMP");
	//tempRegulation->addSelVal ("POWER");

	tempRegulation->setValueInteger (0);

	createValue (coolingPower, "COOL_PWR", "cooling power", true, RTS2_VALUE_WRITABLE);
	coolingPower->setValueInteger (0);

	createValue (fan, "FAN", "camera fan state", true, RTS2_VALUE_WRITABLE);

	pcam = NULL;
	usb_port = 0;
	reqSerialNumber = NULL;
	addOption (OPT_USBPORT, "usb-port", 1, "USB port number - defaults to 0");
	addOption (OPT_SERIAL, "serial-number", 1, "SBIG serial number to accept for that camera");
}

Sbig::~Sbig ()
{
	delete pcam;
}

int Sbig::processOption (int in_opt)
{
	switch (in_opt)
	{
		case OPT_USBPORT:
			usb_port = atoi (optarg);
			if (usb_port <= 0 || usb_port > 3)
			{
				usb_port = 0;
				return -1;
			}
			break;
		case OPT_SERIAL:
			if (usb_port)
				return -1;
			reqSerialNumber = optarg;
			break;
		default:
			return Camera::processOption (in_opt);
	}
	return 0;
}

SBIG_DEVICE_TYPE Sbig::getDevType ()
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

int Sbig::init ()
{
	int ret_c_init;
	OpenDeviceParams odp;

	ret_c_init = Camera::init ();
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
		if (pcam->SBIGUnivDrvCommand (CC_QUERY_USB, NULL, &qusbres) != CE_NO_ERROR)
		{
			delete pcam;
			return -1;
		}
		// search for serial number..
		for (usb_port = 0; usb_port < 4; usb_port++)
		{
			if (qusbres.usbInfo[usb_port].cameraFound == TRUE && !strncmp (qusbres.usbInfo[usb_port].serialNumber, reqSerialNumber, 10))
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

int Sbig::info ()
{
	QueryTemperatureStatusResults qtsr;
	QueryCommandStatusParams qcsp;
	QueryCommandStatusResults qcsr;
	if (pcam->SBIGUnivDrvCommand (CC_QUERY_TEMPERATURE_STATUS, NULL, &qtsr) != CE_NO_ERROR)
		return -1;
	qcsp.command = CC_MISCELLANEOUS_CONTROL;
	if (pcam->SBIGUnivDrvCommand (CC_QUERY_COMMAND_STATUS, &qcsp, &qcsr) != CE_NO_ERROR)
		return -1;
	fan->setValueInteger (qcsr.status & 0x100);
	tempAir->setValueFloat (pcam->ADToDegreesC (qtsr.ambientThermistor, FALSE));
	tempSet->setValueFloat (pcam->ADToDegreesC (qtsr.ccdSetpoint, TRUE));
	tempCCD->setValueFloat (pcam->ADToDegreesC (qtsr.ccdThermistor, TRUE));
	coolingPower->setValueInteger (qtsr.power);
	return Camera::info ();
}

long Sbig::camWaitExpose ()
{
	long ret;
	ret = Camera::camWaitExpose ();
	if (ret == -2)
	{
		camStopExpose ();		 // SBIG devices are strange, there is same command for forced stop and normal stop
		return -2;
	}
	return ret;
}

int Sbig::camStopExpose ()
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

int Sbig::camBox (int x, int y, int width, int height)
{
	return -1;
}

int Sbig::fanState (int newFanState)
{
	PAR_ERROR ret;
	MiscellaneousControlParams mcp;
	mcp.fanEnable = newFanState;
	mcp.shutterCommand = 0;
	mcp.ledState = 0;
	ret = pcam->SBIGUnivDrvCommand (CC_MISCELLANEOUS_CONTROL, &mcp, NULL);
	return checkSbigHw (ret);
}

int Sbig::camCoolMax ()
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

int Sbig::camCoolHold ()
{
	int ret;
	ret = fanState (TRUE);
	if (ret)
		return -1;
	if (isnan (nightCoolTemp->getValueFloat ()))
		ret = setCoolTemp (-5);
	else
		ret = setCoolTemp (nightCoolTemp->getValueFloat ());
	if (ret)
		return -1;
	return fanState (TRUE);
}

int Sbig::setCoolTemp (float new_temp)
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
	if (checkSbigHw (ret))
		return -1;
	return Camera::setCoolTemp (new_temp);
}

int Sbig::tempOff ()
{
	SetTemperatureRegulationParams temp;
	PAR_ERROR ret;
	temp.regulation = REGULATION_OFF;
	temp.ccdSetpoint = 0;
	if (fanState (FALSE))
		return -1;
	ret = pcam->SBIGUnivDrvCommand (CC_SET_TEMPERATURE_REGULATION, &temp, NULL);
	checkSbigHw (ret);
	return ret;
}

void Sbig::afterNight ()
{
	if (tempOff ())
		logStream (MESSAGE_WARNING) << "cannot turn off temperature regulation at the end of night" << sendLog;
}

int main (int argc, char **argv)
{
	Sbig device = Sbig (argc, argv);
	return device.run ();
}
