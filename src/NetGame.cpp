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

#include "NetGame.h"
#include "GlobalContainer.h"
#include "Order.h"
#include "NetDefine.h"
#include <assert.h>
#include "Marshaling.h"
#include "LogFileManager.h"
#include "Utilities.h"

NetGame::NetGame(UDPsocket socket, int numberOfPlayer, Player *players[32])
{
	// we have right number of player & valid players
	assert((numberOfPlayer>=0) && (numberOfPlayer<=32));
	assert(players);
	assert(defaultLatency+1<256);
	assert(defaultLatency>=1);

	// we initialize and copy stuff
	this->numberOfPlayer=numberOfPlayer;
	memcpy(this->players, players, numberOfPlayer*sizeof(Player *));

	// we find local player
	localPlayerNumber=-1;
	for (int p=0; p<numberOfPlayer; p++)
	{
		assert(players[p]);
		if (players[p]->type==Player::P_LOCAL)
			localPlayerNumber=p;
	}
	assert(localPlayerNumber!=-1);
	
	waitingForPlayerMask=0;
	ordersByPackets=2;

	this->socket=socket;
	
	logFile=globalContainer->logFileManager->getFile("NetGame.log");
	//logFile=stdout;
	assert(logFile);
	
	for (int p=0; p<32; p++)
		for (int step=0; step<256; step++)
			ordersQueue[p][step]=NULL;
	
	init();
};

NetGame::~NetGame()
{
	for (int p=0; p<32; p++)
	{
		for (int step=0; step<256; step++)
			if (ordersQueue[p][step])
			{
				delete ordersQueue[p][step];
				ordersQueue[p][step]=NULL;
			}
	}
	if (socket)
	{
		// We have to prevents players to close the same socket:
		for (int p=0; p<numberOfPlayer; p++)
			if (players[p]->socket==socket)
				players[p]->channel=-1;
		SDLNet_UDP_Close(socket);
		socket=NULL;
	}
}

void NetGame::init(void)
{
	executeStep=0;
	pushStep=0+defaultLatency;
	freeingStep=256-maxLatency-2;
	myLocalWishedLatency=defaultLatency;
	for (int p=0; p<numberOfPlayer; p++)
		wishedLatency[p]=defaultLatency;
	
	waitingForPlayerMask=0;
	
	// We start with no checkSum aviable:
	for (int i=0; i<256; i++)
	{
		checkSumsLocal[i]=0;
		checkSumsRemote[i]=0;
	}
	
	for (int p=0; p<numberOfPlayer; p++)
	{
		// We free any order:
		for (int step=0; step<256; step++)
		{
			if (ordersQueue[p][step])
				delete ordersQueue[p][step];
			ordersQueue[p][step]=NULL;
		}
		
		// We have to create new orders to be freed:
		for (int step=freeingStep; step<256; step++)
		{
			Order *order=new NullOrder();
			order->sender=p;
			order->inQueue=true;
			ordersQueue[p][step]=order;
		}
		
		// We have to create the first unsent orders:
		for (int step=0; step<pushStep; step++)
		{
			Order *order=new NullOrder();
			order->sender=p;
			order->inQueue=true;
			ordersQueue[p][step]=order;
		}
		
		// We write that the first unsent orders are received:
		lastReceivedFromMe[p]=pushStep-1;
		
		// At the beginning, no-one is late:
		countDown[p]=0;
		// And everyone thinks that everyone is connected:
		droppingPlayersMask[p]=0;
		
		for (int i=0; i<64; i++)
			recentsPingPong[p][i]=defaultLatency;
		pingPongCount[p]=0;
		recentsPingPong[p][pingPongCount[p]]=0;
		pingPongMax[p]=defaultLatency;
		
		for (int i=0; i<64; i++)
			recentOrderMarginTime[p][i]=defaultLatency;
		orderMarginTimeCount[p]=0;
		orderMarginTimeMin[p]=defaultLatency;
		orderMarginTimeMax[p]=defaultLatency;
	}
	
	dropState=DS_NO_DROP_PROCESSING;
};

/*Uint32 NetGame::whoMaskAreWeWaitingFor(void)
{
	Uint32 waitingPlayersMask=0;
	for (int p=0; p<numberOfPlayer; p++)
	{
		int type=players[p]->type;
		if (type==Player::P_IP && !players[p]->quitting)
			if (ordersQueue[p][executeStep]==NULL || (lastReceivedFromMe[p]+1)==freeingStep)
				waitingPlayersMask|=1<<p;
	}
	return waitingPlayersMask;
};*/

Uint32 NetGame::whoMaskAreWeWaitingFor(void)
{
	Uint32 waitingPlayersMask=0;
	for (int p=0; p<numberOfPlayer; p++)
	{
		int type=players[p]->type;
		if (type==Player::P_IP && !players[p]->quitting)
		{
			if (ordersQueue[p][executeStep]==NULL)
			{
				printf("Can't execute step=%d of player=%d.\n", executeStep, p);
				waitingPlayersMask|=1<<p;
			}
			if ((lastReceivedFromMe[p]+1)==freeingStep)
			{
				printf("Can't free step=%d of player=%d.\n", freeingStep, p);
				waitingPlayersMask|=1<<p;
			}
		}
	}
	return waitingPlayersMask;
};

Uint32 NetGame::whoMaskCountedOut(void)
{
	Uint32 countedOutMask=0;
	for (int p=0; p<numberOfPlayer; p++)
		if (players[p]->type==Player::P_IP && countDown[p]>1)
			countedOutMask|=1<<p;
	return countedOutMask;
};

Uint8 NetGame::lastReceivedFromHim(int player)
{
	Uint8 step=freeingStep;
	while (ordersQueue[player][step])
		step++;
	return (step-1);
}

