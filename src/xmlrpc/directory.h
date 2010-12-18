/* 
 * Class providing directory to GET request.
 * Copyright (C) 2010 Petr Kubanek <petr@kubanek.net>
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

#ifndef __RTS2_DIRECTORY__
#define __RTS2_DIRECTORY__

#include "httpreq.h"
#include "xmlrpcd.h"

namespace rts2xmlrpc
{

/**
 * Maps directory to request space.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class Directory: public GetRequestAuthorized
{
	public:
		Directory (const char* prefix, const char *_dirPath, const char *_defaultFile, XmlRpc::XmlRpcServer* s);

		virtual void authorizedExecute (std::string path, XmlRpc::HttpParams *params, const char* &response_type, char* &response, size_t &response_length);

	private:
		std::string dirPath;
		std::string defaultFile;

		// file type, based on file extension
		std::map <std::string, const char *> responseTypes;
};

}

#endif /**! __RTS2_DIRECTORY__ */
