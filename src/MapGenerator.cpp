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
#include "MapGenerationDescriptor.h"
#include <math.h>
#include <float.h>


void Map::makeHomogenMap(TerrainType terrainType)
{
	for (int y=0; y<h; y++)
		for (int x=0; x<w; x++)
			undermap[y*w+x]=terrainType;
	regenerateMap(0, 0, w, h);
}

void Map::controlSand(void)
{
	for (int y=0; y<h; y++)
		for (int x=0; x<w; x++)
		{
			int tt=(int)undermap[y*w+x];
			switch (tt)
			{
				case WATER:
					for (int dy=-1; dy<=1; dy++)
						for (int dx=-1; dx<=1; dx++)
						{
							int stt=getUMTerrain(x+dx, y+dy);
							if (stt==GRASS)
								goto bad;
						}
				break;
				case SAND:
				continue;
				case GRASS:
					for (int dy=-1; dy<=1; dy++)
						for (int dx=-1; dx<=1; dx++)
						{
							int stt=getUMTerrain(x+dx, y+dy);
							if (stt==WATER)
								goto bad;
						}
				break;
			}
			continue;
			bad:
			undermap[y*w+x]=SAND;
		}
}

void Map::smoothRessources(int times)
{
	for (int s=0; s<times; s++)
		for (int y=0; y<h; y++)
			for (int x=0; x<w; x++)
			{
				int d=getTerrain(x, y)-272;
				int r=d/10;
				int l=d%5;
				if ((d>=0)&&(d<40)&&(syncRand()&4))
				{
					if (l<=(int)(syncRand()&3))
						setTerrain(x, y, d+273);
					else 
					{
						// we extand ressource:
						int dx, dy;
						Unit::dxdyfromDirection(syncRand()&7, &dx, &dy);
						int nx=x+dx;
						int ny=y+dy;
						if (getGroundUnit(nx, ny)==NOGUID)
							if (((r==WOOD||r==CORN||r==STONE)&&isGrass(nx, ny))||((r==ALGA)&&isWater(nx, ny)))
								setTerrain(nx, ny, 272+(r*10)+((syncRand()&1)*5));
					}
				}
			}
}

void simulateRandomMap(int smooth, double baseWater, double baseSand, double baseGrass, double *finalWater, double *finalSand, double *finalGrass)
{
	int w=32<<(smooth>>2);
	int h=w;
	int s=w*h;
	int m=s-1;
	VARARRAY(int,undermap,w*h);
	
	int totalRatio=0x7FFF;
	int waterRatio=(int)(baseWater*((double)totalRatio));
	int sandRatio =(int)(baseSand *((double)totalRatio));
	int grassRatio=(int)(baseGrass*((double)totalRatio));
	totalRatio=waterRatio+sandRatio+grassRatio;
	
	// First, we create a fully random patchwork:
	for (int y=0; y<h; y++)
		for (int x=0; x<w; x++)
		{
			int r=syncRand()%totalRatio;
			r-=waterRatio;
			if (r<0)
			{
				undermap[y*w+x]=0;
				continue;
			}
			r-=sandRatio;
			if (r<0)
			{
				undermap[y*w+x]=1;
				continue;
			}
			r-=grassRatio;
			if (r<0)
			{
				undermap[y*w+x]=2;
				continue;
			}
			assert(false);//Want's to sing ?
		}
	
	for (int i=0; i<smooth; i++)
		for (int y=0; y<h; y++)
			for (int x=0; x<w; x++)
			{
				if (syncRand()&4)
				{
					int a=undermap[(y*w+x+1+s)&m];
					int b=undermap[(y*w+x-1+s)&m];
					if (a==b)
					{
						undermap[y*w+x]=a;
						continue;
					}
				}
				else
				{
					int a=undermap[(y*w+x+w+s)&m];
					int b=undermap[(y*w+x-w+s)&m];
					if (a==b)
					{
						undermap[y*w+x]=a;
						continue;
					}
				}
				if (syncRand()&4)
				{
					int a=undermap[(y*w+x+w+1+s)&m];
					int b=undermap[(y*w+x-w-1+s)&m];
					if (a==b)
					{
						undermap[y*w+x]=a;
						continue;
					}
				}
				else
				{
					int a=undermap[(y*w+x+w-1+s)&m];
					int b=undermap[(y*w+x-w+1+s)&m];
					if (a==b)
					{
						undermap[y*w+x]=a;
						continue;
					}
				}
				if (syncRand()&4)
				{
					int a=undermap[(y*w+x+w-2+s)&m];
					int b=undermap[(y*w+x-w+2+s)&m];
					if (a==b)
					{
						undermap[y*w+x]=a;
						continue;
					}
				}
				else
				{
					int a=undermap[(y*w+x+w-(h<<1)+s)&m];
					int b=undermap[(y*w+x-w+(h<<1)+s)&m];
					if (a==b)
					{
						undermap[y*w+x]=a;
						continue;
					}
				}
				if (syncRand()&4)
				{
					int a=undermap[(y*w+x+w+2+(h<<1)+s)&m];
					int b=undermap[(y*w+x-w-2-(h<<1)+s)&m];
					if (a==b)
					{
						undermap[y*w+x]=a;
						continue;
					}
				}
				else
				{
					int a=undermap[(y*w+x+w+2-(h<<1)+s)&m];
					int b=undermap[(y*w+x-w-2+(h<<1)+s)&m];
					if (a==b)
					{
						undermap[y*w+x]=a;
						continue;
					}
				}
			}
	// What's finally in ?
	int waterCount=0;
	int sandCount =0;
	int grassCount=0;
	for (int y=0; y<h; y++)
		for (int x=0; x<w; x++)
			switch (undermap[y*w+x])
			{
				case 0:
					waterCount++;
				continue;
				case 1:
					sandCount ++;
				continue;
				case 2:
					grassCount++;
				continue;
			}
	
	int totalCount=waterCount+sandCount+grassCount;
	
	*finalWater=((double)waterCount)/((double)totalCount);
	*finalSand =((double)sandCount )/((double)totalCount);
	*finalGrass=((double)grassCount)/((double)totalCount);
}

