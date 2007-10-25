#include "rts2devscript.h"
#include "rts2execcli.h"

Rts2DevScript::Rts2DevScript (Rts2Conn * in_script_connection)
{
	currentTarget = NULL;
	nextComd = NULL;
	nextScript = NULL;
	nextTarget = NULL;
	script = NULL;
	blockMove = 0;
	waitScript = NO_WAIT;
	dont_execute_for = -1;
	scriptLoopCount = 0;
	lastTargetObsID = -1;
	script_connection = in_script_connection;
}


Rts2DevScript::~Rts2DevScript (void)
{
	// cannot call deleteScript there, as unsetWait is pure virtual
}


void
Rts2DevScript::startTarget ()
{
	// currentTarget should be nulled when script ends in
	// deleteScript
	#ifdef DEBUG_EXTRA
	logStream (MESSAGE_DEBUG) << "Rts2DevScript::startTarget this " << this <<
		" currentTarget " << currentTarget << " nextTarget " << nextTarget << sendLog;
	#endif						 /* DEBUG_EXTRA */
	if (!currentTarget)
	{
		if (!nextTarget)
			return;
		currentTarget = nextTarget;
		nextScript = NULL;
		nextTarget = NULL;
		if (lastTargetObsID == currentTarget->getObsTargetID ())
			scriptLoopCount++;
		else
			scriptLoopCount = 0;
	}
	else
	{
		scriptLoopCount++;
	}
	setScript (new Rts2Script (script_connection->getMaster (),
		script_connection->getName (), currentTarget));

	clearFailedCount ();
	queCommandFromScript (new
		Rts2CommandScriptEnds (script_connection->
		getMaster ()));

	scriptBegin ();

	delete nextComd;
	nextComd = NULL;

	#ifdef DEBUG_EXTRA
	logStream (MESSAGE_DEBUG) << "Rts2DevScript::startTarget currentTarget " <<
		currentTarget->getTargetID () << " OBS ID " << currentTarget->
		getObsTargetID () << sendLog;
	#endif

	script_connection->getMaster ()->
		postEvent (new Rts2Event (EVENT_SCRIPT_STARTED));
}


