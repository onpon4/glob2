/*
  Copyright (C) 2001-2004 Stephane Magnenat & Luc-Olivier de Charrière
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

#include <GraphicContext.h>
#include "SDLGraphicContext.h"
#include "GLGraphicContext.h"
#include <Toolkit.h>
#include <FileManager.h>
#include <assert.h>
#include <string>
#include <sstream>
#include <iostream>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


GraphicContext *GraphicContext::createGraphicContext(GraphicContextType type)
{
	if (type==GC_SDL)
	{
		return new SDLGraphicContext();
	}
#ifdef HAVE_LIBGL
	else if (type==GC_GL)
	{
		return new GLGraphicContext();
	}
#endif
	else
	{
		fprintf(stderr, "GAG : Critical, don't know how to create graphic context 0x%x\n", (unsigned)type );
		assert(false);
		return NULL;
	}
}


GraphicContext::GraphicContext()
{
	minW = minH = 0;
}

void GraphicContext::setMinRes(int w, int h)
{
	minW = w;
	minH = h;
}

void GraphicContext::beginVideoModeListing(void)
{
	modes=SDL_ListModes(NULL, SDL_FULLSCREEN|SDL_HWSURFACE);
}

bool GraphicContext::getNextVideoMode(int *w, int *h)
{
	if (modes && (modes!=(SDL_Rect **)-1))
	{
		while (*modes)
		{
			int nw = (*modes)->w;
			int nh = (*modes)->h;
			modes++;
			
			if ( ((minW == 0) || (nw >= minW))
				&& ((minH == 0) || (nh >= minH)))
			{
				*w = nw;
				*h = nh;
				return true;
			}
		}
	}
	return false;
}

GraphicContext::~GraphicContext(void)
{
}

void GraphicContext::loadSprite(const char *filename, const char *name)
{
	SDL_RWops *frameStream;
	SDL_RWops *rotatedStream;
	unsigned i=0;

	Sprite *sprite=new Sprite;

	while (true)
	{
		std::ostringstream frameName;
		frameName << filename << i << ".png";
		frameStream = Toolkit::getFileManager()->open(frameName.str().c_str(), "rb");

		std::ostringstream frameNameRot;
		frameNameRot << filename << i << "r.png";
		rotatedStream = Toolkit::getFileManager()->open(frameNameRot.str().c_str(), "rb");

		if (!((frameStream) || (rotatedStream)))
			break;

		sprite->loadFrame(frameStream, rotatedStream);

		if (frameStream)
			SDL_RWclose(frameStream);
		if (rotatedStream)
			SDL_RWclose(rotatedStream);
		i++;
	}
	Toolkit::spriteMap[std::string(name)] = sprite;
}

int Font::getStringWidth(const int i) const
{
	std::ostringstream temp;
	temp << i;
	return getStringWidth(temp.str().c_str());
}

int Font::getStringWidth(const char *string, int len) const
{
	std::string temp;
	temp.append(string, len);
	return getStringWidth(temp.c_str());
}

int Font::getStringHeight(const char *string, int len) const
{
	std::string temp;
	temp.append(string, len);
	return getStringHeight(temp.c_str());
}

int Font::getStringHeight(const int i) const
{
	std::ostringstream temp;
	temp << i;
	return getStringHeight(temp.str().c_str());
}

