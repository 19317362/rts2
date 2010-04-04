/* 
 * Ncurses menus.
 * Copyright (C) 2003-2007,2010 Petr Kubanek <petr@kubanek.net>
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

#ifndef __RTS2_NMENU__
#define __RTS2_NMENU__

#include "daemonwindow.h"

#include <vector>

namespace rts2ncurses
{

/**
 * One action item for NSubmenu.
 */
class NAction
{
	private:
		const char *text;
		int code;
	public:
		NAction (const char *in_text, int in_code)
		{
			text = in_text;
			code = in_code;
		}
		virtual ~ NAction (void)
		{
		}
		void draw (WINDOW * master_window, int y);

		int getCode ()
		{
			return code;
		}
};

/**
 * Submenu class. Holds list of actions which will be displayed in
 * submenu.
 */
class NSubmenu:public NSelWindow
{
	private:
		std::vector < NAction * >actions;
		const char *text;
	public:
		NSubmenu (const char *in_text);
		virtual ~ NSubmenu (void);
		void addAction (NAction * in_action)
		{
			actions.push_back (in_action);
		}
		void createAction (const char *in_text, int in_code)
		{
			addAction (new NAction (in_text, in_code));
			grow (strlen (in_text) + 2, 1);
		}
		void draw (WINDOW * master_window);
		void drawSubmenu ();
		NAction *getSelAction ()
		{
			return actions[getSelRow ()];
		}
};

/**
 * Application menu. It holds submenus, which organize different
 * actions.
 */
class NMenu:public NWindow
{
	private:
		std::vector < NSubmenu * >submenus;
		std::vector < NSubmenu * >::iterator selSubmenuIter;
		NSubmenu *selSubmenu;
		int top_x;
	public:
		NMenu ();
		virtual ~ NMenu (void);
		virtual keyRet injectKey (int key);
		virtual void draw ();
		virtual void enter ();
		virtual void leave ();

		void addSubmenu (NSubmenu * in_submenu);
		NAction *getSelAction ()
		{
			if (selSubmenu)
				return selSubmenu->getSelAction ();
			return NULL;
		}
};

}
#endif							 /* !__RTS2_NMENU__ */