void NetGame::sendPushOrder(int targetPlayer)
{
	assert(players[targetPlayer]->type==Player::P_IP);
	assert(targetPlayer!=localPlayerNumber);
	
	int totalSize=4;
	Uint8 step=pushStep;
	int n;
	for (n=0; n<ordersByPackets; n++) //TODO: only send twice small orders. This means if size<=16
	{
		Order *order=ordersQueue[localPlayerNumber][step];
		assert(order);
		if (order->latencyPadding)
		{
			printf("sendPushOrder skipped step=%d.\n", step);
			assert(order->getOrderType()==ORDER_NULL);
			step++; // We skip orders which have been added to increase latency.
			order=ordersQueue[localPlayerNumber][step];
			assert(order);
		}
		totalSize+=3+order->getDataLength();
		if (step==executeStep)
		{
			n++;
			break;
		}
		step--;
	}
	Uint8 *data=(Uint8 *)malloc(totalSize);
	data[0]=MULTIPLE_ORDERS;
	data[1]=localPlayerNumber;
	data[2]=lastReceivedFromHim(targetPlayer);
	data[3]=myLocalWishedLatency;
	fprintf(logFile, "sendPushOrder.\n");
	fprintf(logFile, " myLocalWishedLatency=%d.\n", myLocalWishedLatency);
	fprintf(logFile, " lastReceivedFromHim(%d)=%d\n", targetPlayer, lastReceivedFromHim(targetPlayer));
	int l=4;
	step=pushStep;
	for (int i=0; i<n; i++)
	{
		Order *order=ordersQueue[localPlayerNumber][step];
		assert(order);
		if (order->latencyPadding)
		{
			assert(order->getOrderType()==ORDER_NULL);
			step++; // We skip orders which have been added to increase latency.
			order=ordersQueue[localPlayerNumber][step];
			assert(order);
		}
		int orderSize=order->getDataLength();
		data[l++]=step;
		data[l++]=orderSize;
		fprintf(logFile, " sending, step=%d, orderSize=%d\n", step, orderSize);
		data[l++]=order->getOrderType();
		memcpy(data+l, order->getData(), orderSize);
		l+=orderSize;
		step--;
	}
	assert(l==totalSize);
	//printf("send data=[%d, %d, %d, %d, %d]\n", data[0], data[1], data[2], data[3], data[4]);
	fprintf(logFile, " %d totalSize=%d.\n", n, totalSize);
	players[targetPlayer]->send(data, totalSize);
	
	free(data);
}

void NetGame::sendWaitingForPlayerOrder(int targetPlayer)
{
	assert(players[targetPlayer]->type==Player::P_IP || (players[targetPlayer]->type==Player::P_LOST_FINAL && players[targetPlayer]->quitting));
	assert(targetPlayer!=localPlayerNumber);

	int totalSize=4;
	WaitingForPlayerOrder *wfpo=new WaitingForPlayerOrder(waitingForPlayerMask);
	totalSize+=3+wfpo->getDataLength();

	Uint8 resendStep=lastReceivedFromMe[targetPlayer]+1;
	Order *order=ordersQueue[localPlayerNumber][resendStep];
	if (order)
	{
		totalSize+=3+order->getDataLength();
		fprintf(logFile, "resendAviable, resendStep[%d]=%d, v-wfpo\n", targetPlayer, resendStep);
	}
	
	Uint8 *data=(Uint8 *)malloc(totalSize);
	data[0]=MULTIPLE_ORDERS;
	data[1]=localPlayerNumber;
	data[2]=lastReceivedFromHim(targetPlayer);
	data[3]=myLocalWishedLatency;

	int l=4;
	int size=wfpo->getDataLength();
	data[l++]=executeStep;
	data[l++]=size;
	data[l++]=wfpo->getOrderType();
	memcpy(data+l, wfpo->getData(), size);
	l+=size;

	if (order)
	{
		int orderSize=order->getDataLength();
		data[l++]=resendStep;
		data[l++]=orderSize;
		data[l++]=order->getOrderType();
		memcpy(data+l, order->getData(), orderSize);
		l+=orderSize;
	}
	assert(l==totalSize);

	//printf("send data=[%d, %d, %d, %d, %d]\n", data[0], data[1], data[2], data[3], data[4]);
	fprintf(logFile, "totalSize=%d.\n", totalSize);
	players[targetPlayer]->send(data, totalSize);

	free(data);
	delete wfpo;
}

void NetGame::sendDroppingPlayersMask(int targetPlayer, bool askForReply)
{
	assert(players[targetPlayer]->type==Player::P_IP);
	assert(targetPlayer!=localPlayerNumber);

	int n=0;
	for (int p=0; p<numberOfPlayer; p++)
		if (players[p]->type==Player::P_IP && (droppingPlayersMask[localPlayerNumber]&(1<<p)))
			n++;
	int totalSize=12+n;
	fprintf(logFile, "sending ORDER_DROPPING_PLAYER to player %d, myDroppingPlayersMask=%x, executeStep=%d, n=%d\n", targetPlayer, droppingPlayersMask[localPlayerNumber], executeStep, n);
	
	Uint8 *data=(Uint8 *)malloc(totalSize);
	data[0]=MULTIPLE_ORDERS;
	data[1]=localPlayerNumber;
	data[2]=lastReceivedFromHim(targetPlayer);
	data[3]=myLocalWishedLatency;
	
	data[4]=executeStep;
	data[5]=5+n;
	data[6]=ORDER_DROPPING_PLAYER;
	data[7]=askForReply;
	addUint32(data, droppingPlayersMask[localPlayerNumber], 8);
	
	int l=0;
	for (int p=0; p<numberOfPlayer; p++)
		if (players[p]->type==Player::P_IP && (droppingPlayersMask[localPlayerNumber]&(1<<p)))
		{
			data[12+l]=lastReceivedFromHim(p);
			fprintf(logFile, " lastReceivedFromHim(%d)=%d.\n", p, data[12+l]);
			l++;
		}
	
	players[targetPlayer]->send(data, totalSize);

	free(data);
}

