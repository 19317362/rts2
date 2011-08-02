/* 
 * Classes for camera image.
 * Copyright (C) 2007 Petr Kubanek <petr@kubanek.net>
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

#include "cameraimage.h"
#include "devcliimg.h"
#include "../utils/timestamp.h"

using namespace rts2image;

CameraImage::~CameraImage (void)
{
	for (std::vector < ImageDeviceWait * >::iterator iter =	deviceWaits.begin (); iter != deviceWaits.end (); iter++)
	{
		delete *iter;
	}
	deviceWaits.clear ();
	delete image;
	image = NULL;
}

void CameraImage::waitForDevice (rts2core::Rts2DevClient * devClient, double after)
{
	// check if this device was not prematurely triggered
	if (std::find (prematurelyReceived.begin (), prematurelyReceived.end (), devClient) == prematurelyReceived.end ())
		deviceWaits.push_back (new ImageDeviceWait (devClient, after));
	else
		std::cout << "prematurely received " << devClient->getName () << std::endl;
}

bool CameraImage::waitingFor (rts2core::Rts2DevClient * devClient)
{
	bool ret = false;
	for (std::vector < ImageDeviceWait * >::iterator iter = deviceWaits.begin (); iter != deviceWaits.end ();)
	{
		ImageDeviceWait *idw = *iter;
		if (idw->getClient () == devClient && (isnan (devClient->getConnection ()->getInfoTime ()) || idw->getAfter () <= devClient->getConnection ()->getInfoTime ()))
		{
			delete idw;
			iter = deviceWaits.erase (iter);
			ret = true;
		}
		else
		{
			logStream (MESSAGE_DEBUG) << "waitingFor "
				<< idw->getClient ()->getName () << " "
				<< (idw->getClient () == devClient) << " "
				<< Timestamp (devClient->getConnection ()->getInfoTime ()) << " "
				<< Timestamp (idw->getAfter ()) <<
				sendLog;
			iter++;
		}
	}
	if (ret)
	{
		image->writeConn (devClient->getConnection (), INFO_CALLED);
	}
	return ret;
}

void CameraImage::waitForTrigger (rts2core::Rts2DevClient * devClient)
{
	// check if this device was not prematurely triggered
	if (std::find (prematurelyReceived.begin (), prematurelyReceived.end (), devClient) == prematurelyReceived.end ())
		triggerWaits.push_back (devClient);
	else
		std::cout << "received premature trigger " << devClient->getName () << std::endl;
}

bool CameraImage::wasTriggered (rts2core::Rts2DevClient * devClient)
{
	for (std::vector < rts2core::Rts2DevClient * >::iterator iter = triggerWaits.begin (); iter != triggerWaits.end (); iter++)
	{
		if (*iter == devClient)
		{
			image->writeConn (devClient->getConnection (), TRIGGERED);
			triggerWaits.erase (iter);
			return true;
		}
	}
	return false;
}

bool CameraImage::canDelete ()
{
	if (isnan (exEnd) || !dataWriten)
		return false;
	return !waitForMetaData ();
}

CameraImages::~CameraImages (void)
{
	for (CameraImages::iterator iter = begin (); iter != end (); iter++)
	{
		delete (*iter).second;
	}
	clear ();
}


void
CameraImages::deleteOld ()
{
	for (CameraImages::iterator iter = begin (); iter != end ();)
	{
		CameraImage *ci = (*iter).second;
		if (ci->canDelete ())
		{
			delete ci;
			erase (iter++);
		}
		else
		{
			iter++;
		}
	}
}

void CameraImages::infoOK (DevClientCameraImage * master, rts2core::Rts2DevClient * client)
{
	for (CameraImages::iterator iter = begin (); iter != end ();)
	{
		CameraImage *ci = (*iter).second;
		if (ci->waitingFor (client))
		{
			if (ci->canDelete ())
			{
				master->processCameraImage (iter++);
			}
			else
			{
				iter++;
			}
		}
		else
		{
			iter++;
		}
	}
}

void CameraImages::infoFailed (DevClientCameraImage * master, rts2core::Rts2DevClient * client)
{
	for (CameraImages::iterator iter = begin (); iter != end ();)
	{
		CameraImage *ci = (*iter).second;
		if (ci->waitingFor (client))
		{
			if (ci->canDelete ())
			{
				master->processCameraImage (iter++);
			}
			else
			{
				iter++;
			}
		}
		else
		{
			iter++;
		}
	}
}

bool CameraImages::wasTriggered (DevClientCameraImage * master, rts2core::Rts2DevClient * client)
{
	bool ret = false;

	for (CameraImages::iterator iter = begin (); iter != end ();)
	{
		CameraImage *ci = (*iter).second;
		if (ci->wasTriggered (client))
		{
			ret = true;
			if (ci->canDelete ())
				master->processCameraImage (iter++);
			else
				iter++;
		}
		else
		{
			iter++;
		}
	}
	return ret;
}
