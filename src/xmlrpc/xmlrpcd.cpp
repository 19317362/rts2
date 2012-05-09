/* 
 * XML-RPC daemon.
 * Copyright (C) 2007-2012 Petr Kubanek <petr@kubanek.net>
 * Copyright (C) 2007 Stanislav Vitek <standa@iaa.es>
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

#include "xmlrpcd.h"

#ifdef HAVE_PGSQL
#include "rts2db/messagedb.h"
#else
#endif /* HAVE_PGSQL */

#if defined(HAVE_LIBJPEG) && HAVE_LIBJPEG == 1
#include <Magick++.h>
using namespace Magick;
#endif // HAVE_LIBJPEG

#include "r2x.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define OPT_STATE_CHANGE        OPT_LOCAL + 76
#define OPT_NO_EMAILS           OPT_LOCAL + 77

using namespace XmlRpc;

/**
 * @file
 * XML-RPC access to RTS2.
 *
 * @defgroup XMLRPC XML-RPC
 */

using namespace rts2xmlrpc;

void XmlDevInterface::stateChanged (rts2core::ServerState * state)
{
	(getMaster ())->stateChangedEvent (getConnection (), state);
}

void XmlDevInterface::valueChanged (rts2core::Value * value)
{
	changedTimes[value] = getMaster()->getNow ();
	(getMaster ())->valueChangedEvent (getConnection (), value);
}

double XmlDevInterface::getValueChangedTime (rts2core::Value *value)
{
	std::map <rts2core::Value *, double>::iterator iter = changedTimes.find (value);
	if (iter == changedTimes.end ())
		return rts2_nan ("f");
	return iter->second;
}

XmlDevCameraClient::XmlDevCameraClient (rts2core::Connection *conn):rts2script::DevClientCameraExec (conn), XmlDevInterface (), nexpand (""), screxpand (""), currentTarget (this)
{
	previmage = NULL;
	scriptKillCommand = NULL;

	Configuration::instance ()->getString ("xmlrpcd", "images_path", path, "/tmp");
	Configuration::instance ()->getString ("xmlrpcd", "images_name", fexpand, "xmlrpcd_%c.fits");

	createOrReplaceValue (lastFilename, conn, RTS2_VALUE_STRING, "_lastimage", "last image from camera", false, RTS2_VALUE_WRITABLE);
	createOrReplaceValue (callScriptEnds, conn, RTS2_VALUE_BOOL, "_callscriptends", "call script ends before executing script on device", false);
	callScriptEnds->setValueBool (false);

	createOrReplaceValue (scriptRunning, conn, RTS2_VALUE_BOOL, "_scriptrunning", "if script is running on device", false);
	scriptRunning->setValueBool (false);

	createOrReplaceValue (scriptStart, conn, RTS2_VALUE_TIME, "_script_start", "script start time", false);
	createOrReplaceValue (scriptEnd, conn, RTS2_VALUE_TIME, "_script_end", "script end time", false);

	createOrReplaceValue (exposureWritten, conn, RTS2_VALUE_INTEGER, "_images_to_write", "number of images to write", false);
}

XmlDevCameraClient::~XmlDevCameraClient ()
{
	if (exposureScript.get ())
	{
		if (!exposureScript->knowImage (previmage))
			delete previmage;
		else
			previmage->unkeepImage ();
	}
	else
	{
		delete previmage;
	}
}

void XmlDevCameraClient::deleteConnection (Connection *_conn)
{
	if (_conn == getConnection ())
	{
		scriptRunning->setValueBool (false);
		getMaster ()->sendValueAll (scriptRunning);
	}
}

rts2image::Image *XmlDevCameraClient::createImage (const struct timeval *expStart)
{
 	exposureScript = getScript ();
	std::string usp;
	bool overwrite = false;
	if (nexpand.length () == 0)
	{
		if (screxpand.length () == 0)
		{
			usp = fexpand;
		}
		else
		{
			usp = screxpand;
		}
	}
	else
	{
		usp = nexpand;
		overwrite = getOverwrite ();
	}

	std::string imagename = path + '/' + usp;
	// make nexpand available for next exposure
	nexpand = std::string ("");
	setOverwrite (false);

	rts2image::Image * ret = new rts2image::Image ((imagename).c_str (), getExposureNumber (), expStart, connection, overwrite);

	ret->keepImage ();

	lastFilename->setValueCharArr (ret->getFileName ());
	((rts2core::Daemon *) (connection->getMaster ()))->sendValueAll (lastFilename);
	return ret;
}

