/* 
 * Constraints.
 * Copyright (C) 2003-2007 Petr Kubanek <petr@kubanek.net>
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

#include "constraints.h"
#include "../utils/utilsfunc.h"
#include "../utils/rts2config.h"

bool between (double val, double low, double upper)
{
	if (isnan (val))
		return false;
	if (isnan (low) && isnan (upper))
		return true;
	if (isnan (low))
		return val < upper;
	if (isnan (upper))
		return val >= low;
	return val >= low && val < upper;
}

using namespace rts2db;

bool ConstraintDoubleInterval::satisfy (double val)
{
	return between (val, lower, upper);
}

void ConstraintDoubleInterval::print (std::ostream &os)
{
	os << "    <interval>";
	if (!isnan (lower))
		os << std::endl << "      <lower>" << lower << "</lower>";
	if (!isnan (upper))
		os << std::endl << "      <upper>" << upper << "</upper>";
	os << std::endl << "    </interval>" << std::endl;
}

ConstraintInterval::ConstraintInterval (ConstraintInterval &cons)
{
	for (std::list <ConstraintDoubleInterval>::iterator iter = cons.intervals.begin (); iter != cons.intervals.end (); iter++)
		add (*iter);
}

void ConstraintInterval::load (xmlNodePtr cons)
{
	intervals.clear ();
	for (xmlNodePtr inter = cons->children; inter != NULL; inter = inter->next)
	{
		if (xmlStrEqual (inter->name, (xmlChar *) "interval"))
		{
			double lower = rts2_nan ("f");
			double upper = rts2_nan ("f");

			for (xmlNodePtr par = inter->children; par != NULL; par = par->next)
			{
				if (xmlStrEqual (par->name, (xmlChar *) "lower"))
					lower = atof ((const char *) par->children->content);
				else if (xmlStrEqual (par->name, (xmlChar *) "upper"))
					upper = atof ((const char *) par->children->content);
				else if (par->type == XML_COMMENT_NODE)
				  	continue;
				else
					throw XmlUnexpectedNode (par);
			}
			addInterval (lower, upper);
		}
		else if (inter->type == XML_COMMENT_NODE)
		{
			continue;
		}
		else
		{
			throw XmlUnexpectedNode (inter);
		}
	}
}

void ConstraintInterval::parse (const char *arg)
{
	double lower = rts2_nan ("f");
	double upper = rts2_nan ("f");

	char *sint = new char [strlen (arg) + 1];
	strcpy (sint, arg);

	char *cp = strchr (sint, ':');
	if (cp == NULL)
		throw rts2core::Error ((std::string ("the interval must contain a single : - cannot find : in interval ") + arg).c_str ());

	char *endp;
	*cp = '\0';

	if (cp != sint)
	{
		lower = strtof (sint, &endp);
		if (*endp != 0)
			throw rts2core::Error ((std::string ("cannot parse lower intrval - ") + arg).c_str ());
	}
	
	if (*(cp + 1) != '\0')
	{
		upper = strtof (cp + 1, &endp);
		if (*endp != 0)
			throw rts2core::Error ((std::string ("cannot find : in interval ") + (cp + 1)).c_str ());
	}

	addInterval (lower, upper);

	delete[] sint;
}

void ConstraintInterval::print (std::ostream &os)
{
	os << "  <" << getName () << ">" << std::endl;
	for (std::list <ConstraintDoubleInterval>::iterator iter = intervals.begin (); iter != intervals.end (); iter++)
	{
		iter->print (os);
	}
	os << "  </" << getName () << ">" << std::endl;
}

bool ConstraintInterval::isBetween (double val)
{
	for (std::list <ConstraintDoubleInterval>::iterator iter = intervals.begin (); iter != intervals.end (); iter++)
	{
		if (iter->satisfy (val))
			return true;
	}
	return false;
}

void Constraint::getViolatedIntervals (Target *tar, time_t from, time_t to, int step, interval_arr_t &ret)
{
	double vf = rts2_nan ("f");

	double to_JD = ln_get_julian_from_timet (&to);

	double t;
	for (t = ln_get_julian_from_timet (&from); t < to_JD; t += step / 86400.0)
	{
		if (!satisfy (tar, t))
		{
			if (isnan (vf))
				vf = t;
		}
		else if (!isnan (vf))
		{
			ln_get_timet_from_julian (vf, &from);
			ln_get_timet_from_julian (t, &to);
			ret.push_back (std::pair <time_t, time_t> (from, to));
			vf = rts2_nan ("f");
		}
	}
	if (!isnan (vf))
	{
		ln_get_timet_from_julian (vf, &from);
		ln_get_timet_from_julian (t, &to);
		ret.push_back (std::pair <time_t, time_t> (from, to));
	}
}

void ConstraintTime::load (xmlNodePtr cons)
{
	clearIntervals ();
	for (xmlNodePtr inter = cons->children; inter != NULL; inter = inter->next)
	{
		if (xmlStrEqual (inter->name, (xmlChar *) "interval"))
		{
			double from = rts2_nan ("f");
			double to = rts2_nan ("f");

			for (xmlNodePtr par = inter->children; par != NULL; par = par->next)
			{
				if (xmlStrEqual (par->name, (xmlChar *) "from"))
					parseDate ((const char *) par->children->content, from);
				else if (xmlStrEqual (par->name, (xmlChar *) "to"))
					parseDate ((const char *) par->children->content, to);
				else if (par->type == XML_COMMENT_NODE)
					continue;
				else
					throw XmlUnexpectedNode (par);
			}
			addInterval (from, to);
		}
		else if (inter->type == XML_COMMENT_NODE)
		{
			continue;
		}
		else
		{
			throw XmlUnexpectedNode (inter);
		}
	}
}

bool ConstraintTime::satisfy (Target *target, double JD)
{
	return isBetween (JD);
}

bool ConstraintAirmass::satisfy (Target *tar, double JD)
{
	double am = tar->getAirmass (JD);
	if (isnan (am))
		return true;
	return isBetween (am);
}

bool ConstraintZenithDistance::satisfy (Target *tar, double JD)
{
	double zd = tar->getZenitDistance (JD);
	if (isnan (zd))
		return true;
	return isBetween(zd);
}

bool ConstraintHA::satisfy (Target *tar, double JD)
{
	double ha = tar->getHourAngle (JD);
	if (isnan (ha))
		return true;
	return isBetween (ha);
}

bool ConstraintLunarDistance::satisfy (Target *tar, double JD)
{
	double ld = tar->getLunarDistance (JD);
	if (isnan (ld))
		return true;
	return isBetween (ld);
}

bool ConstraintLunarAltitude::satisfy (Target *tar, double JD)
{
	struct ln_equ_posn eq_lun;
	struct ln_hrz_posn hrz_lun;
	ln_get_lunar_equ_coords (JD, &eq_lun);
	ln_get_hrz_from_equ (&eq_lun, Rts2Config::instance ()->getObserver (), JD, &hrz_lun);
	return isBetween (hrz_lun.alt);
}

bool ConstraintLunarPhase::satisfy (Target *tar, double JD)
{
	return isBetween (ln_get_lunar_phase (JD));
}

bool ConstraintSolarDistance::satisfy (Target *tar, double JD)
{
	double sd = tar->getSolarDistance (JD);
	if (isnan (sd))
		return true;
	return isBetween (sd);
}

bool ConstraintSunAltitude::satisfy (Target *tar, double JD)
{
	struct ln_equ_posn eq_sun;
	struct ln_hrz_posn hrz_sun;
	ln_get_solar_equ_coords (JD, &eq_sun);
	ln_get_hrz_from_equ (&eq_sun, Rts2Config::instance ()->getObserver (), JD, &hrz_sun);
	return isBetween (hrz_sun.alt);
}

void ConstraintMaxRepeat::load (xmlNodePtr cons)
{
	if (!cons->children || !cons->children->content)
		throw XmlUnexpectedNode (cons);
	maxRepeat = atoi ((const char *) cons->children->content);
}

bool ConstraintMaxRepeat::satisfy (Target *tar, double JD)
{
	if (maxRepeat > 0)
		return tar->getTotalNumberOfObservations () < maxRepeat;
	return true;
}

void ConstraintMaxRepeat::parse (const char *arg)
{
	maxRepeat = atoi (arg);
}

void ConstraintMaxRepeat::print (std::ostream &os)
{
	os << "  <" << getName () << ">" << maxRepeat << "</" << getName () << ">" << std::endl;
}

void ConstraintMaxRepeat::getViolatedIntervals (Target *tar, time_t from, time_t to, int step, interval_arr_t &ret)
{
	if (maxRepeat > 0 && tar->getTotalNumberOfObservations () > maxRepeat)
		ret.push_back ( std::pair <time_t, time_t> (from, to) );
}


Constraints::Constraints (Constraints &cs): std::map <std::string, ConstraintPtr > (cs)
{
	for (Constraints::iterator iter = cs.begin (); iter != cs.end (); iter++)
	{
		Constraint *con = createConstraint (iter->first.c_str ());
		// can "compare" chars, as they are const char*
		if (con->getName () == CONSTRAINT_TIME
			|| con->getName () == CONSTRAINT_AIRMASS
			|| con->getName () == CONSTRAINT_ZENITH_DIST
			|| con->getName () == CONSTRAINT_HA
			|| con->getName () == CONSTRAINT_LDISTANCE
			|| con->getName () == CONSTRAINT_LALTITUDE
			|| con->getName () == CONSTRAINT_LPHASE
			|| con->getName () == CONSTRAINT_SDISTANCE
			|| con->getName () == CONSTRAINT_SALTITUDE)
		{
			((ConstraintInterval *) con)->copyIntervals (((ConstraintInterval *) iter->second->th ()));
		}
		else if (con->getName () == CONSTRAINT_MAXREPEATS)
		{
			((ConstraintMaxRepeat *) con)->copyConstraint (((ConstraintMaxRepeat *) iter->second->th ()));
		}
		else
		{
			std::cerr << "unsuported constraint type in copy constructor " __FILE__ ":" << __LINE__ << std::endl;
			exit (10);
		}

		(*this)[iter->first] = ConstraintPtr (con);
	}
}

Constraints::~Constraints ()
{
	for (Constraints::iterator iter = begin (); iter != end (); iter++)
		iter->second.null ();
	clear ();
}

bool Constraints::satisfy (Target *tar, double JD)
{
	for (Constraints::iterator iter = begin (); iter != end (); iter++)
	{
		if (!(iter->second->satisfy (tar, JD)))
			return false;
	}
	return true;
}

size_t Constraints::getViolated (Target *tar, double JD, ConstraintsList &violated)
{
	for (Constraints::iterator iter = begin (); iter != end (); iter++)
	{
		if (!(iter->second->satisfy (tar, JD)))
			violated.push_back (iter->second);
	}
	return violated.size ();
}

size_t Constraints::getSatisfied (Target *tar, double JD, ConstraintsList &satisfied)
{
	for (Constraints::iterator iter = begin (); iter != end (); iter++)
	{
		if (iter->second->satisfy (tar, JD))
			satisfied.push_back (iter->second);
	}
	return satisfied.size ();
}

// find first satisifing interval ending after first violation..
void findFirst (interval_arr_t::iterator si, const interval_arr_t::iterator &end, double t)
{
	while (si->second > t && si != end)
		si++;
}

void Constraints::getSatisfiedIntervals (Target *tar, time_t from, time_t to, int length, int step, interval_arr_t &satisfiedIntervals)
{
	satisfiedIntervals.clear ();
	// at beginning, full interval is satisfied
	satisfiedIntervals.push_back (std::pair <time_t, time_t> (from, to));
	for (Constraints::iterator iter = begin (); iter != end (); iter++)
	{
		interval_arr_t intervals;
		iter->second->getViolatedIntervals (tar, from, to, step, intervals);
		// now look for join with current intervals..
		interval_arr_t::iterator si = satisfiedIntervals.begin ();
		for (interval_arr_t::iterator vi = intervals.begin (); vi != intervals.end (); vi++)
		{
			findFirst (si, satisfiedIntervals.end (), vi->first);
			if (si == satisfiedIntervals.end ())
				break;
			si->second = vi->first;
			// remove empty interval
			if (si->second <= si->first || (si->first >= vi->first && si->second <= vi->second) )
				si = satisfiedIntervals.erase (si);
			else	
				si++;
			if (si == satisfiedIntervals.end ())
			{
				for (interval_arr_t::iterator vi2 = vi + 1; vi2 != intervals.end (); vi++, vi2++)
					satisfiedIntervals.push_back (std::pair <time_t, time_t> (vi->second, vi2->first));
				if (vi->second < to)
					satisfiedIntervals.push_back (std::pair <time_t, time_t> (vi->second, to));
				break;
			}
			// now found all intervals inside violation, and remove them..
			while (si->second < vi->second && si != satisfiedIntervals.end ())
				si = satisfiedIntervals.erase (si);
			if (si == satisfiedIntervals.end ())
				break;
			// si now holds something, which has end after end of violation, and begin before end of violation
			si->first = vi->second;
		}
	}
}

double Constraints::getSatisfiedDuration (Target *tar, double from, double to, double length, double step)
{
	time_t fti = (time_t) to;

	double to_JD = ln_get_julian_from_timet (&fti);
	fti = (time_t) (from + length);
	
	double t;
	for (t = ln_get_julian_from_timet (&fti); t < to_JD; t += step / 86400.0)
	{
		if (!satisfy (tar, t))
		{
			time_t ret;  
		  	ln_get_timet_from_julian (t, &ret);
			return ret;
		}
	}
	return rts2_nan ("f");
}

void Constraints::load (xmlNodePtr _node, bool overwrite)
{
	for (xmlNodePtr cons = _node->children; cons != NULL; cons = cons->next)
	{
		if (cons->type == XML_COMMENT_NODE)
			continue;
	  	ConstraintPtr con;
		Constraints::iterator candidate = find (std::string ((const char *) cons->name));
		// found existing constrain - if commanded to not overwrite, do not overwrite it
		if (candidate != end ())
		{
			if (overwrite == false)
				continue;  
			con = candidate->second;
		}
		else 
		{
			Constraint *cp = createConstraint ((const char *) cons->name);
			if (cp == NULL)
				throw XmlUnexpectedNode (cons);
			con = ConstraintPtr (cp);
		}
		try
		{
			con->load (cons);
		}
		catch (XmlError er)
		{
			con.null ();
			throw er;
		}
		if (candidate == end ())
		{
			(*this)[std::string (con->getName ())] = con;
		}
	}
}

void Constraints::load (const char *filename, bool overwrite)
{
	LIBXML_TEST_VERSION

	xmlLineNumbersDefault (1);
	xmlDoc *doc = xmlReadFile (filename, NULL, XML_PARSE_NOBLANKS | XML_PARSE_NOWARNING);
	if (doc == NULL)
		throw XmlError ("cannot parse constraint file " + std::string (filename));

	xmlNodePtr root_element = xmlDocGetRootElement (doc);
	if (!xmlStrEqual (root_element->name, (xmlChar *) "constraints"))
		throw XmlUnexpectedNode (root_element);

	load (root_element, overwrite);
	xmlFreeDoc (doc);
	xmlCleanupParser ();
}

void Constraints::parse (const char *name, const char *arg)
{
	Constraints::iterator iter = find(std::string (name));
	if (iter != end ())
	{
		iter->second->parse (arg);
	}
	else
	{
		Constraint *con = createConstraint (name);
		if (con == NULL)
			throw rts2core::Error ((std::string ("cannot allocate constraint with name ") + name).c_str ());
		con->parse (arg);
		(*this)[std::string (con->getName ())] = ConstraintPtr (con);
	}
}

void Constraints::print (std::ostream &os)
{
	os << "<constraints>" << std::endl;
	for (Constraints::iterator iter = begin (); iter != end (); iter++)
	{
		iter->second->print (os);
	}
	os << "</constraints>" << std::endl;
}

Constraint *Constraints::createConstraint (const char *name)
{
	if (!strcmp (name, CONSTRAINT_TIME))
		return new ConstraintTime ();
	else if (!strcmp (name, CONSTRAINT_AIRMASS))
		return new ConstraintAirmass ();
	else if (!strcmp (name, CONSTRAINT_HA))
		return new ConstraintHA ();
	else if (!strcmp (name, CONSTRAINT_LDISTANCE))
		return new ConstraintLunarDistance ();
	else if (!strcmp (name, CONSTRAINT_LALTITUDE))
		return new ConstraintLunarDistance ();
	else if (!strcmp (name, CONSTRAINT_LPHASE))
		return new ConstraintLunarPhase ();
	else if (!strcmp (name, CONSTRAINT_SDISTANCE))
		return new ConstraintSolarDistance ();
	else if (!strcmp (name, CONSTRAINT_SALTITUDE))
		return new ConstraintSunAltitude ();
	else if (!strcmp (name, CONSTRAINT_MAXREPEATS))
		return new ConstraintMaxRepeat ();
	return NULL;
}

static Constraints *masterCons = NULL;

Constraints & MasterConstraints::getConstraint ()
{
	if (masterCons)
		return *masterCons;
	masterCons = new Constraints ();
	masterCons->load (Rts2Config::instance ()->getMasterConstraintFile ());
	return *masterCons;
}

void ConstraintsList::printJSON (std::ostream &os)
{
	os << "[";
	for (ConstraintsList::iterator iter = begin (); iter != end (); iter++)
	{
		if (iter != begin ())
			os << ",";
		os << '"' << (*iter)->getName() << '"';
	}
	os << "]";
}
