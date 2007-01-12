/*! 
 * @file Dome control deamon for Dome Control Modules - BOOTESes
 *
 * $Id$
 *
 * Packtets are separated by space; they are in ascii, and ends with 0
 * followed by checksum calculated using function ..
 *
 * <dome_name> YYYY-MM-DD hh:mm:ssZ <temperature> <humidity> <rain> <switch1 - o|c> <switch2 .. 4>
 * <status>
 *
 * where:
 * <dome_name> is name of dome (1A or 1B)
 * YYYY-MM-DD hh:mm:ssZ is UTC date of message
 * temperature is float number in oC
 * humidity is float 0-100
 * rain is 1 when it's raining, otherwise 0
 * switch1-n are switch position - o for open (not switched), c for
 * closed (switched)
 * status is ok or error. Ok when all sensors are ready
 *
 * Example packet:
 *
 * 1A 2005-07-21 23:56:56 -10.67 98.9 0 c c o o ok
 *
 * @author petr
 */

#include <fcntl.h>
#include <errno.h>
#include "dome.h"
#include "udpweather.h"

#define DCM_WEATHER_TIMEOUT	40

class Rts2DevDomeDcm;

class Rts2ConnDcm:public Rts2ConnFramWeather
{
private:
  Rts2DevDomeDcm * master;
public:
  Rts2ConnDcm (int in_weather_port, Rts2DevDomeDcm * in_master);
    virtual ~ Rts2ConnDcm (void);
  virtual int receive (fd_set * set);
};

class Rts2DevDomeDcm:public Rts2DevDome
{
private:
  Rts2ConnFramWeather * weatherConn;
  int dcm_weather_port;

protected:
    virtual int processOption (int in_opt);
public:
    Rts2DevDomeDcm (int argc, char **argv);
    virtual ~ Rts2DevDomeDcm (void);
  virtual int init ();

  virtual int ready ();
  virtual int baseInfo ();
};

Rts2ConnDcm::Rts2ConnDcm (int in_weather_port, Rts2DevDomeDcm * in_master):
Rts2ConnFramWeather (in_weather_port, DCM_WEATHER_TIMEOUT, in_master)
{
  master = in_master;
}

Rts2ConnDcm::~Rts2ConnDcm (void)
{
}

int
Rts2ConnDcm::receive (fd_set * set)
{
  int ret;
  char Wbuf[100];
  char Wstatus[10];
  char dome_name[10];
  int data_size = 0;
  struct tm statDate;
  float temp;
  float humidity;

  int sw1, sw2, sw3, sw4;

  float sec_f;
  if (sock >= 0 && FD_ISSET (sock, set))
    {
      struct sockaddr_in from;
      socklen_t size = sizeof (from);
      data_size =
	recvfrom (sock, Wbuf, 80, 0, (struct sockaddr *) &from, &size);
      if (data_size < 0)
	{
	  logStream (MESSAGE_DEBUG) << "error in receiving weather data: " <<
	    strerror (errno) << sendLog;
	  return 1;
	}
      Wbuf[data_size] = 0;
#ifdef DEBUG_ALL
      logStream (MESSAGE_DEBUG) << "readed: " << data_size << " " << Wbuf <<
	" from " << inet_ntoa (from.sin_addr) << " " << ntohs (from.
							       sin_port) <<
	sendLog;
#endif
      // parse weather info
      // * 1A 2005-07-21 23:56:56 -10.67 98.9 0 c c o o ok
      ret =
	sscanf (Wbuf,
		"%s %i-%u-%u %u:%u:%f %f %f %i %i %i %i %i %s",
		dome_name, &statDate.tm_year, &statDate.tm_mon,
		&statDate.tm_mday, &statDate.tm_hour, &statDate.tm_min,
		&sec_f, &temp, &humidity, &rain, &sw1, &sw2, &sw3, &sw4,
		Wstatus);
      if (ret != 15)
	{
	  logStream (MESSAGE_ERROR) << "sscanf on udp data returned: " << ret
	    << " ( " << Wbuf << " )" << sendLog;
	  rain = 1;
	  setWeatherTimeout (FRAM_CONN_TIMEOUT);
	  return data_size;
	}
      statDate.tm_isdst = 0;
      statDate.tm_year -= 1900;
      statDate.tm_mon--;
      statDate.tm_sec = (int) sec_f;
      lastWeatherStatus = mktime (&statDate);
      if (strcmp (Wstatus, "ok"))
	{
	  // if sensors doesn't work, switch rain on
	  rain = 1;
	}
      // log only rain messages..they are interesting
      if (rain)
	logStream (MESSAGE_DEBUG) << "rain: " << rain << "  date: " <<
	  lastWeatherStatus << " status: " << Wstatus << sendLog;
      master->setTemperature (temp);
      master->setHumidity (humidity);
      master->setRain (rain);
      master->setSwState ((sw1 << 3) | (sw2 << 2) | (sw3 << 1) | (sw4));
      if (sw1 && sw2)
	{
	  master->setMasterOn ();
	}
      else if (!sw1 && !sw2 && sw3 && sw4)
	{
	  master->setMasterStandby ();
	}
    }
  return data_size;
}

Rts2DevDomeDcm::Rts2DevDomeDcm (int in_argc, char **in_argv):
Rts2DevDome (in_argc, in_argv)
{
  weatherConn = NULL;
  addOption ('W', "dcm_weather", 1,
	     "UPD port number of packets from DCM (default to 4998)");
  dcm_weather_port = 4998;
}

Rts2DevDomeDcm::~Rts2DevDomeDcm (void)
{
}

int
Rts2DevDomeDcm::processOption (int in_opt)
{
  switch (in_opt)
    {
    case 'W':
      dcm_weather_port = atoi (optarg);
      break;
    default:
      return Rts2DevDome::processOption (in_opt);
    }
  return 0;
}

int
Rts2DevDomeDcm::init ()
{
  int ret;

  ret = Rts2DevDome::init ();
  if (ret)
    return ret;

  weatherConn = new Rts2ConnDcm (dcm_weather_port, this);
  weatherConn->init ();

  addConnection (weatherConn);

  return 0;
}

int
Rts2DevDomeDcm::ready ()
{
  return 0;
}

int
Rts2DevDomeDcm::baseInfo ()
{
  return 0;
}

int
main (int argc, char **argv)
{
  Rts2DevDomeDcm *device = new Rts2DevDomeDcm (argc, argv);
  int ret = device->run ();
  delete device;
  return ret;
}
