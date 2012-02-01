/* 
 * Classes for answers to HTTP requests.
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

#ifndef __RTS2_HTTPREQ__
#define __RTS2_HTTPREQ__

#include "config.h"

#ifdef HAVE_PGSQL
#include "rts2db/recvals.h"
#include "rts2db/records.h"
#include "rts2db/devicedb.h"
#include "rts2db/targetset.h"
#if defined(HAVE_LIBJPEG) && HAVE_LIBJPEG == 1
#include <Magick++.h>
#include "valueplot.h"
#endif // HAVE_LIBJPEG
#include "configuration.h"
#else

#endif // HAVE_PGSQL

#include "xmlrpc++/XmlRpc.h"

#include "libnova_cpp.h"
#include <string.h>

// maximal age of static documents
#define CACHE_MAX_STATIC   864000

namespace rts2xmlrpc
{

/**
 * Abstract class for authorized requests. Acts similarly
 * to servelets under JSP container. The class have method
 * to reply to requests arriving on the incoming connection.
 *
 * The most important is GetRequestAuthorized::authorizedExecute method.  This
 * is called after user properly authorised agains setup authorization
 * mechanism.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class GetRequestAuthorized: public XmlRpc::XmlRpcServerGetRequest
{
	public:
		GetRequestAuthorized (const char* prefix, const char *description = NULL, XmlRpc::XmlRpcServer* s = 0):XmlRpcServerGetRequest (prefix, description, s) { executePermission = false; }

		virtual void execute (XmlRpc::XmlRpcSource *source, struct sockaddr_in *saddr, std::string path, XmlRpc::HttpParams *params, int &http_code, const char* &response_type, char* &response, size_t &response_length);

		/**
		 * Received exact path and HTTP params. Returns response - MIME
		 * type, its data and length. This request is password
		 * protected - pasword protection can be removed by listing
		 * path in public section of XML-RPC config file.
		 *
		 * @param path            exact path of the request, excluding prefix part
		 * @param params          HTTP parameters. Please see xmlrpc++/XmlRpcServerGetRequest.h for allowed methods.
		 * @param response_type   MIME type of response. Ussually you will put there something like "text/html" or "image/jpeg".
		 * @param response        response data. Must be allocated, preferably by new char[]. They will be deleted by calling code
		 * @param response_length response lenght in bytes
		 *
		 * @see GetRequestAuthorized::execute
		 */
		virtual void authorizedExecute (std::string path, XmlRpc::HttpParams *params, const char* &response_type, char* &response, size_t &response_length) = 0;

	protected:
		/**
		 * Register asynchronous call.
		 */

		/**
		 * Prints document header.
		 *
		 * @param os      output stream
		 * @param title   document title
		 * @param css     optional CSS styles
		 * @param cssLink optional link to external CSS
		 * @param onLoad  on load script
		 */
		void printHeader (std::ostream &os, const char *title, const char *css = NULL, const char *cssLink = NULL, const char *onLoad = NULL);

		/**
		 * Prints menu for subpages. Forms structure to href pointing to the pages, labeled with decsription.
		 *
		 * @param _os       stream which will receive the menu
		 * @param current   current submenu. This submenu will not receive href highllting
		 * @param submenus  NULL terminated structur holding const char* references to hrefs and names to print
		 */
		void printSubMenus (std::ostream &os, const char *prefix, const char *current, const char *submenus[][2]);

		/**
		 * Prints document footer.
		 *
		 * @param os    output stream
		 */
		void printFooter (std::ostream &os);

		/**
		 * Prints command to include JavaScript page from JavaScript library direcotry.
		 *
		 * @param os    output streem
		 * @param name  document name (without directory prefix)
		 */
		void includeJavaScript (std::ostream &os, const char *name);

		/**
		 * Prints command for inclusion of JavaScript, preceeded by pagePrefix = xx.
		 */
		void includeJavaScriptWithPrefix (std::ostream &os, const char *name);

		/**
		 * Return true if user can execute commands (make changes, set next targets, ..).
		 *
		 * @return true if user has usr_execute_permission set
		 */
		bool canExecute () { return executePermission; }

		/**
		 * Return JSON page.
		 *
		 * @param msg             null terminated char array which will be returned
		 * @param response_type   response type (will be filled with application/json)
		 * @param response        response text (output)
		 * @param response_length response length in bytes
		 */
		void returnJSON (const char *msg, const char* &response_type, char* &response, size_t &response_length)
		{
			response_type = "application/json";
			response_length = strlen (msg);
			response = new char[response_length];
			memcpy (response, msg, response_length);
		}

		void returnJSON (std::ostringstream &_os, const char* &response_type, char* &response, size_t &response_length)
		{
			response_type = "application/json";
			response_length = _os.str ().length ();
			response = new char[response_length];
			memcpy (response, _os.str ().c_str (), response_length);
		}

	private:
		bool executePermission;
};

#ifdef HAVE_LIBJPEG

/**
 * Plot current position of telescope, target and next target position.
 */
class CurrentPosition:public GetRequestAuthorized
{
	public:
		CurrentPosition (const char *prefix, XmlRpc::XmlRpcServer *s):GetRequestAuthorized (prefix, "current telescope position", s) {};

		virtual void authorizedExecute (std::string path, XmlRpc::HttpParams *params, const char* &response_type, char* &response, size_t &response_length);
};

#endif /* HAVE_LIBJPEG */

#ifdef HAVE_PGSQL

#ifdef HAVE_LIBJPEG

/**
 * Plot targets on the alt-az graph.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class AltAzTarget: public GetRequestAuthorized
{
	public:
		AltAzTarget (const char *prefix, XmlRpc::XmlRpcServer *s):GetRequestAuthorized (prefix, "altitude target graph", s) {};

		virtual void authorizedExecute (std::string path, XmlRpc::HttpParams *params, const char* &response_type, char* &response, size_t &response_length);
};

#endif /* HAVE_LIBJPEG */ 

#endif /* HAVE_PGSQL */

}

#endif /* !__RTS2_HTTPREQ__ */