void XmlDevCameraClient::scriptProgress (double start, double end)
{
	scriptStart->setValueDouble (start);
	scriptEnd->setValueDouble (end);
	getMaster ()->sendValueAll (scriptStart);
	getMaster ()->sendValueAll (scriptEnd);

	((XmlRpcd *) getMaster ())->scriptProgress (start, end);
}

void XmlRpcd::doOpValue (const char *v_name, char oper, const char *operand)
{
	Value *ov = getOwnValue (v_name);
	if (ov == NULL)
		throw JSONException ("cannot find value");
	Value *nv = duplicateValue (ov, false);
	nv->setValueCharArr (operand);
	nv->doOpValue (oper, ov);
	int ret = setValue (ov, nv);
	if (ret < 0)
	{
		delete nv;
		throw JSONException ("cannot set value");
	}
	if (!ov->isEqual (nv))
	{
		ov->setFromValue (nv);
		valueChanged (ov);
	}
	delete nv;
}

void XmlRpcd::clientNewDataConn (rts2core::Connection *conn, int data_conn)
{
	for (std::list <AsyncAPI *>::iterator iter = asyncAPIs.begin (); iter != asyncAPIs.end (); iter++)
		(*iter)->newDataConn (conn, data_conn);
}

void XmlRpcd::clientDataReceived (rts2core::Connection *conn, rts2core::DataAbstractRead *data)
{
	for (std::list <AsyncAPI *>::iterator iter = asyncAPIs.begin (); iter != asyncAPIs.end (); iter++)
		(*iter)->dataReceived (conn, data);
}

void XmlRpcd::clientFullDataReceived (rts2core::Connection *conn, rts2core::DataChannels *data)
{
	for (std::list <AsyncAPI *>::iterator iter = asyncAPIs.begin (); iter != asyncAPIs.end (); iter++)
		(*iter)->fullDataReceived (conn, data);
}

void XmlRpcd::clientExposureFailed (Connection *conn, int status)
{
	for (std::list <AsyncAPI *>::iterator iter = asyncAPIs.begin (); iter != asyncAPIs.end (); iter++)
		(*iter)->exposureFailed (conn, status);
}

void XmlDevCameraClient::setCallScriptEnds (bool nv)
{
	callScriptEnds->setValueBool (nv);
	getMaster ()->sendValueAll (callScriptEnds);
}

bool XmlDevCameraClient::isScriptRunning ()
{
	int runningScripts = 0;

	if (exposureScript.get ())
	{
		// if there are some images which need to be written
		connection->postEvent (new Event (EVENT_NUMBER_OF_IMAGES, (void *) &runningScripts));
		exposureWritten->setValueInteger (runningScripts);
		getMaster ()->sendValueAll (exposureWritten);
		if (runningScripts > 0)
			return true;
	}
	else
	{
		exposureWritten->setValueInteger (0);
		getMaster ()->sendValueAll (exposureWritten);
	}

	connection->postEvent (new Event (EVENT_SCRIPT_RUNNING_QUESTION, (void *) &runningScripts));
	if (runningScripts > 0)
		return true;
	
	return false;
}

