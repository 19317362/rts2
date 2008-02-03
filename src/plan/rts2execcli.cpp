/**
 * Executor client for telescope and camera.
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

#define DEBUG_EXTRA

#include <limits.h>
#include <iostream>

#include "rts2execcli.h"
#include "../writers/rts2image.h"
#include "../utilsdb/target.h"
#include "../utils/rts2command.h"

#define DEBUG_EXTRA

Rts2DevClientCameraExec::Rts2DevClientCameraExec (Rts2Conn * in_connection, Rts2ValueString *in_expandPath)
:Rts2DevClientCameraImage (in_connection), Rts2DevScript (in_connection)
{
	expandPath = in_expandPath;
	imgCount = 0;
}


Rts2DevClientCameraExec::~Rts2DevClientCameraExec ()
{
	deleteScript ();
}


Rts2Image *
Rts2DevClientCameraExec::createImage (const struct timeval *expStart)
{
	if (expandPath)
		return new Rts2Image (expandPath->getValue (), expStart, connection);
	return Rts2DevClientCameraImage::createImage (expStart);
}


void
Rts2DevClientCameraExec::postEvent (Rts2Event * event)
{
	Rts2Image *image;
	switch (event->getType ())
	{
		case EVENT_QUE_IMAGE:
			image = (Rts2Image *) event->getArg ();
			if (!strcmp (image->getCameraName (), getName ()))
				queImage (image);
			break;
		case EVENT_COMMAND_OK:
			nextCommand ();
			break;
		case EVENT_COMMAND_FAILED:
			nextCommand ();
			//deleteScript ();
			break;
	}
	Rts2DevScript::postEvent (event);
	Rts2DevClientCameraImage::postEvent (event);
}


void
Rts2DevClientCameraExec::startTarget ()
{
	Rts2DevScript::startTarget ();
}


int
Rts2DevClientCameraExec::getNextCommand ()
{
	int ret = getScript ()->nextCommand (*this, &nextComd, cmd_device);
	if (nextComd)
		nextComd->setOriginator (this);
	return ret;
}


void
Rts2DevClientCameraExec::nextCommand ()
{
	int ret;
	ret = haveNextCommand (this);
	#ifdef DEBUG_EXTRA
	logStream (MESSAGE_DEBUG)
		<< "Rts2DevClientCameraExec::nextComd " << ret
		<< sendLog;
	#endif						 /* DEBUG_EXTRA */
	if (!ret)
		return;

	if (nextComd->getBopMask () & BOP_TEL_MOVE)
	{
		// if command cannot be executed when telescope is moving, do not execute it
		// before target was moved
		if (currentTarget && !currentTarget->wasMoved())
			return;
	}

	if (nextComd->getBopMask () & BOP_WHILE_STATE)
	{
		// if there are qued exposures, do not execute command
		Rts2Value *val = getConnection ()->getValue ("que_exp_num");
		if (val && val->getValueInteger () != 0)
			return;
	}

	// send command to other device
	if (strcmp (getName (), cmd_device))
	{
		Rts2Conn *cmdConn = getMaster ()->getOpenConnection (cmd_device);
		if (!cmdConn)
		{
			logStream (MESSAGE_ERROR)
				<< "Unknow device : " << cmd_device
				<< sendLog;
			nextComd = NULL;
			return;
		}

		if (!(nextComd->getBopMask () & BOP_WHILE_STATE))
		{
			// if there are some commands in que, do not proceed, as they might change state of the device
			if (!connection->queEmptyForOriginator (this))
			{
				return;
			}

			nextComd->setBopMask (BOP_TEL_MOVE);

			// do not execute if there are some exposures in que
			Rts2Value *val = getConnection ()->getValue ("que_exp_num");
			if (val && val->getValueInteger () > 0)
			{
				return;
			}
		}

		// execute command
		// when it returns, we can execute next command
		cmdConn->queCommand (nextComd);
		nextComd = NULL;
		return;
	}

	#ifdef DEBUG_EXTRA
	logStream (MESSAGE_DEBUG) << "For " << getName () << " queing " <<
		nextComd->getText () << sendLog;
	#endif						 /* DEBUG_EXTRA */
	queCommand (nextComd);
	nextComd = NULL;			 // after command execute, it will be deleted
}


