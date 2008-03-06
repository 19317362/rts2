/* 
 * Generic serial port connection.
 * Copyright (C) 2008 Petr Kubanek <petr@kubanek.net>
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

#include <fcntl.h>
#include <termios.h>

#include "rts2connserial.h"
#include "rts2block.h"
#include <iomanip>

Rts2ConnSerial::Rts2ConnSerial (const char *in_devName, Rts2Block * in_master, bSpeedT in_baudSpeed,
cSizeT in_cSize, parityT in_parity, int in_vTime)
:Rts2Conn (in_master)
{
	sock = open (in_devName, O_RDWR);

	if (sock < 0)
		logStream (MESSAGE_ERROR) << "cannot open serial port:" << in_devName << sendLog;

	// some defaults
	baudSpeed = in_baudSpeed;

	cSize = in_cSize;
	parity = in_parity;

	vMin = 0;
	vTime = in_vTime;

	debugPortComm = false;
}


Rts2ConnSerial::~Rts2ConnSerial (void)
{
	if (sock > 0)
		close (sock);
}


const char *
Rts2ConnSerial::getBaudSpeed ()
{
	switch (baudSpeed)
	{
		case BS2400:
			return "2400";
		case BS4800:
			return "4800";
		case BS9600:
			return "9600";
		case BS115200:
			return "115200";
	}
	return NULL;
}


int
Rts2ConnSerial::init ()
{
	if (sock < 0)
		return -1;

	struct termios s_termios;
	speed_t b_speed;
	int ret;

	ret = tcgetattr (sock, &s_termios);
	if (ret)
	{
		logStream (MESSAGE_ERROR) << "cannot get serial port attributes:" << strerror (errno) << sendLog;
		return -1;
	}

	switch (baudSpeed)
	{
		case BS2400:
			b_speed = B2400;
			break;
		case BS4800:
			b_speed = B4800;
			break;
		case BS9600:
			b_speed = B9600;
			break;
		case BS115200:
			b_speed = B115200;
			break;
	}

	ret = cfsetospeed (&s_termios, b_speed);
	if (ret)
	{
		logStream (MESSAGE_ERROR) << "cannot set output speed to " << getBaudSpeed () << sendLog;
		return -1;
	}

	ret = cfsetispeed (&s_termios, b_speed);
	if (ret)
	{
		logStream (MESSAGE_ERROR) << "cannot set input speed to " << getBaudSpeed () << sendLog;
		return -1;
	}

	s_termios.c_iflag = IGNBRK & ~(IXON | IXOFF | IXANY);
	s_termios.c_oflag = 0;
	// clear CSIZE and parity from cflags..
	s_termios.c_cflag = s_termios.c_cflag & ~(CSIZE | PARENB | PARODD);
	switch (cSize)
	{
		case C7:
			s_termios.c_cflag |= CS7;
			break;
		case C8:
			s_termios.c_cflag |= CS8;
			break;
	}
	switch (parity)
	{
		case NONE:
			break;
		case ODD:
			s_termios.c_cflag |= PARODD;
		case EVEN:
			s_termios.c_cflag |= PARENB;
			break;
	}
	s_termios.c_lflag = 0;
	s_termios.c_cc[VMIN] = getVMin ();
	s_termios.c_cc[VTIME] = getVTime ();

	ret = tcsetattr (sock, TCSANOW, &s_termios);
	if (ret < 0)
	{
		logStream (MESSAGE_ERROR) << "cannot set device parameters" << sendLog;
		return -1;
	}

	tcflush (sock, TCIOFLUSH);
	return 0;
}


int
Rts2ConnSerial::writePort (const char *wbuf, int b_len)
{
	int wlen = 0;
	if (debugPortComm)
	{
		char *tmp_b = new char[b_len + 1];
		memcpy (tmp_b, wbuf, b_len);
		tmp_b[b_len] = '\0';
		logStream (MESSAGE_DEBUG) << "will write to port: '" << tmp_b << "'" << sendLog;
		delete []tmp_b;
	}
	while (wlen < b_len)
	{
		int ret = write (sock, wbuf, b_len);
		if (ret == -1 && errno != EINTR)
		{
			logStream (MESSAGE_ERROR) << "cannot write to serial port "
				<< strerror (errno) << sendLog;
			return -1;
		}
		if (ret == 0)
		{
			logStream (MESSAGE_ERROR) << "write 0 bytes to serial port" << sendLog;
			return -1;
		}
		wlen += ret;
	}
	return 0;
}


int
Rts2ConnSerial::readPort (char *rbuf, int b_len)
{
	int rlen = 0;
	while (rlen < b_len)
	{
		int ret = read (sock, rbuf + rlen, b_len - rlen);
		if (ret == -1 && errno != EINTR)
		{
			if (rlen > 0)
			{
				rbuf[rlen] = '\0';
				logStream (MESSAGE_ERROR) << "cannot read from serial port after reading '"
					<< rbuf << "', error is "
					<< strerror (errno) << sendLog;
			}
			else
			{
				logStream (MESSAGE_ERROR) << "cannot read from serial port "
					<< strerror (errno) << sendLog;
			}
			return -1;
		}
		if (ret == 0)
		{
			logStream (MESSAGE_ERROR) << "read 0 bytes from serial port" << sendLog;
			return -1;
		}

		rlen += ret;
	}
	if (debugPortComm)
	{
		char *tmp_b = new char[rlen + 1];
		memcpy (tmp_b, rbuf, rlen);
		tmp_b[rlen] = '\0';
		logStream (MESSAGE_DEBUG) << "readed from port '" << tmp_b << "'" << sendLog;
		delete []tmp_b;
	}
	return rlen;
}


int
Rts2ConnSerial::readPort (char *rbuf, int b_len, char endChar)
{
	int rlen = 0;
	while (rlen < b_len)
	{
		int ret = read (sock, rbuf + rlen, 1);
		if (ret == -1 && errno != EINTR)
		{
			if (rlen > 0)
			{
				rbuf[rlen] = '\0';
				logStream (MESSAGE_ERROR) << "cannot read from serial port after reading '"
					<< rbuf << "', error is "
					<< strerror (errno) << sendLog;
			}
			else
				logStream (MESSAGE_ERROR) << "cannot read from serial port "
					<< strerror (errno) << sendLog;
			return -1;
		}
		if (*(rbuf + rlen) == endChar)
		{
			rlen += ret;
			if (debugPortComm)
			{
				char *tmp_b = new char[rlen + 1];
				memcpy (tmp_b, rbuf, rlen);
				tmp_b[rlen] = '\0';
				logStream (MESSAGE_DEBUG) << "readed from port '" << tmp_b << "'" << sendLog;
				delete []tmp_b;
			}
			return rlen;
		}
		rlen += ret;
	}
	char *tmp_b = new char[rlen + 1];
	memcpy (tmp_b, rbuf, rlen);
	tmp_b[rlen] = '\0';
	logStream (MESSAGE_ERROR) << "did not find end char '" << endChar
		<< "', readed '" << tmp_b << "'" << sendLog;
	delete []tmp_b;
	return -1;
}


int
Rts2ConnSerial::writeRead (const char* wbuf, int wlen, char *rbuf, int rlen)
{
	int ret;
	ret = writePort (wbuf, wlen);
	if (ret < 0)
		return -1;
	return readPort (rbuf, rlen);
}


int
Rts2ConnSerial::writeRead (const char* wbuf, int wlen, char *rbuf, int rlen, char endChar)
{
	int ret;
	ret = writePort (wbuf, wlen);
	if (ret < 0)
		return -1;
	return readPort (rbuf, rlen, endChar);
}


int
Rts2ConnSerial::flushPortIO ()
{
	return tcflush (sock, TCIOFLUSH);
}