void fastSimulateRandomMap(int smooth, double baseWater, double baseSand, double baseGrass, double *finalWater, double *finalSand, double *finalGrass)
{
	int n=smooth*2+1;
	VARARRAY(double,finalWaters,n);
	VARARRAY(double,finalSands,n);
	VARARRAY(double,finalGrasss,n);
	
	for (int i=0; i<n; i++)
		simulateRandomMap(4, baseWater, baseSand, baseGrass, &finalWaters[i], &finalSands[i], &finalGrasss[i]);
	
	double medianFinalWater=finalWaters[0];
	double medianFinalSand =finalSands [0];
	double medianFinalGrass=finalGrasss[0];
	for (int i=0; i<n; i++)
	{
		double wf=finalWaters[i];
		double sf=finalSands [i];
		double gf=finalGrasss[i];
		int ws=0;
		int wb=0;
		int we=0;
		int ss=0;
		int sb=0;
		int se=0;
		int gs=0;
		int gb=0;
		int ge=0;
		for (int j=0; j<n; j++)
		{
			if (wf<finalWaters[j])
				wb++;
			else if (wf>finalWaters[j])
				ws++;
			else
				we++;
			if (sf<finalSands[j])
				sb++;
			else if (sf>finalSands[j])
				ss++;
			else
				se++;
			if (gf<finalGrasss[j])
				gb++;
			else if (gf>finalGrasss[j])
				gs++;
			else
				ge++;
		}
		if (abs(wb-ws)<we)
			medianFinalWater=wf;
		if (abs(sb-ss)<se)
			medianFinalSand=sf;
		if (abs(gb-gs)<ge)
			medianFinalGrass=gf;
	}
	*finalWater=medianFinalWater;
	*finalSand=medianFinalSand;
	*finalGrass=medianFinalGrass;
}