void
Rts2DevClientCameraExec::queImage (Rts2Image * image)
{
	// if unknow type, don't process image..
	if (image->getShutter () != SHUT_OPENED
		|| image->getImageType () == IMGTYPE_FLAT)
		return;

	// find image processor with lowest que number..
	Rts2Conn *minConn = getMaster ()->getMinConn ("que_size");
	if (!minConn)
		return;
	image->saveImage ();
	minConn->queCommand (new Rts2CommandQueImage (getMaster (), image));
}


imageProceRes Rts2DevClientCameraExec::processImage (Rts2Image * image)
{
	int ret;
	// try processing in script..
	if (getScript ())
	{
		ret = getScript ()->processImage (image);
		if (ret > 0)
		{
			return IMAGE_KEEP_COPY;
		}
		else if (ret == 0)
		{
			return IMAGE_DO_BASIC_PROCESSING;
		}
		// otherwise que image processing
	}
	queImage (image);
	return IMAGE_DO_BASIC_PROCESSING;
}


void
Rts2DevClientCameraExec::idle ()
{
	Rts2DevScript::idle ();
	Rts2DevClientCameraImage::idle ();
	// when it is the first command in the script..
	if (getScript () && getScript ()->getExecutedCount () == 0)
		nextCommand ();
}


void
Rts2DevClientCameraExec::exposureStarted ()
{
	if (nextComd && (nextComd->getBopMask () & BOP_WHILE_STATE))
		nextCommand ();

	Rts2DevClientCameraImage::exposureStarted ();
}


void
Rts2DevClientCameraExec::exposureEnd ()
{
	Rts2Value *val = getConnection ()->getValue ("que_exp_num");
	// if script is running and it does not have anything to do, end it
	if (getScript ()
		&& !nextComd
		&& getScript ()->isLastCommand ()
		&& (!val || val->getValueInteger () == 0)
		&& !getMaster ()->commandOriginatorPending (this, NULL)
		)
	{
		deleteScript ();
	}
	// execute value change, if we do not execute that during exposure
	if (strcmp (getName (), cmd_device) && nextComd && (!(nextComd->getBopMask () & BOP_WHILE_STATE)) &&
		!isExposing () && val && val->getValueInteger () == 0
		)
		nextCommand ();

	// send readout after we deal with next command - which can be filter move
	Rts2DevClientCameraImage::exposureEnd ();
}


void
Rts2DevClientCameraExec::exposureFailed (int status)
{
	// in case of an error..
	Rts2DevClientCameraImage::exposureFailed (status);
}


void
Rts2DevClientCameraExec::readoutEnd ()
{
	//nextCommand ();
	// we don't want camera to react to that..
}


Rts2DevClientTelescopeExec::Rts2DevClientTelescopeExec (Rts2Conn * in_connection):Rts2DevClientTelescopeImage
(in_connection)
{
	currentTarget = NULL;
	cmdChng = NULL;
	fixedOffset.ra = 0;
	fixedOffset.dec = 0;
}