void NetGame::sendRequestingDeadAwayOrder(int missingPlayer, int targetPlayer, Uint8 resendingStep)
{
	assert(players[targetPlayer]->type==Player::P_IP);
	assert(targetPlayer!=localPlayerNumber);

	Uint8 *data=(Uint8 *)malloc(8);
	data[0]=MULTIPLE_ORDERS;
	data[1]=localPlayerNumber;
	data[2]=lastReceivedFromHim(targetPlayer);
	data[3]=myLocalWishedLatency;
	
	data[4]=resendingStep;
	data[5]=1;
	data[6]=ORDER_REQUESTING_AWAY;
	
	data[7]=missingPlayer;
	
	players[targetPlayer]->send(data, 8);

	free(data);
}

void NetGame::sendDeadAwayOrder(int missingPlayer, int targetPlayer, Uint8 resendingStep)
{
	assert(players[targetPlayer]->type==Player::P_IP);
	assert(targetPlayer!=localPlayerNumber);
	
	int totalSize=4;
	Uint8 step=resendingStep;
	int nbp=0;
	for (int n=0; n<ordersByPackets; n++)
	{
		Order *order=ordersQueue[missingPlayer][step];
		if (order)
		{
			totalSize+=3+order->getDataLength();
			nbp++;
		}
		step--;
	}
	
	Uint8 *data=(Uint8 *)malloc(totalSize);
	
	data[0]=MULTIPLE_ORDERS;
	data[1]=missingPlayer;
	data[2]=0;
	data[3]=0;
	
	fprintf(logFile, "sendDeadAwayOrder missingPlayer=%d, targetPlayer=%d.\n", missingPlayer, targetPlayer);
	int l=4;
	step=resendingStep;
	for (int n=0; n<ordersByPackets; n++)
	{
		Order *order=ordersQueue[missingPlayer][step];
		if (order)
		{
			int orderSize=order->getDataLength();
			data[l++]=step;
			data[l++]=orderSize;
			data[l++]=order->getOrderType();
			memcpy(data+l, order->getData(), orderSize);
			l+=orderSize;
		}
		step--;
	}
	assert(l==totalSize);
	
	fprintf(logFile, "%d totalSize=%d.\n", nbp, totalSize);
	players[targetPlayer]->send(data, totalSize);
	
	free(data);
}

void NetGame::pushOrder(Order *order, int playerNumber)
{
	assert(order);
	assert((playerNumber>=0) && (playerNumber<numberOfPlayer));
	assert(players[playerNumber]->type==Player::P_AI || players[playerNumber]->type==Player::P_LOCAL);
	
	//if ((order->getOrderType()!=73)&&(order->getOrderType()!=51))
	
	fprintf(logFile, "\n");
	fprintf(logFile, "NetGame::pushOrder playerNumber=(%d), pushStep=(%d), getOrderType=(%d).\n", playerNumber, pushStep, order->getOrderType());
	
	assert(ordersQueue[playerNumber][pushStep]==NULL);
	
	if (players[playerNumber]->quitting)
	{
		delete order;
		order=new NullOrder();
	}
	// TODO: add checksum.
	order->sender=playerNumber;
	order->inQueue=true;
	order->wishedLatency=myLocalWishedLatency;
	ordersQueue[playerNumber][pushStep]=order;
	
	if (localPlayerNumber==playerNumber && (pushStep&1==1))
	{
		for (int p=0; p<numberOfPlayer; p++)
			if (players[p]->type==Player::P_IP)
				sendPushOrder(p);
			else
				lastReceivedFromMe[p]=pushStep; // for AI and local player.
	}
}

