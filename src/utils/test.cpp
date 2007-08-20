/*! @file Test file
* $Id$
* @author petr
*/
#include <iostream>
#include <assert.h>
#include <errno.h>
#include <libnova/libnova.h>

#include "mkpath.h"

#include "rts2app.h"
#include "rts2config.h"
#include "rts2expander.h"

class Rts2TestApp:public Rts2App
{
public:
  Rts2TestApp (int in_argc, char **in_argv):Rts2App (in_argc, in_argv)
  {
  }
  virtual int run ();
};

int
Rts2TestApp::run ()
{
  double value;
  int i_value;

  std::string buf;

  i_value = init ();
  if (i_value)
    return i_value;

  assert (mkpath ("test/test1/test2/test3/", 0777) == -1);
  assert (mkpath ("aa/bb/cc/dd", 0777) == 0);

  Rts2Config *conf = Rts2Config::instance ();

  std::cout << "ret " << conf->loadFile ("test.ini") << std::endl;

  struct ln_lnlat_posn *observer;
  observer = conf->getObserver ();
  std::cout << "observatory long " << observer->lng << std::endl;
  std::cout << "observatory lat " << observer->lat << std::endl;
  std::cout << "C0 rotang: " << conf->getDouble ("C0", "rotang", value);
  std::cout << " val " << value << std::endl;
  std::cout << "C1 name ret: " << conf->getString ("C1", "name", buf);
  std::cout << " val " << buf << std::endl;
  std::cout << "centrald day_horizont ret: " << conf->getDouble ("centrald",
								 "day_horizont",
								 value);
  std::cout << " val " << value << std::endl;
  std::cout << "centrald night_horizont ret: " << conf->getDouble ("centrald",
								   "night_horizont",
								   value);
  std::cout << " val " << value << std::endl;
  std::cout << "hete dark_frequency ret: " << conf->getInteger ("hete",
								"dark_frequency",
								i_value);
  std::cout << " val " << i_value << std::endl;

  std::cout << "CNF1 script ret: " << conf->getString ("CNF1", "script", buf);
  std::cout << " val " << buf << std::endl;
  std::cout << "horizon ret: " << conf->getString ("observatory", "horizont",
						   buf) << std::endl;

  std::cout << std::
    endl <<
    "************************ CONFIG FILE test.ini DUMP **********************"
    << std::endl;

  // step throught sections..
  for (Rts2Config::iterator iter = conf->begin (); iter != conf->end ();
       iter++)
    {
      Rts2ConfigSection *sect = *iter;
      std::cout << std::endl << "[" << sect->getName () << "]" << std::endl;
      for (Rts2ConfigSection::iterator viter = sect->begin ();
	   viter != sect->end (); viter++)
	std::cout << *viter << std::endl;
    }

  // now do test expansions..
  Rts2Expander *exp = new Rts2Expander ();
  std::cout << "%Z%D:%y-%m-%dT%H:%M:%S:%s:%u: " << exp->
    expand ("%Z%D:%y-%m-%dT%H:%M:%S:%s:%u") << std::endl;
  delete exp;

  delete conf;

  return 0;
}

int
main (int argc, char **argv)
{
  Rts2TestApp app (argc, argv);
  return app.run ();
}