void
Rts2DevScript::postEvent (Rts2Event * event)
{
	int sig;
	int acqEnd;
	Rts2Script *tmp_script;
	AcquireQuery *ac;
	switch (event->getType ())
	{
		case EVENT_KILL_ALL:
			currentTarget = NULL;
			if (nextScript)
			{
				tmp_script = nextScript;
				nextScript = NULL;
				delete tmp_script;
			}
			nextTarget = NULL;
		#ifdef DEBUG_EXTRA
			logStream(MESSAGE_DEBUG) << "EVENT_KILL_ALL" << sendLog;
		#endif					 /* DEBUG_EXTRA */
			// stop actual observation..
			blockMove = 0;
			unsetWait ();
			waitScript = NO_WAIT;
			if (script)
			{
				tmp_script = script;
				setScript (NULL);
				delete tmp_script;
			}
			delete nextComd;
			nextComd = NULL;
			// null dont_execute_for
			dont_execute_for = -1;
			break;
		case EVENT_STOP_OBSERVATION:
			deleteScript ();
			break;
		case EVENT_SET_TARGET:
			setNextTarget ((Rts2Target *) event->getArg ());
			if (currentTarget)
				break;
		#ifdef DEBUG_EXTRA
			logStream (MESSAGE_DEBUG) << "EVENT_SET_TARGET " << this << sendLog;
		#endif					 /* DEBUG_EXTRA */
			// we don't have target..so let's observe us
			startTarget ();
			nextCommand ();
			break;
			// when we finish move, we can observe
		case EVENT_MOVE_OK:
		case EVENT_CORRECTING_OK:
			break;
		case EVENT_LAST_READOUT:
		case EVENT_SCRIPT_ENDED:
		#ifdef DEBUG_EXTRA
			logStream (MESSAGE_DEBUG) << "EVENT_SCRIPT_ENDED Rts2DevScript currentTarget " << currentTarget << " nextTarget " << nextTarget << sendLog;
		#endif					 /* DEBUG_EXTRA */
			if (event->getArg ())
				// we get new target..handle that same way as EVENT_OBSERVE command,
				// telescope will not move
				setNextTarget ((Rts2Target *) event->getArg ());
			// we still have something to do
		#ifdef DEBUG_EXTRA
			logStream (MESSAGE_DEBUG) << "EVENT_LAST_READOUT " << currentTarget << " next " << nextTarget << " arg " << event->getArg () << sendLog;
		#endif					 /* DEBUG_EXTRA */
			if (currentTarget)
				break;
			// currentTarget is defined - tested in if (event->getArg ())
		case EVENT_OBSERVE:
			// we can start script before we get EVENT_OBSERVE - don't start us in such case
			if (currentTarget)
			{
				// only check if we have something to do
				nextCommand ();
				break;
			}
		#ifdef DEBUG_EXTRA
			logStream (MESSAGE_DEBUG) << "EVENT_OBSERVE " << this << " current " << currentTarget << " next " << nextTarget << sendLog;
		#endif					 /* DEBUG_EXTRA */
			startTarget ();
			nextCommand ();
			break;
		case EVENT_CLEAR_WAIT:
			clearWait ();
			nextCommand ();
			// if we are still exposing, exposureEnd/readoutEnd will query new command
			break;
		case EVENT_MOVE_QUESTION:
			if (blockMove)
			{
				((Rts2ValueInteger *) event->getArg ())->inc ();
			}
			break;
		case EVENT_SCRIPT_RUNNING_QUESTION:
			// either we have script or there are some commands in que
			if ((script && !script->isLastCommand ())
				|| !script_connection->queEmpty ()
				|| blockMove || script_connection->getRealState () != 0)
				(*((int *) event->getArg ()))++;
			break;
		case EVENT_OK_ASTROMETRY:
		case EVENT_NOT_ASTROMETRY:
		case EVENT_STAR_DATA:
			if (script)
			{
				script->postEvent (new Rts2Event (event));
				if (isWaitMove ())
					// get a chance to process updates..
					nextCommand ();
			}
			break;
		case EVENT_MIRROR_FINISH:
			if (script && waitScript == WAIT_MIRROR)
			{
				script->postEvent (new Rts2Event (event));
				waitScript = NO_WAIT;
				nextCommand ();
			}
			break;
		case EVENT_ACQUSITION_END:
			if (waitScript != WAIT_SLAVE)
				break;
			waitScript = NO_WAIT;
		#ifdef DEBUG_EXTRA
			logStream (MESSAGE_DEBUG) <<
				"Rts2DevScript::postEvent EVENT_ACQUSITION_END " <<
				script_connection->getName () << sendLog;
		#endif
			acqEnd = *(int *) event->getArg ();
			switch (acqEnd)
			{
				case NEXT_COMMAND_PRECISION_OK:
					nextCommand ();
					break;
				case -5:		 // failed with script deletion..
					logStream (MESSAGE_DEBUG)
						<< "Rts2DevScript::postEvent EVENT_ACQUSITION_END -5 "
						<< script_connection->getName () << sendLog;
					break;
				case NEXT_COMMAND_PRECISION_FAILED:
					deleteScript ();
					break;
			}
			break;
		case EVENT_ACQUIRE_QUERY:
			ac = (AcquireQuery *) event->getArg ();
			if (currentTarget
				&& ac->tar_id == currentTarget->getObsTargetID ()
				&& currentTarget->isAcquired ())
			{
				// target that was already acquired will not be acquired again
				ac->count = 0;
				break;
			}
			// we have to wait for decesion from next target
			if (nextTarget && ac->tar_id == nextTarget->getObsTargetID ())
			{
				if (nextTarget->isAcquired ())
					ac->count = 0;
				else
					ac->count++;
				break;
			}
			// that's intentional, as acqueryQuery should be send to all
			// scripts for processing
		case EVENT_SIGNAL_QUERY:
			if (script)
				script->postEvent (new Rts2Event (event));
			if (nextScript)
				nextScript->postEvent (new Rts2Event (event));
			break;
		case EVENT_SIGNAL:
			if (!script)
				break;
			sig = *(int *) event->getArg ();
			script->postEvent (new Rts2Event (EVENT_SIGNAL, (void *) &sig));
			if (sig == -1)
			{
				waitScript = NO_WAIT;
				nextCommand ();
			}
			break;
		case EVENT_TEL_SEARCH_END:
		case EVENT_TEL_SEARCH_STOP:
		case EVENT_TEL_SEARCH_SUCCESS:
			if (waitScript != WAIT_SEARCH)
				break;
			waitScript = NO_WAIT;
			if (script)
				script->postEvent (new Rts2Event (event));
			// give script chance to finish searching (e.g. move filter back
			// to 0, then delete script)
			nextCommand ();
			break;
	}
}