void
Rts2DevClientTelescopeExec::postEvent (Rts2Event * event)
{
	int ret;
	struct ln_equ_posn *offset;
	GuidingParams *gp;
	Rts2ScriptElementSearch *searchEl;
	switch (event->getType ())
	{
		case EVENT_KILL_ALL:
			clearWait ();
			break;
		case EVENT_SET_TARGET:
			currentTarget = (Target *) event->getArg ();
			break;
		case EVENT_SLEW_TO_TARGET:
			if (currentTarget)
			{
				currentTarget->beforeMove ();
				ret = syncTarget ();
				switch (ret)
				{
					case OBS_DONT_MOVE:
						getMaster ()->
							postEvent (new
							Rts2Event (EVENT_OBSERVE, (void *) currentTarget));
						break;
					case OBS_MOVE:
						fixedOffset.ra = 0;
						fixedOffset.dec = 0;
					case OBS_MOVE_FIXED:
						queCommand (new Rts2CommandScriptEnds (getMaster ()));
					case OBS_ALREADY_STARTED:
						break;
				}
			}
			break;
		case EVENT_TEL_SCRIPT_RESYNC:
			cmdChng = NULL;
			checkInterChange ();
			break;
		case EVENT_TEL_SCRIPT_CHANGE:
			cmdChng =
				new Rts2CommandChange ((Rts2CommandChange *) event->getArg (), this);
			checkInterChange ();
			break;
		case EVENT_TEL_SEARCH_START:
			searchEl = (Rts2ScriptElementSearch *) event->getArg ();
			queCommand (new
				Rts2CommandSearch (this, searchEl->getSearchRadius (),
				searchEl->getSearchSpeed ()));
			searchEl->getJob ();
			break;
		case EVENT_TEL_SEARCH_SUCCESS:
			queCommand (new Rts2CommandSearchStop (this));
			break;
		case EVENT_ENTER_WAIT:
			if (cmdChng)
			{
				queCommand (cmdChng);
				cmdChng = NULL;
			}
			else
			{
				ret = syncTarget ();
				if (ret == OBS_DONT_MOVE)
				{
					postEvent (new Rts2Event (EVENT_MOVE_OK));
				}
			}
			break;
		case EVENT_MOVE_OK:
		case EVENT_CORRECTING_OK:
		case EVENT_MOVE_FAILED:
			break;
		case EVENT_ADD_FIXED_OFFSET:
			offset = (ln_equ_posn *) event->getArg ();
			// ra hold offset in HA - that increase on west
			// but we get offset in RA, which increase on east
			fixedOffset.ra += offset->ra;
			fixedOffset.dec += offset->dec;
			break;
		case EVENT_ACQUSITION_END:
			break;
		case EVENT_TEL_START_GUIDING:
			gp = (GuidingParams *) event->getArg ();
			queCommand (new
				Rts2CommandStartGuide (getMaster (), gp->dir, gp->dist));
			break;
		case EVENT_TEL_STOP_GUIDING:
			queCommand (new Rts2CommandStopGuideAll (getMaster ()));
			break;
	}
	Rts2DevClientTelescopeImage::postEvent (event);
}


int
Rts2DevClientTelescopeExec::syncTarget ()
{
	struct ln_equ_posn coord;
	int ret;
	if (!currentTarget)
		return -1;
	getEqu (&coord);
	ret = currentTarget->startSlew (&coord);
	switch (ret)
	{
		case OBS_MOVE:
			currentTarget->moveStarted ();
			queCommand (
				new Rts2CommandMove (getMaster (), this, coord.ra, coord.dec),
				BOP_TEL_MOVE
				);
			break;
		case OBS_MOVE_UNMODELLED:
			currentTarget->moveStarted ();
			queCommand (
				new Rts2CommandMoveUnmodelled (getMaster (), this, coord.ra, coord.dec),
				BOP_TEL_MOVE
				);
			break;
		case OBS_MOVE_FIXED:
			currentTarget->moveStarted ();
			logStream (MESSAGE_DEBUG)
				<< "Rts2DevClientTelescopeExec::syncTarget ha "
				<< coord.ra << " dec " << coord.dec
				<< " oha " << fixedOffset.ra << " odec " << fixedOffset.
				dec << sendLog;
			// we are ofsetting in HA, but offset is in RA - hence -
			queCommand (new
				Rts2CommandMoveFixed (getMaster (), this,
				coord.ra - fixedOffset.ra,
				coord.dec + fixedOffset.dec));
			break;
		case OBS_ALREADY_STARTED:
			currentTarget->moveStarted ();
			if (fixedOffset.ra != 0 || fixedOffset.dec != 0)
			{
			#ifdef DEBUG_EXTRA
				logStream (MESSAGE_DEBUG)
					<< "Rts2DevClientTelescopeExec::syncTarget resync offsets: ra "
					<< fixedOffset.ra << " dec " << fixedOffset.dec << sendLog;
			#endif
				queCommand (new
					Rts2CommandChange (this, fixedOffset.ra,
					fixedOffset.dec));
				fixedOffset.ra = 0;
				fixedOffset.dec = 0;
				break;
			}
			queCommand (new
				Rts2CommandResyncMove (getMaster (), this,
				coord.ra, coord.dec));
			break;
		case OBS_DONT_MOVE:
			break;
	}
	return ret;
}


