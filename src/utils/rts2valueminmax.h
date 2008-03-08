/* 
 * Min-max value.
 * Copyright (C) 2007-2008 Petr Kubanek <petr@kubanek.net>
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

#ifndef __RTS2_VALUE_MINMAX__
#define __RTS2_VALUE_MINMAX__

#include "rts2value.h"

/**
 * This class is double value with limits.
 */

class Rts2ValueDoubleMinMax:public Rts2ValueDouble
{
	private:
		double min;
		double max;
	public:
		Rts2ValueDoubleMinMax (std::string in_val_name);
		Rts2ValueDoubleMinMax (std::string in_val_name,
			std::string in_description, bool writeToFits =
			true, int32_t flags = 0);

		virtual int setValue (Rts2Conn * connection);
		virtual int doOpValue (char op, Rts2Value * old_value);
		virtual const char *getValue ();
		virtual const char *getDisplayValue ();
		virtual void setFromValue (Rts2Value * newValue);

		void copyMinMax (Rts2ValueDoubleMinMax * from)
		{
			min = from->getMin ();
			max = from->getMax ();
		}

		void setMin (double in_min)
		{
			min = in_min;
		}

		double getMin ()
		{
			return min;
		}

		void setMax (double in_max)
		{
			max = in_max;
		}

		double getMax ()
		{
			return max;
		}

		/**
		 * @return False if new float value is invalid.
		 */
		bool testValue (double in_v)
		{
			return (in_v >= getMin () && in_v <= getMax ());
		}
};
#endif							 /* !__RTS2_VALUE_MINMAX__ */