bool Map::makeRandomMap(MapGenerationDescriptor &descriptor)
{
	int waterRatio=descriptor.waterRatio;
	int sandRatio =descriptor.sandRatio ;
	int grassRatio=descriptor.grassRatio;
	int totalRatio=waterRatio+sandRatio+grassRatio;
	int smooth=descriptor.smooth;
	
	double baseWater=(float)waterRatio/(float)totalRatio;
	double baseSand =(float)sandRatio /(float)totalRatio;
	double baseGrass=(float)grassRatio/(float)totalRatio;
	printf("makeRandomMap::old-base=(%f, %f, %f).\n", baseWater, baseSand, baseGrass);
	
	//Sorry, the equation is too complex for me. We uses a numeric aproach:
	double alphaWater=baseWater;
	double alphaSand =baseSand ;
	double alphaGrass=baseGrass;
	double alphaSum=alphaWater+alphaSand+alphaGrass;
	alphaWater/=alphaSum;
	alphaSand /=alphaSum;
	alphaGrass/=alphaSum;
	
	for (int r=1; r<=smooth; r++)
	{
		for (int prec=0; prec<3; prec++) 
		{
			double finalAlphaWater, finalAlphaSand, finalAlphaGrass;
			simulateRandomMap(r, alphaWater, alphaSand, alphaGrass, &finalAlphaWater, &finalAlphaSand, &finalAlphaGrass);
			//printf("alpha-alpha=(%f, %f, %f) (%f).\n", alphaWater, alphaSand, alphaGrass, alphaWater+alphaSand+alphaGrass);
			//printf("alpha-final=(%f, %f, %f) (%f).\n", finalAlphaWater, finalAlphaSand, finalAlphaGrass, finalAlphaWater+finalAlphaSand+finalAlphaGrass);
			
			double errAlphaWater=finalAlphaWater-baseWater;
			double errAlphaSand =finalAlphaSand -baseSand ;
			double errAlphaGrass=finalAlphaGrass-baseGrass;
			//double errAlpha=(errAlphaWater*errAlphaWater+errAlphaSand*errAlphaSand+errAlphaGrass*errAlphaGrass);
			//printf("errAlpha=(%f, %f, %f) (%f).\n", errAlphaWater, errAlphaSand, errAlphaGrass, errAlpha);

			double betaWater;
			double betaSand ; 
			double betaGrass;
			
			if (finalAlphaWater)
				betaWater=(alphaWater*baseWater)/finalAlphaWater;
			else
				betaWater=0;
			if (finalAlphaSand)
				betaSand =(alphaSand *baseSand )/finalAlphaSand ;
			else
				betaSand=0;
			if (finalAlphaGrass)
				betaGrass=(alphaGrass*baseGrass)/finalAlphaGrass;
			else
				betaGrass=0;
			double betaSum=betaWater+betaSand+betaGrass;
			betaWater/=betaSum;
			betaSand /=betaSum;
			betaGrass/=betaSum;
			
			double finalBetaWater, finalBetaSand, finalBetaGrass;
			simulateRandomMap(r, betaWater, betaSand, betaGrass, &finalBetaWater, &finalBetaSand, &finalBetaGrass);
			//printf("beta-beta=(%f, %f, %f) (%f).\n", betaWater, betaSand, betaGrass, betaWater+betaSand+betaGrass);
			//printf("beta-final=(%f, %f, %f) (%f).\n", finalBetaWater, finalBetaSand, finalBetaGrass, finalBetaWater+finalBetaSand+finalBetaGrass);
			
			double errBetaWater=finalBetaWater-baseWater;
			double errBetaSand =finalBetaSand -baseSand ;
			double errBetaGrass=finalBetaGrass-baseGrass;
			//double errBeta=(errBetaWater*errBetaWater+errBetaSand*errBetaSand+errBetaGrass*errBetaGrass);
			//printf("errBeta=(%f, %f, %f) (%f).\n", errBetaWater, errBetaSand, errBetaGrass, errBeta);
			
			double projNom=(errBetaWater*errAlphaWater+errBetaSand*errAlphaSand+errBetaGrass*errAlphaGrass);
			double projDen=(errAlphaWater*errAlphaWater+errAlphaSand*errAlphaSand+errAlphaGrass*errAlphaGrass);
			if (projDen<=0)
				continue;
			double proj=projNom/projDen;
			//printf("proj=%f.\n", proj);
			
			
			double minErr=DBL_MAX;
			for (double cfi=0.0; cfi<=1.0; cfi+=0.1)
			{
				double cf=cfi*proj;
				
				double sumCenter=1.0-cf;
				double gammaWater=(-cf*betaWater+1.0*alphaWater)/sumCenter;
				double gammaSand =(-cf*betaSand +1.0*alphaSand )/sumCenter;
				double gammaGrass=(-cf*betaGrass+1.0*alphaGrass)/sumCenter;
				if (gammaWater<0.0)
					gammaWater=0.0;
				if (gammaSand<0.0)
					gammaSand=0.0;
				if (gammaGrass<0.0)
					gammaGrass=0.0;
				double gammaSum=betaWater+betaSand+betaGrass;
				if (gammaSum<=0)
					continue;
				gammaWater/=gammaSum;
				gammaSand /=gammaSum;
				gammaGrass/=gammaSum;

				double finalGammaWater, finalGammaSand, finalGammaGrass;
				simulateRandomMap(r, gammaWater, gammaSand, gammaGrass, &finalGammaWater, &finalGammaSand, &finalGammaGrass);
				//printf("[%f]gamma-gamma=(%f, %f, %f) (%f).\n", cf, gammaWater, gammaSand, gammaGrass, gammaWater+gammaSand+gammaGrass);
				//printf("[%f]gamma-final=(%f, %f, %f) (%f).\n", cf, finalGammaWater, finalGammaSand, finalGammaGrass,  finalGammaWater+finalGammaSand+finalGammaGrass);

				double errGammaWater=finalGammaWater-baseWater;
				double errGammaSand =finalGammaSand -baseSand ;
				double errGammaGrass=finalGammaGrass-baseGrass;
				double errGamma=(errGammaWater*errGammaWater+errGammaSand*errGammaSand+errGammaGrass*errGammaGrass);
				//printf("[%f]err=%f.\n", cf, errGamma);
				if (errGamma<minErr)
				{
					minErr=errGamma;
					alphaWater=gammaWater;
					alphaSand =gammaSand;
					alphaGrass=gammaGrass;
				}
			}
			//printf("best-gamma=(%f, %f, %f) (%f) err=%f.\n", alphaWater, alphaSand, alphaGrass, alphaWater+alphaSand+alphaGrass, minErr);
			
		}
	}
	
	printf("makeRandomMap::new-base =(%f, %f, %f).\n", alphaWater, alphaSand, alphaGrass);
	
	double simWater, simSand, simGrass;
	simulateRandomMap(smooth, alphaWater, alphaSand, alphaGrass, &simWater, &simSand, &simGrass);
	printf("makeRandomMap::simulateRandomMap=(%f, %f, %f).\n", simWater, simSand, simGrass);
	
	totalRatio=0x7FFF;
	waterRatio=(int)(((double)alphaWater)*((double)totalRatio));
	sandRatio =(int)(((double)alphaSand )*((double)totalRatio));
	grassRatio=(int)(((double)alphaGrass)*((double)totalRatio));
	if (waterRatio<0)
		waterRatio=0;
	if (sandRatio<0)
		sandRatio=0;
	if (grassRatio<0)
		grassRatio=0;
	totalRatio=waterRatio+sandRatio+grassRatio;
	//printf("ratios=(%d, %d, %d) / (%d).\n", waterRatio, sandRatio, grassRatio, totalRatio);
	
	// First, we create a fully random patchwork:
	for (int y=0; y<h; y++)
		for (int x=0; x<w; x++)
		{
			int r=syncRand()%totalRatio;
			r-=waterRatio;
			if (r<0)
			{
				undermap[y*w+x]=WATER;
				continue;
			}
			r-=sandRatio;
			if (r<0)
			{
				undermap[y*w+x]=SAND;
				continue;
			}
			r-=grassRatio;
			if (r<0)
			{
				undermap[y*w+x]=GRASS;
				continue;
			}
			assert(false);//Want's to sing ?
		}
	
	// What's finally in ?
	int waterCount=0;
	int sandCount =0;
	int grassCount=0;
	for (int y=0; y<h; y++)
		for (int x=0; x<w; x++)
		{
			switch (undermap[y*w+x])
			{
				case WATER:
					waterCount++;
				continue;
				case SAND :
					sandCount ++;
				continue;
				case GRASS:
					grassCount++;
				continue;
			}
		}
	double totalCount=(double)(waterCount+sandCount+grassCount);
	printf("makeRandomMap::beforeCount=(%f, %f, %f).\n", waterCount/totalCount, sandCount/totalCount, grassCount/totalCount);
	
	for (int i=0; i<smooth; i++)
	{
		// What's in now?
		waterCount=0;
		sandCount =0;
		grassCount=0;
		for (int y=0; y<h; y++)
			for (int x=0; x<w; x++)
			{
				switch (undermap[y*w+x])
				{
					case WATER:
						waterCount++;
					continue;
					case SAND :
						sandCount ++;
					continue;
					case GRASS:
						grassCount++;
					continue;
				}
			}
		double totalRatioCount=(double)(waterCount+sandCount+grassCount);
		double waterRatioCount=waterCount/totalRatioCount;
		double sandRatioCount =sandCount /totalRatioCount;
		double grassRatioCount=grassCount/totalRatioCount;
		
		double errWaterRatioCount=waterRatioCount-baseWater;
		double errSandRatioCount =sandRatioCount -baseSand ;
		double errGrassRatioCount=grassRatioCount-baseGrass;
		
		//printf("[%d]errCount=(%f, %f, %f).\n", i, errWaterRatioCount, errSandRatioCount, errGrassRatioCount);
		Uint32 allowed[3];
		if (errWaterRatioCount>0)
			allowed[0]=(Uint32)(pow(errWaterRatioCount, 0.125)*4294967296.0);
		else
			allowed[0]=0;
		if (errSandRatioCount>0)
			allowed[1]=(Uint32)(pow(errSandRatioCount , 0.125)*4294967296.0);
		else
			allowed[1]=0;
		if (errGrassRatioCount>0)
			allowed[2]=(Uint32)(pow(errGrassRatioCount, 0.125)*4294967296.0);
		else
			allowed[2]=0;
		
		assert(allowed[0]<=(Uint32)0xFFFFFFFF);
		assert(allowed[1]<=(Uint32)0xFFFFFFFF);
		assert(allowed[2]<=(Uint32)0xFFFFFFFF);
		
		if (i==0)
		{
			allowed[0]=0;
			allowed[1]=0;
			allowed[2]=0;
		}
		//printf("[%d]allowed=(%d, %d, %d).\n", i, allowed[0], allowed[1], allowed[2]);
		
		for (int y=0; y<h; y++)
			for (int x=0; x<w; x++)
			{
				if (syncRand()&4)
				{
					int a=getUMTerrain(x+1, y);
					int b=getUMTerrain(x-1, y);
					if ((a==b)&&(allowed[a]<=syncRand()))
					{
						setUMTerrain(x, y, (TerrainType)a);
						continue;
					}
				}
				else
				{
					int a=getUMTerrain(x, y-1);
					int b=getUMTerrain(x, y+1);
					if ((a==b)&&(allowed[a]<=syncRand()))
					{
						setUMTerrain(x, y, (TerrainType)a);
						continue;
					}
				}
				if (syncRand()&4)
				{
					int a=getUMTerrain(x+1, y+1);
					int b=getUMTerrain(x-1, y-1);
					if ((a==b)&&(allowed[a]<=syncRand()))
					{
						setUMTerrain(x, y, (TerrainType)a);
						continue;
					}
				}
				else
				{
					int a=getUMTerrain(x+1, y-1);
					int b=getUMTerrain(x-1, y+1);
					if ((a==b)&&(allowed[a]<=syncRand()))
					{
						setUMTerrain(x, y, (TerrainType)a);
						continue;
					}
				}
				if (syncRand()&4)
				{
					int a=getUMTerrain(x+2, y);
					int b=getUMTerrain(x-2, y);
					if ((a==b)&&(allowed[a]<=syncRand()))
					{
						setUMTerrain(x, y, (TerrainType)a);
						continue;
					}
				}
				else
				{
					int a=getUMTerrain(x, y-2);
					int b=getUMTerrain(x, y+2);
					if ((a==b)&&(allowed[a]<=syncRand()))
					{
						setUMTerrain(x, y, (TerrainType)a);
						continue;
					}
				}
				if (syncRand()&4)
				{
					int a=getUMTerrain(x+2, y+2);
					int b=getUMTerrain(x-2, y-2);
					if ((a==b)&&(allowed[a]<=syncRand()))
					{
						setUMTerrain(x, y, (TerrainType)a);
						continue;
					}
				}
				else
				{
					int a=getUMTerrain(x+2, y-2);
					int b=getUMTerrain(x-2, y+2);
					if ((a==b)&&(allowed[a]<=syncRand()))
					{
						setUMTerrain(x, y, (TerrainType)a);
						continue;
					}
				}
			}
	}
	// What's finally in ?
	waterCount=0;
	sandCount =0;
	grassCount=0;
	for (int y=0; y<h; y++)
		for (int x=0; x<w; x++)
		{
			switch (undermap[y*w+x])
			{
				case WATER:
					waterCount++;
				continue;
				case SAND :
					sandCount ++;
				continue;
				case GRASS:
					grassCount++;
				continue;
			}
		}
	totalCount=(double)(waterCount+sandCount+grassCount);
	printf("makeRandomMap::final count=(%f, %f, %f).\n", waterCount/totalCount, sandCount/totalCount, grassCount/totalCount);
	
	controlSand();
	
	//Now, we have to find suitable places for teams:
	int nbTeams=descriptor.nbTeams;
	int minDistSquare=(int)((double)((double)w*(double)h*(double)grassCount)/(double)((double)nbTeams*(double)totalCount));
	printf("minDistSquare=%d (%f).\n", minDistSquare, sqrt((double)minDistSquare));
	if (minDistSquare<=0)
		return false;
	assert(minDistSquare>0);
	int* bootX=descriptor.bootX;
	int* bootY=descriptor.bootY;
	
	//TODO: First pass to find the number of aviable places.
	for (int team=0; team<nbTeams; team++)
	{
		int maxSurface=0;
		int maxX=0;
		int maxY=0;
		for (int y=0; y<h; y++)
		{
			int width=0;
			int startx=0;
			for (int x=0; x<w; x++)
			{
				int a=undermap[y*w+x];
				if (a==GRASS)
					width++;
				else
				{
					if (width>7)
					{
						int centerx=((x+startx)>>1);
						int top, bot;
						for (top=0; top<h; top++)
							if (getUMTerrain(centerx, y-top)!=GRASS)
								break;
						for (bot=0; bot<h; bot++)
							if (getUMTerrain(centerx, y+bot)!=GRASS)
								break;
						int height=top+bot-1;
						int surface=height*width;
						assert(surface>0);
						
						int centery=y+((bot-top)>>1);
						bool farEnough=true;
						for (int ti=0; ti<team; ti++)
							if (warpDistSquare(centerx, centery, bootX[ti], bootY[ti])<minDistSquare)
							{
								farEnough=false;
								break;
							}
						
						if (surface>maxSurface && farEnough)
						{
							maxSurface=surface;
							maxX=centerx;
							maxY=centery;
						}
					}
					width=0;
					startx=x;
				}
			}
		}
		
		if (maxSurface<=0)
			return false;
		assert(maxSurface);
		bootX[team]=maxX;
		bootY[team]=maxY;
		
		for (int dx=-1; dx<6; dx++)
			for (int dy=0; dy<6; dy++)
				setUMTerrain(maxX+dx, maxY+dy, GRASS);
		
		//printf("team=%d, maxSurface=%d, maxX=%d, maxY=%d.\n", team, maxSurface, maxX, maxY);
	}
	
	// Let's add some green space for teams:
	int squareSize=5+(int)(sqrt((double)minDistSquare)/4.5);
	printf("squareSize=%d.\n", squareSize);
	for (int team=0; team<nbTeams; team++)
	{
		setUMatPos(descriptor.bootX[team]+2, descriptor.bootY[team]+0, GRASS, squareSize);
		setUMatPos(descriptor.bootX[team]+2, descriptor.bootY[team]+2, GRASS, squareSize);
	}
	
	controlSand();
	regenerateMap(0, 0, w, h);
	
	return true;
}