void XmlDevCameraClient::executeScript (const char *scriptbuf, bool killScripts)
{
	currentscript = std::string (scriptbuf);
	// verify that the script can be parsed
	try
	{
		rts2script::Script sc (currentscript.c_str ());
		sc.parseScript (&currentTarget);
		int failedcount = sc.getFaultLocation ();
		if (failedcount != -1)
		{
			std::ostringstream er;
			er << "parsing of script '" << currentscript << " failed at position " << failedcount;
			throw JSONException (er.str ());
		}
	}
	catch (rts2script::ParsingError &er)
	{
		throw JSONException (er.what ());
	}
	
	if (killScripts)
	{
		if (scriptRunning->getValueBool ())
			logStream (MESSAGE_INFO) << "killing currently running script" << sendLog;
		postEvent (new Event (EVENT_KILL_ALL));
		if (callScriptEnds->getValueBool ())
			connection->queCommand (scriptKillCommand = new rts2core::CommandKillAll (connection->getMaster ()), 0, this);
		else
			connection->queCommand (scriptKillCommand = new rts2core::CommandKillAllWithoutScriptEnds (connection->getMaster ()), 0, this);
	}
	else
	{
		if (getScript ().get ())
		{
			std::ostringstream er;
			er << "there is script running on device " << getName () << ", please kill it first";
			throw JSONException (er.str ());
		}
		else
		{
			connection->postEvent (new Event (callScriptEnds->getValueBool () ? EVENT_SET_TARGET : EVENT_SET_TARGET_NOT_CLEAR, (void *) &currentTarget));
			connection->postEvent (new Event (EVENT_OBSERVE));
		}
	}
}

void XmlDevCameraClient::killScript ()
{
	if (scriptRunning->getValueBool ())
		logStream (MESSAGE_INFO) << "killing currently running script" << sendLog;
	postEvent (new Event (EVENT_KILL_ALL));
	if (callScriptEnds->getValueBool ())
		connection->queCommand (new rts2core::CommandKillAll (connection->getMaster ()));
	else
		connection->queCommand (new rts2core::CommandKillAllWithoutScriptEnds (connection->getMaster ()));
	scriptRunning->setValueBool (false);
	getMaster ()->sendValueAll (scriptRunning);
}

void XmlDevCameraClient::setExpandPath (const char *fe)
{
	nexpand = std::string (fe);
	screxpand = std::string ("");
}

void XmlDevCameraClient::setScriptExpand (const char *fe)
{
	screxpand = std::string (fe);
	nexpand = std::string ("");
}

void XmlDevCameraClient::newDataConn (int data_conn)
{
	getMaster ()->clientNewDataConn (getConnection (), data_conn);
	rts2script::DevClientCameraExec::newDataConn (data_conn);
}

void XmlDevCameraClient::dataReceived (DataAbstractRead *data)
{
	getMaster ()->clientDataReceived (getConnection (), data);
	rts2script::DevClientCameraExec::dataReceived (data);
}

void XmlDevCameraClient::fullDataReceived (int data_conn, DataChannels *data)
{
	getMaster ()->clientFullDataReceived (getConnection (), data);
	rts2script::DevClientCameraExec::fullDataReceived (data_conn, data);
}

void XmlDevCameraClient::exposureFailed (int status)
{
	rts2script::DevClientCameraExec::exposureFailed (status);
	getMaster ()->clientExposureFailed (getConnection (), status);
}

void XmlDevCameraClient::postEvent (Event *event)
{
	switch (event->getType ())
	{
		case EVENT_COMMAND_OK:
			if (scriptKillCommand && event->getArg () == scriptKillCommand)
			{
				scriptKillCommand = NULL;
				connection->postEvent (new Event (callScriptEnds->getValueBool () ? EVENT_SET_TARGET : EVENT_SET_TARGET_NOT_CLEAR, (void *) &currentTarget));
				connection->postEvent (new Event (EVENT_OBSERVE));
			}
			break;
		case EVENT_SCRIPT_STARTED:
		case EVENT_SCRIPT_ENDED:
		case EVENT_LAST_READOUT:
		case EVENT_ALL_IMAGES_WRITTEN:
			scriptRunning->setValueBool (isScriptRunning ());
			getMaster ()->sendValueAll (scriptRunning);
			break;
	}
	rts2script::DevClientCameraExec::postEvent (event);
}

