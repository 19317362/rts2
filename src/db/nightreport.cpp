/* 
 * Night reporting utility.
 * Copyright (C) 2005-2008 Petr Kubanek <petr@kubanek.net>
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

#include "imgdisplay.h"
#include "../utils/rts2config.h"
#include "../utils/libnova_cpp.h"
#include "../utils/rts2format.h"
#include "../utilsdb/rts2appdb.h"
#include "../utilsdb/observationset.h"

#include "imgdisplay.h"

#include <time.h>
#include <iostream>
#include <iomanip>

#define STAT_NONE            0
#define STAT_COLLOCATE       1
#define STAT_SUPER           2

/**
 * Produces night report output.
 *
 * @addgroup RTS2DbApps
 */
class Rts2NightReport:public Rts2AppDb
{
	private:
		time_t t_from, t_to;
		struct ln_date *tm_night;
		int printImages;
		int printCounts;
		bool printStat;
		int statType;

		long totalImages;
		long totalGoodImages;
		long totalObs;

		int observationsNights;

		void printObsList ();
		void printStatistics ();
		void printFromTo (time_t *t_start, time_t *t_end, bool printEmpty);
		rts2db::ObservationSet *obs_set;

		std::vector <rts2db::ObservationSet *> allObs;

	protected:
		virtual void usage ();

		virtual int processOption (int in_opt);
		virtual int init ();


		virtual int doProcessing ();
	public:
		Rts2NightReport (int argc, char **argv);
		virtual ~ Rts2NightReport (void);
};

Rts2NightReport::Rts2NightReport (int in_argc, char **in_argv):
Rts2AppDb (in_argc, in_argv)
{
	t_from = 0;
	t_to = 0;
	tm_night = NULL;
	obs_set = NULL;
	printImages = 0;
	printCounts = 0;
	printStat = false;
	statType = STAT_NONE;

	observationsNights = 0;

	totalImages = 0;
	totalGoodImages = 0;
	totalObs = 0;

	addOption ('f', NULL, 1, "date from which take measurements; default to current date - 24 hours. Date is in YYYY-MM-DD format.");
	addOption ('t', NULL, 1, "date to which show measurements; default to from + 24 hours. Date is in YYYY-MM-DD format.");
	addOption ('n', NULL, 1, "report for night around given date. Date format is YYYY-MM-DD.");
	addOption ('N', NULL, 0, "prints out all values as numbers, do not pretty format them");
	addOption ('l', NULL, 0, "print full image names");
	addOption ('i', NULL, 0, "print image listing");
	addOption ('I', NULL, 0, "print image summary row");
	addOption ('p', NULL, 2, "print counts listing; can be followed by format, txt for plain");
	addOption ('P', NULL, 0, "print counts summary row");
	addOption ('s', NULL, 0, "print night statistics");
	addOption ('S', NULL, 0, "print night statistics - % of time used for observation");
	addOption ('c', NULL, 0, "collocate statistics by nights, targets, ..");
}


Rts2NightReport::~Rts2NightReport (void)
{
	delete tm_night;
	for (std::vector <rts2db::ObservationSet *>::iterator iter = allObs.begin (); iter != allObs.end (); iter++)
	{
		delete (*iter);
	}
	allObs.clear ();
}


void
Rts2NightReport::usage ()
{
	std::cout << "\t" << getAppName () << "-n 2007-12-31" << std::endl
		<< "\t" << getAppName () << "-f 2007-12-15 -t 2007-12-18" << std::endl;
}

int
Rts2NightReport::processOption (int in_opt)
{
	switch (in_opt)
	{
		case 'f':
			return parseDate (optarg, &t_from);
		case 't':
			return parseDate (optarg, &t_to);
			break;
		case 'n':
			tm_night = new struct ln_date;
			return Rts2CliApp::parseDate (optarg, tm_night);
		case 'N':
			std::cout << pureNumbers;	
			break;
		case 'l':
			printImages |= DISPLAY_FILENAME;
			break;
		case 'i':
			printImages |= DISPLAY_ALL;
			break;
		case 'I':
			printImages |= DISPLAY_SUMMARY;
			break;
		case 'p':
			if (optarg)
			{
				if (!strcmp (optarg, "txt"))
					printCounts = DISPLAY_SHORT;
				else
					printCounts |= DISPLAY_ALL;
			}
			else
			{
				printCounts |= DISPLAY_ALL;
			}
			break;
		case 'P':
			printCounts |= DISPLAY_SUMMARY;
			break;
		case 's':
			printStat = true;
			break;
		case 'S':
			statType = STAT_SUPER;
			break;
		case 'c':
			statType = STAT_COLLOCATE;
			break;
		default:
			return Rts2AppDb::processOption (in_opt);
	}
	return 0;
}