void Map::addRessourcesRandomMap(MapGenerationDescriptor &descriptor)
{
	int *bootX=descriptor.bootX;
	int *bootY=descriptor.bootY;
	int nbTeams=descriptor.nbTeams;
	int limiteDist=(w+h)/(2*nbTeams);
	
	// let's add ressources...
	for (int team=0; team<nbTeams; team++)
	{
		int smallestWidth=limiteDist;
		int smallestRessource=0;
		
		bool dirUsed[8];
		for (int i=0; i<8; i++)
			dirUsed[i]=false;
		int ressOrder[8];
		ressOrder[0]=CORN;
		ressOrder[1]=WOOD;
		ressOrder[2]=STONE;
		ressOrder[3]=FUNGUS;
		ressOrder[4]=CORN;
		
		int distWeight[8];
		distWeight[0]=1;
		distWeight[1]=1;
		distWeight[2]=1;
		distWeight[3]=1;
		distWeight[4]=1;
		
		int widthWeight[8];
		widthWeight[0]=1;
		widthWeight[1]=1;
		widthWeight[2]=1;
		widthWeight[3]=1;
		widthWeight[4]=1;
		
		for (int resi=0; resi<=4; resi++)
		{
			int ress=ressOrder[resi];
			int maxDir=0;
			int maxWidth=0;
			int maxDist=0;
			for (int dir=0; dir<8; dir++)
				if (!dirUsed[dir])
				{
					int width=0;
					int dx, dy, dist;
					Unit::dxdyfromDirection(dir, &dx, &dy);
					for (dist=0; dist<limiteDist; dist++)
						if (isGrass(bootX[team]+dx*dist, bootY[team]+dy*dist))
							width++;
						else if (width>3)
							break;
						else
							width=1;

					if (dist*distWeight[resi]+width*widthWeight[resi]>maxDist*distWeight[resi]+maxWidth*widthWeight[resi])
					{
						maxWidth=width;
						maxDist=dist;
						maxDir=dir;
					}
				}
			dirUsed[maxDir]=true;
			if (maxWidth<smallestWidth)
			{
				smallestWidth=maxWidth;
				smallestRessource=ress;
			}
			
			int dx, dy;
			Unit::dxdyfromDirection(maxDir, &dx, &dy);
			int d=maxDist-(maxWidth>>1);
			dx*=d;
			dy*=d;
			//printf("ress=%d, maxDir=%d, maxDist=%d, d=(%d, %d), pos=(%d, %d).\n", ress, maxDir, maxDist, dx, dy, bootX[team]+dx, bootY[team]+dy);
			int amount=descriptor.ressource[ress];
			if (amount>0)
				setRessource(bootX[team]+dx, bootY[team]+dy, (RessourcesTypes::intResType)ress, amount);
		}

		if (smallestWidth<limiteDist)
		{
			int maxDir=0;
			int maxWidth=0;
			int maxDist=0;
			for (int dir=0; dir<8; dir++)
				if (!dirUsed[dir])
				{
					int width=0;
					int dx, dy, dist;
					Unit::dxdyfromDirection(dir, &dx, &dy);
					for (dist=0; dist<2*limiteDist; dist++)
						if (isGrass(bootX[team]+dx*dist, bootY[team]+dy*dist))
							width++;
						else if (width>3)
							break;
						else
							width=1;

					if (dist+width>maxDist+maxWidth)
					{
						maxWidth=width;
						maxDist=dist;
						maxDir=dir;
					}
				}
			dirUsed[maxDir]=true;
			
			int dx, dy;
			Unit::dxdyfromDirection(maxDir, &dx, &dy);
			int d=maxDist-(maxWidth>>1);
			dx*=d;
			dy*=d;
			//printf("ress=%d, maxDir=%d, maxDist=%d, d=(%d, %d), pos=(%d, %d).\n", smallestRessource, maxDir, maxDist, dx, dy, bootX[team]+dx, bootY[team]+dy);
			int amount=descriptor.ressource[smallestRessource];
			if (amount>0)
				setRessource(bootX[team]+dx, bootY[team]+dy, (RessourcesTypes::intResType)smallestRessource, amount);
		}

		int maxDir=0;
		int maxWidth=0;
		int maxDist=0;
		for (int dir=0; dir<8; dir++)
		{
			int width=0;
			int dx, dy, dist;
			Unit::dxdyfromDirection(dir, &dx, &dy);
			for (dist=0; dist<2*limiteDist; dist++)
				if (isWater(bootX[team]+dx*dist, bootY[team]+dy*dist))
					width++;
				else if (width>3)
					break;
				else
					width=1;
				
			if (dist+width>width+maxWidth)
			{
				maxWidth=width;
				maxDist=dist;
				maxDir=dir;
			}
		}
		
		int dx, dy;
		Unit::dxdyfromDirection(maxDir, &dx, &dy);
		int d=maxDist-(maxWidth>>1);
		dx*=d;
		dy*=d;
		//printf("ALGA, maxDir=%d, maxWidth=%d, d=(%d, %d), pos=(%d, %d).\n", maxDir, maxWidth, dx, dy, bootX[team]+dx, bootY[team]+dy);
		int amount=descriptor.ressource[ALGA];
		if (amount>0)
			setRessource(bootX[team]+dx, bootY[team]+dy, ALGA, amount);
	}
	
	// Let's smooth ressources...
	int maxAmount=0;
	for (int r=0; r<4; r++)
		if (maxAmount<descriptor.ressource[r])
			maxAmount=descriptor.ressource[r];
	smoothRessources(maxAmount*3);
}

