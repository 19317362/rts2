/* 
 * Configuration file read routines.
 * Copyright (C) 2006-2010 Petr Kubanek <petr@kubanek.net>
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

#include "message.h"
#include "block.h"

#include <sys/time.h>
#include <time.h>
#include <sstream>

using namespace rts2core;

Message::Message (const struct timeval &in_messageTime, std::string in_messageOName, messageType_t in_messageType, std::string in_messageString)
{
	messageTime = in_messageTime;
	messageOName = in_messageOName;
	messageType = in_messageType;
	messageString = in_messageString;
}

Message::Message (const char *in_messageOName, messageType_t in_messageType, const char *in_messageString)
{
	gettimeofday (&messageTime, NULL);
	messageOName = std::string (in_messageOName);
	messageType = in_messageType;
	messageString = std::string (in_messageString);
}

Message::Message (double in_messageTime, const char *in_messageOName, messageType_t in_messageType, const char *in_messageString)
{
#ifdef HAVE_TRUNC
	messageTime.tv_sec = trunc (in_messageTime);
#else
	messageTime.tv_sec = (time_t) floor (in_messageTime);
#endif
	messageTime.tv_usec = USEC_SEC * (in_messageTime - messageTime.tv_sec);
	messageOName = std::string (in_messageOName);
	messageType = in_messageType;
	messageString = std::string (in_messageString);
}

Message::~Message ()
{
}

std::string Message::toConn ()
{
	std::ostringstream os;
	// replace \r\n
	std::string msg = messageString;
	size_t pos;
	for (pos = msg.find_first_of ("\0"); pos != std::string::npos; pos = msg.find_first_of ("\0", pos))
	{
		msg.replace (pos, 1, "\\0");
	}
	for (pos = msg.find_first_of ("\r"); pos != std::string::npos; pos = msg.find_first_of ("\r", pos))
	{
		msg.replace (pos, 1, "\\r");
	}
	for (pos = msg.find_first_of ("\n"); pos != std::string::npos; pos = msg.find_first_of ("\n", pos))
	{
		msg.replace (pos, 1, "\\n");
	}

	os << PROTO_MESSAGE
		<< " " << messageTime.tv_sec
		<< " " << messageTime.tv_usec
		<< " " << messageOName << " " << messageType << " " << msg;
	return os.str ();
}
