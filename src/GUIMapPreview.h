/*
    Copyright (C) 2001, 2002 Stephane Magnenat & Luc-Olivier de Charriere
    for any question or comment contact us at nct@ysagoon.com

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#ifndef __GUIMAPPREVIEW_H
#define __GUIMAPPREVIEW_H

#include "GUIBase.h"

//! Widget to preview map
/*!
	This widget is used to preview map.
	Each time setMapThmubnail is called a map htumbnail is loaded.
	A map thumbnail is a little image generated by the map editor.
*/
class MapPreview: public Widget
{
public:
	//! Constructor, takes position and initial map name
	MapPreview(int x, int y, const char *mapName=NULL);
	//! Destructor
	virtual ~MapPreview() { }
	//! First paint call
	virtual void paint(DrawableSurface *gfx);
	//! Reload thumbnail for a new map
	virtual void setMapThumbnail(const char *mapName=NULL);

protected:
	//! internal paint routine
	void repaint(void);

	//! internal name, is a pointer to a char* somewhere.
	const char *mapName;
	//! position on widget on screen
	int x, y;
	//! internal copy of gfx pointer
	DrawableSurface *gfx;
};

#endif
