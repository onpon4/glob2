/*
  Copyright (C) 2001, 2002, 2003 Stephane Magnenat & Luc-Olivier de Charrière
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

#include "Map.h"
#include "Game.h"
#include "Utilities.h"


Map::Map()
{
	game=NULL;

	arraysBuilt=false;
	
	mapDiscovered=NULL;
	fogOfWar=NULL;
	fogOfWarA=NULL;
	fogOfWarB=NULL;
	cases=NULL;
	for (int t=0; t<32; t++)
		for (int r=0; r<MAX_NB_RESSOURCES; r++)
			for (int s=0; s<2; s++)
			{
				ressourcesGradient[t][r][s]=NULL;
				gradientUpdatedDepth[t][r][s]=0;
			}
	undermap=NULL;
	sectors=NULL;
	
	w=0;
	h=0;
	size=0;
	wMask=0;
	hMask=0;
	wDec=0;
	hDec=0;
	wSector=0;
	hSector=0;
	sizeSector=0;
	
	stepCounter=0;
}

Map::~Map(void)
{
	clear();
}

void Map::clear()
{
	if (arraysBuilt)
	{
		assert(mapDiscovered);
		delete[] mapDiscovered;
		mapDiscovered=NULL;

		fogOfWar=NULL;
		
		assert(fogOfWarA);
		delete[] fogOfWarA;
		fogOfWarA=NULL;

		assert(fogOfWarB);
		delete[] fogOfWarB;
		fogOfWarB=NULL;

		assert(cases);
		delete[] cases;
		cases=NULL;
		for (int t=0; t<32; t++)
			if (ressourcesGradient[t][0][0])
				for (int r=0; r<MAX_RESSOURCES; r++)
					for (int s=0; s<2; s++)
					{
						assert(ressourcesGradient[t][r][s]);
						delete ressourcesGradient[t][r][s];
						ressourcesGradient[t][r][s]=NULL;
					}
		
		assert(undermap);
		delete[] undermap;
		undermap=NULL;
		
		assert(sectors);
		delete[] sectors;
		sectors=NULL;

		arraysBuilt=false;
	}
	else
	{
		assert(mapDiscovered==NULL);
		assert(fogOfWar==NULL);
		assert(fogOfWarA==NULL);
		assert(fogOfWarB==NULL);
		assert(cases==NULL);
		for (int t=0; t<32; t++)
			for (int r=0; r<MAX_RESSOURCES; r++)
				for (int s=0; s<2; s++)
					assert(ressourcesGradient[t][r][s]==NULL);
		assert(undermap==NULL);
		assert(sectors==NULL);

		assert(w==0);
		assert(h==0);
		assert(size==0);
		assert(wMask==0);
		assert(hMask==0);
		assert(wDec==0);
		assert(hDec==0);
		assert(wSector==0);
		assert(hSector==0);
		assert(sizeSector==0);

		assert(stepCounter==0);
	}
	
	w=h=0;
	size=0;
	wMask=hMask=0;
	wDec=hDec=0;
	wSector=hSector=0;
	sizeSector=0;

	stepCounter=0;
	
	for (int t=0; t<32; t++)
		for (int r=0; r<MAX_RESSOURCES; r++)
			for (int s=0; s<2; s++)
				gradientUpdatedDepth[t][r][s]=0;
}

void Map::setSize(int wDec, int hDec, TerrainType terrainType)
{
	clear();

	assert(wDec<16);
	assert(hDec<16);
	this->wDec=wDec;
	this->hDec=hDec;
	w=1<<wDec;
	h=1<<hDec;
	wMask=w-1;
	hMask=h-1;
	size=w*h;

	mapDiscovered=new Uint32[size];
	memset(mapDiscovered, 0, size*sizeof(Uint32));

	fogOfWarA=new Uint32[size];
	memset(fogOfWarA, 0, size*sizeof(Uint32));
	fogOfWarB=new Uint32[size];
	memset(fogOfWarB, 0, size*sizeof(Uint32));
	fogOfWar=fogOfWarA;
	
	cases=new Case[size];

	/*Ressource initRessource;
	initRessource.field.type=0;
	initRessource.field.variety=0;
	initRessource.field.amount=0;
	initRessource.field.animation=0;*/
	Case initCase;
	initCase.terrain=0; // default, not really meanfull.
	initCase.building=NOGBID;
	initCase.ressource.id=NORESID;
	initCase.groundUnit=NOGUID;
	initCase.airUnit=NOGUID;
	initCase.forbidden=0;
	
	undermap=new Uint8[size];
	memset(undermap, terrainType, size);
	
	for (int i=0; i<size; i++)
		cases[i]=initCase;
		
	//numberOfTeam=0, then ressourcesGradient[][][] is emptih. This is done by clear();
	
	regenerateMap(0, 0, w, h);
	
	stepCounter=0;

	wSector=w>>4;
	hSector=h>>4;
	sizeSector=wSector*hSector;
	
	sectors=new Sector[sizeSector];
	
	arraysBuilt=true;
}


void Map::setGame(Game *game)
{
	assert(game);
	printf("Map::setGame(%p)\n", game);
	this->game=game;
	assert(arraysBuilt);
	assert(sectors);
	for (int i=0; i<sizeSector; i++)
		sectors[i].setGame(game);
}

bool Map::load(SDL_RWops *stream, SessionGame *sessionGame, Game *game)
{
	assert(sessionGame);
	assert(sessionGame->versionMinor>=16);
	
	clear();

	char signature[4];
	SDL_RWread(stream, signature, 4, 1);
	if (memcmp(signature, "MapB", 4)!=0)
	{
		fprintf(stderr, "Map:: Failed to find signature at the beginning of Map.\n");
		return false;
	}
	
	// We load and compute size:
	wDec=SDL_ReadBE32(stream);
	hDec=SDL_ReadBE32(stream);
	w=1<<wDec;
	h=1<<hDec;
	wMask=w-1;
	hMask=h-1;
	size=w*h;
	
	// We alocate memory:
	mapDiscovered=new Uint32[size];
	fogOfWarA=new Uint32[size];
	fogOfWarB=new Uint32[size];
	fogOfWar=fogOfWarA;
	cases=new Case[size];
	
	undermap=new Uint8[size];
	
	// We read what's inside the map:
	SDL_RWread(stream, undermap, size, 1);
	for (int i=0; i<size; i++)
	{
		mapDiscovered[i]=SDL_ReadBE32(stream);
		
		cases[i].terrain=SDL_ReadBE16(stream);
		cases[i].building=SDL_ReadBE16(stream);
		
		cases[i].ressource.id=SDL_ReadBE32(stream);
		
		cases[i].groundUnit=SDL_ReadBE16(stream);
		cases[i].airUnit=SDL_ReadBE16(stream);

		cases[i].forbidden=SDL_ReadBE32(stream);
	}
	
	for (int t=0; t<sessionGame->numberOfTeam; t++)
		for (int r=0; r<MAX_RESSOURCES; r++)
			for (int s=0; s<2; s++)
			{
				assert(ressourcesGradient[t][r][s]==NULL);
				ressourcesGradient[t][r][s]=new Uint8[size];
				updateGradient(t, r, s, true);
			}
	for (int t=0; t<32; t++)
		for (int r=0; r<MAX_RESSOURCES; r++)
			for (int s=0; s<1; s++)
				gradientUpdatedDepth[t][r][s]=0;

	memset(fogOfWarA, 0, size*sizeof(Uint32));
	memset(fogOfWarB, 0, size*sizeof(Uint32));
	
	if (game)
		this->game=game;
	
	// We load sectors:
	wSector=SDL_ReadBE32(stream);
	hSector=SDL_ReadBE32(stream);
	sizeSector=wSector*hSector;
	assert(sectors==NULL);
	sectors=new Sector[sizeSector];
	arraysBuilt=true;

	for (int i=0; i<sizeSector; i++)
		if (!sectors[i].load(stream, this->game))
			return false;

	SDL_RWread(stream, signature, 4, 1);
	if (memcmp(signature, "MapE", 4)!=0)
	{
		fprintf(stderr, "Map:: Failed to find signature at the end of Map.\n");
		return false;
	}
	
	return true;
}

