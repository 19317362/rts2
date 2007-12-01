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

#ifndef __RTS2_CAMERA_IMAGE__
#define __RTS2_CAMERA_IMAGE__

#include "rts2image.h"
#include <map>
#include <vector>

class Rts2DevClient;
class Rts2DevClientCameraImage;

/**
 * Holds informations about which device waits for which info time.
 */
class ImageDeviceWait
{
	private:
		Rts2DevClient * devclient;
		double after;
	public:
		ImageDeviceWait (Rts2DevClient * in_devclient, double in_after)
		{
			devclient = in_devclient;
			after = in_after;
		}

		Rts2DevClient *getClient ()
		{
			return devclient;
		}

		double getAfter ()
		{
			return after;
		}
};

/**
 * Holds information about single image.
 */
class CameraImage
{
	private:
		std::vector < ImageDeviceWait * >deviceWaits;
	public:
		double exStart;
		double exEnd;
		bool dataWriten;
		Rts2Image *image;

		CameraImage (Rts2Image * in_image, double in_exStart)
		{
			image = in_image;
			exStart = in_exStart;
			exEnd = nan ("f");
			dataWriten = false;
		}
		virtual ~ CameraImage (void);

		void waitForDevice (Rts2DevClient * devClient, double after);
		bool waitingFor (Rts2DevClient * devClient);

		void setExEnd (double in_exEnd)
		{
			exEnd = in_exEnd;
		}

		void setDataWriten ()
		{
			dataWriten = true;
		}

		bool canDelete ();
};

/**
 * That holds images for camd. Images are hold there till all informations are
 * collected, then they are put to processing.
 */
class CameraImages:public std::map <int, CameraImage * >
{
	public:
		CameraImages ()
		{
		}
		virtual ~CameraImages (void);

		void deleteOld ();

		void infoOK (Rts2DevClientCameraImage * master, Rts2DevClient * client);
		void infoFailed (Rts2DevClientCameraImage * master, Rts2DevClient * client);
};
#endif							 /* !__RTS2_CAMERA_IMAGE__ */