Order *NetGame::getOrder(int playerNumber)
{
	assert((playerNumber>=0) && (playerNumber<numberOfPlayer));
	
	if (players[localPlayerNumber]->quitting)
	{
		if (playerNumber!=localPlayerNumber)
		{
			Order *order=new NullOrder();
			order->sender=playerNumber;
			order->inQueue=false;
			return order;
		}
		bool canQuit=true;
		for (int p=0; p<numberOfPlayer; p++)
		{
			bool good=false;
			Uint8 step=freeingStep;
			while (true)
				if (step==players[playerNumber]->quitStep)
				{
					good=true;
					break;
				}
				else if (step==lastReceivedFromMe[p])
					break;
				else
					step++;

			if (!good)
			{
				fprintf(logFile, "bad lastReceivedFromMe[%d]=%d.\n", p, lastReceivedFromMe[playerNumber]);
				canQuit=false;
				break;
			}
		}
		Order *order;
		if (canQuit)
		{
			players[playerNumber]->type=Player::P_LOST_FINAL;
			fprintf(logFile, "players[%d]->type=Player::P_LOST_FINAL, me\n", playerNumber);
			Uint8 step=freeingStep;
			while (step!=pushStep)
			{
				Order *order=ordersQueue[playerNumber][step];
				assert(order);
				delete order;
				ordersQueue[playerNumber][step]=NULL;
				step++;
			}
			for (int i=0; i<256; i++)
			{
				Order *order=ordersQueue[playerNumber][i];
				if (order)
				{
					assert(order->getOrderType()==ORDER_NULL);
					delete order;
					ordersQueue[playerNumber][i]=NULL;
				}
			}
			order=new QuitedOrder();
		}
		else
			order=new NullOrder();
		order->sender=playerNumber;
		order->inQueue=false;
		return order;
	}
	else if (waitingForPlayerMask)
	{
		Order *order=new WaitingForPlayerOrder(whoMaskCountedOut());
		order->sender=playerNumber;
		order->inQueue=false;
		return order;
	}
	else if (players[playerNumber]->type==Player::P_LOST_FINAL || players[playerNumber]->quitting)
	{
		Order *order=new NullOrder();
		order->sender=playerNumber;
		order->inQueue=false;
		return order;
	}
	else
	{
		Order *order=ordersQueue[playerNumber][executeStep];
		if (players[playerNumber]->type==Player::P_LOST_DROPPING && order==NULL)
		{
			order=new DeconnectedOrder();
			order->sender=playerNumber;
			order->inQueue=false;
			players[playerNumber]->type=Player::P_LOST_FINAL;
			fprintf(logFile, "players[%d]->type=Player::P_LOST_FINAL, dropped, executeStep=%d\n", playerNumber, executeStep);
			Uint8 step=freeingStep;
			while (step!=executeStep)
			{
				Order *order=ordersQueue[playerNumber][step];
				assert(order);
				delete order;
				ordersQueue[playerNumber][step]=NULL;
				step++;
			}
			for (int i=0; i<256; i++)
			{
				Order *order=ordersQueue[playerNumber][i];
				if (order)
				{
					assert(order->getOrderType()==ORDER_NULL);
					delete order;
					ordersQueue[playerNumber][i]=NULL;
				}
			}
		}
		else
		{
			if (order==NULL)
				fclose(logFile);
			assert(order);
			assert(order->inQueue);
		}
		assert(whoMaskAreWeWaitingFor()==0);
		// TODO: check check-sums here
		assert(order);
		if (order->getOrderType()==ORDER_PLAYER_QUIT_GAME)
		{
			PlayerQuitsGameOrder *pqgo=(PlayerQuitsGameOrder *)order;
			int ap=pqgo->player;
			fprintf(logFile, "players[%d]->quitting, executeStep=%d\n", ap, executeStep);
			players[playerNumber]->quitting=true;
			players[ap]->quitStep=executeStep;
		}
		
		fprintf(logFile, "wishedLatency[%d]=%d\n", playerNumber, wishedLatency[playerNumber]);
		wishedLatency[playerNumber]=order->wishedLatency;
		
		return order;
	}
}


void NetGame::orderHasBeenExecuted(Order *order)
{
	// "order" is used as a communication channel from "NetGame" to "Game" and "GameGUI".
	// The ordred in queue "playersNetQueue" are deleted when new orders comes,
	// or in the netGame destructor. The other orders which are not in the queue
	// need to be deleted after use (by "Game" or "GameGUI").
	assert(order);
	if (!order->inQueue)
	{
		fprintf(logFile, "deleting order type %d, executeStep=%d\n", order->getOrderType(), executeStep);
		delete order;
	};
}

void NetGame::updateDelays(int player, Uint8 receivedStep)
{
	// We compute the ping-pongs times:
	
	// We compute the delay from pushStep to receivedStep:
	Uint8 step=pushStep-1;
	Uint8 delay=0;
	while (true)
		if (step==receivedStep)
			break;
		else if (step==freeingStep)
			return;
		else
		{
			step--;
			delay++;
		}

	// We update the pingPongCount[] record:
	int count=pingPongCount[player];
	recentsPingPong[player][count]+=delay;
	count=(count+1)&63;
	recentsPingPong[player][count]=0;
	pingPongCount[player]=count;

	// We compute the pingPongMax[p]:
	int max=0;
	for (int i=0; i<64; i++)
	{
		int delay=recentsPingPong[player][i];
		if (delay>max)
			max=delay;
	}
	pingPongMax[player]=max;
	//printf("pingPongMax[%d]=%d\n", player, pingPongMax[player]);
	
	countDown[player]=0;
}