void Map::save(SDL_RWops *stream)
{
	SDL_RWwrite(stream, "MapB", 4, 1);
	
	// We save size:
	SDL_WriteBE32(stream, wDec);
	SDL_WriteBE32(stream, hDec);

	// We write what's inside the map:
	SDL_RWwrite(stream, undermap, size, 1);
	for (int i=0; i<size ;i++)
	{
		SDL_WriteBE32(stream, mapDiscovered[i]);

		SDL_WriteBE16(stream, cases[i].terrain);
		SDL_WriteBE16(stream, cases[i].building);
		
		SDL_WriteBE32(stream, cases[i].ressource.id);
		
		SDL_WriteBE16(stream, cases[i].groundUnit);
		SDL_WriteBE16(stream, cases[i].airUnit);
		SDL_WriteBE32(stream, cases[i].forbidden);
	}

	// We save sectors:
	SDL_WriteBE32(stream, wSector);
	SDL_WriteBE32(stream, hSector);
	for (int i=0; i<sizeSector; i++)
		sectors[i].save(stream);

	SDL_RWwrite(stream, "MapE", 4, 1);
}

void Map::addTeam(void)
{
	int numberOfTeam=game->session.numberOfTeam;
	int oldNumberOfTeam=numberOfTeam-1;
	assert(numberOfTeam>0);
	
	for (int t=0; t<oldNumberOfTeam; t++)
		for (int r=0; r<MAX_RESSOURCES; r++)
			for (int s=0; s<2; s++)
				assert(ressourcesGradient[t][r][s]);
	for (int t=oldNumberOfTeam; t<32; t++)
		for (int r=0; r<MAX_RESSOURCES; r++)
			for (int s=0; s<2; s++)
				assert(ressourcesGradient[t][r][s]==NULL);
	
	int t=oldNumberOfTeam;
	for (int r=0; r<MAX_RESSOURCES; r++)
		for (int s=0; s<2; s++)
		{
			assert(ressourcesGradient[t][r][s]==NULL);
			ressourcesGradient[t][r][s]=new Uint8[size];
			updateGradient(t, r, s, true);
		}
}

void Map::removeTeam(void)
{
	int numberOfTeam=game->session.numberOfTeam;
	int oldNumberOfTeam=numberOfTeam+1;
	assert(numberOfTeam<32);
	
	for (int t=0; t<oldNumberOfTeam; t++)
		for (int r=0; r<MAX_RESSOURCES; r++)
			for (int s=0; s<2; s++)
				assert(ressourcesGradient[t][r][s]);
	for (int t=oldNumberOfTeam; t<32; t++)
		for (int r=0; r<MAX_RESSOURCES; r++)
			for (int s=0; s<2; s++)
				assert(ressourcesGradient[t][r][s]==NULL);
	
	int t=numberOfTeam;
	for (int r=0; r<MAX_RESSOURCES; r++)
		for (int s=0; s<2; s++)
		{
			assert(ressourcesGradient[t][r][s]);
			delete[] ressourcesGradient[t][r][s];
			ressourcesGradient[t][r][s]=NULL;
		}
}

// TODO: completely recreate:
void Map::growRessources(void)
{
	int dy=syncRand()&0x3;
	for (int y=dy; y<h; y+=4)
	{
		for (int x=(syncRand()&0xF); x<w; x+=(syncRand()&0x1F))
		//for (int x=0; x<w; x++)
		{
			//int y=syncRand()&hMask;
			Ressource r=getRessource(x, y);
			if (r.id!=NORESID)
			{
				// we look around to see if there is any water :
				// TODO: uses UnderMap.
				static const int waterDist=0xF;
				int dwax=(syncRand()&waterDist)-(syncRand()&waterDist);
				int dway=(syncRand()&waterDist)-(syncRand()&waterDist);
				int wax1=x+dwax;
				int way1=y+dway;

				int wax2=x+dway*2;
				int way2=y+dwax*2;

				int wax3=x-dwax;
				int way3=y-dway;

				//int wax4=x+dway*2;
				//int way4=y-dwax*2;

				// alga, wood and corn are limited by near underground. Others are not.
				bool expand=true;
				if (r.field.type==ALGA)
					expand=isWater(wax1, way1)&&isSand(wax2, way2);
				else if (r.field.type==WOOD)
					expand=isWater(wax1, way1)&&(!isSand(wax3, way3));
				else if (r.field.type==CORN)
					expand=isWater(wax1, way1)&&(!isSand(wax3, way3));

				if (expand)
				{
					if (r.field.amount<=(int)(syncRand()&7))
					{
						// we grow ressource:
						incRessource(x, y, (RessourcesTypes::intResType)r.field.type);
					}
					else if (globalContainer->ressourcesTypes->get(r.field.type)->expendable)
					{
						// we extand ressource:
						int dx, dy;
						Unit::dxdyfromDirection(syncRand()&7, &dx, &dy);
						int nx=x+dx;
						int ny=y+dy;
						incRessource(nx, ny, (RessourcesTypes::intResType)r.field.type);
					}
				}
			}
		}
	}
}

void Map::step(void)
{
	growRessources();
	for (int i=0; i<sizeSector; i++)
		sectors[i].step();
	
	// We only update one gradient per step:
	bool updated=false;
	while (!updated)
	{
		int numberOfTeam=game->session.numberOfTeam;
		for (int t=0; t<numberOfTeam; t++)
			for (int r=0; r<MAX_RESSOURCES; r++)
				for (int s=0; s<2; s++)
				{
					int gud=gradientUpdatedDepth[t][r][s];
					if (gud<2)
					{
						//printf("updateGradient(%d, %d, %d, %d)\n", t, r, s, gud==0);
						updateGradient(t, r, s, gud==0);
						gradientUpdatedDepth[t][r][s]++;
						updated=true;
						goto tripleBreak;
					}
				}
		tripleBreak:
		if (!updated)
		{
			for (int t=0; t<numberOfTeam; t++)
				for (int r=0; r<MAX_RESSOURCES; r++)
					for (int s=0; s<2; s++)
						gradientUpdatedDepth[t][r][s]=0;
		}
	}
	
	stepCounter++;
}

void Map::switchFogOfWar(void)
{
	memset(fogOfWar, 0, size*sizeof(Uint32));
	if (fogOfWar==fogOfWarA)
		fogOfWar=fogOfWarB;
	else
		fogOfWar=fogOfWarA;
}

void Map::setForbiddenArea(int x, int y, int r, Uint32 me)
{
	int rm=(r<<1)+1;
	int r2=rm*rm;
	for (int yi=-r; yi<=r; yi++)
	{
		int yi2=((yi*yi)<<2);
		for (int xi=-r; xi<=r; xi++)
			if (yi2+((xi*xi)<<2)<r2)
				(*(cases+w*((y+yi)&hMask)+((x+xi)&wMask))).forbidden|=me;
	}
}

void Map::clearForbiddenArea(int x, int y, int r, Uint32 me)
{
	int rm=(r<<1)+1;
	int r2=rm*rm;
	for (int yi=-r; yi<=r; yi++)
	{
		int yi2=((yi*yi)<<2);
		for (int xi=-r; xi<=r; xi++)
			if (yi2+((xi*xi)<<2)<r2)
				(*(cases+w*((y+yi)&hMask)+((x+xi)&wMask))).forbidden&=~me;
	}
}

void Map::clearForbiddenArea(Uint32 me)
{
	for (int y=0; y<h; y++)
		for (int x=0; x<w; x++)
			(*(cases+w*y+x)).forbidden&=~me;
}

bool Map::decRessource(int x, int y)
{
	Ressource *rp=&(*(cases+w*(y&hMask)+(x&wMask))).ressource;
	Ressource r=*rp;

	if (r.id==NORESID)
		return false;

	RessourcesTypes::intResType type=(RessourcesTypes::intResType)r.field.type;
	const RessourceType *fulltype=globalContainer->ressourcesTypes->get(type);
	unsigned amount=r.field.amount;
	assert(amount);

	if (!fulltype->shrinkable || ((fulltype->eternal) && (amount==1)))
		return false;
	else if (!fulltype->granular || amount==1)
		rp->id=NORESID;
	else
		rp->field.amount=amount-1;
	return true;
}

bool Map::decRessource(int x, int y, RessourcesTypes::intResType ressourceType)
{
	if (isRessource(x, y, ressourceType))
		return decRessource(x, y);
	else
		return false;
}

