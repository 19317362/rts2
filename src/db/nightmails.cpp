#include "imgdisplay.h"
#include "../utils/rts2config.h"
#include "../utilsdb/rts2appdb.h"
#include "../utilsdb/rts2obsset.h"
#include "../utilsdb/target.h"

#include "imgdisplay.h"

#include <time.h>
#include <iostream>

class Rts2NightMail:public Rts2AppDb
{
private:
  time_t t_from, t_to;
  struct tm *tm_night;
  int printImages;
  int printCounts;
public:
    Rts2NightMail (int argc, char **argv);
    virtual ~ Rts2NightMail (void);

  virtual int processOption (int in_opt);
  virtual int init ();
  virtual int run ();
};

Rts2NightMail::Rts2NightMail (int in_argc, char **in_argv):
Rts2AppDb (in_argc, in_argv)
{
  time (&t_to);
  // 24 hours before..
  t_from = t_to - 86400;
  tm_night = NULL;
  printImages = 0;
  printCounts = 0;
  addOption ('f', "from", 1,
	     "date from which take measurements; default to current date - 24 hours");
  addOption ('t', "to", 1,
	     "date to which show measurements; default to from + 24 hours");
  addOption ('n', "night-date", 1, "report for night around given date");
  addOption ('i', "images", 0, "print image listing");
  addOption ('I', "images_summary", 0, "print image summary row");
  addOption ('p', "photometer", 0, "print counts listing");
  addOption ('P', "photometer_summary", 0, "print counts summary row");
  addOption ('s', "statistics", 0, "print night statistics");
}

Rts2NightMail::~Rts2NightMail (void)
{
  if (tm_night)
    delete tm_night;
}

int
Rts2NightMail::processOption (int in_opt)
{
  int ret;
  struct tm tm_ret;
  switch (in_opt)
    {
    case 'f':
      ret = Rts2App::parseDate (optarg, &tm_ret);
      if (ret)
	return ret;
      t_from = mktime (&tm_ret);
      break;
    case 't':
      ret = Rts2App::parseDate (optarg, &tm_ret);
      if (ret)
	return ret;
      t_to = mktime (&tm_ret);
      break;
    case 'n':
      tm_night = new struct tm;
      ret = Rts2App::parseDate (optarg, tm_night);
      if (ret)
	return ret;
      break;
    case 'i':
      printImages |= DISPLAY_ALL;
      break;
    case 'I':
      printImages |= DISPLAY_SUMMARY;
      break;
    case 'p':
      printCounts |= DISPLAY_ALL;
      break;
    case 'P':
      printCounts |= DISPLAY_SUMMARY;
      break;
    default:
      return Rts2AppDb::processOption (in_opt);
    }
  return 0;
}

int
Rts2NightMail::init ()
{
  int ret;
  ret = Rts2AppDb::init ();
  if (ret)
    return ret;

  if (tm_night)
    {
      // let's calculate time from..t_from will contains start of night
      // local 12:00 will be at ~ give time..
      tm_night->tm_hour =
	(int) ln_range_degrees (Rts2Config::instance ()->getObserver ()->lng /
				15);
      if (tm_night->tm_hour > 12)
	tm_night->tm_hour = 24 - tm_night->tm_hour;
      tm_night->tm_min = tm_night->tm_sec = 0;
      t_from = mktime (tm_night);
      if (t_from + 86400 > t_to)
	t_from -= 86400;
      t_to = t_from + 86400;
      if (t_from > t_to)
	t_from -= 86400;
    }
  return 0;
}

int
Rts2NightMail::run ()
{
  Rts2Config *config;
  config = Rts2Config::instance ();
  // email all possible targets
  sendEndMails (&t_from, &t_to, printImages, printCounts,
		config->getObserver ());
  return 0;
}

Rts2NightMail *app;


int
main (int argc, char **argv)
{
  int ret;
  app = new Rts2NightMail (argc, argv);
  ret = app->init ();
  if (ret)
    {
      std::cerr << "Cannot init app" << std::endl;
      return 1;
    }
  ret = app->run ();
  delete app;
  return ret;
}