bool Map::makeIslandsMap(MapGenerationDescriptor &descriptor)
{
	// First, fill with water:
	for (int y=0; y<h; y++)
		for (int x=0; x<w; x++)
			undermap[y*w+x]=WATER;
		
	// Two, plants "bootstraps"
	int* bootX=descriptor.bootX;
	int* bootY=descriptor.bootY;
	int nbIslands=descriptor.nbTeams;
	int islandsSize=(int)(((w+h)*descriptor.islandsSize)/(400.0*sqrt((double)nbIslands)));
	if (islandsSize<8)
		islandsSize=8;
	int minDistSquare=(w*h)/nbIslands;
	//printf("minDistSquare=%d.\n", minDistSquare);
	
	int c=0;
	for (int i=0; i<nbIslands; i++)
	{
		int x=syncRand()%w;
		int y=syncRand()%h;
		bool failed=false;
		int j;
		for (j=0; j<i; j++)
			if (warpDistSquare(x, y, bootX[j], bootY[j])<minDistSquare)
			{
				failed=true;
				break;
			}
		if (failed)
		{
			//printf("failed new(%d, %d) boot[%d](%d, %d).\n", x, y, i, bootX[j], bootY[j]);
			i--;
			if (c++>65536)
			{
				minDistSquare=minDistSquare>>1;
				//I think that you need to do this only once, in worst case.
				//With a few luck you doesn't need to.
				c=0;
				//printf("minDistSquare=%d.\n", minDistSquare);
			}
		}
		else
		{
			bootX[i]=x;
			bootY[i]=y;
			for (int dx=-1; dx<6; dx++)
				for (int dy=0; dy<6; dy++)
					setUMTerrain(x+dx, y+dy, GRASS);
		}
	}
	
	//printf("w=%d, h=%d, m=%d\n", w, h, m);
	//for (int i=0; i<descriptor.nbTeams; i++)
	//	printf("boot[%d]=(%d, %d).\n", i, bootX[i], bootY[i]);
	
	// Three, expands islands
	for (int s=0; s<islandsSize; s++)
	{
		for (int y=0; y<h; y+=2)
			for (int x=0; x<w; x+=2)
			{
				TerrainType umt=getUMTerrain(x, y);
				if (umt==GRASS)
					continue;
				
				int a, b;
				switch (syncRand()&15)
				{
				case 0:
					a=getUMTerrain(x+1, y);
					b=getUMTerrain(x-1, y);
					if ((a==GRASS)||(b==GRASS))
					{
						setUMTerrain(x, y, GRASS);
						continue;
					}
				break;
				case 1:
					a=getUMTerrain(x, y-1);
					b=getUMTerrain(x, y+1);
					if ((a==GRASS)||(b==GRASS))
					{
						setUMTerrain(x, y, GRASS);
						continue;
					}
				break;
				case 2:
					a=getUMTerrain(x+1, y+1);
					b=getUMTerrain(x-1, y-1);
					if ((a==GRASS)||(b==GRASS))
					{
						setUMTerrain(x, y, GRASS);
						continue;
					}
				break;
				case 3:
					a=getUMTerrain(x+1, y-1);
					b=getUMTerrain(x-1, y+1);
					if ((a==GRASS)||(b==GRASS))
					{
						setUMTerrain(x, y, GRASS);
						continue;
					}
				break;
				case 4:
					a=getUMTerrain(x+2, y);
					b=getUMTerrain(x-2, y);
					if ((a==GRASS)||(b==GRASS))
					{
						setUMTerrain(x, y, GRASS);
						continue;
					}
				break;
				case 5:
					a=getUMTerrain(x, y-2);
					b=getUMTerrain(x, y+2);
					if ((a==GRASS)||(b==GRASS))
					{
						setUMTerrain(x, y, GRASS);
						continue;
					}
				break;
				case 6:
					a=getUMTerrain(x+2, y+2);
					b=getUMTerrain(x-2, y-2);
					if ((a==GRASS)||(b==GRASS))
					{
						setUMTerrain(x, y, GRASS);
						continue;
					}
				break;
				case 7:
					a=getUMTerrain(x+2, y-2);
					b=getUMTerrain(x-2, y+2);
					if ((a==GRASS)||(b==GRASS))
					{
						setUMTerrain(x, y, GRASS);
						continue;
					}
				break;
				default:
				break;
				}
			}
		for (int y=1; y<h; y+=2)
			for (int x=1; x<w; x+=2)
			{
				TerrainType umt=getUMTerrain(x, y);
				if (umt==GRASS)
					continue;
				
				int a, b;
				switch (syncRand()&15)
				{
				case 0:
					a=getUMTerrain(x+1, y);
					b=getUMTerrain(x-1, y);
					if ((a==GRASS)||(b==GRASS))
					{
						setUMTerrain(x, y, GRASS);
						continue;
					}
				break;
				case 1:
					a=getUMTerrain(x, y-1);
					b=getUMTerrain(x, y+1);
					if ((a==GRASS)||(b==GRASS))
					{
						setUMTerrain(x, y, GRASS);
						continue;
					}
				break;
				case 2:
					a=getUMTerrain(x+1, y+1);
					b=getUMTerrain(x-1, y-1);
					if ((a==GRASS)||(b==GRASS))
					{
						setUMTerrain(x, y, GRASS);
						continue;
					}
				break;
				case 3:
					a=getUMTerrain(x+1, y-1);
					b=getUMTerrain(x-1, y+1);
					if ((a==GRASS)||(b==GRASS))
					{
						setUMTerrain(x, y, GRASS);
						continue;
					}
				break;
				case 4:
					a=getUMTerrain(x+2, y);
					b=getUMTerrain(x-2, y);
					if ((a==GRASS)||(b==GRASS))
					{
						setUMTerrain(x, y, GRASS);
						continue;
					}
				break;
				case 5:
					a=getUMTerrain(x, y-2);
					b=getUMTerrain(x, y+2);
					if ((a==GRASS)||(b==GRASS))
					{
						setUMTerrain(x, y, GRASS);
						continue;
					}
				break;
				case 6:
					a=getUMTerrain(x+2, y+2);
					b=getUMTerrain(x-2, y-2);
					if ((a==GRASS)||(b==GRASS))
					{
						setUMTerrain(x, y, GRASS);
						continue;
					}
				break;
				case 7:
					a=getUMTerrain(x+2, y-2);
					b=getUMTerrain(x-2, y+2);
					if ((a==GRASS)||(b==GRASS))
					{
						setUMTerrain(x, y, GRASS);
						continue;
					}
				break;
				default:
				break;
				}
			}
	}
	
	// Four, avoid too much sand. Let's smooth
	for (int s=0; s<2; s++)
		for (int y=0; y<h; y++)
			for (int x=0; x<w; x++)
			{
				int a, b;
				a=getUMTerrain(x+1, y);
				b=getUMTerrain(x-1, y);
				if ((a==GRASS)&&(b==GRASS))
				{
					setUMTerrain(x, y, GRASS);
					continue;
				}
				a=getUMTerrain(x, y-1);
				b=getUMTerrain(x, y+1);
				if ((a==GRASS)&&(b==GRASS))
				{
					setUMTerrain(x, y, GRASS);
					continue;
				}
				a=getUMTerrain(x+1, y+1);
				b=getUMTerrain(x-1, y-1);
				if ((a==GRASS)&&(b==GRASS))
				{
					setUMTerrain(x, y, GRASS);
					continue;
				}
				a=getUMTerrain(x+1, y-1);
				b=getUMTerrain(x-1, y+1);
				if ((a==GRASS)&&(b==GRASS))
				{
					setUMTerrain(x, y, GRASS);
					continue;
				}
			}
	
	controlSand();
	
	// Five, add some sand
	for (int s=0; s<descriptor.beach; s++)
		for (int dy=0; dy<4; dy++)
			for (int dx=0; dx<4; dx++)
				for (int y=dy; y<h; y+=4)
					for (int x=dx; x<w; x+=4)
					{
						int a, b;
						switch (syncRand()&7)
						{
						case 0:
							a=getUMTerrain(x+1, y);
							b=getUMTerrain(x-1, y);
							if ((a==SAND)&&(b==WATER)||(a==WATER)&&(b==SAND))
							{
								setUMTerrain(x, y, SAND);
								continue;
							}
						break;
						case 1:
							a=getUMTerrain(x, y-1);
							b=getUMTerrain(x, y+1);
							if ((a==SAND)&&(b==WATER)||(a==WATER)&&(b==SAND))
							{
								setUMTerrain(x, y, SAND);
								continue;
							}
						break;
						case 2:
							a=getUMTerrain(x+1, y+1);
							b=getUMTerrain(x-1, y-1);
							if ((a==SAND)&&(b==WATER)||(a==WATER)&&(b==SAND))
							{
								setUMTerrain(x, y, SAND);
								continue;
							}
						break;
						case 3:
							a=getUMTerrain(x+1, y-1);
							b=getUMTerrain(x-1, y+1);
							if ((a==SAND)&&(b==WATER)||(a==WATER)&&(b==SAND))
							{
								setUMTerrain(x, y, SAND);
								continue;
							}
						break;
						
						
						case 4:
							a=getUMTerrain(x+1, y);
							b=getUMTerrain(x-1, y);
							if ((a==SAND)&&(b==SAND))
							{
								setUMTerrain(x, y, SAND);
								continue;
							}
						break;
						case 5:
							a=getUMTerrain(x, y-1);
							b=getUMTerrain(x, y+1);
							if ((a==SAND)&&(b==SAND))
							{
								setUMTerrain(x, y, SAND);
								continue;
							}
						break;
						case 6:
							a=getUMTerrain(x+1, y+1);
							b=getUMTerrain(x-1, y-1);
							if ((a==SAND)&&(b==SAND))
							{
								setUMTerrain(x, y, SAND);
								continue;
							}
						break;
						case 7:
							a=getUMTerrain(x+1, y-1);
							b=getUMTerrain(x-1, y+1);
							if ((a==SAND)&&(b==SAND))
							{
								setUMTerrain(x, y, SAND);
								continue;
							}
						break;
						}
				}
	
	
	//controlSand();
	regenerateMap(0, 0, w, h);
	return true;
}