void
Rts2DevScript::scriptBegin ()
{
	Rts2DevClient *cli = script_connection->getOtherDevClient ();
	if (cli == NULL)
	{
		return;
	}
	if (script == NULL)
	{
		queCommandFromScript (new
			Rts2CommandChangeValue (cli,
			std::string ("SCRIPREP"),
			'=', 0));
		queCommandFromScript (new
			Rts2CommandChangeValue (cli,
			std::string ("SCRIPT"),
			'=', std::string ("")));
	}
	else
	{
		queCommandFromScript (new
			Rts2CommandChangeValue (cli,
			std::string ("SCRIPREP"),
			'=', scriptLoopCount));
		queCommandFromScript (new
			Rts2CommandChangeValue (cli,
			std::string ("SCRIPT"),
			'=',
			script->
			getWholeScript ()));
	}
}


void
Rts2DevScript::idle ()
{
	if (getScript ())
	{
		int ret = getScript ()->idle ();
		if (ret == NEXT_COMMAND_NEXT)
		{
			nextCommand ();
		}
	}
}


void
Rts2DevScript::deleteScript ()
{
	Rts2Script *tmp_script;
	clearBlockMove ();
	unsetWait ();
	if (waitScript == WAIT_MASTER)
	{
		int acqRet;
		// should not happen
		acqRet = -5;
		#ifdef DEBUG_EXTRA
		logStream (MESSAGE_DEBUG)
			<< "Rts2DevScript::deleteScript sending EVENT_ACQUSITION_END" <<
			sendLog;
		#endif
		script_connection->getMaster ()->
			postEvent (new Rts2Event (EVENT_ACQUSITION_END, (void *) &acqRet));
	}
	// make sure we don't left any garbage for start of observation
	waitScript = NO_WAIT;
	delete nextComd;
	nextComd = NULL;
	if (script)
	{
		if (currentTarget)
		{
			lastTargetObsID = currentTarget->getObsTargetID ();
			if (script->getExecutedCount () == 0)
			{
				dont_execute_for = currentTarget->getTargetID ();
				if (nextTarget
					&& nextTarget->getTargetID () == dont_execute_for)
				{
					tmp_script = nextScript;
					nextScript = NULL;
					delete tmp_script;
					nextTarget = NULL;
				}
			}
		}
		if (getFailedCount () > 0)
		{
			// don't execute us for current target..
			dont_execute_for = currentTarget->getTargetID ();
		}
		tmp_script = script;
		setScript (NULL);
		delete tmp_script;
		currentTarget = NULL;
		// that can result in call to startTarget and
		// therefore nextCommand, which will set nextComd - so we
		// don't want to touch it after that
		script_connection->getMaster ()->
			postEvent (new Rts2Event (EVENT_SCRIPT_ENDED));
	}
}


void
Rts2DevScript::searchSucess ()
{
	script_connection->getMaster ()->
		postEvent (new Rts2Event (EVENT_TEL_SEARCH_SUCCESS));
}


void
Rts2DevScript::setNextTarget (Rts2Target * in_target)
{
	Rts2Script *tmp_script;
	if (nextTarget)
	{
		// we have to free our memory..
		tmp_script = nextScript;
		nextScript = NULL;
		delete tmp_script;
	}
	nextTarget = in_target;
	if (nextTarget->getTargetID () == dont_execute_for)
	{
		nextTarget = NULL;
	}
	else
	{
		nextScript =
			new Rts2Script (script_connection->getMaster (),
			script_connection->getName (), nextTarget);
	}
	#ifdef DEBUG_EXTRA
	logStream (MESSAGE_DEBUG) << "setNextTarget " << nextTarget << sendLog;
	#endif						 /* DEBUG_EXTRA */
}