rts2image::imageProceRes XmlDevCameraClient::processImage (rts2image::Image * image)
{
	if (exposureScript.get ())
	{
		exposureScript->processImage (image);
		if (!exposureScript->knowImage (previmage))
			delete previmage;
		else
			previmage->unkeepImage ();
	}
	else
	{
		delete previmage;
		if (scriptRunning->getValueBool ())
		{
			logStream (MESSAGE_WARNING) << "received data for script which is not running, lowering isRunning flag" << sendLog;
			scriptRunning->setValueBool (false);
			getMaster ()->sendValueAll (scriptRunning);
		}
	}
	previmage = image;
	return rts2image::IMAGE_KEEP_COPY;
}

int XmlRpcd::idle ()
{
	// delete freed async, check for shared memory data
	for (std::list <AsyncAPI *>::iterator iter = asyncAPIs.begin (); iter != asyncAPIs.end ();)
	{
		if ((*iter)->idle ())
		{
			delete *iter;
			iter = asyncAPIs.erase (iter);
			numberAsyncAPIs->setValueInteger (asyncAPIs.size ());
			sendValueAll (numberAsyncAPIs);
		}
		else
		{
			iter++;
		}
	}
	// 
#ifdef HAVE_PGSQL
	return DeviceDb::idle ();
#else
	return rts2core::Device::idle ();
#endif
}

#ifndef HAVE_PGSQL
int XmlRpcd::willConnect (NetworkAddress *_addr)
{
	if (_addr->getType () < getDeviceType ()
		|| (_addr->getType () == getDeviceType ()
		&& strcmp (_addr->getName (), getDeviceName ()) < 0))
		return 1;
	return 0;
}
#endif

int XmlRpcd::processOption (int in_opt)
{
	switch (in_opt)
	{
		case 'p':
			rpcPort = atoi (optarg);
			break;
		case OPT_STATE_CHANGE:
			stateChangeFile = optarg;
			break;
		case OPT_NO_EMAILS:
			send_emails->setValueBool (false);
			break;
#ifdef HAVE_PGSQL
		default:
			return DeviceDb::processOption (in_opt);
#else
		case OPT_CONFIG:
			config_file = optarg;
			break;
		default:
			return rts2core::Device::processOption (in_opt);
#endif
	}
	return 0;
}

int XmlRpcd::init ()
{
#ifdef HAVE_PGSQL
	int ret = DeviceDb::init ();
#else
	int ret = rts2core::Device::init ();
#endif
	if (ret)
		return ret;

	ret = notifyConn->init ();
	if (ret)
		return ret;

	addConnection (notifyConn);

	if (printDebug ())
		XmlRpc::setVerbosity (5);

	XmlRpcServer::bindAndListen (rpcPort);
	XmlRpcServer::enableIntrospection (true);

	// try states..
	if (stateChangeFile != NULL)
	{
		try
		{
		  	reloadEventsFile ();
		}
		catch (XmlError ex)
		{
			logStream (MESSAGE_ERROR) << ex << sendLog;
			return -1;
		}
	}

	for (std::vector <DirectoryMapping>::iterator iter = events.dirs.begin (); iter != events.dirs.end (); iter++)
		directories.push_back (new Directory (iter->getTo (), iter->getPath (), "", this));
	
	if (events.docroot.length () > 0)
		XmlRpcServer::setDefaultGetRequest (new Directory (NULL, events.docroot.c_str (), "index.html", NULL));
	if (events.defchan != INT_MAX)
		defchan = events.defchan;
	else
		defchan = 0;

	setMessageMask (MESSAGE_MASK_ALL);

	if (events.bbServers.size () != 0)
		addTimer (30, new Event (EVENT_XMLRPC_BB));

#ifndef HAVE_PGSQL
	ret = Configuration::instance ()->loadFile (config_file);
	if (ret)
		return ret;
	// load users-login pairs
	std::string lf;
	Configuration::instance ()->getString ("xmlrpcd", "logins", lf, RTS2_PREFIX "/etc/rts2/logins");

	userLogins.load (lf.c_str ());
#endif
	// get page prefix
	Configuration::instance ()->getString ("xmlrpcd", "page_prefix", page_prefix, "");

	// auth_localhost
	auth_localhost = Configuration::instance ()->getBoolean ("xmlrpcd", "auth_localhost", auth_localhost);

#ifdef HAVE_LIBJPEG
	Magick::InitializeMagick (".");
#endif /* HAVE_LIBJPEG */
	return ret;
}