bool Map::incRessource(int x, int y, RessourcesTypes::intResType ressourceType)
{
	Ressource *rp=&(*(cases+w*(y&hMask)+(x&wMask))).ressource;
	Ressource &r=*rp;
	const RessourceType *fulltype;

	if (r.id==NORESID)
	{
		if (getBuilding(x, y)!=NOGBID)
			return false;
		if (getGroundUnit(x, y)!=NOGUID)
			return false;

		fulltype=globalContainer->ressourcesTypes->get(ressourceType);
		if (getTerrainType(x, y) == fulltype->terrain)
		{
			rp->field.type=(Uint8)ressourceType;
			rp->field.variety=syncRand()%fulltype->varietiesCount;
			rp->field.amount=1;
			rp->field.animation=0;
			return true;
		}
		else
			return false;
	}
	else
	{
		RessourcesTypes::intResType type=(RessourcesTypes::intResType)r.field.type;
		fulltype=globalContainer->ressourcesTypes->get(type);
	}

	if (r.field.type!=ressourceType)
		return false;
	if (!fulltype->shrinkable)
		return false;
	if (r.field.amount<fulltype->sizesCount-1)
	{
		rp->field.amount=r.field.amount+1;
		return true;
	}
	return false;
}

bool Map::isFreeForGroundUnit(int x, int y, bool canSwim, Uint32 teamMask)
{
	if (isRessource(x+w, y+h))
		return false;
	if (getBuilding(x+w, y+h)!=NOGBID)
		return false;
	if (getGroundUnit(x+w, y+h)!=NOGUID)
		return false;
	if (!canSwim && isWater(x+w, y+h))
		return false;
	if (getForbidden(x+w, y+h)&teamMask)
		return false;
	return true;
}

bool Map::isFreeForAirUnit(int x, int y)
{
	return (getAirUnit(x+w, y+h)==NOGUID);
}

bool Map::isFreeForBuilding(int x, int y)
{
	if (isRessource(x, y))
		return false;
	if (getBuilding(x, y)!=NOGBID)
		return false;
	if (getGroundUnit(x, y)!=NOGUID)
		return false;
	if (isGrass(x, y))
		return true;
	else
		return false;
}

bool Map::isFreeForBuilding(int x, int y, int w, int h)
{
	for (int yi=y; yi<y+h; yi++)
		for (int xi=x; xi<x+w; xi++)
			if (!isFreeForBuilding(xi, yi))
				return false;
	return true;
}

bool Map::isFreeForBuilding(int x, int y, int w, int h, Uint16 gid)
{
	for (int yi=y; yi<y+h; yi++)
		for (int xi=x; xi<x+w; xi++)
			if (!isFreeForBuilding(xi, yi))
			{
				if (isRessource(xi, yi))
					return false;
				Uint16 buid=getBuilding(xi, yi);
				if (buid!=NOGBID && buid!=gid)
					return false;
				if (getGroundUnit(xi, yi)!=NOGUID)
					return false;
				if (!isGrass(xi, yi))
					return false;
			}
	return true;
}

bool Map::isHardSpaceForGroundUnit(int x, int y, bool canSwim, Uint32 me)
{
	if (isRessource(x, y))
		return false;
	if (getBuilding(x, y)!=NOGBID)
		return false;
	if (!canSwim && isWater(x, y))
		return false;
	if (getForbidden(x, y)&me)
		return false;
	return true;
}

bool Map::isHardSpaceForBuilding(int x, int y)
{
	if (isRessource(x, y))
		return false;
	if (getBuilding(x, y)!=NOGBID)
		return false;
	if (isGrass(x, y))
		return true;
	else
		return false;
}

bool Map::isHardSpaceForBuilding(int x, int y, int w, int h)
{
	for (int yi=y; yi<y+h; yi++)
		for (int xi=x; xi<x+w; xi++)
			if (!isHardSpaceForBuilding(xi, yi))
				return false;
	return true;
}

bool Map::isHardSpaceForBuilding(int x, int y, int w, int h, Uint16 gid)
{
	for (int yi=y; yi<y+h; yi++)
		for (int xi=x; xi<x+w; xi++)
		{
			if (isRessource(xi, yi))
				return false;
			Uint16 buid=getBuilding(xi, yi);
			if (buid!=NOGBID && buid!=gid)
				return false;
			if (!isGrass(xi, yi))
				return false;
		}
	return true;
}

bool Map::doesUnitTouchBuilding(Unit *unit, Uint16 gbid, int *dx, int *dy)
{
	int x=unit->posX;
	int y=unit->posY;
	
	for (int tdx=-1; tdx<=1; tdx++)
		for (int tdy=-1; tdy<=1; tdy++)
			if (getBuilding(x+tdx, y+tdy)==gbid)
			{
				*dx=tdx;
				*dy=tdy;
				return true;
			}
	return false;
}

bool Map::doesPosTouchBuilding(int x, int y, Uint16 gbid)
{
	for (int tdx=-1; tdx<=1; tdx++)
		for (int tdy=-1; tdy<=1; tdy++)
			if (getBuilding(x+tdx, y+tdy)==gbid)
				return true;
	return false;
}

bool Map::doesPosTouchBuilding(int x, int y, Uint16 gbid, int *dx, int *dy)
{
	for (int tdx=-1; tdx<=1; tdx++)
		for (int tdy=-1; tdy<=1; tdy++)
			if (getBuilding(x+tdx, y+tdy)==gbid)
			{
				*dx=tdx;
				*dy=tdy;
				return true;
			}
	return false;
}

bool Map::doesUnitTouchRessource(Unit *unit, int *dx, int *dy)
{
	int x=unit->posX;
	int y=unit->posY;
	
	for (int tdx=-1; tdx<=1; tdx++)
		for (int tdy=-1; tdy<=1; tdy++)
			if (isRessource(x+tdx, y+tdy))
			{
				*dx=tdx;
				*dy=tdy;
				return true;
			}
	return false;
}

bool Map::doesUnitTouchRemovableRessource(Unit *unit, int *dx, int *dy)
{
	int x=unit->posX;
	int y=unit->posY;
	
	for (int tdx=-1; tdx<=1; tdx++)
		for (int tdy=-1; tdy<=1; tdy++)
			if (isRemovableRessource(x+tdx, y+tdy))
			{
				*dx=tdx;
				*dy=tdy;
				return true;
			}
	return false;
}

bool Map::doesUnitTouchRessource(Unit *unit, RessourcesTypes::intResType ressourceType, int *dx, int *dy)
{
	int x=unit->posX;
	int y=unit->posY;
	
	for (int tdx=-1; tdx<=1; tdx++)
		for (int tdy=-1; tdy<=1; tdy++)
			if (isRessource(x+tdx, y+tdy, ressourceType))
			{
				*dx=tdx;
				*dy=tdy;
				return true;
			}
	return false;
}

bool Map::doesPosTouchRessource(int x, int y, RessourcesTypes::intResType ressourceType, int *dx, int *dy)
{
	for (int tdx=-1; tdx<=1; tdx++)
		for (int tdy=-1; tdy<=1; tdy++)
			if (isRessource(x+tdx, y+tdy,ressourceType))
			{
				*dx=tdx;
				*dy=tdy;
				return true;
			}
	return false;
}

//! This method gives a good direction to hit for a warrior, and return fase is nothing was found.
//! Currently, it chooses to hit any turret if aviable, then units, then other buildings.
bool Map::doesUnitTouchEnemy(Unit *unit, int *dx, int *dy)
{
	int x=unit->posX;
	int y=unit->posY;
	int bestTime=256;//Shorter is better
	int bdx=0, bdy=0;

	Uint32 enemies=unit->owner->enemies;
	for (int tdx=-1; tdx<=1; tdx++)
		for (int tdy=-1; tdy<=1; tdy++)
		{
			Sint32 gbid=getBuilding(x+tdx, y+tdy);
			if (gbid!=NOGBID)
			{
				int otherTeam=Building::GIDtoTeam(gbid);
				Uint32 otherTeamMask=1<<otherTeam;
				if (enemies & otherTeamMask)
				{
					assert(game);
					assert(game->teams[otherTeam]);
					int otherID=Building::GIDtoID(gbid);
					Building *b=game->teams[otherTeam]->myBuildings[otherID];
					if (!b->type->defaultUnitStayRange)
					{
						if (b->type->shootingRange)
							bestTime=0;
						else
							bestTime=255;
						bdx=tdx;
						bdy=tdy;
					}
				}
			}
			Sint32 guid=getGroundUnit(x+tdx, y+tdy);
			if (guid!=NOGUID)
			{
				int otherTeam=Unit::GIDtoTeam(guid);
				Uint32 otherTeamMask=1<<otherTeam;
				if (enemies & otherTeamMask)
				{
					assert(game);
					assert(game->teams[otherTeam]);
					int otherID=Unit::GIDtoID(guid);
					Unit *otherUnit=game->teams[otherTeam]->myUnits[otherID];
					int time=(256-otherUnit->delta)/otherUnit->speed;
					if (time<bestTime)
					{
						bestTime=time;
						bdx=tdx;
						bdy=tdy;
					}
				}
			}
			//TODO: can ground WARRIOR hit flying EXPLORER ?
		}
	
	if (bestTime<256)
	{
		*dx=bdx;
		*dy=bdy;
		return true;
	}

	return false;
}

