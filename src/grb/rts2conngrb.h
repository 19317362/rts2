/* 
 * GCN socket connection.
 * Copyright (C) 2003-2008 Petr Kubanek <petr@kubanek.net>
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

#ifndef __RTS2_GRBCONN__
#define __RTS2_GRBCONN__

#include "../utils/rts2connnosend.h"

#include "grbconst.h"
#include "grbd.h"

class Rts2DevGrb;

class Rts2ConnGrb:public Rts2ConnNoSend
{
	private:
		Rts2DevGrb * master;
		// path to exec when we get new burst; pass parameters on command line
		char *addExe;
		// whenewer to exec script even for follow-ups slew (only Swift can make those)
		int execFollowups;

		int gcnReceivedBytes;	 // number of bytes received

		int32_t lbuf[SIZ_PKT];		 // local buffer - swaped for Linux
		int32_t nbuf[SIZ_PKT];		 // network buffer
		struct timeval last_packet;
		double here_sod;		 // machine SOD (seconds after 0 GMT)
		double last_imalive_sod; // SOD of the previous imalive packet

		double deltaValue;
		char *last_target;
		double last_target_time;
		double last_ra;
		double last_dec;

		// init listen (listening on given port) and call (try to connect to given
		// port; there must be GCN packet receiving running on oppoiste side) GCN
		// connection
		int init_listen ();
		int init_call ();

		// utility functions..
		double getPktSod ();

		void getTimeTfromTJD (long TJD, double SOD, time_t * in_time, long *usec =
			NULL);

		double getJDfromTJD (long TJD, double SOD)
		{
			return TJD + 2440000.5 + SOD / 86400.0;
		}
		// process various messages..
		int pr_test ();
		int pr_imalive ();
		int pr_swift_point ();	 // swift pointing.
		int pr_integral_point ();// integral pointing
		// burst messages
		int pr_hete ();
		int pr_integral ();
		int pr_integral_spicas ();
		int pr_swift_with_radec ();
		int pr_swift_without_radec ();
		int pr_agile ();  // AGILE messages (100-102)
		int pr_glast ();

		// GRB db stuff
		int addSwiftPoint (double roll, char *name, float obstime, float merit);
		int addIntegralPoint (double ra, double dec, const time_t * t);

		void getGrbBound (int grb_type, int &grb_start, int &grb_end);
		bool gcnContainsGrbPos (int grb_type);
		float getInstrumentErrorBox (int grb_type);

		// DB operations
		// insert GCN position
		// that's for notices with which we are sure they contain position
		// if they are various NACK types, we set insertOnly flag to true and will
		// only produce insert when it's new GRB (when packet with detection get lost, as
		// was cause of GRB060929 and most probably others).
		// Return -1 on error, 1 when insertOnly flag is true and it's update packet
		int addGcnPoint (int grb_id, int grb_seqn, int grb_type, double grb_ra,
			double grb_dec, bool grb_is_grb, time_t * grb_date,
			long grb_date_usec, float grb_errorbox, bool insertOnly);
		int addGcnRaw (int grb_id, int grb_seqn, int grb_type);

		int gcn_port;
		char *gcn_hostname;
		int do_hete_test;

		int gcn_listen_sock;

		time_t swiftLastPoint;
		double swiftLastRa;
		double swiftLastDec;

		time_t nextTime;
	public:
		Rts2ConnGrb (char *in_gcn_hostname, int in_gcn_port, int in_do_hete_test,
			char *in_addExe, int in_execFollowups, Rts2DevGrb * in_master);
		virtual ~ Rts2ConnGrb (void);
		virtual int idle ();
		virtual int init ();

		virtual int add (fd_set * readset, fd_set * writeset, fd_set * expset);

		virtual void connectionError (int last_data_size);
		virtual int receive (fd_set * set);

		int lastPacket ();
		double delta ();
		char *lastTarget ();
		double lastTargetTime ()
		{
			return last_target_time;
		}
		double lastRa ()
		{
			return last_ra;
		}
		double lastDec ()
		{
			return last_dec;
		}
};
#endif							 /* !__RTS2_GRBCONN__ */