void
Rts2DevClientTelescopeExec::checkInterChange ()
{
	int waitNum = 0;
	getMaster ()->
		postEvent (new Rts2Event (EVENT_QUERY_WAIT, (void *) &waitNum));
	if (waitNum == 0)
		getMaster ()->postEvent (new Rts2Event (EVENT_ENTER_WAIT));
}


void
Rts2DevClientTelescopeExec::moveEnd ()
{
	if (currentTarget)
		currentTarget->moveEnded ();
	if (moveWasCorrecting)
		getMaster ()->postEvent (new Rts2Event (EVENT_CORRECTING_OK));
	else
		getMaster ()->postEvent (new Rts2Event (EVENT_MOVE_OK));
	Rts2DevClientTelescopeImage::moveEnd ();
}


void
Rts2DevClientTelescopeExec::moveFailed (int status)
{
	if (status == DEVDEM_E_IGNORE)
	{
		moveEnd ();
		return;
	}
	if (currentTarget && currentTarget->moveWasStarted ())
		currentTarget->moveFailed ();
	Rts2DevClientTelescopeImage::moveFailed (status);
	// move failed, either because of priority change, or because device failure
	if (havePriority ())
		getMaster ()->postEvent (new Rts2Event (EVENT_MOVE_FAILED, (void *) &status));
}


void
Rts2DevClientTelescopeExec::searchEnd ()
{
	Rts2DevClientTelescopeImage::searchEnd ();
	getMaster ()->postEvent (new Rts2Event (EVENT_TEL_SEARCH_END));
}


void
Rts2DevClientTelescopeExec::searchFailed (int status)
{
	Rts2DevClientTelescopeImage::searchFailed (status);
	getMaster ()->postEvent (new Rts2Event (EVENT_TEL_SEARCH_END));
}


Rts2DevClientMirrorExec::Rts2DevClientMirrorExec (Rts2Conn * in_connection):Rts2DevClientMirror
(in_connection)
{
}


void
Rts2DevClientMirrorExec::postEvent (Rts2Event * event)
{
	Rts2ScriptElementMirror *se;
	switch (event->getType ())
	{
		case EVENT_MIRROR_SET:
			se = (Rts2ScriptElementMirror *) event->getArg ();
			if (se->isMirrorName (connection->getName ()))
			{
				queCommand (new Rts2CommandMirror (this, se->getMirrorPos ()));
				se->takeJob ();
			}
			break;
	}
	Rts2DevClientMirror::postEvent (event);
}


void
Rts2DevClientMirrorExec::mirrorA ()
{
	getMaster ()->postEvent (new Rts2Event (EVENT_MIRROR_FINISH));
}


void
Rts2DevClientMirrorExec::mirrorB ()
{
	getMaster ()->postEvent (new Rts2Event (EVENT_MIRROR_FINISH));
}


void
Rts2DevClientMirrorExec::moveFailed (int status)
{
	getMaster ()->postEvent (new Rts2Event (EVENT_MIRROR_FINISH));
}
