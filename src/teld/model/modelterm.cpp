/* 
 * Telescope model terms.
 * Copyright (C) 2003-2006 Martin Jelinek <mates@iaa.es>
 * Copyright (C) 2006-2007 Petr Kubanek <petr@kubanek.net>
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

#include "modelterm.h"
#include <math.h>

std::ostream & operator << (std::ostream & os, Rts2ModelTerm * term)
{
  // correction is (internally) in degrees!
  os << term->name << " " << (term->corr *
			      3600.0) << " " << term->sigma << std::endl;
  return os;
}

// status OK
void
Rts2TermME::apply (struct ln_equ_posn *pos,
		   Rts2ObsConditions * obs_conditions)
{
  // simple method
  double dh;
  double dd;

  dh = corr * sin (ln_deg_to_rad (pos->ra)) * tan (ln_deg_to_rad (pos->dec));
  dd = corr * cos (ln_deg_to_rad (pos->ra));

  pos->ra += dh;
  pos->dec += dd;
}

// status OK
void
Rts2TermMA::apply (struct ln_equ_posn *pos,
		   Rts2ObsConditions * obs_conditions)
{
  double d, h;

  h =
    -1 * corr * cos (ln_deg_to_rad (pos->ra)) *
    tan (ln_deg_to_rad (pos->dec));
  d = corr * sin (ln_deg_to_rad (pos->ra));

  pos->ra += h;
  pos->dec += d;
}

// status: OK
void
Rts2TermIH::apply (struct ln_equ_posn *pos,
		   Rts2ObsConditions * obs_conditions)
{
  pos->ra = ln_range_degrees (pos->ra + corr);
}

// status: OK
void
Rts2TermID::apply (struct ln_equ_posn *pos,
		   Rts2ObsConditions * obs_conditions)
{
  // Add a zero point to the declination
  pos->dec = pos->dec + corr;
  // No change for hour angle
}

// status: OK
void
Rts2TermCH::apply (struct ln_equ_posn *pos,
		   Rts2ObsConditions * obs_conditions)
{
  pos->ra += corr / cos (ln_deg_to_rad (pos->dec));
}

// status: OK
void
Rts2TermNP::apply (struct ln_equ_posn *pos,
		   Rts2ObsConditions * obs_conditions)
{
  pos->ra += corr * tan (ln_deg_to_rad (pos->dec));
}

// status: ok
void
Rts2TermPHH::apply (struct ln_equ_posn *pos,
		    Rts2ObsConditions * obs_conditions)
{
  pos->ra =
    pos->ra + ln_rad_to_deg (ln_deg_to_rad (corr) * ln_deg_to_rad (pos->ra));
}

// status: ok
void
Rts2TermPDD::apply (struct ln_equ_posn *pos,
		    Rts2ObsConditions * obs_conditions)
{
  pos->dec =
    pos->dec -
    ln_rad_to_deg (ln_deg_to_rad (corr) * ln_deg_to_rad (pos->dec));
}

// status: testing
void
Rts2TermA1H::apply (struct ln_equ_posn *pos,
		    Rts2ObsConditions * obs_conditions)
{
  pos->ra =
    pos->ra -
    ln_rad_to_deg (ln_deg_to_rad (corr) * obs_conditions->getFlip ());
}

// status: testing
void
Rts2TermA1D::apply (struct ln_equ_posn *pos,
		    Rts2ObsConditions * obs_conditions)
{
  pos->dec =
    pos->dec -
    ln_rad_to_deg (ln_deg_to_rad (corr) * obs_conditions->getFlip ());
}

void
Rts2TermTF::apply (struct ln_equ_posn *pos,
		   Rts2ObsConditions * obs_conditions)
{
  double d, h, f;
  d = ln_deg_to_rad (pos->dec);
  h = ln_deg_to_rad (pos->ra);
  f = ln_deg_to_rad (obs_conditions->getLatitude ());

  pos->ra += corr * cos (f) * sin (h) / cos (d);
  pos->dec += corr * (cos (f) * cos (h) * sin (d) - sin (f) * cos (d));
}

void
Rts2TermTX::apply (struct ln_equ_posn *pos,
		   Rts2ObsConditions * obs_conditions)
{
  double d, h, f;
  d = ln_deg_to_rad (pos->dec);
  h = ln_deg_to_rad (pos->ra);
  f = ln_deg_to_rad (obs_conditions->getLatitude ());

  pos->ra +=
    (corr * cos (f) * sin (h) / cos (d)) /
    (sin (d) * sin (f) + cos (d) * cos (h) * cos (f));
  pos->dec +=
    (corr) * (cos (f) * cos (h) * sin (d) - sin (f) * cos (d)) /
    (sin (d) * sin (f) + cos (d) * cos (h) * cos (f));
}

void
Rts2TermHCEC::apply (struct ln_equ_posn *pos,
		     Rts2ObsConditions * obs_conditions)
{
  pos->ra += corr * cos (ln_deg_to_rad (pos->ra));
}

void
Rts2TermHCES::apply (struct ln_equ_posn *pos,
		     Rts2ObsConditions * obs_conditions)
{
  pos->ra += corr * sin (ln_deg_to_rad (pos->ra));
}

void
Rts2TermDCEC::apply (struct ln_equ_posn *pos,
		     Rts2ObsConditions * obs_conditions)
{
  pos->dec += corr * cos (ln_deg_to_rad (pos->dec));
}

void
Rts2TermDCES::apply (struct ln_equ_posn *pos,
		     Rts2ObsConditions * obs_conditions)
{
  pos->dec += corr * sin (ln_deg_to_rad (pos->dec));
}

void
Rts2TermDAB::apply (struct ln_equ_posn *pos,
		    Rts2ObsConditions * obs_conditions)
{
  double d, h, f;
  d = ln_deg_to_rad (pos->dec);
  h = ln_deg_to_rad (pos->ra);
  f = ln_deg_to_rad (obs_conditions->getLatitude ());

  double sh = sin (h);
  double sf = sin (f);
  double ch = cos (h);

  pos->ra -=
    corr * (sh * sh * sf * sf + ch * ch) * (sf * tan (d) + cos (f) * ch);
}

void
Rts2TermDAF::apply (struct ln_equ_posn *pos,
		    Rts2ObsConditions * obs_conditions)
{
  double d, h, f;
  d = ln_deg_to_rad (pos->dec);
  h = ln_deg_to_rad (pos->ra);
  f = ln_deg_to_rad (obs_conditions->getLatitude ());

  pos->ra -= corr * (sin (f) * tan (d) + cos (f) * cos (h));
}

Rts2TermHarmonics::Rts2TermHarmonics (double in_corr, double in_sigma,
				      const char *in_name):
Rts2ModelTerm (in_name, in_corr, in_sigma)
{
  func[0] = NOT;
  func[1] = NOT;
  const char *end;
  // parse it
  if (strlen (in_name) < 4)
    return;
  resType = in_name[1];
  end = getFunc (in_name + 2, 0);
  if (!end)
    return;
  if (*end)
    getFunc (end, 1);
}

const char *
Rts2TermHarmonics::getFunc (const char *in_func, int i)
{
  const char *end = in_func + 1;
  int times = 1;
  if (!end)
    return NULL;
  switch (*in_func)
    {
    case 'S':
      func[i] = SIN;
      break;
    case 'C':
      func[i] = COS;
      break;
    default:
      logStream (MESSAGE_ERROR) << "Unknow function " << in_func << sendLog;
      return NULL;
    }
  param[i] = *end;
  end++;
  mul[i] = 0;
  while (isdigit (*end))
    {
      mul[i] = mul[i] * times + (*end - '0');
      times *= 10;
      end++;
    }
  // if there isn't multiplier..
  if (mul[i] == 0)
    mul[i] = 1;
  return end;
}

double
Rts2TermHarmonics::getValue (struct ln_equ_posn *pos,
			     Rts2ObsConditions * obs_conditions, int i)
{
  double val = mul[i];
//  struct ln_hrz_posn hrz;
  switch (param[i])
    {
    case 'H':
      val *= pos->ra;
      break;
    case 'D':
      val *= pos->dec;
      break;
/*    case 'A':
      ln_get_hrz_from_equ_sidereal_time (pos,  
      break; */
    default:
      logStream (MESSAGE_ERROR) << "Unknow parameter " << param[i] << sendLog;
      val = nan ("f");
    }
  return val;
}

double
Rts2TermHarmonics::getMember (struct ln_equ_posn *pos,
			      Rts2ObsConditions * obs_conditions, int i)
{
  double val;
  switch (func[i])
    {
    case SIN:
      val = ln_deg_to_rad (getValue (pos, obs_conditions, i));
      return sin (val);
      break;
    case COS:
      val = ln_deg_to_rad (getValue (pos, obs_conditions, i));
      return cos (val);
      break;
    case NOT:
    default:
      return (i == 0 ? 0 : 1);
    }
}

void
Rts2TermHarmonics::apply (struct ln_equ_posn *pos,
			  Rts2ObsConditions * obs_conditions)
{
  double resVal = corr;
  for (int i = 0; i < 2; i++)
    resVal *= getMember (pos, obs_conditions, i);
  switch (resType)
    {
    case 'H':
      pos->ra += resVal;
      break;
    case 'D':
      pos->dec += resVal;
      break;
    default:
      logStream (MESSAGE_ERROR) << "Cannot process (yet?) resType " << resType
	<< sendLog;
    }
}