void Map::setUMatPos(int x, int y, TerrainType t, int size)
{
	for (int dx=x-(size>>1); dx<x+(size>>1)+1; dx++)
		for (int dy=y-(size>>1); dy<y+(size>>1)+1; dy++)
		{
			if (t==GRASS)
			{
				if (getUMTerrain(dx,dy-1)==WATER)
					setUMTerrain(dx,dy-1,SAND);
				if (getUMTerrain(dx,dy+1)==WATER)
					setUMTerrain(dx,dy+1,SAND);

				if (getUMTerrain(dx-1,dy)==WATER)
					setUMTerrain(dx-1,dy,SAND);
				if (getUMTerrain(dx+1,dy)==WATER)
					setUMTerrain(dx+1,dy,SAND);

				if (getUMTerrain(dx-1,dy-1)==WATER)
					setUMTerrain(dx-1,dy-1,SAND);
				if (getUMTerrain(dx+1,dy-1)==WATER)
					setUMTerrain(dx+1,dy-1,SAND);

				if (getUMTerrain(dx+1,dy+1)==WATER)
					setUMTerrain(dx+1,dy+1,SAND);
				if (getUMTerrain(dx-1,dy+1)==WATER)
					setUMTerrain(dx-1,dy+1,SAND);
			}
			else if (t==WATER)
			{
				if (getUMTerrain(dx,dy-1)==GRASS)
					setUMTerrain(dx,dy-1,SAND);
				if (getUMTerrain(dx,dy+1)==GRASS)
					setUMTerrain(dx,dy+1,SAND);

				if (getUMTerrain(dx-1,dy)==GRASS)
					setUMTerrain(dx-1,dy,SAND);
				if (getUMTerrain(dx+1,dy)==GRASS)
					setUMTerrain(dx+1,dy,SAND);

				if (getUMTerrain(dx-1,dy-1)==GRASS)
					setUMTerrain(dx-1,dy-1,SAND);
				if (getUMTerrain(dx+1,dy-1)==GRASS)
					setUMTerrain(dx+1,dy-1,SAND);

				if (getUMTerrain(dx+1,dy+1)==GRASS)
					setUMTerrain(dx+1,dy+1,SAND);
				if (getUMTerrain(dx-1,dy+1)==GRASS)
					setUMTerrain(dx-1,dy+1,SAND);
			}
			setUMTerrain(dx,dy,t);
		}
	if (t==SAND)
		regenerateMap(x-(size>>1)-1,y-(size>>1)-1,size+1,size+1);
	else
		regenerateMap(x-(size>>1)-2,y-(size>>1)-2,size+3,size+3);
}

void Map::setNoRessource(int x, int y, int size)
{
	assert(size>=0);
	assert(size<w);
	assert(size<h);
	for (int dx=x-(size>>1); dx<x+(size>>1); dx++)
		for (int dy=y-(size>>1); dy<y+(size>>1); dy++)
			(cases+w*(dy&hMask)+(dx&wMask))->ressource.id=NORESID;
}

void Map::setRessource(int x, int y, int type, int size)
{
	assert(size>=0);
	assert(size<w);
	assert(size<h);
	for (int dx=x-(size>>1); dx<x+(size>>1)+1; dx++)
		for (int dy=y-(size>>1); dy<y+(size>>1)+1; dy++)
			if (isRessourceAllowed(dx, dy, type))
			{
				Ressource *rp=&((cases+w*(dy&hMask)+(dx&wMask))->ressource);
				rp->field.type=type;
				rp->field.variety=0; // TODO: syncRand()%sizeOfVariety
				rp->field.amount=1; // TODO: syncRand()%maxAmount
				rp->field.animation=0;
			}
}

bool Map::isRessourceAllowed(int x, int y, int type)
{
	return (getTerrainType(x, y)==globalContainer->ressourcesTypes->get(type)->terrain);
}

void Map::mapCaseToDisplayable(int mx, int my, int *px, int *py, int viewportX, int viewportY)
{
	int x=mx-viewportX;
	int y=my-viewportY;
	
	if (x>(w/2))
		x-=w;
	if (y>(h/2))
		y-=h;
	if ((x)<-(w/2))
		x+=w;
	if ((y)<-(h/2))
		y+=getH();
	*px=x<<5;
	*py=y<<5;
}

void Map::displayToMapCaseAligned(int mx, int my, int *px, int *py, int viewportX, int viewportY)
{
	*px=((mx>>5)+viewportX+getW())&getMaskW();
	*py=((my>>5)+viewportY+getH())&getMaskH();
}

void Map::displayToMapCaseUnaligned(int mx, int my, int *px, int *py, int viewportX, int viewportY)
{
	*px=(((mx+16)>>5)+viewportX+getW())&getMaskW();
	*py=(((my+16)>>5)+viewportY+getH())&getMaskH();
}

void Map::cursorToBuildingPos(int mx, int my, int buildingWidth, int buildingHeight, int *px, int *py, int viewportX, int viewportY)
{
	int tempX, tempY;
	if (buildingWidth&0x1)
		tempX=((mx)>>5)+viewportX;
	else
		tempX=((mx+16)>>5)+viewportX;
			
	if (buildingHeight&0x1)
		tempY=((my)>>5)+viewportY;
	else
		tempY=((my+16)>>5)+viewportY;
		
	*px=(tempX+getW())&getMaskW();
	*py=(tempY+getH())&getMaskH();
}

void Map::buildingPosToCursor(int px, int py, int buildingWidth, int buildingHeight, int *mx, int *my, int viewportX, int viewportY)
{
	mapCaseToDisplayable(px, py, mx, my, viewportX, viewportY);
	*mx+=buildingWidth*16;
	*my+=buildingHeight*16;
}

bool Map::nearestRessource(int x, int y, RessourcesTypes::intResType ressourceType, int *dx, int *dy)
{
	for (int i=1; i<32; i++)
	{
		for (int j=-i; j<i; j++)
		{
			if (isRessource(x+i, y+j, ressourceType))
			{
				*dx=(x+i)&getMaskW();
				*dy=(y+j)&getMaskH();
				return true;
			}
			if (isRessource(x-i, y+j, ressourceType))
			{
				*dx=(x-i)&getMaskW();
				*dy=(y+j)&getMaskH();
				return true;
			}
			if (isRessource(x+j, y+i, ressourceType))
			{
				*dx=(x+j)&getMaskW();
				*dy=(y+i)&getMaskH();
				return true;
			}
			if (isRessource(x+j, y-i, ressourceType))
			{
				*dx=(x+j)&getMaskW();
				*dy=(y-i)&getMaskH();
				return true;
			}
		}
	}
    return false;
}

bool Map::nearestRessource(int x, int y, RessourcesTypes::intResType *ressourceType, int *dx, int *dy)
{
	for (int i=1; i<32; i++)
	{
		for (int j=-i; j<i; j++)
		{
			if (isRessource(x+i, y+j, ressourceType))
			{
				*dx=(x+i)&getMaskW();
				*dy=(y+j)&getMaskH();
				return true;
			}
			if (isRessource(x-i, y+j, ressourceType))
			{
				*dx=(x-i)&getMaskW();
				*dy=(y+j)&getMaskH();
				return true;
			}
			if (isRessource(x+j, y+i, ressourceType))
			{
				*dx=(x+j)&getMaskW();
				*dy=(y+i)&getMaskH();
				return true;
			}
			if (isRessource(x+j, y-i, ressourceType))
			{
				*dx=(x+j)&getMaskW();
				*dy=(y-i)&getMaskH();
				return true;
			}
		}
	}
    return false;
}