void NetGame::treatData(Uint8 *data, int size, IPaddress ip)
{
	if (data[0]!=MULTIPLE_ORDERS)
	{
		fprintf(logFile, "Other packet data[0]=%d packet received from %s, v1\n", data[0], Utilities::stringIP(ip));
		return;
	}
	if (size<4)
	{
		fprintf(logFile, "Warning, dangerous too small packet received from %s, v2\n", Utilities::stringIP(ip));
		return;
	}
	
	int player=data[1];
	Uint8 receivedStep=data[2];
	assert(player>=0);
	assert(player<32);
	lastReceivedFromMe[player]=receivedStep;
	Uint8 receivedWishedLatency=data[3];
	
	fprintf(logFile, "treatData, receivedWishedLatency=%d\n", receivedWishedLatency);
	fprintf(logFile, " lastReceivedFromMe[%d]=%d\n", player, receivedStep);
	
	int l=4;
	while (l<size)
	{
		if (size-l<3)
		{
			fprintf(logFile, " Warning, dangerous too small packet received from %s, v3\n", Utilities::stringIP(ip));
			return;
		}
		
		//printf("treat data=[%d, %d, %d, %d, %d]\n", data[0], data[1], data[2], data[3], data[4]);
		Uint8 orderStep=data[l++];
		int orderSize=data[l++];
		Uint8 orderType=data[l++];
		fprintf(logFile, " oneOrder player=%d, orderStep=%d, orderSize=%d, orderType=%d\n", player, orderStep, orderSize, orderType);
	
		if (!players[player]->sameip(ip))
			if (dropState!=DS_EXCHANGING_ORDERS)
				fprintf(logFile, "  Warning, late packet or bad packet from ip=%s, dropState=%d\n", Utilities::stringIP(ip), dropState);
		
		assert(players[player]->type!=Player::P_AI);
		assert(players[player]->type!=Player::P_LOCAL);

		if (players[player]->type==Player::P_LOST_FINAL)
		{
			// We have enough orders from player, but he may need something more from us:
			if (players[player]->quitting)
				sendWaitingForPlayerOrder(player);
		}
		else if (orderType==ORDER_WAITING_FOR_PLAYER)
		{
			assert(orderSize==4);
			Uint32 wfpm=getUint32(data, l);
			l+=4;
			fprintf(logFile, "  player %d has wfpm=%x\n", player, wfpm);
		}
		else if (orderType==ORDER_DROPPING_PLAYER)
		{
			assert(orderSize>=5);
			bool askForReply=data[l++];
			Uint32 dropMask=getUint32(data+l, 0);
			l+=4;
			droppingPlayersMask[player]=dropMask;
			lastExecutedStep[player]=orderStep-1;
			
			fprintf(logFile, "  player %d has askForReply=%d, dropMask=%x, lastExecutedStep=%d, orderSize=%d\n", player, askForReply, dropMask, orderStep-1, orderSize);
			
			if (askForReply && players[player]->type==Player::P_IP)
				sendDroppingPlayersMask(player, false);
			
			for (int p=0; p<numberOfPlayer; p++)
				if (players[p]->type==Player::P_IP && dropMask&(1<<p))
				{
					lastAviableStep[player][p]=data[l];
					fprintf(logFile, "  lastReceivedFromHim(%d)=%d.\n", player, p);
					l++;
				}
			if (dropState<DS_EXCHANGING_DROPPING_MASK)
			{
				dropState=DS_EXCHANGING_DROPPING_MASK;
				fprintf(logFile, "  dropState=DS_EXCHANGING_DROPPING_MASK, player %d request.\n", player);
				for (int p=0; p<numberOfPlayer; p++)
					droppingPlayersMask[p]=0;
			}
		}
		else if (orderType==ORDER_REQUESTING_AWAY)
		{
			Uint8 missingPlayer=data[l++];
			sendDeadAwayOrder(missingPlayer, player, orderStep);
		}
		else if (players[player]->type==Player::P_LOST_DROPPING)
		{
			if (dropState==DS_EXCHANGING_ORDERS)
			{
				Uint8 step=freeingStep;
				bool valid;
				while (true)
				{
					if (step==orderStep)
					{
						valid=true;
						break;
					}
					else if (step==theLastExecutedStep)
					{
						valid=false;
						break;
					}
					else
						step++;
				}
				if (valid)
				{
					Order *order=Order::getOrder(data+l-1, orderSize+1);
					l+=orderSize;
					assert(order);
					assert(order->getOrderType()==orderType);

					if (orderType==ORDER_SUBMIT_CHECK_SUM)
					{
						SubmitCheckSumOrder *cso=(SubmitCheckSumOrder *)order;
						checkSumsRemote[orderStep]=cso->checkSumValue;
					}

					if (ordersQueue[player][orderStep])
					{
						fprintf(logFile, "  duplicated packet. player=%d, orderStep=%d, v-f\n", player, orderStep);
						Order *order=ordersQueue[player][orderStep];
						assert(order->getOrderType()==orderType);
						delete order;
						ordersQueue[player][orderStep]=NULL;
					}
					order->sender=player;
					order->inQueue=true;
					order->wishedLatency=receivedWishedLatency;
					ordersQueue[player][orderStep]=order;
					
					lastAviableStep[localPlayerNumber][player]=lastReceivedFromHim(player);
				}
				else
					fprintf(logFile, "  packet from a P_LOST_DROPPING player=%d, outside range, freeingStep=%d, orderStep=%d, theLastExecutedStep=%d\n", player, freeingStep, orderStep, theLastExecutedStep);
			}
			else
				fprintf(logFile, "  packet from a P_LOST_DROPPING player=%d, while dropState=%d, orderStep=%d\n", player, dropState, orderStep);
		}
		else
		{
			Order *order=Order::getOrder(data+l-1, orderSize+1);
			l+=orderSize;
			assert(order);
			assert(order->getOrderType()==orderType);
			
			if (orderType==ORDER_SUBMIT_CHECK_SUM)
			{
				SubmitCheckSumOrder *cso=(SubmitCheckSumOrder *)order;
				checkSumsRemote[orderStep]=cso->checkSumValue;
			}
			
			if (ordersQueue[player][orderStep])
			{
				fprintf(logFile, "  duplicated packet. player=%d, orderStep=%d, v-n\n", player, orderStep);
				Order *order=ordersQueue[player][orderStep];
				if (order->getOrderType()!=orderType)
					fclose(logFile);
				assert(order->getOrderType()==orderType);
				delete order;
				ordersQueue[player][orderStep]=NULL;
			}
			order->sender=player;
			order->inQueue=true;
			order->wishedLatency=receivedWishedLatency;
			ordersQueue[player][orderStep]=order;
		}
		
		assert(l<=size);
	}
	
	updateDelays(player, receivedStep);
}

void NetGame::receptionStep(void)
{ 
	if (socket)
	{
		UDPpacket *packet=NULL;
		packet=SDLNet_AllocPacket(MAX_GAME_PACKET_SIZE);
		assert(packet);
		while (SDLNet_UDP_Recv(socket, packet)==1)
			treatData(packet->data, packet->len, packet->address);
		SDLNet_FreePacket(packet);
	}
}

