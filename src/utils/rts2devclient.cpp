#include <algorithm>

#include "rts2block.h"
#include "rts2command.h"
#include "rts2devclient.h"

Rts2DevClient::Rts2DevClient (Rts2Conn * in_connection):Rts2Object ()
{
  connection = in_connection;
  processedBaseInfo = NOT_PROCESED;

  waiting = NOT_WAITING;

  failedCount = 0;
}

Rts2DevClient::~Rts2DevClient ()
{
  unblockWait ();
}

void
Rts2DevClient::postEvent (Rts2Event * event)
{
  switch (event->getType ())
    {
    case EVENT_QUERY_WAIT:
      if (waiting == WAIT_NOT_POSSIBLE)
	*((int *) event->getArg ()) = *((int *) event->getArg ()) + 1;
      break;
    case EVENT_ENTER_WAIT:
      waiting = WAIT_MOVE;
      break;
    case EVENT_CLEAR_WAIT:
      clearWait ();
      break;
    }
  Rts2Object::postEvent (event);
}

int
Rts2DevClient::command ()
{
  return -2;
}

void
Rts2DevClient::stateChanged (Rts2ServerState * state)
{
  if (connection->getErrorState () == DEVICE_ERROR_HW)
    incFailedCount ();
}

void
Rts2DevClient::priorityInfo (bool have)
{
  if (have)
    getPriority ();
  else
    lostPriority ();
}

void
Rts2DevClient::getPriority ()
{
}

void
Rts2DevClient::lostPriority ()
{
}

void
Rts2DevClient::died ()
{
  lostPriority ();
}

void
Rts2DevClient::blockWait ()
{
  waiting = WAIT_NOT_POSSIBLE;
}

void
Rts2DevClient::unblockWait ()
{
  if (waiting == WAIT_NOT_POSSIBLE)
    {
      int numNonWaits = 0;
      waiting = NOT_WAITING;
      connection->getMaster ()->
	postEvent (new Rts2Event (EVENT_QUERY_WAIT, (void *) &numNonWaits));
      if (numNonWaits == 0)	// still zero, enter wait
	connection->getMaster ()->
	  postEvent (new Rts2Event (EVENT_ENTER_WAIT));
    }
}

void
Rts2DevClient::unsetWait ()
{
  unblockWait ();
  clearWait ();
}

int
Rts2DevClient::isWaitMove ()
{
  return (waiting == WAIT_MOVE);
}

void
Rts2DevClient::clearWait ()
{
  waiting = NOT_WAITING;
}

void
Rts2DevClient::setWaitMove ()
{
  if (waiting == NOT_WAITING)
    waiting = WAIT_MOVE;
}

int
Rts2DevClient::getStatus ()
{
  return connection->getState ();
}

void
Rts2DevClient::idle ()
{
}

Rts2DevClientCamera::Rts2DevClientCamera (Rts2Conn * in_connection):Rts2DevClient
  (in_connection)
{
}

void
Rts2DevClientCamera::exposureStarted ()
{
}

void
Rts2DevClientCamera::exposureEnd ()
{
}

void
Rts2DevClientCamera::exposureFailed (int status)
{
}

void
Rts2DevClientCamera::readoutEnd ()
{
}

void
Rts2DevClientCamera::stateChanged (Rts2ServerState * state)
{
  switch (state->
	  getValue () & (CAM_MASK_EXPOSE | CAM_MASK_READING | CAM_MASK_DATA))
    {
    case CAM_EXPOSING:
      if (connection->getErrorState () == DEVICE_NO_ERROR)
	exposureStarted ();
      else
	exposureFailed (connection->getErrorState ());
      break;
    case CAM_DATA:
      if (connection->getErrorState () == DEVICE_NO_ERROR)
	exposureEnd ();
      else
	exposureFailed (connection->getErrorState ());
      break;
    case CAM_NODATA | CAM_NOTREADING | CAM_NOEXPOSURE:
      if (connection->getErrorState () == DEVICE_NO_ERROR)
	readoutEnd ();
      else
	exposureFailed (connection->getErrorState ());
      break;
    }
  Rts2DevClient::stateChanged (state);
}

bool
Rts2DevClientCamera::isIdle ()
{
  return ((connection->
	   getState () & (CAM_MASK_EXPOSE | CAM_MASK_DATA |
			  CAM_MASK_READING)) ==
	  (CAM_NOEXPOSURE | CAM_NODATA | CAM_NOTREADING));
}

bool
Rts2DevClientCamera::isExposing ()
{
  return (connection->getState () && CAM_MASK_EXPOSE) == CAM_EXPOSING;
}

bool
Rts2DevClientCamera::handlingData ()
{
  return ((connection->
	   getState () & CAM_MASK_DATA) ||
	  (connection->getState () & CAM_MASK_DATA));
}

Rts2DevClientTelescope::Rts2DevClientTelescope (Rts2Conn * in_connection):Rts2DevClient
  (in_connection)
{
}