bool Map::nearestRessourceInCircle(int x, int y, int fx, int fy, int fsr, int *dx, int *dy)
{
	fsr*=fsr;
	for (int i=1; i<32; i++)
		for (int j=-i; j<i; j++)
		{
			if (isRemovableRessource(x+i, y+j) && warpDistSquare(x+i, y+j, fx, fy)<=fsr)
			{
				*dx=(x+i)&getMaskW();
				*dy=(y+j)&getMaskH();
				return true;
			}
			if (isRemovableRessource(x-i, y+j) && warpDistSquare(x-i, y+j, fx, fy)<=fsr)
			{
				*dx=(x-i)&getMaskW();
				*dy=(y+j)&getMaskH();
				return true;
			}
			if (isRemovableRessource(x+j, y+i) && warpDistSquare(x+j, y+i, fx, fy)<=fsr)
			{
				*dx=(x+j)&getMaskW();
				*dy=(y+i)&getMaskH();
				return true;
			}
			if (isRemovableRessource(x+j, y-i) && warpDistSquare(x+j, y-i, fx, fy)<=fsr)
			{
				*dx=(x+j)&getMaskW();
				*dy=(y-i)&getMaskH();
				return true;
			}
		}
    return false;
}

bool Map::ressourceAviable(int teamNumber, Uint8 ressourceType, bool canSwim, int x, int y)
{
	Uint8 g=getGradient(teamNumber, ressourceType, canSwim, x, y);
	return g>1; //Becasue 0==obstacle, 1==no obstacle, but you don't know if there is anything around.
}

bool Map::ressourceAviable(int teamNumber, Uint8 ressourceType, bool canSwim, int x, int y, Sint32 *targetX, Sint32 *targetY, int *dist)
{
	Uint8 *gradient=ressourcesGradient[teamNumber][ressourceType][canSwim];
	assert(gradient);
	int wy=w*y;
	Uint8 g=gradient[wy+x];
	if (g<2)
		return false;
	if (dist)
		*dist=255-g;
	
	int vx=x;
	int vy=y;
	//printf("s=(%d, %d), g=%d, r=%d\n", x, y, g, ressourceType);
	//Uint32 teamMask=Team::teamNumberToMask(teamNumber);
	
	while (true)
	{
		Uint8 max=gradient[vx+w*vy];
		bool found=false;
		int vddx, vddy;
		
		for (int sd=1; sd>=0; sd--)
			for (int d=sd; d<8; d+=2)
			{
				int ddx, ddy;
				Unit::dxdyfromDirection(d, &ddx, &ddy);
				
				Uint8 g=*(gradient+((vx+w+ddx)&wMask)+((vy+h+ddy)&hMask)*w);
				if (g>max)
				{
					max=g;
					vddx=ddx;
					vddy=ddy;
					found=true;
				}
			}
		
		if (!found)
		{
			int mvx=vx-2;
			int mvy=vy-2;
			for (int ai=0; ai<4; ai++)
			{
				for (int mi=0; mi<5; mi++)
				{
					Uint8 g=*(gradient+((mvx+w)&wMask)+((mvy+h)&hMask)*w);
					if (g>max)
					{
						max=g;
						vddx=mvx-vx;
						vddy=mvy-vy;
						found=true;
					}
					switch (ai)
					{
						case 0:
							mvx++;
						break;
						case 1:
							mvy++;
						break;
						case 2:
							mvx--;
						break;
						case 3:
							mvy--;
						break;
					}
				}
			}
		}
		
		vx=(vx+vddx+w)&wMask;
		vy=(vy+vddy+h)&hMask;
		//printf("v=(%d, %d) max=%d\n", vx, vy, max);
		if (max>=255)
		{
			*targetX=vx;
			*targetY=vy;
			return true;
		}
		if (!found)
		{
			printf("target *not* found! pos=(%d, %d), vpos=(%d, %d), max=%d, team=%d, res=%d, swim=%d\n", x, y, vx, vy, max, teamNumber, ressourceType, canSwim);
			return false;
		}
	}
}

void Map::updateGradient(int teamNumber, Uint8 ressourceType, bool canSwim, bool init)
{
	Uint8 *gradient=ressourcesGradient[teamNumber][ressourceType][canSwim];
	assert(gradient);
	
	Uint32 teamMask=Team::teamNumberToMask(teamNumber);
	if (init)
	{
		memset(gradient, 1, size);
		for (int y=0; y<h; y++)
		{
			int wy=w*y;
			for (int x=0; x<w; x++)
			{
				Case c=cases[wy+x];
				if (c.ressource.id==NORESID)
				{
					if (c.building!=NOGBID)
						gradient[wy+x]=0;
					else if (c.forbidden&teamMask)
						gradient[wy+x]=0;
					else if (!canSwim && isWater(x, y))
						gradient[wy+x]=0;
				}
				else
				{
					if (c.ressource.field.type==ressourceType)
						gradient[wy+x]=255;
					else
						gradient[wy+x]=0;
				}
			}
		}
	}
	
	//In this algotithm, "l" stands for one case at Left, "r" for one case at Right, "u" for Up, and "d" for Down.
	// Warning, this is *nearly* a copy-past, 4 times, once for each direction.
	
	for (int depth=0; depth<1; depth++) // With a higher depth, we can have more complex obstacles.
	{
		/*for (int y=0; y<h; y++)
		{
			int wy=w*y;
			int wyu=w*((y+hMask)&hMask);
			int wyd=w*((y+1)&hMask);
			for (int x=0; x<w; x++)
			{
				Uint8 max=gradient[wy+x];
				if (max && max!=255)
				{
					int xl=(x+wMask)&wMask;
					int xr=(x+1)&wMask;
					
					Uint8 side[8];
					side[0]=gradient[wyu+xl];
					side[1]=gradient[wyu+x ];
					side[2]=gradient[wyu+xr];

					side[3]=gradient[wy +xr];

					side[4]=gradient[wyd+xr];
					side[5]=gradient[wyd+x ];
					side[6]=gradient[wyd+xl];

					side[7]=gradient[wy +xl];

					for (int i=0; i<8; i++)
						if (side[i]>max)
							max=side[i];
					assert(max);
					if (max==1)
						gradient[wy+x]=1;
					else
						gradient[wy+x]=max-1;
				}
			}
		}*/
		
		for (int y=0; y<h; y++)
		{
			int wy=w*y;
			int wyu=w*((y+hMask)&hMask);
			for (int x=0; x<w; x++)
			{
				Uint8 max=gradient[wy+x];
				if (max && max!=255)
				{
					int xl=(x+wMask)&wMask;
					int xr=(x+1)&wMask;
					
					Uint8 side[4];
					side[0]=gradient[wyu+xl];
					side[1]=gradient[wyu+x ];
					side[2]=gradient[wyu+xr];
					side[3]=gradient[wy +xl];
					//side[4]=gradient[wy +xr];

					for (int i=0; i<4; i++)
						if (side[i]>max)
							max=side[i];
					
					if (max==1)
						gradient[wy+x]=1;
					else
						gradient[wy+x]=max-1;
				}
			}
		}

		for (int y=hMask; y>=0; y--)
		{
			int wy=w*y;
			int wyd=w*((y+1)&hMask);
			for (int x=0; x<w; x++)
			{
				Uint8 max=gradient[wy+x];
				if (max && max!=255)
				{
					int xl=(x+wMask)&wMask;
					int xr=(x+1)&wMask;
					
					Uint8 side[4];
					side[0]=gradient[wyd+xr];
					side[1]=gradient[wyd+x ];
					side[2]=gradient[wyd+xl];
					side[3]=gradient[wy +xl];
					//side[4]=gradient[wy +xr];

					for (int i=0; i<4; i++)
						if (side[i]>max)
							max=side[i];
					if (max==1)
						gradient[wy+x]=1;
					else
						gradient[wy+x]=max-1;
				}
			}
		}

		for (int x=0; x<w; x++)
		{
			int xl=(x+wMask)&wMask;
			for (int y=0; y<h; y++)
			{
				int wy=w*y;
				int wyu=w*((y+hMask)&hMask);
				int wyd=w*((y+1)&hMask);
				Uint8 max=gradient[wy+x];
				if (max && max!=255)
				{
					Uint8 side[4];
					side[0]=gradient[wyu+xl];
					side[1]=gradient[wyd+xl];
					side[2]=gradient[wy +xl];
					side[3]=gradient[wyu+x ];
					//side[4]=gradient[wyd+x ];

					for (int i=0; i<4; i++)
						if (side[i]>max)
							max=side[i];
					if (max==1)
						gradient[wy+x]=1;
					else
						gradient[wy+x]=max-1;
				}
			}
		}

		for (int x=wMask; x>=0; x--)
		{
			int xr=(x+1)&wMask;
			for (int y=0; y<h; y++)
			{
				int wy=w*y;
				int wyu=w*((y+hMask)&hMask);
				int wyd=w*((y+1)&hMask);
				Uint8 max=gradient[wy+x];
				if (max && max!=255)
				{
					Uint8 side[4];
					side[0]=gradient[wyu+xr];
					side[1]=gradient[wy +xr];
					side[2]=gradient[wyd+xr];
					side[3]=gradient[wyu+x ];
					//side[4]=gradient[wyd+x ];

					for (int i=0; i<4; i++)
						if (side[i]>max)
							max=side[i];
					if (max==1)
						gradient[wy+x]=1;
					else
						gradient[wy+x]=max-1;
				}
			}
		}
	}
}

