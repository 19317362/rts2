/* 
 * Copyright (C) 2011 Lee Hicks 
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

#ifndef __ahe_h__
#define __ahe_h__

#define CMD_A_OPEN 'a'
#define CMD_B_OPEN 'b'
#define CMD_A_CLOSE 'A'
#define CMD_B_CLOSE 'B'
#define POLL_A_OPENED 'x'
#define POLL_B_OPENED 'y'
#define POLL_A_CLOSED 'X'
#define POLL_B_CLOSED 'Y'
#define MAX_COMMANDS 50 //think of this as a timeout
#define SERIAL_TIMEOUT 300

#include "../utils/connserial.h"
enum DomeStatus
{
     ERROR=-1, OPENED, CLOSED, OPENING, CLOSING

};

namespace rts2dome
{

class AHE:public Dome
{
    private:
        char * dev;
        int fd, cmdSent;
        DomeStatus status;

        int openALeaf();
        int openBLeaf();
        int closeALeaf();
        int closeBLeaf();

        rts2core::ValueString * domeStatus;
        rts2core::ValueBool * closeDome;
        rts2core::ValueBool * leafA;
        rts2core::ValueBool * leafB;

        rts2core::ConnSerial *sconn;

        char response, junk;

    protected:
        virtual int processOption(int opt);
        virtual int setValue(rts2core::Value *old_value, rts2core::Value *new_value);

        virtual int startOpen();
        virtual long isOpened();
        virtual int endOpen();

        virtual int startClose();
        virtual long isClosed();
        virtual int endClose();

    public:
        AHE(int argc, char ** argv);
        virtual ~AHE();
        virtual int init();
        virtual int info();
};
}


#endif