bool NetGame::stepReadyToExecute(void)
{
	if (dropState<DS_EXCHANGING_ORDERS)
	{
		bool someoneToDrop=false;
		for (int p=0; p<numberOfPlayer; p++)
			if (players[p]->type==Player::P_IP && countDown[p]++>COUNT_DOWN_DEATH)
				someoneToDrop=true;
		if (someoneToDrop)
		{
			if (dropState<DS_EXCHANGING_DROPPING_MASK)
			{
				dropState=DS_EXCHANGING_DROPPING_MASK;
				fprintf(logFile, "dropState=DS_EXCHANGING_DROPPING_MASK, I choosed.\n");
				for (int p=0; p<numberOfPlayer; p++)
					droppingPlayersMask[p]=0;
			}
			Uint32 myDroppingPlayersMask=0;
			for (int p=numberOfPlayer; p<32; p++)
				myDroppingPlayersMask|=1<<p;
			for (int p=0; p<numberOfPlayer; p++)
				if (countDown[p]>COUNT_DOWN_DEATH)
					myDroppingPlayersMask|=1<<p;
			assert(myDroppingPlayersMask);
			droppingPlayersMask[localPlayerNumber]|=myDroppingPlayersMask;
		}
	}
	for (int p=0; p<numberOfPlayer; p++)
		if (players[p]->type==Player::P_IP && countDown[p]>=COUNT_DOWN_DEATH)
			fprintf(logFile, "countDown[%d]=%d\n", p, countDown[p]);
	
	receptionStep();
	
	if (dropState)
	{
		if (dropState==DS_EXCHANGING_DROPPING_MASK)
		{
			Uint32 myDroppingPlayersMask=droppingPlayersMask[localPlayerNumber];
			for (int p=0; p<numberOfPlayer; p++)
				if ((myDroppingPlayersMask&(1<<p))==0 && players[p]->type==Player::P_IP)
					myDroppingPlayersMask|=droppingPlayersMask[p];
			droppingPlayersMask[localPlayerNumber]|=myDroppingPlayersMask;
			bool iAgreeWithEveryone=true;
			for (int p=0; p<numberOfPlayer; p++)
				if ((myDroppingPlayersMask&(1<<p))==0 && players[p]->type==Player::P_IP)
				{
					if (myDroppingPlayersMask!=droppingPlayersMask[p])
					{
						iAgreeWithEveryone=false;
						sendDroppingPlayersMask(p, true);
					}
					else
						sendDroppingPlayersMask(p, false); // Only to tell player p that we are alive.
				}
			if (iAgreeWithEveryone)
			{
				fprintf(logFile, "iAgreeWithEveryone, myDroppingPlayersMask=%x.\n", myDroppingPlayersMask);
				for (int p=0; p<numberOfPlayer; p++)
					if (players[p]->type==Player::P_IP && (myDroppingPlayersMask&(1<<p)))
						players[p]->type=Player::P_LOST_DROPPING;
				dropState=DS_EXCHANGING_ORDERS;
			}
		}
		if (dropState==DS_EXCHANGING_ORDERS)
		{
			lastExecutedStep[localPlayerNumber]=executeStep-1;
			for (int p=0; p<numberOfPlayer; p++)
				lastAviableStep[localPlayerNumber][p]=lastReceivedFromHim(p);
			
			//Uint32 myDroppingPlayersMask=droppingPlayersMask[localPlayerNumber];
			int n=0;
			for (int p=0; p<numberOfPlayer; p++)
				if (players[p]->type==Player::P_IP || players[p]->type==Player::P_LOCAL)
					n++;
			fprintf(logFile, " n=%d\n", n);
			assert(n);
			
			Uint8 les=0;
			Uint8 upToDatePlayer=0;
			Uint8 step=freeingStep;
			int c=0;
			while (c!=n)
			{
				for (int p=0; p<numberOfPlayer; p++)
					if (players[p]->type==Player::P_IP || players[p]->type==Player::P_LOCAL)
						if (lastExecutedStep[p]==step)
						{
							les=step;
							upToDatePlayer=p;
							fprintf(logFile, " les[%d]=%d.\n", p, les);
							c++;
						}
				step++;
			}
			theLastExecutedStep=les;
			fprintf(logFile, " theLastExecutedStep=%d.\n", theLastExecutedStep);
			
			
			// Has everyone all orders ?
			bool hasAllOrder[32][32];
			for (int player=0; player<numberOfPlayer; player++)
				if (players[player]->type==Player::P_IP || players[player]->type==Player::P_LOCAL)
					for (int p=0; p<numberOfPlayer; p++)
						if (players[p]->type==Player::P_LOST_DROPPING)
							hasAllOrder[player][p]=true;

			for (int player=0; player<numberOfPlayer; player++)
				if (players[player]->type==Player::P_IP || players[player]->type==Player::P_LOCAL)
					for (int p=0; p<numberOfPlayer; p++)
						if (players[p]->type==Player::P_LOST_DROPPING)
						{
							Uint8 step=freeingStep;
							while (true)
							{
								if (step==les)
									break;
								else if (step==lastAviableStep[player][p])
									hasAllOrder[player][p]=false;
								step++;
							}
						}
			
			for (int player=0; player<numberOfPlayer; player++)
				if (players[player]->type==Player::P_IP || players[player]->type==Player::P_LOCAL)
					for (int p=0; p<numberOfPlayer; p++)
						if (players[p]->type==Player::P_LOST_DROPPING)
						{
							fprintf(logFile, " lastAviableStep[%d][%d]=%d\n", player, p, lastAviableStep[player][p]);
							fprintf(logFile, " hasAllOrder[%d][%d]=%d\n", player, p, hasAllOrder[player][p]);
						}
			
			// TODO: add a pipeline to avoid to request too many times for the same order !
			bool iHaveAll=true;
			for (int p=0; p<numberOfPlayer; p++)
				if (players[p]->type==Player::P_LOST_DROPPING)
					if (!hasAllOrder[localPlayerNumber][p])
					{
						Uint8 resendingStep=lastAviableStep[localPlayerNumber][p]+1;
						fprintf(logFile, " for player %d, we request player %d for step=%d.\n", p, upToDatePlayer, resendingStep);
						sendRequestingDeadAwayOrder(p, upToDatePlayer, resendingStep);
						iHaveAll=false;
					}
			
			if (iHaveAll)
			{
				fprintf(logFile, " we have all needed orders.\n");
				for (int p=0; p<numberOfPlayer; p++)
					if (players[p]->type==Player::P_LOST_DROPPING)
					{
						Uint8 step=les+1;
						while (step!=freeingStep)
						{
							Order *order=ordersQueue[p][step];
							if (order)
							{
								delete order;
								ordersQueue[p][step]=NULL;
							}
							step++;
						}
					}
				dropState=DS_NO_DROP_PROCESSING;
				for (int p=0; p<numberOfPlayer; p++)
					droppingPlayersMask[p]=0;
			}
		}
		hadToWaitThisStep=true;
		return false;
	}
	else
	{
		waitingForPlayerMask=whoMaskAreWeWaitingFor();
		fprintf(logFile, "waitingForPlayerMask=%x\n", waitingForPlayerMask);
		if (waitingForPlayerMask==0)
		{
			hadToWaitThisStep=false;
			return true;
		}
		
		// We have to tell the others that we are still here, but we are waiting for someone:
		for (int p=0; p<numberOfPlayer; p++)
			if (players[p]->type==Player::P_IP)
				sendWaitingForPlayerOrder(p);
		hadToWaitThisStep=true;
		return false;
	}
}

