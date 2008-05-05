/* 
 * Driver for OpemTPL mounts.
 * Copyright (C) 2005-2007 Petr Kubanek <petr@kubanek.net>
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

#include "irconn.h"
#include "../utils/rts2cliapp.h"
#include "../utils/rts2config.h"

#include <sstream>
#include <iomanip>

#define OPT_SAMPLE          OPT_LOCAL+5
#define OPT_RESET_MODEL     OPT_LOCAL+6

class IrAxis
{
	private:
		double referenced;
		double currpos;
		double targetpos;
		double offset;
		double realpos;
		double power;
		double power_state;
		const char *name;
	public:
		IrAxis (const char *in_name, double in_referenced, double in_currpos,
			double in_targetpos, double in_offset, double in_realpos,
			double in_power, double in_power_state);
		friend std::ostream & operator << (std::ostream & _os, IrAxis irax);
};

IrAxis::IrAxis (const char *in_name, double in_referenced, double in_currpos,
double in_targetpos, double in_offset, double in_realpos,
double in_power, double in_power_state)
{
	name = in_name;
	referenced = in_referenced;
	currpos = in_currpos;
	targetpos = in_targetpos;
	offset = in_offset;
	realpos = in_realpos;
	power = in_power;
	power_state = in_power_state;
}


std::ostream & operator << (std::ostream & _os, IrAxis irax)
{
	std::ios_base::fmtflags old_settings = _os.flags ();
	_os.setf (std::ios_base::fixed, std::ios_base::floatfield);
	_os
		<< irax.name << ".REFERENCED " << irax.referenced << std::endl
		<< irax.name << ".CURRPOS " << irax.currpos << std::endl
		<< irax.name << ".TARGETPOS " << irax.targetpos << std::endl
		<< irax.name << ".OFFSET " << irax.offset << std::endl
		<< irax.name << ".REALPOS " << irax.realpos << std::endl
		<< irax.name << ".POWER " << irax.power << std::endl
		<< irax.name << ".POWER_STATE " << irax.power_state << std::endl;
	_os.setf (old_settings);
	return _os;
}


class Rts2DevIrError:public Rts2CliApp
{
	private:
		std::string ir_ip;
		int ir_port;

		std::list < const char *> errList;
		enum { NO_OP, CAL, RESET, REFERENCED, SAMPLE }
		op;

		IrAxis getAxisStatus (const char *ax_name);
		IrConn *irConn;

		int doReferenced ();
	protected:
		virtual int processOption (int in_opt);
		virtual int processArgs (const char *arg);
		virtual int init ();
	public:
		Rts2DevIrError (int in_argc, char **in_argv);
		virtual ~ Rts2DevIrError (void)
		{
			delete irConn;
		}
		virtual int doProcessing ();
};

IrAxis
Rts2DevIrError::getAxisStatus (const char *ax_name)
{
	double referenced = nan ("f");
	double currpos = nan ("f");
	double targetpos = nan ("f");
	double offset = nan ("f");
	double realpos = nan ("f");
	double power = nan ("f");
	double power_state = nan ("f");
	std::ostringstream * os;
	int status = 0;

	os = new std::ostringstream ();
	(*os) << ax_name << ".REFERENCED";
	status = irConn->tpl_get (os->str ().c_str (), referenced, &status);
	delete os;
	os = new std::ostringstream ();
	(*os) << ax_name << ".CURRPOS";
	status = irConn->tpl_get (os->str ().c_str (), currpos, &status);
	delete os;
	os = new std::ostringstream ();
	(*os) << ax_name << ".TARGETPOS";
	status = irConn->tpl_get (os->str ().c_str (), targetpos, &status);
	delete os;
	os = new std::ostringstream ();
	(*os) << ax_name << ".OFFSET";
	status = irConn->tpl_get (os->str ().c_str (), offset, &status);
	delete os;
	os = new std::ostringstream ();
	(*os) << ax_name << ".REALPOS";
	status = irConn->tpl_get (os->str ().c_str (), realpos, &status);
	delete os;
	os = new std::ostringstream ();
	(*os) << ax_name << ".POWER";
	status = irConn->tpl_get (os->str ().c_str (), power, &status);
	delete os;
	os = new std::ostringstream ();
	(*os) << ax_name << ".POWER";
	status = irConn->tpl_get (os->str ().c_str (), power_state, &status);
	delete os;
	return IrAxis (ax_name, referenced, currpos, targetpos, offset, realpos,
		power, power_state);
}


int
Rts2DevIrError::doReferenced ()
{
	int status = 0;
	int track;
	double fpar;
	std::string slist;
	status = irConn->tpl_get ("CABINET.REFERENCED", fpar, &status);
	std::cout << "CABINET.REFERENCED " << fpar << std::endl;
	status = irConn->tpl_get ("CABINET.POWER", fpar, &status);
	std::cout << "CABINET.POWER " << fpar << std::endl;
	status = irConn->tpl_get ("CABINET.POWER_STATE", fpar, &status);
	std::cout << "CABINET.POWER_STATE " << fpar << std::endl;

	status = irConn->tpl_get ("CABINET.STATUS.LIST", slist, &status);
	std::cout << "CABINET.STATUS.LIST " << slist << std::endl;

	status = irConn->tpl_get ("POINTING.TRACK", track, &status);
	std::cout << "POINTING.TRACK " << track << std::endl;
	status = irConn->tpl_get ("POINTING.CURRENT.RA", fpar, &status);
	std::cout << "POINTING.CURRENT.RA " << fpar << std::endl;
	status = irConn->tpl_get ("POINTING.CURRENT.DEC", fpar, &status);
	std::cout << "POINTING.CURRENT.DEC " << fpar << std::endl;

	status = irConn->tpl_get ("POINTING.TARGET.RA", fpar, &status);
	std::cout << "POINTING.TARGET.RA " << fpar << std::endl;
	status = irConn->tpl_get ("POINTING.TARGET.DEC", fpar, &status);
	std::cout << "POINTING.TARGET.DEC " << fpar << std::endl;

	std::cout << getAxisStatus ("ZD");
	std::cout << getAxisStatus ("AZ");
	std::cout << getAxisStatus ("FOCUS");
	std::cout << getAxisStatus ("MIRROR");
	std::cout << getAxisStatus ("DEROTATOR[3]");
	if (irConn->haveModule ("COVER"))
		std::cout << getAxisStatus ("COVER");
	std::cout << getAxisStatus ("DOME[0]");
	std::cout << getAxisStatus ("DOME[1]");
	std::cout << getAxisStatus ("DOME[2]");
	return status;
}


Rts2DevIrError::Rts2DevIrError (int in_argc, char **in_argv):
Rts2CliApp (in_argc, in_argv)
{
	ir_port = 0;
	irConn = NULL;

	op = NO_OP;

	addOption ('I', "ir_ip", 1, "IR TCP/IP address");
	addOption ('N', "ir_port", 1, "IR TCP/IP port number");

	addOption ('c', NULL, 0, "Calculate model");
	addOption (OPT_RESET_MODEL, "reset-model", 0, "Reset model counts");
	addOption ('f', NULL, 0, "Referencing status");
	addOption (OPT_SAMPLE, "sample", 0, "Sample measurements");
}


int
Rts2DevIrError::processOption (int in_opt)
{
	switch (in_opt)
	{
		case 'I':
			ir_ip = std::string (optarg);
			break;
		case 'N':
			ir_port = atoi (optarg);
			break;
		case 'c':
			op = CAL;
			break;
		case OPT_RESET_MODEL:
			op = RESET;
			break;
		case 'f':
			op = REFERENCED;
			break;
		case OPT_SAMPLE:
			op = SAMPLE;
			break;
		default:
			return Rts2App::processOption (in_opt);
	}
	return 0;
}


int
Rts2DevIrError::processArgs (const char *arg)
{
	errList.push_back (arg);
	return 0;
}


int
Rts2DevIrError::init ()
{
	int ret;
	ret = Rts2App::init ();
	if (ret)
		return ret;

	Rts2Config *config = Rts2Config::instance ();
	config->loadFile (NULL);
	// try to get default from config file
	if (ir_ip.length () == 0)
	{
		config->getString ("ir", "ip", ir_ip);
	}
	if (!ir_port)
	{
		config->getInteger ("ir", "port", ir_port);
	}
	if (ir_ip.length () == 0 || !ir_port)
	{
		std::cerr << "Invalid port or IP address of mount controller PC"
			<< std::endl;
		return -1;
	}

	irConn = new IrConn (ir_ip, ir_port);

	// are we connected ?
	if (!irConn->isOK ())
	{
		std::cerr << "Connection to server failed" << std::endl;
		return -1;
	}
	return 0;
}


int
Rts2DevIrError::doProcessing ()
{
	std::string descri;
	for (std::list < const char *>::iterator iter = errList.begin ();
		iter != errList.end (); iter++)
	{
		irConn->getError (atoi (*iter), descri);
		std::cout << descri << std::endl;
	}
	int status = 0;
	double fparam;

	switch (op)
	{
		case SAMPLE:
			status = irConn->tpl_set ("POINTING.POINTINGPARAMS.SAMPLE", 1, &status);
		case NO_OP:
			break;
		case CAL:
			fparam = 2;
			status = irConn->tpl_set ("POINTING.POINTINGPARAMS.CALCULATE", fparam, &status);
			break;
		case RESET:
			fparam = 0;
			status =
				irConn->tpl_set ("POINTING.POINTINGPARAMS.RECORDCOUNT", fparam, &status);
			break;
		case REFERENCED:
			return doReferenced ();
	}

	// dump model
	double aoff, zoff, ae, an, npae, ca, flex;
	int recordcount;
	int track;
	std::string dumpfile;

	status = irConn->tpl_get ("POINTING.POINTINGPARAMS.DUMPFILE", dumpfile, &status);
	status = irConn->tpl_get ("POINTING.POINTINGPARAMS.AOFF", aoff, &status);
	status = irConn->tpl_get ("POINTING.POINTINGPARAMS.ZOFF", zoff, &status);
	status = irConn->tpl_get ("POINTING.POINTINGPARAMS.AE", ae, &status);
	status = irConn->tpl_get ("POINTING.POINTINGPARAMS.AN", an, &status);
	status = irConn->tpl_get ("POINTING.POINTINGPARAMS.NPAE", npae, &status);
	status = irConn->tpl_get ("POINTING.POINTINGPARAMS.CA", ca, &status);
	status = irConn->tpl_get ("POINTING.POINTINGPARAMS.FLEX", flex, &status);

	std::cout << "POINTING.POINTINGPARAMS.DUMPFILE " << dumpfile << std::endl;
	std::cout.precision (20);
	std::cout << "AOFF = " << aoff << std::endl;
	std::cout << "ZOFF = " << zoff << std::endl;
	std::cout << "AE = " << ae << std::endl;
	std::cout << "AN = " << an << std::endl;
	std::cout << "NPAE = " << npae << std::endl;
	std::cout << "CA = " << ca << std::endl;
	std::cout << "FLEX = " << flex << std::endl;
	// dump offsets
	status = irConn->tpl_get ("AZ.OFFSET", aoff, &status);
	status = irConn->tpl_get ("ZD.OFFSET", zoff, &status);

	std::cout << "AZ.OFFSET " << aoff << std::endl;
	std::cout << "ZD.OFFSET " << zoff << std::endl;

	status = irConn->tpl_get ("POINTING.TRACK ", track, &status);
	std::cout << "POINTING.TRACK " << track << std::endl;

	status =
		irConn->tpl_get ("POINTING.POINTINGPARAMS.RECORDCOUNT ", recordcount, &status);

	std::cout << "POINTING.POINTINGPARAMS.RECORDCOUNT " << recordcount << std::
		endl;

	status = irConn->tpl_get ("POINTING.POINTINGPARAMS.CALCULATE", fparam, &status);

	std::cout << "POINTING.POINTINGPARAMS.CALCULATE " << fparam << std::endl;

	return 0;
}


int
main (int argc, char **argv)
{
	Rts2DevIrError device = Rts2DevIrError (argc, argv);
	return device.run ();
}
