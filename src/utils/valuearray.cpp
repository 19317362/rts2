/* 
 * Array values.
 * Copyright (C) 2008-2010 Petr Kubanek <petr@kubanek.net>
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

#include "rts2conn.h"
#include "valuearray.h"

#include "utilsfunc.h"

using namespace rts2core;

StringArray::StringArray (std::string _val_name):ValueArray (_val_name)
{
	rts2Type |= RTS2_VALUE_ARRAY | RTS2_VALUE_STRING;
}

StringArray::StringArray (std::string _val_name, std::string _description, bool writeToFits, int32_t flags):ValueArray (_val_name, _description, writeToFits, flags)
{
	rts2Type |= RTS2_VALUE_ARRAY | RTS2_VALUE_STRING;
}

int StringArray::setValue (Rts2Conn * connection)
{
	value.clear ();

	while (!(connection->paramEnd ()))
	{
		char *nextVal;
		int ret = connection->paramNextString (&nextVal);
		if (ret)
			return -2;
		value.push_back (std::string (nextVal));
	}
	changed ();
	return 0;
}

int StringArray::setValues (std::vector <int> &index, Rts2Conn *conn)
{
	char *val;
	int ret = conn->paramNextString (&val);
	if (ret || !conn->paramEnd ())
		return -2;
	for (std::vector <int>::iterator iter = index.begin (); iter != index.end (); iter++)
		value[(*iter)] = val;
	changed ();
	return 0;
}

int StringArray::setValueCharArr (const char *_value)
{
	value = SplitStr (std::string (_value), std::string (" "));
	changed ();
	return 0;
}

const char * StringArray::getValue ()
{
	_os = std::string ();
	std::vector <std::string>::iterator iter = value.begin ();
	while (iter != value.end ())
	{
		_os += (*iter);
		iter++;
		if (iter == value.end ())
			break;
		_os += std::string (" ");
	}
	return _os.c_str ();
}

void StringArray::setFromValue (Rts2Value * newValue)
{
	setValueCharArr (newValue->getValue ());
}

bool StringArray::isEqual (Rts2Value *other_val)
{
	return !strcmp (getValue (), other_val->getValue ());
}

DoubleArray::DoubleArray (std::string _val_name):ValueArray (_val_name)
{
	rts2Type |= RTS2_VALUE_ARRAY | RTS2_VALUE_DOUBLE;
}

DoubleArray::DoubleArray (std::string _val_name, std::string _description, bool writeToFits, int32_t flags):ValueArray (_val_name, _description, writeToFits, flags)
{
	rts2Type |= RTS2_VALUE_ARRAY | RTS2_VALUE_DOUBLE;
}

int DoubleArray::setValue (Rts2Conn * connection)
{
	value.clear ();

	while (!(connection->paramEnd ()))
	{
		double nextVal;
		int ret = connection->paramNextDouble (&nextVal);
		if (ret)
			return -2;
		value.push_back (nextVal);
	}
	changed ();
	return 0;
}

int DoubleArray::setValues (std::vector <int> &index, Rts2Conn *conn)
{
	double val;
	int ret = conn->paramNextDouble (&val);
	if (ret || !conn->paramEnd ())
		return -2;
	for (std::vector <int>::iterator iter = index.begin (); iter != index.end (); iter++)
		value[(*iter)] = val;
	changed ();
	return 0;
}

int DoubleArray::setValueCharArr (const char *_value)
{
	std::vector <std::string> sv = SplitStr (std::string (_value), std::string (" "));
	for (std::vector <std::string>::iterator iter = sv.begin (); iter != sv.end (); iter++)
	{
		value.push_back (atof ((*iter).c_str ()));
	}
	changed ();
	return 0;
}

const char * DoubleArray::getValue ()
{
	std::ostringstream oss;
	std::vector <double>::iterator iter = value.begin ();
	oss.setf (std::ios_base::fixed, std::ios_base::floatfield);
	while (iter != value.end ())
	{
		oss << (*iter);
		iter++;
		if (iter == value.end ())
			break;
		oss << std::string (" ");
	}
	_os = oss.str ();
	return _os.c_str ();
}

void DoubleArray::setFromValue (Rts2Value * newValue)
{
	if (newValue->getValueType () == (RTS2_VALUE_ARRAY | RTS2_VALUE_DOUBLE))
	{
		value.clear ();
		DoubleArray *nv = (DoubleArray *) newValue;
		for (std::vector <double>::iterator iter = nv->valueBegin (); iter != nv->valueEnd (); iter++)
			value.push_back (*iter);
		changed ();
	}
	else
	{
		setValueCharArr (newValue->getValue ());
	}
}

bool DoubleArray::isEqual (Rts2Value *other_val)
{
	if (other_val->getValueType () == (RTS2_VALUE_ARRAY | RTS2_VALUE_DOUBLE))
	{
		DoubleArray *ov = (DoubleArray *) other_val;
		if (ov->size () != value.size ())
			return false;
		
		std::vector <double>::iterator iter1;
		std::vector <double>::iterator iter2;
		for (iter1 = valueBegin (), iter2 = ov->valueBegin (); iter1 != valueEnd () && iter2 != ov->valueEnd (); iter1++, iter2++)
		{
			if (*iter1 != *iter2)
				return false;
		}
		return true;
	}
	return false;
}

IntegerArray::IntegerArray (std::string _val_name):ValueArray (_val_name)
{
	rts2Type |= RTS2_VALUE_ARRAY | RTS2_VALUE_INTEGER;
}

IntegerArray::IntegerArray (std::string _val_name, std::string _description, bool writeToFits, int32_t flags):ValueArray (_val_name, _description, writeToFits, flags)
{
	rts2Type |= RTS2_VALUE_ARRAY | RTS2_VALUE_INTEGER;
}

int IntegerArray::setValue (Rts2Conn * connection)
{
	value.clear ();

	while (!(connection->paramEnd ()))
	{
		int nextVal;
		int ret = connection->paramNextInteger (&nextVal);
		if (ret)
			return -2;
		value.push_back (nextVal);
	}
	changed ();
	return 0;
}

int IntegerArray::setValues (std::vector <int> &index, Rts2Conn *conn)
{
	int val;
	int ret = conn->paramNextInteger (&val);
	if (ret || !conn->paramEnd ())
		return -2;
	for (std::vector <int>::iterator iter = index.begin (); iter != index.end (); iter++)
		value[(*iter)] = val;
	changed ();
	return 0;
}

int IntegerArray::setValueCharArr (const char *_value)
{
	std::vector <std::string> sv = SplitStr (std::string (_value), std::string (" "));
	for (std::vector <std::string>::iterator iter = sv.begin (); iter != sv.end (); iter++)
	{
		value.push_back (atoi ((*iter).c_str ()));
	}
	changed ();
	return 0;
}

const char * IntegerArray::getValue ()
{
	std::ostringstream oss;
	std::vector <int>::iterator iter = valueBegin ();
	oss.setf (std::ios_base::fixed, std::ios_base::floatfield);
	while (iter != valueEnd ())
	{
		oss << *iter;
		iter++;
		if (iter == valueEnd ())
			break;
		oss << std::string (" ");
	}
	_os = oss.str ();
	return _os.c_str ();
}

void IntegerArray::setFromValue (Rts2Value * newValue)
{
	if (newValue->getValueType () == (RTS2_VALUE_ARRAY | RTS2_VALUE_INTEGER))
	{
		value.clear ();
		IntegerArray *nv = (IntegerArray *) newValue;
		for (std::vector <int>::iterator iter = nv->valueBegin (); iter != nv->valueEnd (); iter++)
			value.push_back (*iter);
		changed ();
	}
	else
	{
		setValueCharArr (newValue->getValue ());
	}
}

bool IntegerArray::isEqual (Rts2Value *other_val)
{
	if (other_val->getValueType () == (RTS2_VALUE_ARRAY | RTS2_VALUE_INTEGER))
	{
		IntegerArray *ov = (IntegerArray *) other_val;
		if (ov->size () != value.size ())
			return false;
		
		std::vector <int>::iterator iter1;
		std::vector <int>::iterator iter2;
		for (iter1 = valueBegin (), iter2 = ov->valueBegin (); iter1 != valueEnd () && iter2 != ov->valueEnd (); iter1++, iter2++)
		{
			if (*iter1 != *iter2)
				return false;
		}
		return true;
	}
	return false;
}

BoolArray::BoolArray (std::string val_name):IntegerArray (val_name)
{
	rts2Type &= ~RTS2_VALUE_INTEGER;
	rts2Type |= RTS2_VALUE_BOOL;
}

BoolArray::BoolArray (std::string _val_name, std::string _description, bool writeToFits, int32_t flags):IntegerArray (_val_name, _description, writeToFits, flags)
{
	rts2Type &= ~RTS2_VALUE_INTEGER;
	rts2Type |= RTS2_VALUE_BOOL;
}

const char * BoolArray::getDisplayValue ()
{
	std::ostringstream oss;
	std::vector <int>::iterator iter = valueBegin ();
	oss.setf (std::ios_base::fixed, std::ios_base::floatfield);
	while (iter != valueEnd ())
	{
		if (getFlags () & RTS2_DT_ONOFF)
			oss << (*iter ? "on" : "off");
		else
			oss << (*iter ? "true" : "false");
		iter++;
		if (iter == valueEnd ())
			break;
		oss << std::string (" ");
	}
	_os = oss.str ();
	return _os.c_str ();
}

int BoolArray::setValue (Rts2Conn * connection)
{
	value.clear ();

	while (!(connection->paramEnd ()))
	{
		char *nextVal;
		int ret = connection->paramNextString (&nextVal);
		if (ret)
			return -2;
		bool b;
		ret = charToBool (nextVal, b);
		if (ret)
			return -2;
		value.push_back (b);
	}
	changed ();
	return 0;
}

int BoolArray::setValues (std::vector <int> &index, Rts2Conn *conn)
{
	char *val;
	int ret = conn->paramNextString (&val);
	if (ret || !conn->paramEnd ())
		return -2;
	bool b;
	ret = charToBool (val, b);
	if (ret)
		return -2;
	for (std::vector <int>::iterator iter = index.begin (); iter != index.end (); iter++)
		value[(*iter)] = b;
	changed ();
	return 0;
}

int BoolArray::setValueCharArr (const char *_value)
{
	std::vector <std::string> sv = SplitStr (std::string (_value), std::string (" "));
	for (std::vector <std::string>::iterator iter = sv.begin (); iter != sv.end (); iter++)
	{
		bool b;
		int ret = charToBool (_value, b);
		if (ret)
			return ret;
		value.push_back (b);
	}
	changed ();
	return 0;
}

void BoolArray::setFromValue (Rts2Value * newValue)
{
	if (newValue->getValueType () == (RTS2_VALUE_ARRAY | RTS2_VALUE_BOOL))
	{
		value.clear ();
		BoolArray *nv = (BoolArray *) newValue;
		for (std::vector <int>::iterator iter = nv->valueBegin (); iter != nv->valueEnd (); iter++)
			value.push_back (*iter);
		changed ();
	}
	else
	{
		setValueCharArr (newValue->getValue ());
	}
}

bool BoolArray::isEqual (Rts2Value *other_val)
{
	if (other_val->getValueType () == (RTS2_VALUE_ARRAY | RTS2_VALUE_BOOL))
	{
		BoolArray *ov = (BoolArray *) other_val;
		if (ov->size () != value.size ())
			return false;
		
		std::vector <int>::iterator iter1;
		std::vector <int>::iterator iter2;
		for (iter1 = valueBegin (), iter2 = ov->valueBegin (); iter1 != valueEnd () && iter2 != ov->valueEnd (); iter1++, iter2++)
		{
			if (*iter1 != *iter2)
				return false;
		}
		return true;
	}
	return false;
}