bool Map::pathfindRessource(int teamNumber, Uint8 ressourceType, bool canSwim, int x, int y, int *dx, int *dy)
{
	Uint8 *gradient=ressourcesGradient[teamNumber][ressourceType][canSwim];
	assert(gradient);
	Uint8 max=*(gradient+x+y*w);
	bool found=false;
	Uint32 teamMask=Team::teamNumberToMask(teamNumber);
	if (max<2)
		return false;
	
	// We don't use for (int d=0; d<8; d++), this way units won't take two diagonals if not needed.
	
	for (int sd=1; sd>=0; sd--)
		for (int d=sd; d<8; d+=2)
		{
			int ddx, ddy;
			Unit::dxdyfromDirection(d, &ddx, &ddy);
			if (isFreeForGroundUnit(x+w+ddx, y+h+ddy, canSwim, teamMask))
			{
				Uint8 g=*(gradient+((x+w+ddx)&wMask)+((y+h+ddy)&hMask)*w);
				if (g>max)
				{
					max=g;
					*dx=ddx;
					*dy=ddy;
					found=true;
				}
			}
		}
	
	if (found)
		return true;
	
	if (!found)
	{
		int mvx=x-2;
		int mvy=y-2;
		for (int ai=0; ai<4; ai++)
			for (int mi=0; mi<5; mi++)
			{
				int ddx=SIGN(mvx-x);
				int ddy=SIGN(mvy-y);
				if (isFreeForGroundUnit(x+w+ddx, y+h+ddy, canSwim, teamMask))
				{
					Uint8 g=*(gradient+((mvx+w)&wMask)+((mvy+h)&hMask)*w);
					if (g>max)
					{
						max=g;
						*dx=ddx;
						*dy=ddy;
						found=true;
					}
				}
				switch (ai)
				{
					case 0:
						mvx++;
					break;
					case 1:
						mvy++;
					break;
					case 2:
						mvx--;
					break;
					case 3:
						mvy--;
					break;
				}
			}
	}

	if (!found)
	{
		printf("locked at (%d, %d)\n", x, y);
		*dx=0;
		*dy=0;
	}
	return true;
}

void Map::clearBuildingGradient(Uint8 gradient[2][1024])
{
	memset(gradient, 1, 2*1024);
}

void Map::updateLocalGradient(Building *building, bool canSwim)
{
	assert(building);
	building->dirtyLocalGradient[canSwim]=false;
	int posX=building->posX;
	int posY=building->posY;
	int posW=building->type->width;
	int posH=building->type->height;
	Uint32 teamMask=building->owner->me;
	Uint16 bgid=building->gid;
	
	Uint8 *gradient=building->localGradient[canSwim];

	memset(gradient, 1, 1024);
	for (int dy=0; dy<posH; dy++)
		for (int dx=0; dx<posW; dx++)
			gradient[15+dx+(15+dy)*32]=255;

	// Here g=Global(map axis), l=Local(map axis)

	for (int yl=0; yl<32; yl++)
	{
		int wyl=32*yl;
		int yg=(yl+posY-15+h)&hMask;
		int wyg=w*yg;
		for (int xl=0; xl<32; xl++)
		{
			int xg=(xl+posX-15+w)&wMask;
			Case c=cases[wyg+xg];
			int addrl=wyl+xl;
			if (gradient[addrl]!=0 && gradient[addrl]!=255)
			{
				if (c.ressource.id!=NORESID)
					gradient[addrl]=0;
				else if (c.building!=NOGBID && c.building!=bgid)
					gradient[addrl]=0;
				else if (c.forbidden&teamMask)
					gradient[addrl]=0;
				else if (!canSwim && isWater(xg, yg))
					gradient[addrl]=0;
			}
		}
	}

	//In this algotithm, "l" stands for one case at Left, "r" for one case at Right, "u" for Up, and "d" for Down.

	for (int depth=0; depth<2; depth++) // With a higher depth, we can have more complex obstacles.
	{
		for (int down=0; down<2; down++)
		{
			int x, y, dis, die, ddi;
			if (down)
			{
				x=0;
				y=0;
				dis=31;
				die=1;
				ddi=-2;
			}
			else
			{
				x=15;
				y=15;
				dis=1;
				die=31;
				ddi=+2;
			}
			
			for (int di=dis; di!=die; di+=ddi) //distance-iterator
			{
				for (int bi=0; bi<2; bi++) //back-iterator
				{
					for (int ai=0; ai<4; ai++) //angle-iterator
					{
						for (int mi=0; mi<di; mi++) //move-iterator
						{
							//printf("di=%d, ai=%d, mi=%d, p=(%d, %d)\n", di, ai, mi, x, y);
							assert(x>=0);
							assert(y>=0);
							assert(x<32);
							assert(y<32);

							int wy=32*y;
							int wyu, wyd;
							if (y==0)
								wyu=32*0;
							else
								wyu=32*(y-1);
							if (y==31)
								wyd=32*31;
							else
								wyd=32*(y+1);
							Uint8 max=gradient[wy+x];
							if (max && max!=255)
							{
								int xl, xr;
								if (x==0)
									xl=0;
								else
									xl=x-1;
								if (x==31)
									xr=31;
								else
									xr=x+1;

								Uint8 side[8];
								side[0]=gradient[wyu+xl];
								side[1]=gradient[wyu+x ];
								side[2]=gradient[wyu+xr];

								side[3]=gradient[wy +xr];

								side[4]=gradient[wyd+xr];
								side[5]=gradient[wyd+x ];
								side[6]=gradient[wyd+xl];

								side[7]=gradient[wy +xl];

								for (int i=0; i<8; i++)
									if (side[i]>max)
										max=side[i];
								assert(max);
								if (max==1)
									gradient[wy+x]=1;
								else
									gradient[wy+x]=max-1;
							}

							if (bi==0)
							{
								switch (ai)
								{
									case 0:
										x++;
									break;
									case 1:
										y++;
									break;
									case 2:
										x--;
									break;
									case 3:
										y--;
									break;
								}
							}
							else
							{
								switch (ai)
								{
									case 0:
										y++;
									break;
									case 1:
										x++;
									break;
									case 2:
										y--;
									break;
									case 3:
										x--;
									break;
								}
							}
						}
					}
				}
				if (down)
				{
					x++;
					y++;
				}
				else
				{
					x--;
					y--;
				}
			}
		}
	}
}


