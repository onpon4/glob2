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

#include "GameGUIDialog.h"
#include "GlobalContainer.h"
#include "GameGUI.h"


InGameScreen::InGameScreen(int w, int h)
{
	gfxCtx=globalContainer->gfx->createDrawableSurface();
	gfxCtx->setRes(w, h);
	gfxCtx->setAlpha(false, 128);
	decX=(globalContainer->gfx->getW()-w)>>1;
	decY=(globalContainer->gfx->getH()-h)>>1;
	endValue=-1;
}

InGameScreen::~InGameScreen()
{
	delete gfxCtx;
}

void InGameScreen::translateAndProcessEvent(SDL_Event *event)
{
	SDL_Event ev=*event;
	switch (ev.type)
	{
		case SDL_MOUSEMOTION:
			ev.motion.x-=decX;
			ev.motion.y-=decY;
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			ev.button.x-=decX;
			ev.button.y-=decY;
			break;
		default:
			break;
	}
	dispatchEvents(&ev);
}

void InGameScreen::paint(int x, int y, int w, int h)
{
	gfxCtx->drawFilledRect(x, y, w, h, 0, 0, 255);
}


//! Main menu screen
InGameMainScreen::InGameMainScreen()
:InGameScreen(300, 275)
{
	addWidget(new TextButton(10, 10, 280, 35, NULL, -1, -1, globalContainer->menuFont, globalContainer->texts.getString("[load game]"), 0));
	addWidget(new TextButton(10, 50, 280, 35, NULL, -1, -1, globalContainer->menuFont, globalContainer->texts.getString("[save game]"), 1));
	addWidget(new TextButton(10, 90, 280, 35, NULL, -1, -1, globalContainer->menuFont, globalContainer->texts.getString("[options]"), 2));
	addWidget(new TextButton(10, 130, 280, 35, NULL, -1, -1, globalContainer->menuFont, globalContainer->texts.getString("[alliances]"), 3));
	addWidget(new TextButton(10, 180, 280, 35, NULL, -1, -1, globalContainer->menuFont, globalContainer->texts.getString("[return to game]"), 4));
	addWidget(new TextButton(10, 230, 280, 35, NULL, -1, -1, globalContainer->menuFont, globalContainer->texts.getString("[quit the game]"), 5));
}

void InGameMainScreen::onAction(Widget *source, Action action, int par1, int par2)
{
	if (action==BUTTON_RELEASED)
		endValue=par1;
}

void InGameMainScreen::onSDLEvent(SDL_Event *event)
{
	if ((event->type==SDL_KEYDOWN) && (event->key.keysym.sym==SDLK_ESCAPE))
		endValue=4;
}

//! Alliance screen
InGameAlliance8Screen::InGameAlliance8Screen(GameGUI *gameGUI)
:InGameScreen(300, 295)
{
	int i;
	for (i=0; i<gameGUI->game.session.numberOfPlayer; i++)
	{
		allied[i]=new OnOffButton(200, 40+i*25, 20, 20, false, i);
		addWidget(allied[i]);
		vision[i]=new OnOffButton(235, 40+i*25, 20, 20, false, 10+i);
		addWidget(vision[i]);
		chat[i]=new OnOffButton(270, 40+i*25, 20, 20, false, 20+i);
		addWidget(chat[i]);
	}
	for (;i<8;i++)
	{
		allied[i]=vision[i]=chat[i]=NULL;
	}
	addWidget(new TextButton(10, 250, 280, 35, NULL, -1, -1, globalContainer->menuFont, globalContainer->texts.getString("[ok]"), 40));
	firstPaint=true;
	this->gameGUI=gameGUI;
}

void InGameAlliance8Screen::onAction(Widget *source, Action action, int par1, int par2)
{
	if (action==BUTTON_RELEASED)
		endValue=par1;
	else if (action==BUTTON_STATE_CHANGED)
		setCorrectValueForPlayer(par1%10);
}

void InGameAlliance8Screen::onSDLEvent(SDL_Event *event)
{
	if ((event->type==SDL_KEYDOWN) && (event->key.keysym.sym==SDLK_ESCAPE))
		endValue=30;
}

void InGameAlliance8Screen::paint(int x, int y, int w, int h)
{
	InGameScreen::paint(x, y, w, h);
	if (firstPaint)
	{
		gfxCtx->drawString(200, 10, globalContainer->menuFont, "A");
		gfxCtx->drawString(236, 10, globalContainer->menuFont, "V");
		gfxCtx->drawString(272, 10, globalContainer->menuFont, "C");
		{
			for (int i=0; i<gameGUI->game.session.numberOfPlayer; i++)
			{
				gfxCtx->drawString(10, 40+i*25, globalContainer->menuFont, names[i]);
			}
		}
		firstPaint=false;
	}
}

void InGameAlliance8Screen::setCorrectValueForPlayer(int i)
{
	Game *game=&(gameGUI->game);
	for (int j=0; j<game->session.numberOfPlayer; j++)
	{
		if (j!=i)
		{
			// if two players are the same team, we must have the same alliance and vision
			if (game->players[j]->teamNumber==game->players[i]->teamNumber)
			{
				allied[j]->setState(allied[i]->getState());
				vision[j]->setState(vision[i]->getState());
			}
		}
	}
}

Uint32 InGameAlliance8Screen::getAllianceMask(void)
{
	Uint32 mask=0;
	for (int i=0; i<gameGUI->game.session.numberOfPlayer; i++)
	{
		if (allied[i]->getState())
			mask|=1<<i;
	}
	return mask;
}

Uint32 InGameAlliance8Screen::getVisionMask(void)
{
	Uint32 mask=0;
	for (int i=0; i<gameGUI->game.session.numberOfPlayer; i++)
	{
		if (vision[i]->getState())
			mask|=1<<i;
	}
	return mask;
}

Uint32 InGameAlliance8Screen::getChatMask(void)
{
	Uint32 mask=0;
	for (int i=0; i<gameGUI->game.session.numberOfPlayer; i++)
	{
		if (chat[i]->getState())
			mask|=1<<i;
	}
	return mask;
}