int
Rts2NightReport::init ()
{
	int ret;
	ret = Rts2AppDb::init ();
	if (ret)
		return ret;

	if (t_from == 0)
		t_from = Rts2Config::instance ()->getNight ();
	if (t_to == 0)
	  	t_to = Rts2Config::instance ()->getNight () + 86400;

	if (tm_night)
	{
		// let's calculate time from..t_from will contains start of night
		// local 12:00 will be at ~ give time..
		Rts2Night night =
			Rts2Night (tm_night, Rts2Config::instance ()->getObserver ());
		t_from = *(night.getFrom ());
		t_to = *(night.getTo ());
	}
	return 0;
}


void
Rts2NightReport::printObsList ()
{
	if (printImages)
		obs_set->printImages (printImages);
	if (printCounts)
		obs_set->printCounts (printCounts);
	if (statType == STAT_COLLOCATE)
		obs_set->collocate ();
	std::cout << *obs_set;
}


void
Rts2NightReport::printStatistics ()
{
	obs_set->printStatistics (std::cout);
}


void
Rts2NightReport::printFromTo (time_t *t_start, time_t * t_end, bool printEmpty)
{
	obs_set = new rts2db::ObservationSet (t_start, t_end);

	if (!printEmpty && obs_set->empty ())
	{
		delete obs_set;
		return;
	}

	if (!obs_set->empty ())		 // used nights..
		observationsNights++;

	if (statType == STAT_SUPER)
	{
		obs_set->computeStatistics ();
		totalImages += obs_set->getNumberOfImages ();
		totalGoodImages += obs_set->getNumberOfGoodImages ();
		totalObs += obs_set->size ();
		allObs.push_back (obs_set);
	}
	else
	{
		// from which date to which..
		std::cout << "From " << Timestamp (*t_start) << " to " << Timestamp (*t_end) << std::endl;

		printObsList ();

		if (printStat)
			printStatistics ();
		delete obs_set;
	}
}


int
Rts2NightReport::doProcessing ()
{
	//  char *whereStr;
	Rts2Config *config;
	//  Rts2SqlColumnObsState *obsState;
	config = Rts2Config::instance ();

	int totalNights = 0;

	if (statType != STAT_NONE && t_to - t_from > 86400)
	{
		time_t t_start = t_from;
		// iterate by nights
		for (time_t t_end = t_from + 86400; t_end <= t_to; t_start = t_end, t_end += 86400)
		{
			totalNights++;
			printFromTo (&t_start, &t_end, false);
		}
	}
	else
	{
		totalNights++;
		printFromTo (&t_from, &t_to, true);
	}

	// print number of nights
	if (statType == STAT_SUPER)
	{
		if (observationsNights == 0)
		{
			std::cerr << "Cannot find any observations from " << Timestamp (t_from)
				<< " to " << Timestamp (t_to) << std::endl;
			return -1;
		}
		std::cout << "From " << Timestamp (t_from) << " to " << Timestamp (t_to) << std::endl
			<< "Number of nights " << totalNights << " observations nights " << observationsNights
			<< " (" << Percentage (observationsNights, totalNights) << "%)" << std::endl;
		if (totalImages == 0)
		{
			std::cerr << "Cannot find any images from " << Timestamp (t_from)
				<< " to " << Timestamp (t_to) << std::endl;
		}
		else
		{
			std::cout << "Images " << totalImages << " good images " << totalGoodImages
				<< " (" << Percentage (totalGoodImages, totalImages) << "%)" << std::endl;

		}

		time_t t_start = t_from;

		std::cout << " Date                          Images      % of good" << std::endl
			<< "                    Observations   Good Images" << std::endl;

		for (std::vector <rts2db::ObservationSet *>::iterator iter = allObs.begin (); iter != allObs.end (); iter++)
		{
			std::cout << SEP << Timestamp (t_start)
				<< SEP << std::setw (5) << (*iter)->size ()
				<< SEP << std::setw (5) << (*iter)->getNumberOfImages ()
				<< SEP << std::setw (5) << (*iter)->getNumberOfGoodImages ()
				<< SEP << Percentage ((*iter)->getNumberOfGoodImages (), (*iter)->getNumberOfImages ())
				<< std::endl;
			t_start += 86400;
		}
	}

	return 0;
}


int
main (int argc, char **argv)
{
	Rts2NightReport app = Rts2NightReport (argc, argv);
	return app.run ();
}