Rts2DevClientTelescope::~Rts2DevClientTelescope (void)
{
  moveFailed (DEVICE_ERROR_HW);
}

void
Rts2DevClientTelescope::stateChanged (Rts2ServerState * state)
{
  switch (state->getValue () & TEL_MASK_COP_MOVING)
    {
    case TEL_MOVING:
    case TEL_MOVING | TEL_WAIT_COP:
    case TEL_PARKING:
      moveStart (state->getValue () & TEL_CORRECTING);
      break;
    case TEL_OBSERVING:
    case TEL_PARKED:
      if (connection->getErrorState () == DEVICE_NO_ERROR)
	moveEnd ();
      else
	moveFailed (connection->getErrorState ());
      break;
    }
  switch (state->getValue () & TEL_MASK_SEARCHING)
    {
    case TEL_SEARCH:
      searchStart ();
      break;
    case TEL_NOSEARCH:
      if (connection->getErrorState () == DEVICE_NO_ERROR)
	searchEnd ();
      else
	searchFailed (connection->getErrorState ());
      break;
    }
  Rts2DevClient::stateChanged (state);
}

void
Rts2DevClientTelescope::moveStart (bool correcting)
{
  moveWasCorrecting = correcting;
}

void
Rts2DevClientTelescope::moveEnd ()
{
  moveWasCorrecting = false;
}

void
Rts2DevClientTelescope::searchStart ()
{
}

void
Rts2DevClientTelescope::searchEnd ()
{
}

void
Rts2DevClientTelescope::postEvent (Rts2Event * event)
{
  bool qe;
  switch (event->getType ())
    {
    case EVENT_QUICK_ENABLE:
      qe = *((bool *) event->getArg ());
      connection->
	queCommand (new
		    Rts2CommandChangeValue (this,
					    std::string ("quick_enabled"),
					    '=',
					    qe ? std::string ("2") : std::
					    string ("1")));
      break;
    }
  Rts2DevClient::postEvent (event);
}

Rts2DevClientDome::Rts2DevClientDome (Rts2Conn * in_connection):Rts2DevClient
  (in_connection)
{
}

Rts2DevClientCupola::Rts2DevClientCupola (Rts2Conn * in_connection):Rts2DevClientDome
  (in_connection)
{
}

void
Rts2DevClientCupola::syncStarted ()
{
}

void
Rts2DevClientCupola::syncEnded ()
{
}

void
Rts2DevClientCupola::syncFailed (int status)
{
}

void
Rts2DevClientCupola::stateChanged (Rts2ServerState * state)
{
  switch (state->getValue () & DOME_COP_MASK_SYNC)
    {
    case DOME_COP_NOT_SYNC:
      if (connection->getErrorState ())
	syncFailed (state->getValue ());
      else
	syncStarted ();
      break;
    case DOME_COP_SYNC:
      if (connection->getErrorState ())
	syncFailed (state->getValue ());
      else
	syncEnded ();
      break;
    }
  Rts2DevClientDome::stateChanged (state);
}

Rts2DevClientMirror::Rts2DevClientMirror (Rts2Conn * in_connection):Rts2DevClient
  (in_connection)
{
}

Rts2DevClientMirror::~Rts2DevClientMirror (void)
{
  moveFailed (DEVICE_ERROR_HW);
}

void
Rts2DevClientMirror::stateChanged (Rts2ServerState * state)
{
  if (connection->getErrorState ())
    {
      moveFailed (connection->getErrorState ());
    }
  else
    {
      switch (state->getValue () & MIRROR_MASK)
	{
	case MIRROR_A:
	  mirrorA ();
	  break;
	case MIRROR_B:
	  mirrorB ();
	  break;
	case MIRROR_UNKNOW:
	  // strange, but that can happen
	  moveFailed (DEVICE_ERROR_HW);
	  break;
	}
    }
  Rts2DevClient::stateChanged (state);
}

void
Rts2DevClientMirror::mirrorA ()
{
}

void
Rts2DevClientMirror::mirrorB ()
{
}

void
Rts2DevClientMirror::moveFailed (int status)
{
}

Rts2DevClientPhot::Rts2DevClientPhot (Rts2Conn * in_connection):Rts2DevClient
  (in_connection)
{
  lastCount = -1;
  lastExp = -1.0;
  integrating = false;
}

Rts2DevClientPhot::~Rts2DevClientPhot ()
{
  integrationFailed (DEVICE_ERROR_HW);
}

int
Rts2DevClientPhot::commandValue (const char *name)
{
  int count;
  float exp;
  int is_ov;
  if (!strcmp (name, "count"))
    {
      if (connection->paramNextInteger (&count)
	  || connection->paramNextFloat (&exp)
	  || connection->paramNextInteger (&is_ov))
	return -3;
      addCount (count, exp, is_ov);
      return 0;
    }
//  return Rts2DevClient::commandValue (name);
}