void Map::addRessourcesIslandsMap(MapGenerationDescriptor &descriptor)
{
	int *bootX=descriptor.bootX;
	int *bootY=descriptor.bootY;
	
	int islandsSize=(int)(((w+h)*descriptor.islandsSize)/(400.0*sqrt((double)descriptor.nbTeams)));
	if (islandsSize<8)
		islandsSize=8;
	// let's add ressources...
	int smoothRessources=islandsSize/4;
	for (int s=0; s<descriptor.nbTeams; s++)
	{
		int d, p, amount;
		int smallestAmount;
		RessourcesTypes::intResType smallestRessource;

		//WOOD
		for (d=0; d<islandsSize; d++)
			if (!isGrass(bootX[s], bootY[s]-d))
				break;
		amount=descriptor.ressource[WOOD];
		amount=d-smoothRessources-2;
		if (amount<1)
			amount=1;
		p=d-1-amount/2;
		if (amount>0)
			setRessource(bootX[s], bootY[s]-p, WOOD, amount);
		smallestAmount=amount;
		smallestRessource=WOOD;
		
		//CORN
		for (d=0; d<islandsSize; d++)
			if (!isGrass(bootX[s]-d, bootY[s]))
				break;
		amount=descriptor.ressource[CORN];
		amount=d-smoothRessources-0;
		if (amount<1)
			amount=1;
		p=d-1-amount/2;
		if (amount>0)
			setRessource(bootX[s]-p, bootY[s], CORN, amount);
		if (amount<smallestAmount)
		{
			smallestAmount=amount;
			smallestRessource=CORN;
		}
		
		//STONE
		for (d=0; d<islandsSize; d++)
			if (!isGrass(bootX[s], bootY[s]+d))
				break;
		amount=descriptor.ressource[STONE];
		amount=d-smoothRessources-3;
		if (amount<1)
			amount=1;
		p=d-1-amount/2;
		if (amount>0)
			setRessource(bootX[s], bootY[s]+p, STONE, amount);
		if (amount<smallestAmount)
		{
			smallestAmount=amount;
			smallestRessource=STONE;
		}
		
		
		//We add the ressource with the smallest amount:
		for (d=0; d<islandsSize; d++)
			if (!isGrass(bootX[s]+d, bootY[s]+d))
				break;
		amount=descriptor.ressource[smallestRessource];
		amount=d-smoothRessources-3;
		if (amount<1)
			amount=1;
		p=d-1-amount/2;
		if (amount>0)
			setRessource(bootX[s]+p, bootY[s]+p, smallestRessource, amount);
		
		//ALGA
		for (d=0; d<2*islandsSize; d++)
			if (isWater(bootX[s]+d, bootY[s]))
				break;
		amount=descriptor.ressource[ALGA];
		amount=smoothRessources;
		p=d+smoothRessources-1+amount/2;
		if (amount>0)
			setRessource(bootX[s]+p, bootY[s], ALGA, amount);
	}

	// Let's smooth ressources...
	this->smoothRessources(smoothRessources*2);
}