void NetGame::stepExecuted(void)
{
	if (hadToWaitThisStep)
	{
		for (int p=0; p<numberOfPlayer; p++)
			recentsPingPong[p][pingPongCount[p]]++;
	}
	else
	{
		Uint8 targetLatency=0;// The gobaly-choosen-latency, we have to reach.
		int n=0;
		for (int p=0; p<numberOfPlayer; p++)
			if (players[p]->type==Player::P_IP || players[p]->type==Player::P_LOCAL)
			{
				Uint8 latency=wishedLatency[p];
				if (latency)
				{
					n++;
					if (latency>targetLatency)
						targetLatency=latency;
				}
			}
		Uint8 latency=pushStep-executeStep;
		
		if (n==0)
			targetLatency=latency;
		else if (latency!=targetLatency)
			fprintf(logFile, "executeStep=%d, wishedLatency=(%d, %d), latency=%d, targetLatency=%d.\n", executeStep, wishedLatency[0], wishedLatency[1], latency, targetLatency);
		
		int timesToEatSteps=1;
		
		if (targetLatency>latency)
		{
			bool allNullOrders=true;
			for (int p=0; p<numberOfPlayer; p++)
				if (players[p]->type==Player::P_IP || players[p]->type==Player::P_LOCAL)
				{
					Order *order=ordersQueue[p][executeStep];
					if (order==NULL)
					{
						allNullOrders=false;
						break;
					}
					Uint8 type=order->getOrderType();
					if (type!=ORDER_SUBMIT_CHECK_SUM && type!=ORDER_NULL)
					{
						allNullOrders=false;
						break;
					}
				}
			if (allNullOrders)
			{
				timesToEatSteps=0; // We simplay execute all thoses useless orders twice!
				
				for (int p=0; p<numberOfPlayer; p++)
					if (players[p]->type==Player::P_IP || players[p]->type==Player::P_LOCAL)
					{
						Order *order=ordersQueue[p][executeStep];
						if (order)
							delete order;
						ordersQueue[p][executeStep]=NULL;
						
						NullOrder *nullOrder=new NullOrder();
						nullOrder->sender=p;
						nullOrder->inQueue=true;
						nullOrder->wishedLatency=0;
						nullOrder->latencyPadding=true;
						ordersQueue[p][executeStep]=nullOrder;
					}
			}
		}
		else if (targetLatency<latency)
		{
			bool allNullOrders=true;
			for (int p=0; p<numberOfPlayer; p++)
				if (players[p]->type==Player::P_IP || players[p]->type==Player::P_LOCAL)
				{
					Order *order=ordersQueue[p][executeStep+1];
					if (order==NULL)
					{
						allNullOrders=false;
						break;
					}
					Uint8 type=order->getOrderType();
					if (type!=ORDER_SUBMIT_CHECK_SUM && type!=ORDER_NULL)
					{
						allNullOrders=false;
						break;
					}
				}
			if (allNullOrders)
				timesToEatSteps=2; // We simplay skip all thoses useless orders !
		}
		
		for (int i=0; i<timesToEatSteps; i++)
		{
			// OK, we have executed the "executeStep"s orders, next we will get the next:
			executeStep++;

			for (int p=0; p<numberOfPlayer; p++)
			{
				Order *order=ordersQueue[p][freeingStep];
				if (players[p]->type==Player::P_LOST_FINAL)
				{
					// By definition, a LOST_FINAL player have only NULLs on orders queue.
					if (order)
						fclose(logFile);
					assert(order==NULL);
				}
				else if (players[p]->type==Player::P_LOST_DROPPING)
				{
					if (order)
					{
						delete order;
						ordersQueue[p][freeingStep]=NULL;
					}
					else
					{
						players[p]->type=Player::P_LOST_FINAL;
						fprintf(logFile, "players[%d]->type=Player::P_LOST_FINAL, deconnect, freeingStep=%d\n", p, freeingStep);
						for (int i=0; i<256; i++)
						{
							Order *order=ordersQueue[p][i];
							if (order)
							{
								assert(order->getOrderType()==ORDER_NULL);
								delete order;
								ordersQueue[p][i]=NULL;
							}
						}
					}
				}
				else
				{
					// All others queues have to have orders.
					assert(order);
					if (order->getOrderType()==ORDER_PLAYER_QUIT_GAME)
					{
						assert(players[p]->quitting);
						players[p]->type=Player::P_LOST_FINAL; // We freed the last order of a LOST_QUIT player, then he becomes a LOST_FINAL.
						// We may have received extra-packets, which we have to clean:
						for (int step=0; step<256; step++)
						{
							Order *order=ordersQueue[p][step];
							if (order)
							{
								delete order;
								ordersQueue[p][step]=NULL;
							}
						}
						fprintf(logFile, "players[%d]->type=Player::P_LOST_FINAL, he, freeingStep=%d\n", p, freeingStep);
						for (int i=0; i<256; i++)
						{
							Order *order=ordersQueue[p][i];
							if (order)
							{
								assert(order->getOrderType()==ORDER_NULL);
								delete order;
								ordersQueue[p][i]=NULL;
							}
						}
					}
					else
					{
						delete order;
						ordersQueue[p][freeingStep]=NULL;
					}
				}
			}
			// OK, we just freed the "freeingStep"s orders, we have to point valid orders now:
			freeingStep++;
		}
		
		// We currently don't change latency, then we simply also increment "pushStep".
		pushStep++;
		
		fprintf(logFile, "\n");
		fprintf(logFile, "freeingStep=%d.\n", freeingStep);
		fprintf(logFile, "executeStep=%d.\n", executeStep);
		fprintf(logFile, "pushStep=%d.\n", pushStep);
		
		// let's checkout for anything wrong...
		
		Uint8 step=pushStep+maxLatency+2;
		while(step!=freeingStep)
		{
			for (int p=0; p<numberOfPlayer; p++)
			{
				Order *order=ordersQueue[p][step];
				if (order)
					fclose(logFile);
				assert(order==NULL);
			}
			step++;
		}
		for (int p=0; p<numberOfPlayer; p++)
			if (players[p]->type==Player::P_LOST_FINAL)
				for (int i=0; i<256; i++)
				{
					Order *order=ordersQueue[p][i];
					if (order)
						fclose(logFile);
					assert(order==NULL);
				}
	}
}

