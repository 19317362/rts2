/* 
 * Alt-Az graph plotting capabilities.
 * Copyright (C) 2009 Petr Kubanek <petr@kubanek.net>
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

#ifndef __RTS2_MAGICK_ALTAZ__
#define __RTS2_MAGICK_ALTAZ__

#include "config.h"

#if defined(HAVE_LIBJPEG) && HAVE_LIBJPEG == 1
#include <Magick++.h>
#include <libnova/libnova.h>

namespace rts2xmlrpc
{

/**
 * Represents AltAz coordinates. Can be aplied ower exising image, or on a new image.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class AltAz
{
	public:
		AltAz () : image (Magick::Geometry (600, 600), "white") {rotation = 0; setCenter (); }
		AltAz (Magick::Image &_image): image (_image) {rotation = 0; setCenter (); }

		/**
		 * Plot alt-az grid on associated image.
		 */
		void plotAltAzGrid ();

		void plotCross (struct ln_hrz_posn *hrz, const char* label = NULL);

		void write (Magick::Blob *blob, const char *type) { image.write (blob, type); }

	private:
		Magick::Image image;

		// angle of true north, in radians. North is on top, if rotation = 0
		double rotation;

		int c; // center position
		int l; // lenght

		void setCenter ();
};

}

#endif

#endif /* ! __RTS2_MAGICK_ALTAZ__ */