bool Game::makeIslandsMap(MapGenerationDescriptor &descriptor)
{
	for (int s=0; s<descriptor.nbTeams; s++)
	{
		if (session.numberOfTeam<=s)
			addTeam();
		int squareSize=5+descriptor.islandsSize/10;
		map.setUMatPos(descriptor.bootX[s]+2, descriptor.bootY[s]+0, GRASS, squareSize);
		map.setUMatPos(descriptor.bootX[s]+2, descriptor.bootY[s]+2, GRASS, squareSize);
		
		Sint32 typeNum=globalContainer->buildingsTypes.getTypeNum(BuildingType::SWARM_BUILDING, 0, false);
		bool good=checkRoomForBuilding(descriptor.bootX[s], descriptor.bootY[s], typeNum, -1);
		assert(good);
		teams[s]->startPosX=descriptor.bootX[s];
		teams[s]->startPosY=descriptor.bootY[s];
		Building *b=addBuilding(descriptor.bootX[s], descriptor.bootY[s], s, typeNum);
		assert(b);
		for (int i=0; i<descriptor.nbWorkers; i++)
		{
			Unit *u=addUnit(descriptor.bootX[s]+(i%4), descriptor.bootY[s]-1-(i/4), s, WORKER, 0, 0, 0, 0);
			if (u==NULL)
				return false;
			assert(u);
		}
		teams[s]->createLists();
	}
	map.smoothRessources(descriptor.islandsSize/10);
	return true;
}