void Map::updateGlobalGradient(Building *building, bool canSwim)
{
	assert(building);
	building->dirtyGlobalGradient[canSwim]=false;
	int posX=building->posX;
	int posY=building->posY;
	int posW=building->type->width;
	int posH=building->type->height;
	Uint32 teamMask=building->owner->me;
	Uint16 bgid=building->gid;
	
	Uint8 *gradient=building->globalGradient[canSwim];
	assert(gradient);

	memset(gradient, 1, size);
	for (int dy=0; dy<posH; dy++)
		for (int dx=0; dx<posW; dx++)
			gradient[posX+dx+(posY+dy)*w]=255;

	for (int y=0; y<h; y++)
	{
		int wy=w*y;
		for (int x=0; x<w; x++)
		{
			int wyx=wy+x;
			Case c=cases[wyx];
			if (c.building==NOGBID)
			{
				if (c.ressource.id!=NORESID)
					gradient[wyx]=0;
				else if (c.forbidden&teamMask)
					gradient[wyx]=0;
				else if (!canSwim && isWater(x, y))
					gradient[wyx]=0;
			}
			else
			{
				if (c.building==bgid)
					gradient[wyx]=255;
				else
					gradient[wyx]=0;
			}
		}
	}

	for (int depth=0; depth<2; depth++) // With a higher depth, we can have more complex obstacles.
	{
		int x=(posX+wMask)&wMask;
		int y=(posY+hMask)&hMask;
		
		for (int di=posW; di<=128; di+=2) //distance-iterator
		{
			for (int bi=0; bi<2; bi++) //back-iterator
				for (int ai=0; ai<4; ai++) //angle-iterator
				{
					for (int mi=0; mi<di; mi++) //move-iterator
					{
						//printf("di=%d, ai=%d, mi=%d, p=(%d, %d)\n", di, ai, mi, x, y);
						assert(x>=0);
						assert(y>=0);
						assert(x<w);
						assert(y<h);

						int wy =w*((y)&hMask);
						int wyu=w*((y+hMask)&hMask);
						int wyd=w*((y+1)&hMask);
						
						Uint8 max=gradient[wy+x];
						if (max && max!=255)
						{
							int xl=(x+wMask)&wMask;
							int xr=(x+1)&wMask;

							Uint8 side[8];
							side[0]=gradient[wyu+xl];
							side[1]=gradient[wyu+x ];
							side[2]=gradient[wyu+xr];

							side[3]=gradient[wy +xr];

							side[4]=gradient[wyd+xr];
							side[5]=gradient[wyd+x ];
							side[6]=gradient[wyd+xl];

							side[7]=gradient[wy +xl];

							for (int i=0; i<8; i++)
								if (side[i]>max)
									max=side[i];
							if (max==1)
								gradient[wy+x]=1;
							else
								gradient[wy+x]=max-1;
						}

						if (bi==0)
						{
							switch (ai)
							{
								case 0:
									x=(x+1)&wMask;
								break;
								case 1:
									y=(y+1)&hMask;
								break;
								case 2:
									x=(x+wMask)&wMask;
								break;
								case 3:
									y=(y+hMask)&hMask;
								break;
							}
						}
						else
						{
							switch (ai)
							{
								case 0:
									y=(y+1)&hMask;
								break;
								case 1:
									x=(x+1)&wMask;
								break;
								case 2:
									y=(y+hMask)&hMask;
								break;
								case 3:
									x=(x+wMask)&wMask;
								break;
							}
						}
					}
				}
			x=(x+wMask)&wMask;
			y=(y+hMask)&hMask;
		}
	}
	
	for (int depth=0; depth<1; depth++) // With a higher depth, we can have more complex obstacles.
	{
		for (int y=0; y<h; y++)
		{
			int wy=w*y;
			int wyu=w*((y+hMask)&hMask);
			for (int x=0; x<w; x++)
			{
				Uint8 max=gradient[wy+x];
				if (max && max!=255)
				{
					int xl=(x+wMask)&wMask;
					int xr=(x+1)&wMask;
					
					Uint8 side[4];
					side[0]=gradient[wyu+xl];
					side[1]=gradient[wyu+x ];
					side[2]=gradient[wyu+xr];
					side[3]=gradient[wy +xl];

					for (int i=0; i<4; i++)
						if (side[i]>max)
							max=side[i];
					
					if (max==1)
						gradient[wy+x]=1;
					else
						gradient[wy+x]=max-1;
				}
			}
		}

		for (int y=hMask; y>=0; y--)
		{
			int wy=w*y;
			int wyd=w*((y+1)&hMask);
			for (int x=0; x<w; x++)
			{
				Uint8 max=gradient[wy+x];
				if (max && max!=255)
				{
					int xl=(x+wMask)&wMask;
					int xr=(x+1)&wMask;
					
					Uint8 side[4];
					side[0]=gradient[wyd+xr];
					side[1]=gradient[wyd+x ];
					side[2]=gradient[wyd+xl];
					side[3]=gradient[wy +xl];

					for (int i=0; i<4; i++)
						if (side[i]>max)
							max=side[i];
					if (max==1)
						gradient[wy+x]=1;
					else
						gradient[wy+x]=max-1;
				}
			}
		}

		for (int x=0; x<w; x++)
		{
			int xl=(x+wMask)&wMask;
			for (int y=0; y<h; y++)
			{
				int wy=w*y;
				int wyu=w*((y+hMask)&hMask);
				int wyd=w*((y+1)&hMask);
				Uint8 max=gradient[wy+x];
				if (max && max!=255)
				{
					Uint8 side[4];
					side[0]=gradient[wyu+xl];
					side[1]=gradient[wyd+xl];
					side[2]=gradient[wy +xl];
					side[3]=gradient[wyu+x ];

					for (int i=0; i<4; i++)
						if (side[i]>max)
							max=side[i];
					if (max==1)
						gradient[wy+x]=1;
					else
						gradient[wy+x]=max-1;
				}
			}
		}

		for (int x=wMask; x>=0; x--)
		{
			int xr=(x+1)&wMask;
			for (int y=0; y<h; y++)
			{
				int wy=w*y;
				int wyu=w*((y+hMask)&hMask);
				int wyd=w*((y+1)&hMask);
				Uint8 max=gradient[wy+x];
				if (max && max!=255)
				{
					Uint8 side[4];
					side[0]=gradient[wyu+xr];
					side[1]=gradient[wy +xr];
					side[2]=gradient[wyd+xr];
					side[3]=gradient[wyu+x ];

					for (int i=0; i<4; i++)
						if (side[i]>max)
							max=side[i];
					if (max==1)
						gradient[wy+x]=1;
					else
						gradient[wy+x]=max-1;
				}
			}
		}
	}
	
}

bool Map::pathfindBuilding(Building *building, bool canSwim, int x, int y, int *dx, int *dy)
{
	assert(building);
	int bx=building->posX;
	int by=building->posY;
	//int bw=building->type->width;
	//int bh=building->type->height;
	 
	Uint32 teamMask=building->owner->me;
	Uint8 *gradient=building->localGradient[canSwim];
	
	if (warpDistMax(x, y, bx, by)<16) //TODO: allow the use ot the last line! (on x and y)
	{
		int lx=(x-bx+15+32)&31;
		int ly=(y-by+15+32)&31;
		int max=gradient[lx+ly*32];
		bool found=false;

		if (max>1 && !building->dirtyLocalGradient[canSwim])
		{
			for (int sd=1; sd>=0; sd--)
				for (int d=sd; d<8; d+=2)
				{
					int ddx, ddy;
					Unit::dxdyfromDirection(d, &ddx, &ddy);
					if (isFreeForGroundUnit(x+w+ddx, y+h+ddy, canSwim, teamMask))
					{
						int lxddx=lx+ddx;
						if (lxddx<0)
							lxddx=0;
						else if(lxddx>31)
							lxddx=31;
						int lxddy=ly+ddy;
						if (lxddy<0)
							lxddy=0;
						else if(lxddy>31)
							lxddy=31;
						Uint8 g=gradient[lxddx+32*lxddy];
						if (g>max)
						{
							max=g;
							*dx=ddx;
							*dy=ddy;
							found=true;
						}
					}
				}
			if (found)
				return true;
		}

		updateLocalGradient(building, canSwim);

		max=gradient[lx+ly*32];
		if (max>1)
			for (int sd=1; sd>=0; sd--)
				for (int d=sd; d<8; d+=2)
				{
					int ddx, ddy;
					Unit::dxdyfromDirection(d, &ddx, &ddy);
					if (isFreeForGroundUnit(x+w+ddx, y+h+ddy, canSwim, teamMask))
					{
						int lxddx=lx+ddx;
						if (lxddx<0)
							lxddx=0;
						else if(lxddx>31)
							lxddx=31;
						int lxddy=ly+ddy;
						if (lxddy<0)
							lxddy=0;
						else if(lxddy>31)
							lxddy=31;
						Uint8 g=gradient[lxddx+32*lxddy];
						if (g>max)
						{
							max=g;
							*dx=ddx;
							*dy=ddy;
							found=true;
						}
					}
				}

		if (found)
			return true;
	}
	
	//Here the "local-32*32-cases-gradient-pathfinding-system" has failed, then we look for a full size gradient.
	
	gradient=building->globalGradient[canSwim];
	if (gradient==NULL)
	{
		gradient=new Uint8[size];
		building->globalGradient[canSwim]=gradient;
		updateGlobalGradient(building, canSwim);
		printf("w=%d, h=%d, size=%d.\n", w, h, size);
	}
	else if (!building->dirtyGlobalGradient[canSwim])
	{
		bool found=false;
		Uint8 max=gradient[(x&wMask)+w*(y&hMask)];
		if (max>1)
			for (int sd=1; sd>=0; sd--)
				for (int d=sd; d<8; d+=2)
				{
					int ddx, ddy;
					Unit::dxdyfromDirection(d, &ddx, &ddy);
					if (isFreeForGroundUnit(x+w+ddx, y+h+ddy, canSwim, teamMask))
					{
						Uint8 g=gradient[((x+w+ddx)&wMask)+w*((y+h+ddy)&hMask)];
						if (g>max)
						{
							max=g;
							*dx=ddx;
							*dy=ddy;
							found=true;
						}
					}
				}

		//printf("found=%d, d=(%d, %d)\n", found, *dx, *dy);
		if (found)
			return true;
	}
	
	updateGlobalGradient(building, canSwim);
	
	bool found=false;
	Uint8 max=gradient[(x&wMask)+w*(y&hMask)];
	if (max>1)
		for (int sd=1; sd>=0; sd--)
			for (int d=sd; d<8; d+=2)
			{
				int ddx, ddy;
				Unit::dxdyfromDirection(d, &ddx, &ddy);
				if (isFreeForGroundUnit(x+w+ddx, y+h+ddy, canSwim, teamMask))
				{
					Uint8 g=gradient[((x+w+ddx)&wMask)+w*((y+h+ddy)&hMask)];
					if (g>max)
					{
						max=g;
						*dx=ddx;
						*dy=ddy;
						found=true;
					}
				}
			}

	if (found)
		return true;
	
	printf("global gradient to building gid=%d failed! p=(%d, %d)\n", building->gid, x, y);
	
	return false;
}

