/*
  Copyright (C) 2001, 2002 Stephane Magnenat & Luc-Olivier de Charri?re
    for any question or comment contact us at nct@ysagoon.com or nuage@ysagoon.com

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifndef __GUIMESSAGEBOX_H
#define __GUIMESSAGEBOX_H

#include "GUIBase.h"

//! This is the type of the message box (the number of buttons)
enum MessageBoxType
{
	//! One button (like OK)
	MB_ONEBUTTON,
	//! Two buttons (like Ok, Cancel)
	MB_TWOBUTTONS,
	//! three buttons, (like Yes, No, Cancel)
	MB_THREEBUTTONS
}

//! The display a modal message box, with a title and some buttons
//! \retval the nummer of the clicked button, -1 on unexpected early-out (CTRL-C, ...)
int MessageBox(DrawableSurface *gfx, Font *font, MessageBoxType type, const char *title, const char *caption1, const char *caption2 = NULL, const char *caption3 = NULL);

#endif