void XmlRpcd::addSelectSocks (fd_set &read_set, fd_set &write_set, fd_set &exp_set)
{
#ifdef HAVE_PGSQL
	DeviceDb::addSelectSocks (read_set, write_set, exp_set);
#else
	rts2core::Device::addSelectSocks (read_set, write_set, exp_set);
#endif
	XmlRpcServer::addToFd (&read_set, &write_set, &exp_set);
}

void XmlRpcd::selectSuccess (fd_set &read_set, fd_set &write_set, fd_set &exp_set)
{
#ifdef HAVE_PGSQL
	DeviceDb::selectSuccess (read_set, write_set, exp_set);
#else
	rts2core::Device::selectSuccess (read_set, write_set, exp_set);
#endif
	XmlRpcServer::checkFd (&read_set, &write_set, &exp_set);
}

void XmlRpcd::signaledHUP ()
{
#ifdef HAVE_PGSQL
	DeviceDb::signaledHUP ();
#else
	rts2core::Device::signaledHUP ();
#endif
	reloadEventsFile ();
}

void XmlRpcd::connectionRemoved (rts2core::Connection *conn)
{
	for (std::list <AsyncAPI *>::iterator iter = asyncAPIs.begin (); iter != asyncAPIs.end ();)
	{
		if ((*iter)->isForConnection (conn))
		{
			iter = asyncAPIs.erase (iter);
			numberAsyncAPIs->setValueInteger (asyncAPIs.size ());
			sendValueAll (numberAsyncAPIs);
		}
		else
		{
			iter++;
		}
	}
#ifdef HAVE_PGSQL
	DeviceDb::connectionRemoved (conn);
#else
	rts2core::Device::connectionRemoved (conn);
#endif
}

void XmlRpcd::asyncFinished (XmlRpcServerConnection *source)
{
	for (std::list <AsyncAPI *>::iterator iter = asyncAPIs.begin (); iter != asyncAPIs.end (); iter++)
	{
		if ((*iter)->isForSource (source))
			(*iter)->nullSource ();
	}
	XmlRpcServer::asyncFinished (source);
}

void XmlRpcd::removeConnection (XmlRpcServerConnection *source)
{
	for (std::list <AsyncAPI *>::iterator iter = asyncAPIs.begin (); iter != asyncAPIs.end (); iter++)
	{
		if ((*iter)->isForSource (source))
			(*iter)->nullSource ();
	}
	XmlRpcServer::removeConnection (source);
}

#ifdef HAVE_PGSQL
XmlRpcd::XmlRpcd (int argc, char **argv): DeviceDb (argc, argv, DEVICE_TYPE_XMLRPC, "XMLRPC")
#else
XmlRpcd::XmlRpcd (int argc, char **argv): rts2core::Device (argc, argv, DEVICE_TYPE_XMLRPC, "XMLRPC")
#endif
,XmlRpcServer (),
  events(this),
// construct all handling events..
  login (this),
  deviceCount (this),
  listDevices (this),
  deviceByType (this),
  deviceType (this),
  deviceCommand (this),
  masterState (this),
  masterStateIs (this),
  deviceState (this),
  listValues (this),
  listValuesDevice (this),
  listPrettValuesDecice (this),
  _getValue (this),
  _setValue (this),
  setValueByType (this),
  incValue (this),
  _getMessages (this),
#ifdef HAVE_PGSQL
  listTargets (this),
  listTargetsByType (this),
  targetInfo (this),
  targetAltitude (this),
  listTargetObservations (this),
  listMonthObservations (this),
  listImages (this),
  ticketInfo (this),
  recordsValues (this),
  records (this),
  recordsAverage (this),
  userLogin (this),
#endif // HAVE_PGSQL

  // web requeusts
#ifdef HAVE_LIBJPEG
  jpegRequest ("/jpeg", this),
  jpegPreview ("/preview", "/", this),
  downloadRequest ("/download", this),
  current ("/current", this),