void Map::dirtyLocalGradient(int x, int y, int wl, int hl, int teamNumber)
{
	//printf("Map::dirtyLocalGradient(%d, %d, %d, %d, %d)\n", x, y, wl, hl, teamNumber);
	for (int hi=0; hi<hl; hi++)
	{
		int wyi=w*((y+h+hi)&hMask);
		for (int wi=0; wi<wl; wi++)
		{
			int xi=(x+w+wi)&wMask;
			int bgid=cases[xi+wyi].building;
			if (bgid!=NOGBID)
				if (Building::GIDtoTeam(bgid)==teamNumber)
				{
					//printf("dirtying-LocalGradient bgid=%d\n", bgid);
					for (int canSwim=0; canSwim<2; canSwim++)
						game->teams[teamNumber]->myBuildings[Building::GIDtoID(bgid)]->dirtyLocalGradient[canSwim]=true;
				}
		}
	}
}

void Map::regenerateMap(int x, int y, int w, int h)
{
	for (int dx=x; dx<x+w; dx++)
		for (int dy=y; dy<y+h; dy++)
			setTerrain(dx, dy, lookup(getUMTerrain(dx,dy), getUMTerrain(dx+1,dy), getUMTerrain(dx,dy+1), getUMTerrain(dx+1,dy+1)));
}

Uint16 Map::lookup(Uint8 tl, Uint8 tr, Uint8 bl, Uint8 br)
{
	/*
		Value of vertice's order in square :

		3 -- 2
		|    |
		|    |
		1 -- 0

		The index in the following table is :
		val[0] + val[1]*k + val[2]*k^2 + val[3]*k^3
		where k is the number of different possibilites.
	*/
	const Uint16 terrainLookupTable[81][2] =
	{
		{ 0, 16 },		// H, H, H, H
		{ 80, 8 },		// H, H, H, S
		{ 0, 16 },		// H, H, H, E
		{ 88, 8 },		// H, H, S, H
		{ 48, 8 },		// H, H, S, S
		{ 0, 16 },		// H, H, S, E
		{ 0, 16 },		// H, H, E, H
		{ 0, 16 },		// H, H, E, S
		{ 0, 16 },		// H, H, E, E
		{ 104, 8 },		// H, S, H, H
		{ 64, 8 },		// H, S, H, S
		{ 0, 16 },		// H, S, H, E
		{ 120, 8 },		// H, S, S, H
		{ 32, 8 },		// H, S, S, S
		{ 0, 16 },		// H, S, S, E
		{ 0, 16 },		// H, S, E, H
		{ 0, 16 },		// H, S, E, S
		{ 0, 16 },		// H, S, E, E
		{ 0, 16 },		// H, E, H, H
		{ 0, 16 },		// H, E, H, S
		{ 0, 16 },		// H, E, H, E
		{ 0, 16 },		// H, E, S, H
		{ 0, 16 },		// H, E, S, S
		{ 0, 16 },		// H, E, S, E
		{ 0, 16 },		// H, E, E, H
		{ 0, 16 },		// H, E, E, S
		{ 0, 16 },		// H, E, E, E

		{ 96, 8 },		// S, H, H, H
		{ 112, 8 },		// S, H, H, S
		{ 0, 16 },		// S, H, H, E
		{ 72, 8 },		// S, H, S, H
		{ 40, 8 },		// S, H, S, S
		{ 0, 16 },		// S, H, S, E
		{ 0, 16 },		// S, H, E, H
		{ 0, 16 },		// S, H, E, S
		{ 0, 16 },		// S, H, E, E
		{ 56, 8 },		// S, S, H, H
		{ 24, 8 },		// S, S, H, S
		{ 0, 16 },		// S, S, H, E
		{ 16, 8 },		// S, S, S, H
		{ 128, 16 },	// S, S, S, S
		{ 208, 8 },		// S, S, S, E
		{ 0, 16 },		// S, S, E, H
		{ 216, 8 },		// S, S, E, S
		{ 176, 8 },		// S, S, E, E
		{ 0, 16 },		// S, E, H, H
		{ 0, 16 },		// S, E, H, S
		{ 0, 16 },		// S, E, H, E
		{ 0, 16 },		// S, E, S, H
		{ 232, 8 },		// S, E, S, S
		{ 192, 8 },		// S, E, S, E
		{ 0, 16 },		// S, E, E, H
		{ 240, 8 },		// S, E, E, S
		{ 160, 8 },		// S, E, E, E

		{ 0, 16 },		// E, H, H, H
		{ 0, 16 },		// E, H, H, S
		{ 0, 16 },		// E, H, H, E
		{ 0, 16 },		// E, H, S, H
		{ 0, 16 },		// E, H, S, S
		{ 0, 16 },		// E, H, S, E
		{ 0, 16 },		// E, H, E, H
		{ 0, 16 },		// E, H, E, S
		{ 0, 16 },		// E, H, E, E
		{ 0, 16 },		// E, S, H, H
		{ 0, 16 },		// E, S, H, S
		{ 0, 16 },		// E, S, H, E
		{ 0, 16 },		// E, S, S, H
		{ 224, 8 },		// E, S, S, S
		{ 248, 8 },		// E, S, S, E
		{ 0, 16 },		// E, S, E, H
		{ 200, 8 },		// E, S, E, S
		{ 168, 8 },		// E, S, E, E
		{ 0, 16 },		// E, E, H, H
		{ 0, 16 },		// E, E, H, S
		{ 0, 16 },		// E, E, H, E
		{ 0, 16 },		// E, E, S, H
		{ 184, 8 },		// E, E, S, S
		{ 152, 8 },		// E, E, S, E
		{ 0, 16 },		// E, E, E, H
		{ 144, 8 },		// E, E, E, S
		{ 256, 16 },	// E, E, E, E
	};

	tl=2-tl;
	tr=2-tr;
	bl=2-bl;
	br=2-br;
	int index=tl*27+tr*9+bl*3+br;

	return terrainLookupTable[index][0]+(syncRand()%terrainLookupTable[index][1]);
}

Sint32 Map::checkSum(bool heavy)
{
	Sint32 cs=size;
	if (heavy)
		for (int y=0; y<h; y++)
			for (int x=0; x<w; x++)
			{
				cs+=(cases+w*(y&hMask)+(x&wMask))->terrain;
				cs+=(cases+w*(y&hMask)+(x&wMask))->building;
				cs+=(cases+w*(y&hMask)+(x&wMask))->ressource.id;
				cs+=(cases+w*(y&hMask)+(x&wMask))->groundUnit;
				cs+=(cases+w*(y&hMask)+(x&wMask))->airUnit;
				cs=(cs<<1)|(cs>>31);
			}
	return cs;
}

Sint32 Map::warpDistSquare(int px, int py, int qx, int qy)
{
	Sint32 dx=abs(px-qx);
	Sint32 dy=abs(py-qy);
	dx&=wMask;
	dy&=hMask;
	if (dx>(w>>1))
		dx=w-dx;
	if (dy>(h>>1))
		dy=h-dy;
	
	return ((dx*dx)+(dy*dy));
}

Sint32 Map::warpDistMax(int px, int py, int qx, int qy)
{
	Sint32 dx=abs(px-qx);
	Sint32 dy=abs(py-qy);
	dx&=wMask;
	dy&=hMask;
	if (dx>(w>>1))
		dx=abs(w-dx);
	if (dy>(h>>1))
		dy=abs(h-dy);
	if (dx>dy)
		return dx;
	else
		return dy;
}
