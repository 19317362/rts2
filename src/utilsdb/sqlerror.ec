/* 
 * Class for reporting sql errors.
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

#include "sqlerror.h"

#include <sstream>

using namespace rts2db;

std::string
SqlError::getError ()
{
	std::ostringstream _os;
	_os << sqlca.sqlerrm.sqlerrmc << " (#" << sqlca.sqlcode << ")";
	return _os.str ();
}

std::ostream &
operator << (std::ostream & _os, SqlError & err)
{
	_os << err.getError ();
	return _os;
}