int
Rts2DevScript::nextPreparedCommand ()
{
	int ret;
	if (nextComd)
		return 0;
	ret = getNextCommand ();
	switch (ret)
	{
		case NEXT_COMMAND_WAITING:
			setWaitMove ();
			break;
		case NEXT_COMMAND_CHECK_WAIT:
			unblockWait ();
			setWaitMove ();
			break;
		case NEXT_COMMAND_RESYNC:
			script_connection->getMaster ()->
				postEvent (new Rts2Event (EVENT_TEL_SCRIPT_RESYNC));
			setWaitMove ();
			break;
		case NEXT_COMMAND_PRECISION_OK:
		case NEXT_COMMAND_PRECISION_FAILED:
			clearWait ();		 // don't wait for mount move - it will not happen
			waitScript = NO_WAIT;
		#ifdef DEBUG_EXTRA
			logStream (MESSAGE_DEBUG)
				<< "Rts2DevScript::nextPreparedCommand sending EVENT_ACQUSITION_END "
				<< script_connection->getName () << " " << ret << sendLog;
		#endif
			script_connection->getMaster ()->
				postEvent (new Rts2Event (EVENT_ACQUSITION_END, (void *) &ret));
			if (ret == NEXT_COMMAND_PRECISION_OK)
			{
				// there wouldn't be a recursion, as Rts2ScriptElement->nextCommand
				// should never return NEXT_COMMAND_PRECISION_OK
				ret = nextPreparedCommand ();
			}
			else
			{
				ret = NEXT_COMMAND_END_SCRIPT;
			}
			break;
		case NEXT_COMMAND_WAIT_ACQUSITION:
			// let's wait for that..
			waitScript = WAIT_SLAVE;
			break;
		case NEXT_COMMAND_ACQUSITION_IMAGE:
			currentTarget->acqusitionStart ();
			script_connection->getMaster ()->
				postEvent (new Rts2Event (EVENT_ACQUIRE_START));
			waitScript = WAIT_MASTER;
			break;
		case NEXT_COMMAND_WAIT_SIGNAL:
			waitScript = WAIT_SIGNAL;
			break;
		case NEXT_COMMAND_WAIT_MIRROR:
			waitScript = WAIT_MIRROR;
			break;
		case NEXT_COMMAND_WAIT_SEARCH:
			waitScript = WAIT_SEARCH;
			break;
	}
	return ret;
}


int
Rts2DevScript::haveNextCommand ()
{
	int ret;

								 // waiting for script or acqusition
	if (!script || waitScript == WAIT_SLAVE)
	{
		return 0;
	}
	ret = nextPreparedCommand ();
	if (ret < 0)
	{
		deleteScript ();
		#ifdef DEBUG_EXTRA
		logStream (MESSAGE_DEBUG) << "Rts2DevScript::haveNextCommand this " <<
			this << " for connection " << script_connection->
			getName () << " ret " << ret << sendLog;
		#endif					 /* DEBUG_EXTRA */
		startTarget ();
		if (!script)
		{
			return 0;
		}
		ret = nextPreparedCommand ();
		// we don't have any next command:(
		if (ret < 0)
		{
			deleteScript ();
			return 0;
		}
	}
	if (isWaitMove () || waitScript == WAIT_SLAVE || waitScript == WAIT_SIGNAL
		|| waitScript == WAIT_MIRROR || waitScript == WAIT_SEARCH
		|| nextComd == NULL)
		return 0;
								 // some telescope command..
	if (!strcmp (cmd_device, "TX"))
	{
		script_connection->getMaster ()->
			postEvent (new
			Rts2Event (EVENT_TEL_SCRIPT_CHANGE, (void *) nextComd));
		// postEvent have to create copy (in case we will serve more devices) .. so we have to delete command
		delete nextComd;
		nextComd = NULL;
		setWaitMove ();
		blockMove = 1;
		return 0;
	}
	return 1;
}