#ifdef HAVE_PGSQL
  graph ("/graph", this),
  altAzTarget ("/altaz", this),
#endif // HAVE_PGSQL
  imageReq ("/images", this),
#endif /* HAVE_LIBJPEG */
  fitsRequest ("/fits", this),
  javaScriptRequests ("/js", this),
  cssRequests ("/css", this),
  api ("/api", this),
#ifdef HAVE_PGSQL
  auger ("/auger", this),
  night ("/nights", this),
  observation ("/observations", this),
  targets ("/targets", this),
  addTarget ("/addtarget", this),
  plan ("/plan", this),
#endif // HAVE_PGSQL
  switchState ("/switchstate", this),
  devices ("/devices", this)
{
	rpcPort = 8889;
	stateChangeFile = NULL;
	defLabel = "%Y-%m-%d %H:%M:%S @OBJECT";

	auth_localhost = true;

	notifyConn = new rts2core::ConnNotify (this);

	createValue (numberAsyncAPIs, "async_APIs", "number of active async APIs", false);
	createValue (sumAsync, "async_sum", "total number of async APIs", false);
	sumAsync->setValueInteger (0);

	createValue (send_emails, "send_email", "if XML-RPC is allowed to send emails", false, RTS2_VALUE_WRITABLE);
	send_emails->setValueBool (true);

	createValue (bbCadency, "bb_cadency", "cadency (in seconds) of upstream BB messages", false, RTS2_VALUE_WRITABLE);
	bbCadency->setValueInteger (60);

#ifndef HAVE_PGSQL
	config_file = NULL;

	addOption (OPT_CONFIG, "config", 1, "configuration file");
#endif
	addOption ('p', NULL, 1, "XML-RPC port. Default to 8889");
	addOption (OPT_STATE_CHANGE, "event-file", 1, "event changes file, list commands which are executed on state change");
	addOption (OPT_NO_EMAILS, "no-emails", 0, "do not send emails");
	XmlRpc::setVerbosity (0);
}


XmlRpcd::~XmlRpcd ()
{
	for (std::vector <Directory *>::iterator id = directories.begin (); id != directories.end (); id++)
		delete *id;

	for (std::map <std::string, Session *>::iterator iter = sessions.begin (); iter != sessions.end (); iter++)
	{
		delete (*iter).second;
	}
	sessions.clear ();
#ifdef HAVE_LIBJPEG
	MagickLib::DestroyMagick ();
#endif /* HAVE_LIBJPEG */
}

rts2core::DevClient * XmlRpcd::createOtherType (rts2core::Connection * conn, int other_device_type)
{
	switch (other_device_type)
	{
		case DEVICE_TYPE_MOUNT:
			return new XmlDevTelescopeClient (conn);
		case DEVICE_TYPE_CCD:
			return new XmlDevCameraClient (conn);
		case DEVICE_TYPE_FOCUS:
			return new XmlDevFocusClient (conn);
		default:
			return new XmlDevClient (conn);
	}
}

void XmlRpcd::stateChangedEvent (rts2core::Connection * conn, rts2core::ServerState * new_state)
{
	double now = getNow ();
	// look if there is some state change command entry, which match us..
	for (StateCommands::iterator iter = events.stateCommands.begin (); iter != events.stateCommands.end (); iter++)
	{
		StateChange *sc = (*iter);
		const char *name;
		if (conn->getOtherType () == DEVICE_TYPE_SERVERD)
			name = "centrald";
		else
			name = conn->getName ();
		if (sc->isForDevice (name, conn->getOtherType ()) && sc->executeOnStateChange (new_state->getOldValue (), new_state->getValue ()))
		{
			sc->run (this, conn, now);
		}
	}
}

