/*
 * Classes for generating pages for planning/scheduling.
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

#include "httpreq.h"

#ifdef HAVE_PGSQL

namespace rts2xmlrpc
{

/**
 * Add, modify and review scheduling.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class Plan: public GetRequestAuthorized
{
	public:
		Plan (const char *prefix, XmlRpc::XmlRpcServer *s):GetRequestAuthorized (prefix, "observing plan management", s) {}
		virtual void authorizedExecute (std::string path, XmlRpc::HttpParams *params, const char* &response_type, char* &response, size_t &response_length);

	private:
		void printPlans (XmlRpc::HttpParams *params, char* &response, size_t &response_length);
		void printPlan (const char *id, char* &response, size_t &response_length);
};

}

#endif
