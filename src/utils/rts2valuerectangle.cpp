/* 
 * Class for rectangle.
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

#include "rts2valuerectangle.h"
#include "rts2conn.h"

void
Rts2ValueRectangle::createValues ()
{
	x = newValue (rts2Type & RTS2_BASE_TYPE, getName () + ".X", getDescription () + " X");
	y = newValue (rts2Type & RTS2_BASE_TYPE, getName () + ".Y", getDescription () + " Y");
	w = newValue (rts2Type & RTS2_BASE_TYPE, getName () + ".WIDTH", getDescription () + " WIDTH");
	h = newValue (rts2Type & RTS2_BASE_TYPE, getName () + ".HEIGHT", getDescription () + " HEIGHT");
}


Rts2ValueRectangle::Rts2ValueRectangle (std::string in_val_name, int32_t baseType):
Rts2Value (in_val_name)
{
	rts2Type = RTS2_VALUE_RECTANGLE | baseType;
	createValues ();
}


Rts2ValueRectangle::Rts2ValueRectangle (std::string in_val_name, std::string in_description,
bool writeToFits, int32_t flags):
Rts2Value (in_val_name, in_description, writeToFits, flags)
{
	rts2Type = RTS2_VALUE_RECTANGLE | flags;
	createValues ();
}


Rts2ValueRectangle::~Rts2ValueRectangle (void)
{
	delete x;
	delete y;
	delete w;
	delete h;
}


void
Rts2ValueRectangle::setInts (int in_x, int in_y, int in_w, int in_h)
{
	x->setValueInteger (in_x);
	y->setValueInteger (in_y);
	w->setValueInteger (in_w);
	h->setValueInteger (in_h);
}


int
Rts2ValueRectangle::setValue (Rts2Conn *connection)
{
	char *val;
	if (connection->paramNextString (&val) || x->setValueString (val))
		return -2;
	if (connection->paramNextString (&val) || y->setValueString (val))
		return -2;
	if (connection->paramNextString (&val) || w->setValueString (val))
		return -2;
	if (connection->paramNextString (&val) || h->setValueString (val))
		return -2;
	return connection->paramEnd () ? 0 : -2;
}


int
Rts2ValueRectangle::setValueString (const char *in_value)
{
	strncpy (buf, in_value, VALUE_BUF_LEN);
	// split on spaces
	char *top = buf;
	char *val_start = top;

	while (!isspace (*top) && *top)
		top++;
	if (!*top)
		return -1;
	*top = '\0';
	x->setValueString (buf);
	top++;
	val_start = top;

	while (!isspace (*top) && *top)
		top++;
	if (!*top)
		return -1;
	*top = '\0';
	y->setValueString (buf);
	top++;
	val_start = top;

	while (!isspace (*top) && *top)
		top++;
	if (!*top)
		return -1;
	*top = '\0';
	w->setValueString (buf);
	top++;
	val_start = top;

	while (!isspace (*top) && *top)
		top++;
	if (!*top)
		return -1;
	*top = '\0';
	h->setValueString (buf);
	return 0;
}


int
Rts2ValueRectangle::setValueInteger (int in_value)
{
	return -1;
}


const char *
Rts2ValueRectangle::getValue ()
{
	sprintf (buf, "%s %s %s %s", x->getValue (), y->getValue (), w->getValue (), h->getValue ());
	return buf;
}


void
Rts2ValueRectangle::setFromValue(Rts2Value * newValue)
{
	if (newValue->getValueExtType () == RTS2_VALUE_RECTANGLE)
	{
		x->setFromValue (((Rts2ValueRectangle *)newValue)->getX ());
		y->setFromValue (((Rts2ValueRectangle *)newValue)->getY ());
		w->setFromValue (((Rts2ValueRectangle *)newValue)->getWidth ());
		h->setFromValue (((Rts2ValueRectangle *)newValue)->getHeight ());
	}
}