void
Rts2DevClientPhot::stateChanged (Rts2ServerState * state)
{
  if (state->maskValueChanged (PHOT_MASK_INTEGRATE))
    {
      switch (state->getValue () & PHOT_MASK_INTEGRATE)
	{
	case PHOT_NOINTEGRATE:
	  if (connection->getErrorState () == DEVICE_NO_ERROR)
	    integrationEnd ();
	  else
	    integrationFailed (connection->getErrorState ());
	  break;
	case PHOT_INTEGRATE:
	  integrationStart ();
	  break;
	}
    }
  if (state->maskValueChanged (PHOT_MASK_FILTER))
    {
      switch (state->getValue () & PHOT_MASK_FILTER)
	{
	case PHOT_FILTER_MOVE:
	  filterMoveStart ();
	  break;
	case PHOT_FILTER_IDLE:
	  if (connection->getErrorState () == DEVICE_NO_ERROR)
	    filterMoveEnd ();
	  else
	    filterMoveFailed (connection->getErrorState ());
	  break;
	}
    }
  Rts2DevClient::stateChanged (state);
}

void
Rts2DevClientPhot::filterMoveStart ()
{
}

void
Rts2DevClientPhot::filterMoveEnd ()
{
}

void
Rts2DevClientPhot::filterMoveFailed (int status)
{
}

void
Rts2DevClientPhot::integrationStart ()
{
  integrating = true;
}

void
Rts2DevClientPhot::integrationEnd ()
{
  integrating = false;
}

void
Rts2DevClientPhot::integrationFailed (int status)
{
  integrating = false;
}

void
Rts2DevClientPhot::addCount (int count, float exp, int is_ov)
{
  lastCount = count;
  lastExp = exp;
}

bool
Rts2DevClientPhot::isIntegrating ()
{
  return integrating;
}

Rts2DevClientFilter::Rts2DevClientFilter (Rts2Conn * in_connection):Rts2DevClient
  (in_connection)
{
}

Rts2DevClientFilter::~Rts2DevClientFilter ()
{
}

void
Rts2DevClientFilter::filterMoveStart ()
{
}

void
Rts2DevClientFilter::filterMoveEnd ()
{
}

void
Rts2DevClientFilter::filterMoveFailed (int status)
{
}

void
Rts2DevClientFilter::stateChanged (Rts2ServerState * state)
{
  if (state->maskValueChanged (FILTERD_MASK))
    {
      switch (state->getValue () & FILTERD_MASK)
	{
	case FILTERD_MOVE:
	  filterMoveStart ();
	  break;
	case FILTERD_IDLE:
	  if (connection->getErrorState () == DEVICE_NO_ERROR)
	    filterMoveEnd ();
	  else
	    filterMoveFailed (connection->getErrorState ());
	  break;
	}
    }
}

Rts2DevClientAugerShooter::Rts2DevClientAugerShooter (Rts2Conn * in_connection):Rts2DevClient
  (in_connection)
{
}

Rts2DevClientFocus::Rts2DevClientFocus (Rts2Conn * in_connection):Rts2DevClient
  (in_connection)
{
}

Rts2DevClientFocus::~Rts2DevClientFocus (void)
{
  focusingFailed (DEVICE_ERROR_HW);
}

void
Rts2DevClientFocus::focusingStart ()
{
}

void
Rts2DevClientFocus::focusingEnd ()
{
}

void
Rts2DevClientFocus::focusingFailed (int status)
{
  focusingEnd ();
}

void
Rts2DevClientFocus::stateChanged (Rts2ServerState * state)
{
  switch (state->getValue () & FOC_MASK_FOCUSING)
    {
    case FOC_FOCUSING:
      focusingStart ();
      break;
    case FOC_SLEEPING:
      if (connection->getErrorState () == DEVICE_NO_ERROR)
	focusingEnd ();
      else
	focusingFailed (connection->getErrorState ());
      break;
    }
  Rts2DevClient::stateChanged (state);
}

Rts2DevClientExecutor::Rts2DevClientExecutor (Rts2Conn * in_connection):Rts2DevClient
  (in_connection)
{
}

void
Rts2DevClientExecutor::lastReadout ()
{
}

void
Rts2DevClientExecutor::stateChanged (Rts2ServerState * state)
{
  switch (state->getValue () & EXEC_STATE_MASK)
    {
    case EXEC_LASTREAD:
      lastReadout ();
      break;
    }
  Rts2DevClient::stateChanged (state);
}

Rts2DevClientSelector::Rts2DevClientSelector (Rts2Conn * in_connection):Rts2DevClient
  (in_connection)
{
}

Rts2DevClientImgproc::Rts2DevClientImgproc (Rts2Conn * in_connection):Rts2DevClient
  (in_connection)
{
}

Rts2DevClientGrb::Rts2DevClientGrb (Rts2Conn * in_connection):Rts2DevClient
  (in_connection)
{
}
