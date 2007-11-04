/* 
 * Device window display.
 * Copyright (C) 2007 Petr Kubanek <petr@kubanek.net>
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

#include "nmonitor.h"
#include "rts2ndevicewindow.h"

#include "../utils/timestamp.h"
#include "../utils/rts2displayvalue.h"
#include "../utils/riseset.h"

Rts2NDeviceWindow::Rts2NDeviceWindow (Rts2Conn * in_connection):
Rts2NSelWindow (10, 1, COLS - 10, LINES - 25)
{
	connection = in_connection;
	connection->resetInfoTime ();
	valueBox = NULL;
	valueBegins = 20;
	draw ();
}


Rts2NDeviceWindow::~Rts2NDeviceWindow ()
{
}


void
Rts2NDeviceWindow::printState ()
{
	wattron (window, A_REVERSE);
	if (connection->getErrorState ())
		wcolor_set (window, CLR_FAILURE, NULL);
	else if (connection->havePriority ())
		wcolor_set (window, CLR_OK, NULL);
	mvwprintw (window, 0, 2, "%s %s (%x) %x priority: %s",
		connection->getName (),
		connection->getStateString ().c_str (),
		connection->getState (),
		connection->getFullBopState (),
		connection->havePriority ()? "yes" : "no"
	);

	wcolor_set (window, CLR_DEFAULT, NULL);
	wattroff (window, A_REVERSE);
}


void
Rts2NDeviceWindow::printValue (const char *name, const char *value)
{
	wprintw (getWriteWindow (), "%-20s %30s\n", name, value);
}


void
Rts2NDeviceWindow::printValue (Rts2Value * value)
{
	// customize value display
	std::ostringstream _os;
	if (value->getWriteToFits ())
		wcolor_set (getWriteWindow (), CLR_FITS, NULL);
	else
		wcolor_set (getWriteWindow (), CLR_DEFAULT, NULL);
	// ultra special handling of SCRIPT value
	if (value->getValueDisplayType () == RTS2_DT_SCRIPT)
	{
		wprintw (getWriteWindow (), "%-20s ", value->getName ().c_str ());
		wcolor_set (getWriteWindow (), CLR_DEFAULT, NULL);
		const char *valStart = value->getValue ();
		if (!valStart)
			return;
		const char *valTop = valStart;
		int scriptPosition = connection->getValueInteger ("scriptPosition");
		int scriptEnd = connection->getValueInteger ("scriptLen") + scriptPosition;

		while (*valTop && (valTop - valStart < scriptPosition))
		{
			waddch (getWriteWindow (), *valTop);
			valTop++;
		}
		wcolor_set (getWriteWindow (), CLR_SCRIPT_CURRENT, NULL);
		while (*valTop && (valTop - valStart < scriptEnd))
		{
			waddch (getWriteWindow (), *valTop);
			valTop++;
		}
		wcolor_set (getWriteWindow (), CLR_DEFAULT, NULL);
		while (*valTop)
		{
			waddch (getWriteWindow (), *valTop);
			valTop++;
		}
		waddch (getWriteWindow (), '\n');
		return;
	}
	switch (value->getValueType ())
	{
		case RTS2_VALUE_TIME:
			_os
				<< LibnovaDateDouble (value->getValueDouble ())
				<< " (" << TimeDiff (now, value->getValueDouble ()) << ")";
			printValue (value->getName ().c_str (), _os.str ().c_str ());
			break;
		case RTS2_VALUE_SELECTION:
			wprintw
				(
				getWriteWindow (), "%-20s %5i %24s\n",
				value->getName ().c_str (),
				value->getValueInteger (),
				((Rts2ValueSelection *) value)->getSelName ().c_str ()
				);
			break;
		default:
			printValue (value->getName ().c_str (),
				getDisplayValue (value).c_str ());
	}
}


void
Rts2NDeviceWindow::drawValuesList ()
{
	gettimeofday (&tvNow, NULL);
	now = tvNow.tv_sec + tvNow.tv_usec / USEC_SEC;

	maxrow = 0;

	for (Rts2ValueVector::iterator iter = connection->valueBegin ();
		iter != connection->valueEnd (); iter++)
	{
		maxrow++;

		printValue (*iter);
	}
}


Rts2Value *
Rts2NDeviceWindow::getSelValue ()
{
	int s = getSelRow ();
	if (s >= 0)
		return connection->valueAt (s);
	return NULL;
}


void
Rts2NDeviceWindow::printValueDesc (Rts2Value * val)
{
	wattron (window, A_REVERSE);
	mvwprintw (window, getHeight () - 1, 2, "D: \"%s\"",
		val->getDescription ().c_str ());
	wattroff (window, A_REVERSE);
}


void
Rts2NDeviceWindow::endValueBox ()
{
	delete valueBox;
	valueBox = NULL;
}


void
Rts2NDeviceWindow::createValueBox ()
{
	int s = getSelRow ();
	if (s < 0)
		return;
	Rts2Value *val = connection->valueAt (s);
	if (!val)
		return;
	s -= getPadoffY ();
	switch (val->getValueType ())
	{
		case RTS2_VALUE_BOOL:
			valueBox = new Rts2NValueBoxBool (this, (Rts2ValueBool *) val, 21, s - 1);
			break;
		case RTS2_VALUE_STRING:
			valueBox = new Rts2NValueBoxString (this, (Rts2ValueString *) val, 21, s - 1);
			break;
		case RTS2_VALUE_INTEGER:
			valueBox = new Rts2NValueBoxInteger (this, (Rts2ValueInteger *) val, 21, s);
			break;
		case RTS2_VALUE_FLOAT:
			valueBox = new Rts2NValueBoxFloat (this, (Rts2ValueFloat *) val, 21, s);
			break;
		case RTS2_VALUE_DOUBLE:
		case RTS2_VALUE_MMAX | RTS2_VALUE_DOUBLE:
			valueBox = new Rts2NValueBoxDouble (this, (Rts2ValueDouble *) val, 21, s);
			break;
		case RTS2_VALUE_SELECTION:
			valueBox = new Rts2NValueBoxSelection (this, (Rts2ValueSelection *) val, 21, s);
			break;
		default:
			switch (val->getValueExtType ())
			{
				case RTS2_VALUE_RECTANGLE:
					valueBox = new Rts2NValueBoxString (this, (Rts2ValueString *) val, 21, s - 1);
					break;
				default:
					logStream (MESSAGE_WARNING) << "Cannot find box for value '"
						<<  val->getName ()
						<< " type " << val->getValueType ()
						<< sendLog;
					valueBox = new Rts2NValueBoxString (this, val, 21, s - 1);
					break;

			}
			break;
	}
}


keyRet Rts2NDeviceWindow::injectKey (int key)
{
	keyRet
		ret;
	switch (key)
	{
		case KEY_ENTER:
		case K_ENTER:
			// don't create new box if one already exists
			if (valueBox)
				break;
		case KEY_F (6):
			if (valueBox)
				endValueBox ();
			createValueBox ();
			return RKEY_HANDLED;
	}
	if (valueBox)
	{
		ret = valueBox->injectKey (key);
		if (ret == RKEY_ENTER || ret == RKEY_ESC)
		{
			if (ret == RKEY_ENTER)
				valueBox->sendValue (connection);
			endValueBox ();
			return RKEY_HANDLED;
		}
		return ret;
	}
	return Rts2NSelWindow::injectKey (key);
}


void
Rts2NDeviceWindow::draw ()
{
	Rts2NSelWindow::draw ();
	werase (getWriteWindow ());
	drawValuesList ();

	wcolor_set (getWriteWindow (), CLR_DEFAULT, NULL);
	mvwvline (getWriteWindow (), 0, valueBegins, ACS_VLINE,
		(maxrow > getHeight ()? maxrow : getHeight ()));
	mvwaddch (window, 0, valueBegins + 1, ACS_TTEE);
	mvwaddch (window, getHeight () - 1, valueBegins + 1, ACS_BTEE);

	printState ();
	Rts2Value *val = getSelValue ();
	if (val != NULL)
		printValueDesc (val);
	refresh ();
}


void
Rts2NDeviceWindow::refresh ()
{
	Rts2NSelWindow::refresh ();
	if (valueBox)
		valueBox->draw ();
}


bool
Rts2NDeviceWindow::setCursor ()
{
	if (valueBox)
		return valueBox->setCursor ();
	return Rts2NSelWindow::setCursor ();
}


Rts2NDeviceCentralWindow::Rts2NDeviceCentralWindow (Rts2Conn * in_connection):Rts2NDeviceWindow
(in_connection)
{
	nightStart = new Rts2ValueTime ("Night start", "Beginnign of current or next night", false);
	nightStop = new Rts2ValueTime ("Night stop", "End of current or next night", false);

	sunAlt = new Rts2ValueDouble ("Sun alt", "Sun altitude", false, RTS2_DT_DEC);
	sunAz = new Rts2ValueDouble ("Sun az", "Sun azimuth", false, RTS2_DT_DEGREES);

	sunRise = new Rts2ValueTime ("Sun rise", "Sun rise", false);
	sunSet = new Rts2ValueTime ("Sun set", "Sun set", false);

	moonAlt = new Rts2ValueDouble ("Moon alt", "Moon altitude", false, RTS2_DT_DEC);
	moonAz = new Rts2ValueDouble ("Moon az", "Moon azimuth", false, RTS2_DT_DEGREES);

	moonPhase = new Rts2ValueDouble ("Moon phase", "Moon phase", false, RTS2_DT_PERCENTS);

	moonRise = new Rts2ValueTime ("Moon rise", "Moon rise", false);
	moonSet = new Rts2ValueTime ("Moon set", "Moon set", false);
}


Rts2NDeviceCentralWindow::~Rts2NDeviceCentralWindow (void)
{
	delete sunAlt;
	delete sunAz;

	delete sunRise;
	delete sunSet;

	delete moonAlt;
	delete moonAz;

	delete moonPhase;

	delete moonRise;
	delete moonSet;
}


void
Rts2NDeviceCentralWindow::printValues ()
{
	// print statusChanges

	Rts2Value *nextState = getConnection ()->getValue ("next_state");
	if (nextState && nextState->getValueType () == RTS2_VALUE_SELECTION)
	{
		for (std::vector < FutureStateChange >::iterator iter =
			stateChanges.begin (); iter != stateChanges.end (); iter++)
		{
			std::ostringstream _os;
			_os << LibnovaDateDouble ((*iter).getEndTime ())
				<< " (" << TimeDiff (now, (*iter).getEndTime ()) << ")";

			printValue (((Rts2ValueSelection *) nextState)->getSelName
				((*iter).getState ()).c_str (), _os.str ().c_str ()
				);
		}
	}

	printValue (nightStart);
	printValue (nightStop);

	printValue (sunAlt);
	printValue (sunAz);

	printValue (sunRise);
	printValue (sunSet);

	printValue (moonAlt);
	printValue (moonAz);
	printValue (moonPhase);

	printValue (moonRise);
	printValue (moonSet);
}


void
Rts2NDeviceCentralWindow::drawValuesList ()
{
	Rts2NDeviceWindow::drawValuesList ();

	if (!getConnection ()->infoTimeChanged ())
	{
		printValues ();
		return;
	}

	Rts2Value *valLng = getConnection ()->getValue ("longitude");
	Rts2Value *valLat = getConnection ()->getValue ("latitude");

	if (valLng && valLat && !isnan (valLng->getValueDouble ())
		&& !isnan (valLat->getValueDouble ()))
	{
		struct ln_equ_posn pos, parallax;
		struct ln_lnlat_posn observer;
		struct ln_hrz_posn hrz;
		struct ln_rst_time rst;
		double JD = ln_get_julian_from_sys ();

		observer.lng = valLng->getValueDouble ();
		observer.lat = valLat->getValueDouble ();

		// get next night, or get beginnign of current night

		Rts2Value *valNightHorizon =
			getConnection ()->getValue ("night_horizon");
		Rts2Value *valDayHorizon = getConnection ()->getValue ("day_horizon");

		Rts2Value *valEveningTime = getConnection ()->getValue ("evening_time");
		Rts2Value *valMorningTime = getConnection ()->getValue ("morning_time");

		if (valNightHorizon
			&& valDayHorizon
			&& !isnan (valNightHorizon->getValueDouble ())
			&& !isnan (valDayHorizon->getValueDouble ()) && valEveningTime
			&& valMorningTime)
		{
			stateChanges.clear ();

			int curr_type = -1, next_type = -1;
			time_t t_start;
			time_t t_start_t;
			time_t ev_time = tvNow.tv_sec;

			while (ev_time < (tvNow.tv_sec + 86400))
			{
				t_start = ev_time;
				t_start_t = ev_time + 1;
				next_event (&observer, &t_start_t, &curr_type, &next_type,
					&ev_time, valNightHorizon->getValueDouble (),
					valDayHorizon->getValueDouble (),
					valEveningTime->getValueInteger (),
					valMorningTime->getValueInteger ());
				if (curr_type == SERVERD_DUSK)
				{
					nightStart->setValueTime (ev_time);
				}
				if (curr_type == SERVERD_NIGHT)
				{
					nightStop->setValueTime (ev_time);
				}
				stateChanges.push_back (FutureStateChange (curr_type, t_start));
			}
		}

		ln_get_solar_equ_coords (JD, &pos);
		ln_get_parallax (&pos, ln_get_earth_solar_dist (JD), &observer, 1700,
			JD, &parallax);
		pos.ra += parallax.ra;
		pos.dec += parallax.dec;
		ln_get_hrz_from_equ (&pos, &observer, JD, &hrz);

		sunAlt->setValueDouble (hrz.alt);
		sunAz->setValueDouble (hrz.az);

		ln_get_solar_rst (JD, &observer, &rst);

		sunRise->setValueDouble (timetFromJD (rst.rise));
		sunSet->setValueDouble (timetFromJD (rst.set));

		ln_get_lunar_equ_coords (JD, &pos);
		ln_get_parallax (&pos, ln_get_earth_solar_dist (JD), &observer, 1700,
			JD, &parallax);
		pos.ra += parallax.ra;
		pos.dec += parallax.dec;
		ln_get_hrz_from_equ (&pos, &observer, JD, &hrz);

		moonAlt->setValueDouble (hrz.alt);
		moonAz->setValueDouble (hrz.az);

		moonPhase->setValueDouble (ln_get_lunar_phase (JD) / 1.8);

		ln_get_lunar_rst (JD, &observer, &rst);

		moonRise->setValueDouble (timetFromJD (rst.rise));
		moonSet->setValueDouble (timetFromJD (rst.set));

	}
	printValues ();
}