void XmlRpcd::valueChangedEvent (rts2core::Connection * conn, rts2core::Value * new_value)
{
	double now = getNow ();
	// look if there is some state change command entry, which match us..
	for (ValueCommands::iterator iter = events.valueCommands.begin (); iter != events.valueCommands.end (); iter++)
	{
		ValueChange *vc = (*iter);
		const char *name;
		if (conn->getOtherType () == DEVICE_TYPE_SERVERD)
			name = "centrald";
		else
			name = conn->getName ();
		if (vc->isForValue (name, new_value->getName (), now))
		 
		{
			try
			{
				vc->run (new_value, now);
				vc->runSuccessfully (now);
			}
			catch (rts2core::Error err)
			{
				logStream (MESSAGE_ERROR) << err << sendLog;
			}
		}
	}
}

void XmlRpcd::message (Message & msg)
{
// log message to DB, if database is present
#ifdef HAVE_PGSQL
	if (msg.isNotDebug ())
	{
		rts2db::MessageDB msgDB (msg);
		msgDB.insertDB ();
	}
#endif
	// look if there is some state change command entry, which match us..
	for (MessageCommands::iterator iter = events.messageCommands.begin (); iter != events.messageCommands.end (); iter++)
	{
		MessageEvent *me = (*iter);
		if (me->isForMessage (&msg))
		{
			try
			{
				me->run (&msg);
			}
			catch (rts2core::Error err)
			{
				logStream (MESSAGE_ERROR) << err << sendLog;
			}
		}
	}
	
	while (messages.size () > 42) // messagesBufferSize ())
	{
		messages.pop_front ();
	}

	messages.push_back (msg);
}

std::string XmlRpcd::addSession (std::string _username, time_t _timeout)
{
	Session *s = new Session (_username, time(NULL) + _timeout);
	sessions[s->getSessionId()] = s;
	return s->getSessionId ();
}

bool XmlRpcd::existsSession (std::string sessionId)
{
	std::map <std::string, Session*>::iterator iter = sessions.find (sessionId);
	if (iter == sessions.end ())
	{
		return false;
	}
	return true;
}

void XmlRpcd::postEvent (Event *event)
{
	switch (event->getType ())
	{
		case EVENT_XMLRPC_BB:
			sendBB ();
			addTimer (bbCadency->getValueInteger (), event);
			return;
	}
#ifdef HAVE_PGSQL
	DeviceDb::postEvent (event);
#else
	rts2core::Device::postEvent (event);
#endif
}

bool XmlRpcd::isPublic (struct sockaddr_in *saddr, const std::string &path)
{
	if (authorizeLocalhost () || ntohl (saddr->sin_addr.s_addr) != INADDR_LOOPBACK)
		return events.isPublic (path);
	return true;
}

const char *XmlRpcd::getDefaultImageLabel ()
{
	return defLabel;
}

void XmlRpcd::scriptProgress (double start, double end)
{
	maskState (EXEC_MASK_SCRIPT, EXEC_SCRIPT_RUNNING, "script running", start, end);
}

void XmlRpcd::sendBB ()
{
	// construct data..
	XmlRpcValue data;

	connections_t::iterator iter;

	for (iter = getConnections ()->begin (); iter != getConnections ()->end (); iter++)
	{
		XmlRpcValue connData;
		connectionValuesToXmlRpc (*iter, connData, false);
		data[(*iter)->getName ()] = connData;
	}

	events.bbServers.sendUpdate (&data);
}

void XmlRpcd::reloadEventsFile ()
{
	if (stateChangeFile != NULL)
	{
		events.load (stateChangeFile);
	  	defLabel = events.getDefaultImageLabel ();
		if (defLabel == NULL)
			defLabel = "%Y-%m-%d %H:%M:%S @OBJECT";
	}
}

#ifndef HAVE_PGSQL
bool XmlRpcd::verifyUser (std::string username, std::string pass, bool &executePermission)
{
	return userLogins.verifyUser (username, pass, executePermission);
}

bool rts2xmlrpc::verifyUser (std::string username, std::string pass, bool &executePermission)
{
	return ((XmlRpcd *) getMasterApp ())->verifyUser (username, pass, executePermission);
}
#endif /* HAVE_PGSQL */

int main (int argc, char **argv)
{
	XmlRpcd device (argc, argv);
	return device.run ();
}