bool Game::makeRandomMap(MapGenerationDescriptor &descriptor)
{
	for (int s=0; s<descriptor.nbTeams; s++)
	{
		assert(session.numberOfTeam==s);
		if (session.numberOfTeam<=s)
			addTeam();
		
		map.setUMatPos(descriptor.bootX[s]+2, descriptor.bootY[s]+0, GRASS, 5);
		map.setUMatPos(descriptor.bootX[s]+2, descriptor.bootY[s]+2, GRASS, 5);
		
		Sint32 typeNum=globalContainer->buildingsTypes.getTypeNum(BuildingType::SWARM_BUILDING, 0, false);
		bool good=checkRoomForBuilding(descriptor.bootX[s], descriptor.bootY[s], typeNum, -1);
		assert(good);
		teams[s]->startPosX=descriptor.bootX[s];
		teams[s]->startPosY=descriptor.bootY[s];
		Building *b=addBuilding(descriptor.bootX[s], descriptor.bootY[s], s, typeNum);
		assert(b);
		for (int i=0; i<descriptor.nbWorkers; i++)
		{
			Unit *u=addUnit(descriptor.bootX[s]+(i%4), descriptor.bootY[s]-1-(i/4), s, WORKER, 0, 0, 0, 0);
			if (u==NULL)
				return false;
			assert(u);
		}
		teams[s]->createLists();
	}
	return true;
}

bool Game::generateMap(MapGenerationDescriptor &descriptor)
{
	printf("Generating map, please wait ....\n");
	descriptor.synchronizeNow();
	map.setSize(descriptor.wDec, descriptor.hDec);
	map.setGame(this);
	switch (descriptor.methode)
	{
		case MapGenerationDescriptor::eUNIFORM:
			map.makeHomogenMap(descriptor.terrainType);
			addTeam();
		break;
		case MapGenerationDescriptor::eRANDOM:
			if (!map.makeRandomMap(descriptor))
				return false;
			map.addRessourcesRandomMap(descriptor);
			if (!makeRandomMap(descriptor))
				return false;
		break;
		case MapGenerationDescriptor::eISLANDS:
			if (!map.makeIslandsMap(descriptor))
				return false;
			map.addRessourcesIslandsMap(descriptor);
			if (!makeIslandsMap(descriptor))
				return false;
		break;
		default:
			assert(false);
	}
	if (session.mapGenerationDescriptor)
		delete session.mapGenerationDescriptor;
	session.mapGenerationDescriptor=new MapGenerationDescriptor(descriptor);
	printf(".... map generated.\n");
	return true;
}
/*
WATER=0,
SAND=1,
GRASS=2
*/
