/* 
 * Image preview and download classes.
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

#include "config.h"
#include "httpreq.h"
#include "xmlapi.h"

#define DEFAULT_QUANTILES    0.005
// number of channels in image
#define CHANNELS             4

namespace rts2xmlrpc
{

/**
 * Create page with JPEG previews. This is an abstract class - all classes
 * which need preview functionality shoudl inherit from this page.
 * @author Petr Kubanek <petr@kubanek.net>
 */
class Previewer
{
	public:
		Previewer () {};

		/**
		 * Returns additional style which is needed for JavaScript.
		 */
		const char *style ();

		/**
		 * Add script entry for image manipulation. This should be
		 * included in head section.
		 */
		void script (std::ostringstream& _os, const char *label_encoded, float quantiles, int chan);

		/**
		 *
		 *
                 * @param ps     preview size
		 * @param s      size
		 * @param c      selected channel (image extension)
		 * @param label  image label
		 */
		void form (std::ostringstream& _os, int page, int ps, int s, int c, const char *label);

		/**
		 * Create image href entry.
		 *
		 * @param _os image output stream
		 * @param i image number
		 * @param fpath path to preview image
		 * @param prevsize size of preview in pixels
		 */
		void imageHref (std::ostringstream& _os, int i, const char *fpath, int prevsize, const char * label, float quantiles, int chan);

		void pageLink (std::ostringstream& _os, int i, int pagesiz, int prevsize, const char * label, bool selected, float quantiles, int chan);
};

#if defined(HAVE_LIBJPEG) && HAVE_LIBJPEG == 1

/**
 * Returns JPEG image, generated from FITS file. Usefull for quick display of images in 
 * web browsers.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class JpegImageRequest: public GetRequestAuthorized
{
	public:
		JpegImageRequest (const char* prefix, XmlRpc::XmlRpcServer* s):GetRequestAuthorized (prefix, NULL, s) {}

		virtual void authorizedExecute (std::string path, HttpParams *params, const char* &response_type, char* &response, size_t &response_length);
};

/**
 * Returns either directory structure, or JPEG image requested in the URL.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class JpegPreview:public GetRequestAuthorized
{
	public:
		JpegPreview (const char* prefix, const char *_dirPath, XmlRpcServer *s):GetRequestAuthorized (prefix, "JPEG image preview", s) { dirPath = _dirPath; }

		virtual void authorizedExecute (std::string path, HttpParams *params, const char* &response_type, char* &response, size_t &response_length);
	private:
		const char *dirPath;
};

#endif // HAVE_LIBJPEG

/**
 * Returns raw FITS file as it is written on the disk.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class FitsImageRequest:public GetRequestAuthorized
{
	public:
		FitsImageRequest (const char* prefix, XmlRpcServer* s):GetRequestAuthorized (prefix, NULL, s) {}

		virtual void authorizedExecute (std::string path, HttpParams *params, const char* &response_type, char* &response, size_t &response_length);
};

/**
 * Creates compressed archive of files for download, send them as binary file 
 * to HTTP client.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class DownloadRequest:public GetRequestAuthorized
{
	public:
#ifdef HAVE_LIBARCHIVE
		DownloadRequest (const char* prefix, XmlRpcServer* s):GetRequestAuthorized (prefix, NULL, s) { buf = NULL; buf_size = 0; }
		virtual ~DownloadRequest () { if (buf) free (buf); }
#else
		DownloadRequest (const char* prefix, XmlRpcServer* s):GetRequestAuthorized (prefix, NULL, s) {}
#endif
		virtual void authorizedExecute (std::string path, HttpParams *params, const char* &response_type, char* &response, size_t &response_length);

#ifdef HAVE_LIBARCHIVE
		char *buf;
		ssize_t buf_size;
#endif
};

}

#ifdef HAVE_LIBARCHIVE

int open_callback (struct archive *a, void *client_data);
ssize_t write_callback (struct archive *a, void *client_data, const void *buffer, size_t length);
int close_callback (struct archive *a, void *client_data);

#endif // HAVE_LIBARCHIVE
