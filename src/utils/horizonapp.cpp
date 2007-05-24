#include "rts2cliapp.h"
#include "rts2config.h"

#include <iostream>

#define OP_DUMP             0x01

/**
 * Class which will plot horizon from horizon file, possibly with
 * commands for GNUPlot.
 *
 * (C) 2007 Petr Kubanek <petr@kubanek.net>
 */

class HorizonApp:public Rts2CliApp
{
private:
  char *configFile;
  char *horizonFile;

  int op;

protected:
    virtual int processOption (int in_arg);

  virtual int init ();
public:
    HorizonApp (int in_argc, char **in_argv);

  virtual int doProcessing ();
};

HorizonApp::HorizonApp (int in_argc, char **in_argv):
Rts2CliApp (in_argc, in_argv)
{
  op = 0;
  configFile = NULL;
  horizonFile = NULL;

  addOption ('c', NULL, 1, "configuration file");
  addOption ('f', "horizon", 1,
	     "horizon file; overwrites file specified in configuration file");
  addOption ('d', "dump", 0, "dump horizon file in AZ-ALT format");
}

int
HorizonApp::processOption (int in_opt)
{
  switch (in_opt)
    {
    case 'c':
      configFile = optarg;
      break;
    case 'f':
      horizonFile = optarg;
      break;
    case 'd':
      op = OP_DUMP;
      break;
    default:
      return Rts2CliApp::processOption (in_opt);
    }
  return 0;
}

int
HorizonApp::init ()
{
  int ret;

  ret = Rts2CliApp::init ();
  if (ret)
    return ret;

  ret = Rts2Config::instance ()->loadFile (configFile);
  if (ret)
    return ret;

  return 0;
}

int
HorizonApp::doProcessing ()
{
  struct ln_hrz_posn hrz;
  ObjectCheck *checker;

  hrz.alt = 0;

  if (!horizonFile)
    checker = Rts2Config::instance ()->getObjectChecker ();
  else
    checker = new ObjectCheck (horizonFile);

  if (op & OP_DUMP)
    {
      std::cout << "AZ-ALT" << std::endl;

      for (horizon_t::iterator iter = checker->begin ();
	   iter != checker->end (); iter++)
	{
	  std::cout << (*iter).hrz.az << " " << (*iter).hrz.alt << std::endl;
	}
    }
  else
    {
      std::cout
	<< "set terminal x11 persist" << std::endl
	<< " set xrange [0:360]" << std::endl;

      std::
	cout << "plot \"-\" u 1:2 smooth csplines lt 2 lw 2 t \"Horizon\"" <<
	std::endl;

      for (hrz.az = 0; hrz.az <= 360; hrz.az += 0.1)
	{
	  std::cout << hrz.az << " " << checker->getHorizonHeight (&hrz,
								   0) << std::
	    endl;
	}
      std::cout << "e" << std::endl;
    }
  return 0;
}

int
main (int argc, char **argv)
{
  HorizonApp app = HorizonApp (argc, argv);
  return app.run ();
}