int NetGame::ticksToDelay(void)
{
	//for (int p=0; p<numberOfPlayer; p++)
	//	if (players[p]->type==Player::P_IP)
	//		printf("pingPongMax[%d]=%d, ", p, pingPongMax[p]);
	
	// First, the Order Margin Times:
	for (int p=0; p<numberOfPlayer; p++)
		if (players[p]->type==Player::P_IP)
		{
			int delay=0;
			Uint8 step=executeStep;
			while (ordersQueue[p][step])
			{
				step++;
				delay++;
				assert(delay<255);
			}

			int count=orderMarginTimeCount[p];
			recentOrderMarginTime[p][count]=delay;
			orderMarginTimeCount[p]=(count+1)&63;
			//printf("delay=%d, ", delay);
			
			int min=255;
			int max=0;
			for (int i=0; i<64; i++)
			{
				int delay=recentOrderMarginTime[p][i];
				if (delay<min)
					min=delay;
				if (delay>max)
					max=delay;
			}
			orderMarginTimeMin[p]=min;
			orderMarginTimeMax[p]=max;
			//printf("orderMarginTime[%d]=(%d, %d), ", p, orderMarginTimeMin[p], orderMarginTimeMax[p]);
		}
	
	
	Uint8 latency=pushStep-executeStep;
	//printf("latency=%d, ", latency);
	
	int minTicksToWait=255;
	int n=0;
	for (int p=0; p<numberOfPlayer; p++)
		if (players[p]->type==Player::P_IP)
		{
			n++;
			int goodOrderMarginTime=latency-((pingPongMax[p]+1)>>1);
			int realOrderMarginTime=orderMarginTimeMin[p]-1;
			int ticksToWait=goodOrderMarginTime-realOrderMarginTime;
			if (ticksToWait<minTicksToWait)
				minTicksToWait=ticksToWait;
		}
	if (n==0)
	{
		myLocalWishedLatency=1;
		return 0;
	}
	//printf("minTicksToWait=%d, ", minTicksToWait);
	
	int maxPingPong=0;
	for (int p=0; p<numberOfPlayer; p++)
		if (players[p]->type==Player::P_IP)
		{
			int pingPong=pingPongMax[p];
			if (pingPong>maxPingPong)
				maxPingPong=pingPong;
		}
	//printf("maxPingPong=%d, ", maxPingPong);
	
	int goodLatency=(ordersByPackets-1)+((maxPingPong+1)>>1);
	if (goodLatency<1)
		goodLatency=1;
	if (minTicksToWait>0)
		goodLatency+=((minTicksToWait+1)>>1);
	//printf("goodLatency=%d, ", goodLatency);
	
	if (goodLatency<latency)
		myLocalWishedLatency=latency-1;
	else if (goodLatency>latency)
		myLocalWishedLatency=latency+1;
	else
		myLocalWishedLatency=latency;
	
	//WARNING: debugg only!: myLocalWishedLatency=1+(rand()&15);
	
	//printf("myLocalWishedLatency=%d.\n", myLocalWishedLatency);
	
	if (minTicksToWait<=0)
		return 0;
	int delay=10*minTicksToWait;
	if (delay>40)
		delay=40;
	return delay;
}


