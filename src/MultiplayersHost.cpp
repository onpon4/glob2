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

#include "MultiplayersHost.h"
#include "GlobalContainer.h"
#include "GAG.h"
#include "NetDefine.h"
#include "YOG.h"
#include "Marshaling.h"
#include "Utilities.h"
#include "LogFileManager.h"

#ifndef INADDR_BROADCAST
#define INADDR_BROADCAST (SDL_SwapBE32(0x7F000001))
#endif

MultiplayersHost::MultiplayersHost(SessionInfo *sessionInfo, bool shareOnYOG, SessionInfo *savedSessionInfo)
:MultiplayersCrossConnectable()
{
	this->sessionInfo=*sessionInfo;
	validSessionInfo=true;
	if (savedSessionInfo)
		this->savedSessionInfo=new SessionInfo(*savedSessionInfo);
	else
		this->savedSessionInfo=NULL;

	logFile=globalContainer->logFileManager->getFile("MultiplayersHost.log");
	logFileDownload=globalContainer->logFileManager->getFile("MultiplayersHostDownload.log");
	assert(logFile);
	// net things:
	initHostGlobalState();

	socket=NULL;
	serverIP.host=0;
	serverIP.port=0;
	fprintf(logFile, "Openning a socket...\n");
	socket=SDLNet_UDP_Open(GAME_SERVER_PORT);
	if (socket)
		fprintf(logFile, "Socket opened at port (%d).\n", GAME_SERVER_PORT);
	else
		socket=SDLNet_UDP_Open(ANY_PORT);
	if (socket)
	{
		IPaddress *localAddress=SDLNet_UDP_GetPeerAddress(socket, -1);
		serverIP.port=localAddress->port;
		fprintf(logFile, "Socket opened at unknow port (%d)\n", SDL_SwapBE16(serverIP.port));
	}
	else
		fprintf(logFile, "failed to open a socket.\n");
		
	

	firstDraw=true;

	this->shareOnYOG=shareOnYOG;
	if (shareOnYOG)
	{
		fprintf(logFile, "sharing on YOG\n");
		yog->shareGame(sessionInfo->getMapName());
		yog->setHostGameSocket(socket);
	}
	
	stream=NULL;
	
	if (sessionInfo->mapGenerationDescriptor && sessionInfo->fileIsAMap)
	{
		fprintf(logFile, "MultiplayersHost() random map.\n");
	}
	else
	{
		const char *s=sessionInfo->getFileName();
		assert(s);
		assert(s[0]);
		fprintf(logFile, "MultiplayersHost() fileName=%s.\n", s);
		stream=globalContainer->fileManager->open(s,"rb");
	}
	
	fileSize=0;
	if (stream)
	{
		SDL_RWseek(stream, 0, SEEK_END);
		fileSize=SDL_RWtell(stream);
	}
	
	for (int p=0; p<32; p++)
	{
		playerFileTra[p].wantsFile=false;
		playerFileTra[p].receivedFile=false;
		playerFileTra[p].unreceivedIndex=0;
		playerFileTra[p].brandwidth=0;
		playerFileTra[p].lastNbPacketsLost=0;
		for (int i=0; i<PACKET_SLOTS; i++)
		{
			playerFileTra[p].packetSlot[i].index=0;
			playerFileTra[p].packetSlot[i].sent=false;
			playerFileTra[p].packetSlot[i].received=false;
			playerFileTra[p].packetSlot[i].brandwidth=0;
			playerFileTra[p].packetSlot[i].time=0;
		}
		playerFileTra[p].time=0;
		playerFileTra[p].latency=32;
		playerFileTra[p].totalSent=0;
		playerFileTra[p].totalLost=0;
		playerFileTra[p].totalReceived=0;
	}
	
	if (!shareOnYOG)
	{
		sendBroadcastLanGameHosting(GAME_JOINER_PORT_1, true);
		SDL_Delay(10);
		sendBroadcastLanGameHosting(GAME_JOINER_PORT_2, true);
		SDL_Delay(10);
		sendBroadcastLanGameHosting(GAME_JOINER_PORT_3, true);
	}
}

MultiplayersHost::~MultiplayersHost()
{
	
	if (shareOnYOG)
	{
		yog->unshareGame();
	}
	else
	{
		sendBroadcastLanGameHosting(GAME_JOINER_PORT_1, false);
		SDL_Delay(10);
		sendBroadcastLanGameHosting(GAME_JOINER_PORT_2, false);
		SDL_Delay(10);
		sendBroadcastLanGameHosting(GAME_JOINER_PORT_3, false);
	}
	
	if (destroyNet)
	{
		assert(channel==-1);
		if (channel!=-1)
		{
			send(CLIENT_QUIT_NEW_GAME);
			SDLNet_UDP_Unbind(socket, channel);
			fprintf(logFile, "Socket unbinded.\n");
		}
		if (socket)
		{
			// We need to have the same Port openened to comunicate with all players to pass firewalls.
			// Then, "socket" is the same in all players and in "MultiplayersHost".
			// Therefore, we need to unbind players BEFORE deleting "socket".
			
			for (int p=0; p<sessionInfo.numberOfPlayer; p++)
				if (sessionInfo.players[p].socket==socket)
					sessionInfo.players[p].unbind();
			SDLNet_UDP_Close(socket);
			socket=NULL;
			fprintf(logFile, "Socket closed.\n");
		}
	}
	
	if (savedSessionInfo)
		delete savedSessionInfo;
		
	if (stream)
		SDL_RWclose(stream);
	
	if (logFileDownload && logFileDownload!=stdout)
		for (int p=0; p<32; p++)
			if (playerFileTra[p].totalSent)
			{
				fprintf(logFileDownload, "player %d \n", p);
				fprintf(logFileDownload, "playerFileTra[p].totalSent=%d.\n", playerFileTra[p].totalSent);
				fprintf(logFileDownload, "playerFileTra[p].totalLost=%d. (%f)\n", playerFileTra[p].totalLost, (float)playerFileTra[p].totalLost/(float)playerFileTra[p].totalSent);
				fprintf(logFileDownload, "playerFileTra[p].totalReceived=%d. (%f)\n", playerFileTra[p].totalReceived, (float)playerFileTra[p].totalReceived/(float)playerFileTra[p].totalSent);
			}
}

int MultiplayersHost::newTeamIndice()
{
	// We put the new player in a team with the less number of player
	// and the shortest indice:
	int t=0;
	int lessPlayer=33;
	for (int ti=0; ti<sessionInfo.numberOfTeam; ti++)
	{
		int numberOfPlayer=0;
		Uint32 m=1;
		Uint32 pm=sessionInfo.team[ti].playersMask;
		for (int i=0; i<32; i++)
		{
			if (m&pm)
				numberOfPlayer++;
			m=m<<1;
		}
		if (numberOfPlayer<lessPlayer)
		{
			lessPlayer=numberOfPlayer;
			t=ti;
		}
	}
	assert(t>=0);
	assert(t<32);
	assert(t<sessionInfo.numberOfTeam);
	
	return t;
}

void MultiplayersHost::initHostGlobalState(void)
{
	for (int i=0; i<32; i++)
		crossPacketRecieved[i]=0;
	
	hostGlobalState=HGS_SHARING_SESSION_INFO;
}

void MultiplayersHost::reinitPlayersState()
{
	for (int j=0; j<sessionInfo.numberOfPlayer; j++)
		if (sessionInfo.players[j].netState>BasePlayer::PNS_PLAYER_SEND_PRESENCE_REQUEST)
		{
			sessionInfo.players[j].netState=BasePlayer::PNS_PLAYER_SEND_SESSION_REQUEST;
			sessionInfo.players[j].netTimeout=2*j; // we just split the sendings by 1/10 seconds.
			sessionInfo.players[j].netTimeoutSize=LONG_NETWORK_TIMEOUT;
			sessionInfo.players[j].netTOTL++;
		}
}

void MultiplayersHost::stepHostGlobalState(void)
{
	switch (hostGlobalState)
	{
	case HGS_BAD :
		fprintf(logFile, "This is a bad hostGlobalState case. Should not happend!\n");
	break;
	case HGS_SHARING_SESSION_INFO :
	{
		bool allOK=true;
		for (int i=0; i<sessionInfo.numberOfPlayer; i++)
			if (sessionInfo.players[i].type==BasePlayer::P_IP)
				if (sessionInfo.players[i].netState<BasePlayer::PNS_OK)
					allOK=false;
		

		if (allOK)
		{
			fprintf(logFile, "OK, now we are waiting for cross connections\n");
			hostGlobalState=HGS_WAITING_CROSS_CONNECTIONS;
			for (int i=0; i<sessionInfo.numberOfPlayer; i++)
				if (sessionInfo.players[i].type==BasePlayer::P_IP)
				{
					sessionInfo.players[i].netState=BasePlayer::PNS_SERVER_SEND_CROSS_CONNECTION_START;
					sessionInfo.players[i].netTimeout=i;
					sessionInfo.players[i].netTOTL++;
				}
		}

	}
	break;
	case HGS_WAITING_CROSS_CONNECTIONS :
	{
		bool allPlayersCrossConnected=true;
		
		for (int j=0; j<sessionInfo.numberOfPlayer; j++)
			if (sessionInfo.players[j].type==BasePlayer::P_IP)
				if (crossPacketRecieved[j]<3)
				{
					fprintf(logFile, "player %d is not cross connected.\n", j);
					allPlayersCrossConnected=false;
					break;
				}
		
		if (allPlayersCrossConnected && (hostGlobalState>=HGS_WAITING_CROSS_CONNECTIONS))
		{
			fprintf(logFile, "Great, all players are cross connected, Game could start, except the file!.\n");
			hostGlobalState=HGS_ALL_PLAYERS_CROSS_CONNECTED;
			stepHostGlobalState();
		}
	}
	break;

	case HGS_ALL_PLAYERS_CROSS_CONNECTED :
	{
		bool allPlayersHaveFile=true;
		
		for (int j=0; j<sessionInfo.numberOfPlayer; j++)
			if (sessionInfo.players[j].type==BasePlayer::P_IP)
				if (playerFileTra[j].wantsFile && !playerFileTra[j].receivedFile)
				{
					fprintf(logFile, "player %d is still downloading game file.\n", j);
					allPlayersHaveFile=false;
					break;
				}
		
		if (allPlayersHaveFile && (hostGlobalState>=HGS_ALL_PLAYERS_CROSS_CONNECTED))
		{
			fprintf(logFile, "Great, all players have the game file too, Game could start!.\n");
			hostGlobalState=HGS_ALL_PLAYERS_CROSS_CONNECTED_AND_HAVE_FILE;
		}
	}
	break;
	
	case HGS_ALL_PLAYERS_CROSS_CONNECTED_AND_HAVE_FILE :
		
	break;

	case HGS_GAME_START_SENDED:
	{
		bool allPlayersPlaying=true;
		
		for (int j=0; j<sessionInfo.numberOfPlayer; j++)
			if (sessionInfo.players[j].type==BasePlayer::P_IP)
			{
				if (crossPacketRecieved[j]<4)
				{
					allPlayersPlaying=false;
					break;
				}
			}
		
		if (allPlayersPlaying && (hostGlobalState>=HGS_ALL_PLAYERS_CROSS_CONNECTED_AND_HAVE_FILE))
		{
			fprintf(logFile, "Great, all players have recieved start info.\n");
			hostGlobalState=HGS_PLAYING_COUNTER;
		}
	}
	break;

	case HGS_PLAYING_COUNTER :
	{

	}
	break;

	default:
	{
		fprintf(logFile, "This is a bad and unknow(%d) hostGlobalState case. Should not happend!\n",hostGlobalState);
	}
	break;

	}

}

void MultiplayersHost::kickPlayer(int p)
{
	if (sessionInfo.players[p].type==BasePlayer::P_IP)
		sessionInfo.players[p].send(SERVER_KICKED_YOU);
	removePlayer(p);
}

void MultiplayersHost::removePlayer(int p)
{
	bool wasKnownByOthers=(sessionInfo.players[p].netState>BasePlayer::PNS_PLAYER_SEND_PRESENCE_REQUEST);
	int t=sessionInfo.players[p].teamNumber;
	fprintf(logFile, "player %d quited the game, from team %d.\n", p, t);
	sessionInfo.team[t].playersMask&=~sessionInfo.players[p].numberMask;
	sessionInfo.team[t].numberOfPlayer--;

	sessionInfo.players[p].netState=BasePlayer::PNS_BAD;
	sessionInfo.players[p].type=BasePlayer::P_NONE;
	sessionInfo.players[p].netTimeout=0;
	sessionInfo.players[p].netTimeoutSize=DEFAULT_NETWORK_TIMEOUT;//Relase version
	sessionInfo.players[p].netTimeoutSize=0;// TODO : Only for debug version
	sessionInfo.players[p].netTOTL=0;

	sessionInfo.players[p].unbind();
	int mp=sessionInfo.numberOfPlayer-1;
	if (mp>p)
	{
		fprintf(logFile, "replace it by another player: %d\n", mp);
		int mt=sessionInfo.players[mp].teamNumber;
		sessionInfo.team[mt].playersMask&=~sessionInfo.players[mp].numberMask;
		sessionInfo.team[mt].numberOfPlayer--;

		sessionInfo.players[p]=sessionInfo.players[mp];

		sessionInfo.players[p].type=sessionInfo.players[mp].type;
		sessionInfo.players[p].netState=sessionInfo.players[mp].netState;
		sessionInfo.players[p].netTimeout=sessionInfo.players[mp].netTimeout;
		sessionInfo.players[p].netTimeoutSize=sessionInfo.players[mp].netTimeoutSize;
		sessionInfo.players[p].netTOTL=sessionInfo.players[mp].netTOTL;
		sessionInfo.players[p].numberMask=sessionInfo.players[mp].numberMask;

		//int t=(p%sessionInfo.numberOfTeam);
		int t=sessionInfo.players[mp].teamNumber;
		sessionInfo.players[p].setNumber(p);
		sessionInfo.players[p].setTeamNumber(t);
		
		// We erase replaced player:
		sessionInfo.players[mp].init();

		sessionInfo.team[t].playersMask|=sessionInfo.players[p].numberMask;
		sessionInfo.team[t].numberOfPlayer++;
	}
	sessionInfo.numberOfPlayer--;
	fprintf(logFile, "nop %d.\n", sessionInfo.numberOfPlayer);
	
	if (wasKnownByOthers)
	{
		// all other players are ignorant of the new situation:
		initHostGlobalState();
		reinitPlayersState();
	}
}

void MultiplayersHost::switchPlayerTeam(int p)
{
	Sint32 teamNumber=(sessionInfo.players[p].teamNumber+1)%sessionInfo.numberOfTeam;
	sessionInfo.players[p].setTeamNumber(teamNumber);
	
	reinitPlayersState();
}

void MultiplayersHost::removePlayer(char *data, int size, IPaddress ip)
{
	int i;
	for (i=0; i<sessionInfo.numberOfPlayer; i++)
		if (sessionInfo.players[i].sameip(ip))
			break;
	if (i>=sessionInfo.numberOfPlayer)
	{
		fprintf(logFile, "An unknow player (%x, %d) has sended a quit game !!!\n", ip.host, ip.port);
		return;
	}
	removePlayer(i);
}

void MultiplayersHost::yogClientRequestsGameInfo(char *rdata, int rsize, IPaddress ip)
{
	if (rsize!=8)
	{
		fprintf(logFile, "bad size for a yogClientRequestsGameInfo from ip %s size=%d\n", Utilities::stringIP(ip), rsize);
		return;
	}
	else
		fprintf(logFile, "yogClientRequestsGameInfo from ip %s size=%d\n", Utilities::stringIP(ip), rsize);
	
	char sdata[128+12];
	sdata[0]=YMT_GAME_INFO_FROM_HOST;
	sdata[1]=0;
	sdata[2]=0;
	sdata[3]=0;
	memcpy(sdata+4, rdata+4, 4); // we copy game's uid
	
	addSint8(sdata, (Sint8)sessionInfo.numberOfPlayer, 8);
	addSint8(sdata, (Sint8)sessionInfo.numberOfTeam, 9);
	addSint8(sdata, (Sint8)sessionInfo.fileIsAMap, 10);
	if (sessionInfo.mapGenerationDescriptor)
		addSint8(sdata, (Sint8)sessionInfo.mapGenerationDescriptor->methode, 11);
	else
		addSint8(sdata, (Sint8)MapGenerationDescriptor::eNONE, 11);
	strncpy(sdata+12, sessionInfo.getMapName(), 64);
	int ssize=Utilities::strmlen(sdata+12, 64)+12;
	assert(ssize<64+12);
	UDPpacket *packet=SDLNet_AllocPacket(ssize);
	if (packet==NULL)
		return;
	if (ip.host==0)
		return;
	packet->len=ssize;

	memcpy((char *)packet->data, sdata, ssize);

	bool sucess;

	packet->address=ip;
	packet->channel=-1;
	sucess=SDLNet_UDP_Send(socket, -1, packet)==1;
	if (!sucess)
		fprintf(logFile, "failed to send yogGameInfo packet!\n");

	SDLNet_FreePacket(packet);
}

void MultiplayersHost::newPlayerPresence(char *data, int size, IPaddress ip)
{
	fprintf(logFile, "MultiplayersHost::newPlayerPresence().\n");
	if (size!=40)
	{
		fprintf(logFile, "Bad size(%d) for an Presence request from ip %s.\n", size, Utilities::stringIP(ip));
		return;
	}
	int p=sessionInfo.numberOfPlayer;
	int t=newTeamIndice();
	assert(BasePlayer::MAX_NAME_LENGTH==32);
	if (savedSessionInfo)
	{
		char playerName[BasePlayer::MAX_NAME_LENGTH];
		memcpy(playerName, (char *)(data+8), 32);
		t=savedSessionInfo->getTeamNumber(playerName, t);
	}
	
	assert(p<32);
	playerFileTra[p].wantsFile=false;
	playerFileTra[p].receivedFile=false;
	playerFileTra[p].unreceivedIndex=0;
	playerFileTra[p].brandwidth=0;
	playerFileTra[p].lastNbPacketsLost=0;
	for (int i=0; i<PACKET_SLOTS; i++)
	{
		playerFileTra[p].packetSlot[i].index=0;
		playerFileTra[p].packetSlot[i].sent=false;
		playerFileTra[p].packetSlot[i].received=false;
		playerFileTra[p].packetSlot[i].brandwidth=0;
		playerFileTra[p].packetSlot[i].time=0;
	}
	playerFileTra[p].latency=32;
	playerFileTra[p].time=0;
	playerFileTra[p].totalSent=0;
	playerFileTra[p].totalLost=0;
	playerFileTra[p].totalReceived=0;
	
	sessionInfo.players[p].init();
	sessionInfo.players[p].type=BasePlayer::P_IP;
	sessionInfo.players[p].setNumber(p);
	sessionInfo.players[p].setTeamNumber(t);
	memcpy(sessionInfo.players[p].name, (char *)(data+8), 32);
	sessionInfo.players[p].setip(ip);
	sessionInfo.players[p].ipFromNAT=(bool)getSint32(data, 4);
	fprintf(logFile, "this ip(%s) has ipFromNAT=(%d)\n", Utilities::stringIP(ip), sessionInfo.players[p].ipFromNAT);

	yog->joinerConnected(ip);
	// we check if this player has already a connection:

	for (int i=0; i<p; i++)
	{
		if (sessionInfo.players[i].sameip(ip))
		{
			fprintf(logFile, "this ip(%s) is already in the player list!\n", Utilities::stringIP(ip));

			sessionInfo.players[i].netState=BasePlayer::PNS_PLAYER_SEND_PRESENCE_REQUEST;
			sessionInfo.players[i].netTimeout=0;
			sessionInfo.players[i].netTimeoutSize=LONG_NETWORK_TIMEOUT;
			sessionInfo.players[i].netTOTL=DEFAULT_NETWORK_TOTL+1;
			return;
		}
	}

	int freeChannel=getFreeChannel();
	if (!sessionInfo.players[p].bind(socket, freeChannel))
	{
		fprintf(logFile, "this ip(%s) is not bindable\n", Utilities::stringIP(ip));
		return;
	}

	if ( sessionInfo.players[p].send(SERVER_PRESENCE) )
	{
		fprintf(logFile, "newPlayerPresence::this ip(%s) is added in player list. (player %d)\n", Utilities::stringIP(ip), p);
		sessionInfo.numberOfPlayer++;
		sessionInfo.team[sessionInfo.players[p].teamNumber].playersMask|=sessionInfo.players[p].numberMask;
		sessionInfo.team[sessionInfo.players[p].teamNumber].numberOfPlayer++;
		sessionInfo.players[p].netState=BasePlayer::PNS_PLAYER_SEND_PRESENCE_REQUEST;
		sessionInfo.players[p].netTimeout=SHORT_NETWORK_TIMEOUT;
		sessionInfo.players[p].netTimeoutSize=SHORT_NETWORK_TIMEOUT;
		sessionInfo.players[p].netTOTL=DEFAULT_NETWORK_TOTL;
	}
}

void MultiplayersHost::playerWantsSession(char *data, int size, IPaddress ip)
{
	if (size!=10)
	{
		fprintf(logFile, "Bad size(%d) for an Session request from ip %s.\n", size, Utilities::stringIP(ip));
		return;
	}
	
	int p;
	for (p=0; p<sessionInfo.numberOfPlayer; p++)
		if (sessionInfo.players[p].sameip(ip))
			break;
	if (p>=sessionInfo.numberOfPlayer)
	{
		fprintf(logFile, "An unknow player (%s) has sended a Session request !!!\n", Utilities::stringIP(ip));
		return;
	}
	
	bool serverIPReceived=false;
	if (!sessionInfo.players[p].ipFromNAT)
	{
		Uint32 newHost=SDL_SwapBE32(getUint32(data, 4));
		Uint16 newPort=SDL_SwapBE16(getUint16(data, 8));
		fprintf(logFile, "serverIP=(%s), serverIP.host=%x, serverIP.port=%x, newHost=%x, newPort=%x\n", Utilities::stringIP(serverIP), serverIP.host, serverIP.port, newHost, newPort);
		if (serverIP.host && (serverIP.host!=SDL_SwapBE32(0x7F000001)))
		{
			if (serverIP.host!=newHost)
			{
				fprintf(logFile, "Bad ip received by(%s). old=(%s) new=(%s)\n", Utilities::stringIP(ip), Utilities::stringIP(serverIP.host), Utilities::stringIP(newHost));
				return;
			}
			if (serverIP.port!=newPort)
			{
				fprintf(logFile, "Bad port received by(%s). old=(%d) new=(%d)\n", Utilities::stringIP(ip), serverIP.port, newPort);
				return;
			}
		}
		else
		{
			serverIP.host=newHost;
			serverIP.port=newPort;
			serverIPReceived=true;
			fprintf(logFile, "I recived my ip!:(%s).\n", Utilities::stringIP(serverIP));
		}
	}

	sessionInfo.players[p].netState=BasePlayer::PNS_PLAYER_SEND_SESSION_REQUEST;
	sessionInfo.players[p].netTimeout=0;
	sessionInfo.players[p].netTimeoutSize=LONG_NETWORK_TIMEOUT;
	sessionInfo.players[p].netTOTL=DEFAULT_NETWORK_TOTL+1;

	fprintf(logFile, "this ip(%s) wantsSession (player %d)\n", Utilities::stringIP(ip), p);
	
	// all other players are ignorant of the new situation:
	initHostGlobalState();
	reinitPlayersState();
	
	if (serverIPReceived)
		sessionInfo.players[p].netTimeout=3; // =1 would be enough, if loopback where safe.
}

void MultiplayersHost::playerWantsFile(char *data, int size, IPaddress ip)
{
	if (size>72)
	{
		fprintf(logFile, "Bad size(%d) for an File request from ip %s.\n", size, Utilities::stringIP(ip));
		fprintf(logFileDownload, "Bad size(%d) for an File request from ip %s.\n", size, Utilities::stringIP(ip));
		return;
	}
	
	int p;
	for (p=0; p<sessionInfo.numberOfPlayer; p++)
		if (sessionInfo.players[p].sameip(ip))
			break;
	if (p>=sessionInfo.numberOfPlayer)
	{
		fprintf(logFile, "An unknow player (%s) has sended a File request !!!\n", Utilities::stringIP(ip));
		fprintf(logFileDownload, "An unknow player (%s) has sended a File request !!!\n", Utilities::stringIP(ip));
		return;
	}
	
	if (data[1]&1)
	{
		char data[12];
		data[0]=FULL_FILE_DATA;
		data[1]=1;
		data[2]=0;
		data[3]=0;
		addUint32(data, fileSize, 4);
		bool success=sessionInfo.players[p].send(data, 8);
		assert(success);
		if (size==4)
		{
			fprintf(logFileDownload, "Size only requested.\n");
			return;
		}
	}
	
	if (!playerFileTra[p].wantsFile)
	{
		if (!playerFileTra[p].receivedFile)
		{
			fprintf(logFile, "player %d (%s) first requests file.\n", p, Utilities::stringIP(ip));
			fprintf(logFileDownload, "player %d (%s) first requests file.\n", p, Utilities::stringIP(ip));
			
			playerFileTra[p].wantsFile=true;
			playerFileTra[p].receivedFile=false;
			playerFileTra[p].unreceivedIndex=0;
			playerFileTra[p].brandwidth=1;
			playerFileTra[p].lastNbPacketsLost=0;
			for (int i=0; i<PACKET_SLOTS; i++)
			{
				playerFileTra[p].packetSlot[i].index=0;
				playerFileTra[p].packetSlot[i].sent=false;
				playerFileTra[p].packetSlot[i].received=false;
				playerFileTra[p].packetSlot[i].brandwidth=0;
				playerFileTra[p].packetSlot[i].time=0;
			}
			playerFileTra[p].latency=32;
			playerFileTra[p].time=0;
			playerFileTra[p].totalSent=0;
			playerFileTra[p].totalLost=0;
			playerFileTra[p].totalReceived=0;
		}
		else
		{
			fprintf(logFile, "player %d (%s) double requests file.\n", p, Utilities::stringIP(ip));
			fprintf(logFileDownload, "player %d (%s) double requests file.\n", p, Utilities::stringIP(ip));
		}
	}
	else if (size>=8)
	{
		Uint32 unreceivedIndex=getUint32(data, 4);
		if (unreceivedIndex<playerFileTra[p].unreceivedIndex)
		{
			fprintf(logFile, "Bad FileRequest packet received !!!\n");
			fprintf(logFileDownload, "Bad FileRequest packet received !!!\n");
			return;
		}
		playerFileTra[p].unreceivedIndex=unreceivedIndex;
		fprintf(logFileDownload, "player %d unreceivedIndex=%d\n", p, unreceivedIndex);
		
		if (unreceivedIndex>=fileSize)
		{
			if (playerFileTra[p].totalSent && !playerFileTra[p].receivedFile)
			{
				fprintf(logFileDownload, "player %d \n", p);
				fprintf(logFileDownload, "playerFileTra[p].totalSent=%d.\n", playerFileTra[p].totalSent);
				fprintf(logFileDownload, "playerFileTra[p].totalLost=%d. (%f)\n", playerFileTra[p].totalLost, (float)playerFileTra[p].totalLost/(float)playerFileTra[p].totalSent);
				fprintf(logFileDownload, "playerFileTra[p].totalReceived=%d. (%f)\n", playerFileTra[p].totalReceived, (float)playerFileTra[p].totalReceived/(float)playerFileTra[p].totalSent);
			}
			playerFileTra[p].wantsFile=true;
			playerFileTra[p].receivedFile=true;
			stepHostGlobalState();
		}
		else
		{
			// We dump the current packet's received's confirmation:
			int ixend=(size-8)/8;
			fprintf(logFileDownload, "ixend=%d\n", ixend);
			Uint32 receivedBegin[8];
			Uint32 receivedEnd[8];
			fprintf(logFileDownload, "received=(");
			for (int ix=0; ix<ixend; ix++)
			{
				receivedBegin[ix]=getUint32(data, 8+ix*8);
				receivedEnd[ix]=getUint32(data, 12+ix*8);
				fprintf(logFileDownload, "(%d to %d)+", receivedBegin[ix], receivedEnd[ix]);
				if (receivedBegin[ix]<=unreceivedIndex)
				{
					fprintf(logFileDownload, "Warning, critical error, (receivedBegin[ix]<=unreceivedIndex), (%d)(%d)\n", receivedBegin[ix], unreceivedIndex);
					return;
				}
			}
			fprintf(logFileDownload, ")\n");
			
			// We record which packets have been received, and how many:
			int nbPacketsReceived=0;
			for (int i=0; i<PACKET_SLOTS; i++)
			{
				Uint32 index=playerFileTra[p].packetSlot[i].index;
				if (playerFileTra[p].packetSlot[i].sent && !playerFileTra[p].packetSlot[i].received)
				{
					if (index<unreceivedIndex)
					{
						nbPacketsReceived++;
						playerFileTra[p].totalReceived++;
						playerFileTra[p].packetSlot[i].received=true;
					}
					else
						for (int ix=0; ix<ixend; ix++)
							if (index>=receivedBegin[ix] && index<=receivedEnd[ix])
							{
								nbPacketsReceived++;
								playerFileTra[p].totalReceived++;
								playerFileTra[p].packetSlot[i].received=true;
								break;
							}
				}
			}
			
			// We compute the number of (probably) lost packets:
			int nbPacketsLost=0;
			int latency=playerFileTra[p].latency;
			for (int i=0; i<PACKET_SLOTS; i++)
			{
				PacketSlot &ps=playerFileTra[p].packetSlot[i];
				if (ps.sent && !ps.received && ps.time>latency)
					nbPacketsLost++;
			}
			if (nbPacketsLost==0)
				playerFileTra[p].brandwidth+=nbPacketsReceived;
		}
	}
	else
	{
		fprintf(logFile, "Bad size(%d) for an File request from ip %s.\n", size, Utilities::stringIP(ip));
		fprintf(logFileDownload, "Bad size(%d) for an File request from ip %s.\n", size, Utilities::stringIP(ip));
		return;
	}
	sessionInfo.players[p].netTimeout=sessionInfo.players[p].netTimeoutSize;
	sessionInfo.players[p].netTOTL=DEFAULT_NETWORK_TOTL;
}

void MultiplayersHost::addAI()
{
	int p=sessionInfo.numberOfPlayer;
	int t=newTeamIndice();
	if (savedSessionInfo)
		t=savedSessionInfo->getAITeamNumber(&sessionInfo, t);
	
	sessionInfo.players[p].init();
	sessionInfo.players[p].type=BasePlayer::P_AI;
	sessionInfo.players[p].setNumber(p);
	sessionInfo.players[p].setTeamNumber(t);
	sessionInfo.players[p].netState=BasePlayer::PNS_PLAYER_SEND_SESSION_REQUEST;
	strncpy(sessionInfo.players[p].name, globalContainer->texts.getString("[AI]", abs(rand())%globalContainer->texts.AI_NAME_SIZE), BasePlayer::MAX_NAME_LENGTH);
	
	sessionInfo.numberOfPlayer++;
	sessionInfo.team[sessionInfo.players[p].teamNumber].playersMask|=sessionInfo.players[p].numberMask;
	sessionInfo.team[sessionInfo.players[p].teamNumber].numberOfPlayer++;
	
	/*sessionInfo.players[p].netState=BasePlayer::PNS_AI;
	sessionInfo.players[p].netTimeout=0;
	sessionInfo.players[p].netTimeoutSize=LONG_NETWORK_TIMEOUT;
	sessionInfo.players[p].netTOTL=DEFAULT_NETWORK_TOTL+1;
	crossPacketRecieved[p]=4;*/
	
	// all other players are ignorant of the new situation:
	initHostGlobalState();
	reinitPlayersState();
}

void MultiplayersHost::confirmPlayer(char *data, int size, IPaddress ip)
{
	Sint32 rcs=getSint32(data, 4);
	Sint32 lcs=sessionInfo.checkSum();

	int i;
	for (i=0; i<sessionInfo.numberOfPlayer; i++)
		if (sessionInfo.players[i].sameip(ip))
			break;
	if (i>=sessionInfo.numberOfPlayer)
	{
		fprintf(logFile, "An unknow player (%s) has sended a checksum !!!\n", Utilities::stringIP(ip));
		return;
	}

	if (rcs!=lcs)
	{
		fprintf(logFile, "this ip(%s) confirmed a wrong checksum (player %d)!\n", Utilities::stringIP(ip), i);
		fprintf(logFile, "rcs=%x, lcs=%x.\n", rcs, lcs);
		sessionInfo.players[i].netState=BasePlayer::PNS_PLAYER_SEND_SESSION_REQUEST;
		sessionInfo.players[i].netTimeout=0;
		sessionInfo.players[i].netTimeoutSize=LONG_NETWORK_TIMEOUT;
		sessionInfo.players[i].netTOTL=DEFAULT_NETWORK_TOTL+1;
		return;
	}
	else
	{
		fprintf(logFile, "this ip(%s) confirmed a good checksum (player %d)\n", Utilities::stringIP(ip), i);
		sessionInfo.players[i].netState=BasePlayer::PNS_PLAYER_SEND_CHECK_SUM;
		sessionInfo.players[i].netTimeout=0;
		sessionInfo.players[i].netTimeoutSize=SHORT_NETWORK_TIMEOUT;
		sessionInfo.players[i].netTOTL=DEFAULT_NETWORK_TOTL+1;
		fprintf(logFile, "this ip(%x) is confirmed in player list.\n", ip.host);
		return;
	}
}
void MultiplayersHost::confirmStartCrossConnection(char *data, int size, IPaddress ip)
{
	int i;
	for (i=0; i<sessionInfo.numberOfPlayer; i++)
		if (sessionInfo.players[i].sameip(ip))
			break;
	if (i>=sessionInfo.numberOfPlayer)
	{
		fprintf(logFile, "An unknow player (%s) has sended a confirmStartCrossConnection !!!\n", Utilities::stringIP(ip));
		return;
	}

	if (sessionInfo.players[i].netState>=BasePlayer::PNS_SERVER_SEND_CROSS_CONNECTION_START)
	{
		sessionInfo.players[i].netState=BasePlayer::PNS_PLAYER_CONFIRMED_CROSS_CONNECTION_START;
		sessionInfo.players[i].netTimeout=SHORT_NETWORK_TIMEOUT;
		sessionInfo.players[i].netTimeoutSize=SHORT_NETWORK_TIMEOUT;
		sessionInfo.players[i].netTOTL=DEFAULT_NETWORK_TOTL;
		fprintf(logFile, "this ip(%s) is start cross connection confirmed..\n", Utilities::stringIP(ip));
		return;
	}
	else
		fprintf(logFile, "this ip(%s) is start cross connection confirmed too early ??\n", Utilities::stringIP(ip));
}
void MultiplayersHost::confirmStillCrossConnecting(char *data, int size, IPaddress ip)
{
	int i;
	for (i=0; i<sessionInfo.numberOfPlayer; i++)
		if (sessionInfo.players[i].sameip(ip))
			break;
	if (i>=sessionInfo.numberOfPlayer)
	{
		fprintf(logFile, "An unknow player (%s) has sended a confirmStillCrossConnecting !!!\n", Utilities::stringIP(ip));
		return;
	}

	if (sessionInfo.players[i].netState>=BasePlayer::PNS_SERVER_SEND_CROSS_CONNECTION_START)
	{
		sessionInfo.players[i].netState=BasePlayer::PNS_PLAYER_CONFIRMED_CROSS_CONNECTION_START;
		sessionInfo.players[i].netTimeout=SHORT_NETWORK_TIMEOUT;
		sessionInfo.players[i].netTimeoutSize=SHORT_NETWORK_TIMEOUT;
		sessionInfo.players[i].netTOTL=DEFAULT_NETWORK_TOTL;
		sessionInfo.players[i].send(SERVER_CONFIRM_CLIENT_STILL_CROSS_CONNECTING);
		fprintf(logFile, "this ip(%s) is continuing cross connection confirmed..\n", Utilities::stringIP(ip));
		return;
	}
}

void MultiplayersHost::confirmCrossConnectionAchieved(char *data, int size, IPaddress ip)
{
	int i;
	for (i=0; i<sessionInfo.numberOfPlayer; i++)
		if (sessionInfo.players[i].sameip(ip))
			break;
	if (i>=sessionInfo.numberOfPlayer)
	{
		fprintf(logFile, "An unknow player (%s) has sended a confirmCrossConnectionAchieved !!!\n", Utilities::stringIP(ip));
		return;
	}

	if (sessionInfo.players[i].netState>=BasePlayer::PNS_SERVER_SEND_CROSS_CONNECTION_START)
	{
		sessionInfo.players[i].netState=BasePlayer::PNS_PLAYER_FINISHED_CROSS_CONNECTION;
		sessionInfo.players[i].netTimeout=0;
		sessionInfo.players[i].netTimeoutSize=SHORT_NETWORK_TIMEOUT;
		sessionInfo.players[i].netTOTL=DEFAULT_NETWORK_TOTL;
		fprintf(logFile, "this ip(%s) player(%d) is cross connection achievement confirmed..\n", Utilities::stringIP(ip), i);

		crossPacketRecieved[i]=3;

		// let's check if all players are cross Connected
		stepHostGlobalState();

		return;
	}
}

void MultiplayersHost::confirmPlayerStartGame(char *data, int size, IPaddress ip)
{
	if (size!=8)
	{
		fprintf(logFile, "A player (%s) has sent a bad sized confirmPlayerStartGame.\n", Utilities::stringIP(ip));
		return;
	}

	int i;
	for (i=0; i<sessionInfo.numberOfPlayer; i++)
		if (sessionInfo.players[i].sameip(ip))
			break;
	if (i>=sessionInfo.numberOfPlayer)
	{
		fprintf(logFile, "An unknow player (%s) has sent a confirmPlayerStartGame.\n", Utilities::stringIP(ip));
		return;
	}

	if (sessionInfo.players[i].netState>=BasePlayer::PNS_SERVER_SEND_START_GAME)
	{
		sessionInfo.players[i].netState=BasePlayer::PNS_PLAYER_CONFIRMED_START_GAME;
		sessionInfo.players[i].netTimeout=0;
		sessionInfo.players[i].netTimeoutSize=SHORT_NETWORK_TIMEOUT;
		sessionInfo.players[i].netTOTL=DEFAULT_NETWORK_TOTL;
		int sgtc=data[4];
		if((abs(sgtc-startGameTimeCounter)<20) && (sgtc>0))
			startGameTimeCounter=(startGameTimeCounter*3+sgtc)/4;
			// ping=(startGameTimeCounter-sgtc)/2
			// startGameTimeCounter=(startGameTimeCounter+sgtc)/2 would be a full direct correction
			// but the division by 4 will gives a fair average ping between all players
		fprintf(logFile, "this ip(%s) confirmed start game within %d seconds.\n", Utilities::stringIP(ip), sgtc/20);

		crossPacketRecieved[i]=4;

		// let's check if all players are playing
		stepHostGlobalState();

		return;
	}
}

void MultiplayersHost::broadcastRequest(char *data, int size, IPaddress ip)
{
	if (size!=4)
	{
		fprintf(logFile, "broad:Bad size(%d) for a broadcast request from ip %s.\n", size, Utilities::stringIP(ip));
		return;
	}

	UDPpacket *packet=SDLNet_AllocPacket(4+64+32);

	if (packet==NULL)
	{
		fprintf(logFile, "broad:can't alocate packet!\n");
		return;
	}

	if (ip.host==0)
	{
		fprintf(logFile, "broad:can't have a null ip.host\n");
		return;
	}

	char sdata[4+64+32];
	if (shareOnYOG)
		sdata[0]=BROADCAST_RESPONSE_YOG;
	else
		sdata[0]=BROADCAST_RESPONSE_LAN;
	sdata[1]=0;
	sdata[2]=0;
	sdata[3]=0;
	// TODO: allow to use a game name different than mapName.
	int mnl=Utilities::strmlen(sessionInfo.getMapName(), 64);
	memcpy(sdata+4, sessionInfo.getMapName(), mnl);
	int unl=Utilities::strmlen(globalContainer->userName, 32);
	memcpy(sdata+4+mnl, globalContainer->userName, unl);

	//fprintf(logFile, "MultiplayersHost sending1 (%d, %d, %d, %d).\n", data[4], data[5], data[6], data[7]);
	//fprintf(logFile, "MultiplayersHost sending2 (%s).\n", sessionInfo.getMapName());
	//fprintf(logFile, "MultiplayersHost sendingB (%s).\n", &data[4]);
	packet->len=4+mnl+unl;
	memcpy((char *)packet->data, sdata, 4+mnl+unl);

	bool sucess;

	packet->address=ip;
	//packet->channel=channel;
	packet->channel=-1;

	sucess=SDLNet_UDP_Send(socket, channel, packet)==1;
	// Notice that we can choose between giving a "channel", or the ip.
	// Here we do both. Then "channel" could be -1.
	// This is interesting because getFreeChannel() may return -1.
	// We have no real use of "channel".
	if (sucess)
		fprintf(logFile, "broad:sucedded to response. shareOnYOG=(%d)\n", shareOnYOG);


	SDLNet_FreePacket(packet);

	fprintf(logFile, "broad:Unbinding (socket=%x)(channel=%d).\n", (int)socket, channel);
	SDLNet_UDP_Unbind(socket, channel);
	channel=-1;
}

void MultiplayersHost::treatData(char *data, int size, IPaddress ip)
{
	if (data[0]!=NEW_PLAYER_WANTS_FILE)
		fprintf(logFile, "\nMultiplayersHost::treatData (%d)\n", data[0]);
	if ((data[2]!=0)||(data[3]!=0))
	{
		printf("Bad packet received (%d,%d,%d,%d)!\n", data[0], data[1], data[2], data[3]);
		return;
	}
	if (hostGlobalState<HGS_GAME_START_SENDED)
	{
		switch (data[0])
		{
		case BROADCAST_REQUEST:
			broadcastRequest(data, size, ip);
		break;
		
		case YOG_CLIENT_REQUESTS_GAME_INFO:
			yogClientRequestsGameInfo(data, size, ip);
		break;
		
		case NEW_PLAYER_WANTS_PRESENCE:
			newPlayerPresence(data, size, ip);
		break;
		
		case NEW_PLAYER_WANTS_SESSION_INFO:
			playerWantsSession(data, size, ip);
		break;
		
		case NEW_PLAYER_WANTS_FILE:
			playerWantsFile(data, size, ip);
		break;
		
		case NEW_PLAYER_SEND_CHECKSUM_CONFIRMATION:
			confirmPlayer(data, size, ip);
		break;

		case CLIENT_QUIT_NEW_GAME:
			removePlayer(data, size, ip);
		break;

		case PLAYERS_CONFIRM_START_CROSS_CONNECTIONS:
			confirmStartCrossConnection(data, size, ip);
		break;

		case PLAYERS_STILL_CROSS_CONNECTING:
			confirmStillCrossConnecting(data, size, ip);
		break;

		case PLAYERS_CROSS_CONNECTIONS_ACHIEVED:
			confirmCrossConnectionAchieved(data, size, ip);
		break;

		default:
			fprintf(logFile, "Unknow kind of packet(%d) recieved by ip(%s).\n", data[0], Utilities::stringIP(ip));
		};
	}
	else
	{
		switch (data[0])
		{
		case PLAYER_CONFIRM_GAME_BEGINNING :
			confirmPlayerStartGame(data, size, ip);
		break;

		default:
			fprintf(logFile, "Unknow kind of packet(%d) recieved by ip(%s).\n", data[0], Utilities::stringIP(ip));
		};
	}
}

void MultiplayersHost::onTimer(Uint32 tick, MultiplayersJoin *multiplayersJoin)
{
	// call yog step
	if (shareOnYOG && multiplayersJoin==NULL)
		yog->step(); // YOG cares about firewall and NAT
	
	if (hostGlobalState>=HGS_GAME_START_SENDED)
	{
		if (--startGameTimeCounter<0)
		{
			send(SERVER_ASK_FOR_GAME_BEGINNING, startGameTimeCounter);
			fprintf(logFile, "Lets quit this screen and start game!\n");
			if (hostGlobalState<=HGS_GAME_START_SENDED)
			{
				// done in game: drop player.
			}
		}
		else if (startGameTimeCounter%20==0)
		{
			send(SERVER_ASK_FOR_GAME_BEGINNING, startGameTimeCounter);
		}
	}
	else
		sendingTime();

	if (socket)
	{
		UDPpacket *packet=NULL;
		packet=SDLNet_AllocPacket(MAX_PACKET_SIZE);
		assert(packet);

		while (SDLNet_UDP_Recv(socket, packet)==1)
		{
			//fprintf(logFile, "packet=%d\n", (int)packet);
			//fprintf(logFile, "packet->channel=%d\n", packet->channel);
			//fprintf(logFile, "packet->len=%d\n", packet->len);
			//fprintf(logFile, "packet->maxlen=%d\n", packet->maxlen);
			//fprintf(logFile, "packet->status=%d\n", packet->status);
			//fprintf(logFile, "packet->address=%x,%d\n", packet->address.host, packet->address.port);

			//fprintf(logFile, "packet->data=%s\n", packet->data);

			treatData((char *)(packet->data), packet->len, packet->address);

			//paintSessionInfo(hostGlobalState);
			//addUpdateRect();
		}

		SDLNet_FreePacket(packet);
	}
}

bool MultiplayersHost::send(const int v)
{
	//fprintf(logFile, "Sending packet to all players (%d).\n", v);
	char data[4];
	data[0]=v;
	data[1]=0;
	data[2]=0;
	data[3]=0;
	for (int i=0; i<sessionInfo.numberOfPlayer; i++)
		sessionInfo.players[i].send(data, 4);

	return true;
}
bool MultiplayersHost::send(const int u, const int v)
{
	//fprintf(logFile, "Sending packet to all players (%d;%d).\n", u, v);
	char data[8];
	data[0]=u;
	data[1]=0;
	data[2]=0;
	data[3]=0;
	data[4]=v;
	data[5]=0;
	data[6]=0;
	data[7]=0;
	for (int i=0; i<sessionInfo.numberOfPlayer; i++)
		sessionInfo.players[i].send(data, 8);

	return true;
}

void MultiplayersHost::sendBroadcastLanGameHosting(Uint16 port, bool create)
{
	UDPpacket *packet=SDLNet_AllocPacket(4);
	assert(packet);
	packet->channel=-1;
	packet->address.host=INADDR_BROADCAST;
	packet->address.port=SDL_SwapBE16(port);
	packet->len=4;
	packet->data[0]=BROADCAST_LAN_GAME_HOSTING;
	packet->data[1]=create;
	packet->data[2]=0;
	packet->data[3]=0;
	if (SDLNet_UDP_Send(socket, -1, packet)==1)
		fprintf(logFile, "Successed to send a BROADCAST_LAN_GAME_HOSTING(%d) packet to port=(%d).\n", create, port);
	else
		fprintf(logFile, "failed to send a BROADCAST_LAN_GAME_HOSTING(%d) packet to port=(%d)!\n", create, port);
	SDLNet_FreePacket(packet);
}

void MultiplayersHost::sendingTime()
{
	bool update=false;
	if (hostGlobalState<HGS_GAME_START_SENDED)
	{
		for (int i=0; i<sessionInfo.numberOfPlayer; i++)
			if (sessionInfo.players[i].netState==BasePlayer::PNS_BAD)
			{
				removePlayer(i);
				update=true;
			}

		if (update)
		{
			// all other players are ignorant of the new situation:
			initHostGlobalState();
			reinitPlayersState();
		}
	}
	
	// We send the file if necessary
	for (int p=0; p<32; p++)
		if (playerFileTra[p].wantsFile && !playerFileTra[p].receivedFile)
		{
			// We compute an average latency:
			int latencySum=0;
			int latencyCount=0;
			for (int i=0; i<PACKET_SLOTS; i++)
				if (playerFileTra[p].packetSlot[i].sent && playerFileTra[p].packetSlot[i].received)
				{
					latencySum+=playerFileTra[p].packetSlot[i].time;
					latencyCount++;
				}
			int latency;
			if (latencyCount>32)
				latency=latencySum/latencyCount;
			else
				latency=32;
			if (playerFileTra[p].latency!=latency)
			{
				fprintf(logFileDownload, "new latency=%d.\n", latency);
				playerFileTra[p].latency=latency;
			}
			
			// We compute the number of (more probably) lost packets:
			latency=2*latency;
			int nbPacketsLost=0;
			for (int i=0; i<PACKET_SLOTS; i++)
				if (playerFileTra[p].packetSlot[i].sent && !playerFileTra[p].packetSlot[i].received && playerFileTra[p].packetSlot[i].time>latency)
					nbPacketsLost++;
			if (nbPacketsLost)
				fprintf(logFileDownload, "nbPacketsLost=%d.\n", nbPacketsLost);
			
			// We update the brandwidth:
			int brandwidth=playerFileTra[p].brandwidth;
			if (nbPacketsLost>playerFileTra[p].lastNbPacketsLost)
			{
				brandwidth-=(nbPacketsLost-playerFileTra[p].lastNbPacketsLost);
				if (brandwidth<1)
					brandwidth=1;
				if (playerFileTra[p].brandwidth!=brandwidth)
					fprintf(logFileDownload, "new brandwidth=%d.\n", brandwidth);
				playerFileTra[p].brandwidth=brandwidth;
			}
			playerFileTra[p].lastNbPacketsLost=nbPacketsLost;
			
			// Can we send something ?
			int time=(playerFileTra[p].time++)&31;
			int toSend=(brandwidth>>5)+(time<=(brandwidth&31));
			
			// OK, let's sens "toSend" packets:
			fprintf(logFileDownload, "*Sending %d packets:\n", toSend);
			for (int s=0; s<toSend; s++)
			{
				// We take the oldest packet:
				Uint32 minIndex=(Uint32)-1;
				int mini=-1;
				for (int i=0; i<PACKET_SLOTS; i++)
					if (!playerFileTra[p].packetSlot[i].sent)
					{
						minIndex=playerFileTra[p].packetSlot[i].index;
						mini=i;
						break;
					}
				if (mini==-1)
					for (int i=0; i<PACKET_SLOTS; i++)
					{
						Uint32 index=playerFileTra[p].packetSlot[i].index;
						if (index<minIndex)
						{
							minIndex=index;
							mini=i;
							if (minIndex==0)
								break;
						}
					}
				assert(mini!=-1);
				assert(mini<PACKET_SLOTS);
				bool sent=playerFileTra[p].packetSlot[mini].sent;
				bool received=playerFileTra[p].packetSlot[mini].received;
				Uint32 sendingIndex;
				if ((sent && received) || !sent)
				{
					// This not a lost packet, we have to find a packet to send:
					// first, is it any late packet ?
					bool found=false;
					Uint32 minIndexLost=(Uint32)-1;
					for (int i=0; i<PACKET_SLOTS; i++)
					{
						PacketSlot &ps=playerFileTra[p].packetSlot[i];
						if (ps.sent && !ps.received && ps.time>latency)
						{
							found=true;
							Uint32 index=ps.index;
							if (index<minIndexLost)
							{
								minIndexLost=index;
								mini=i;
							}
						}
					}
					if (found)
					{
						sendingIndex=minIndexLost;
						fprintf(logFileDownload, "Ressending-v1 now the lost packet, sendingIndex=%d, mini=%d.\n", sendingIndex, mini);
						playerFileTra[p].totalLost++;
					}
					else
					{
						// Here, we have no late packet, we look for a new packet:
						Uint32 maxIndex=0;
						bool firstTime=true;
						for (int i=0; i<PACKET_SLOTS; i++)
							if (playerFileTra[p].packetSlot[i].sent)
							{
								Uint32 index=playerFileTra[p].packetSlot[i].index;
								firstTime=false;
								if (index>maxIndex)
									maxIndex=index;
							}
						if (firstTime)
						{
							fprintf(logFileDownload, " We send the first packet!\n");
							sendingIndex=0;
						}
						else
						{
							sendingIndex=maxIndex+1024; // 1024 is the default size.
							if (sendingIndex>=fileSize)
							{
								fprintf(logFileDownload, "Nothing-v1 more to send to player (%d). sendingIndex=(%d).\n", p, sendingIndex);
								toSend=0;
								break;
							}
							else
								fprintf(logFileDownload, "Sending the next packet, sendingIndex=%d.\n", sendingIndex);
						}
					}
				}
				else
				{
					// This is a definitely lost packet, we send it aggain:
					fprintf(logFileDownload, "Ressending-v2 now the lost packet, sendingIndex=%d, mini=%d.\n", sendingIndex, mini);
					sendingIndex=playerFileTra[p].packetSlot[mini].index;
					playerFileTra[p].totalLost++;
				}
				
				int size=1024; // 1024 is the default size.
				if (sendingIndex+size>fileSize)
					size=fileSize-sendingIndex;
				if (size<0)
				{
					fprintf(logFileDownload, "Nothing-v2 more to send to player (%d). sendingIndex=(%d).\n", p, sendingIndex);
					toSend=0;
					break;
				}
				
				char *data=(char *)malloc(12+size);
				assert(data);
				data[0]=FULL_FILE_DATA;
				data[1]=0;
				data[2]=0;
				data[3]=0;
				if (size<1024)
					addSint32(data, 0, 4); // Send a confirmation at once, beacause we are at the end of the file.
				else
					addSint32(data, brandwidth, 4);
				addUint32(data, sendingIndex, 8);
				SDL_RWseek(stream, sendingIndex, SEEK_SET);
				SDL_RWread(stream, data+12, size, 1);
				bool success=sessionInfo.players[p].send(data, 12+size);
				assert(success);
				free(data);
				playerFileTra[p].totalSent++;
				fprintf(logFileDownload, "sent a (size=%d) packet to player (%d). sendingIndex=(%d).\n", size, p, sendingIndex);
				
				playerFileTra[p].packetSlot[mini].index=sendingIndex;
				playerFileTra[p].packetSlot[mini].sent=true;
				playerFileTra[p].packetSlot[mini].received=false;
				playerFileTra[p].packetSlot[mini].brandwidth=brandwidth;
				playerFileTra[p].packetSlot[mini].time=0;
			}
			
			for (int i=0; i<PACKET_SLOTS; i++)
				if (playerFileTra[p].packetSlot[i].sent && !playerFileTra[p].packetSlot[i].received)
					playerFileTra[p].packetSlot[i].time++;
		}
	
	
	for (int i=0; i<sessionInfo.numberOfPlayer; i++)
	{
		if ((sessionInfo.players[i].type==BasePlayer::P_IP)&&(--sessionInfo.players[i].netTimeout<0))
		{
			update=true;
			sessionInfo.players[i].netTimeout+=sessionInfo.players[i].netTimeoutSize;

			assert(sessionInfo.players[i].netTimeoutSize);

			if (--sessionInfo.players[i].netTOTL<0)
			{
				if (hostGlobalState>=HGS_GAME_START_SENDED)
				{
					// we only drop the players, because other player are already playing.
					// will be done in the game!
				}
				else
				{
					sessionInfo.players[i].netState=BasePlayer::PNS_BAD;
					fprintf(logFile, "Last timeout for player %d has been spent.\n", i);
				}
			}

			switch (sessionInfo.players[i].netState)
			{
			case BasePlayer::PNS_BAD :
			{
				// we remove player out of this loop, to avoid mess.
			}
			break;

			case BasePlayer::PNS_PLAYER_SEND_PRESENCE_REQUEST :
			{
				fprintf(logFile, "Lets send the presence to player %d.\n", i);
				sessionInfo.players[i].send(SERVER_PRESENCE);
			}
			break;

			case BasePlayer::PNS_PLAYER_SEND_SESSION_REQUEST :
			{
				fprintf(logFile, "Lets send the session info to player %d. ip=%s\n", i, Utilities::stringIP(sessionInfo.players[i].ip));

				BasePlayer *backupPlayer[32];
				for (int p=0; p<sessionInfo.numberOfPlayer; p++)
				{
					backupPlayer[p]=(BasePlayer *)malloc(sizeof(BasePlayer));
					*backupPlayer[p]=sessionInfo.players[p];
				}
				if (!sessionInfo.players[i].ipFromNAT)
				{
					for (int p=0; p<sessionInfo.numberOfPlayer; p++)
						if (sessionInfo.players[p].ip.host==SDL_SwapBE32(0x7F000001))
							sessionInfo.players[p].ip.host=serverIP.host;
				
					if (shareOnYOG)
						for (int i=0; i<sessionInfo.numberOfPlayer; i++)
							if (sessionInfo.players[i].ipFromNAT)
							{
								IPaddress newip=yog->ipFromUserName(sessionInfo.players[i].name);
								fprintf(logFile, "for player (%d) name (%s), may replace ip(%s) by ip(%s)\n", i, sessionInfo.players[i].name, Utilities::stringIP(sessionInfo.players[i].ip), Utilities::stringIP(newip));
								if (newip.host)
								{
									sessionInfo.players[i].ip=newip;
									sessionInfo.players[i].ipFromNAT=false;
								}
							}
				}
				
				char *data=NULL;
				int size=sessionInfo.getDataLength(true);

				fprintf(logFile, "sessionInfo.getDataLength()=size=%d.\n", size);
				fprintf(logFile, "sessionInfo.mapGenerationDescriptor=%x.\n", (int)sessionInfo.mapGenerationDescriptor);

				data=(char *)malloc(size+8);
				assert(data);

				data[0]=DATA_SESSION_INFO;
				data[1]=0;
				data[2]=0;
				data[3]=0;
				addSint32(data, i, 4);

				memcpy(data+8, sessionInfo.getData(true), size);

				sessionInfo.players[i].send(data, size+8);

				free(data);
				
				for (int p=0; p<sessionInfo.numberOfPlayer; p++)
				{
					sessionInfo.players[p]=*backupPlayer[p];
					free(backupPlayer[p]);
				}
			}
			break;


			case BasePlayer::PNS_PLAYER_SEND_CHECK_SUM :
			{
				fprintf(logFile, "Lets send the confiramtion for checksum to player %d.\n", i);
				char data[8];
				data[0]=SERVER_SEND_CHECKSUM_RECEPTION;
				data[1]=0;
				data[2]=0;
				data[3]=0;
				addSint32(data, sessionInfo.checkSum(), 4);
				sessionInfo.players[i].send(data, 8);

				// Now that's not our problem if this packet don't sucess.
				// In such a case, the client will reply.
				sessionInfo.players[i].netTimeout=0;
				sessionInfo.players[i].netTimeoutSize=SHORT_NETWORK_TIMEOUT;
				sessionInfo.players[i].netState=BasePlayer::PNS_OK;

				// Lets check if all players has the sessionInfo:
				stepHostGlobalState();

				fprintf(logFile, "player %d is know ok. (%d)\n", i, sessionInfo.players[i].netState);
			}
			break;


			case BasePlayer::PNS_OK :
			{
				if (hostGlobalState>=HGS_WAITING_CROSS_CONNECTIONS)
				{
					sessionInfo.players[i].netState=BasePlayer::PNS_SERVER_SEND_CROSS_CONNECTION_START;
					sessionInfo.players[i].netTimeout=0;
					sessionInfo.players[i].netTimeoutSize=SHORT_NETWORK_TIMEOUT;
					sessionInfo.players[i].netTOTL++;
					fprintf(logFile, "Player %d is newly all right, TOTL %d.\n", i, sessionInfo.players[i].netTOTL);
				}
				else
					fprintf(logFile, "Player %d is all right, TOTL %d.\n", i, sessionInfo.players[i].netTOTL);
				// players keeps ok.
			}
			break;

			case BasePlayer::PNS_SERVER_SEND_CROSS_CONNECTION_START :
			{
				fprintf(logFile, "We have to inform player %d to start cross connection.\n", i);
				sessionInfo.players[i].send(PLAYERS_CAN_START_CROSS_CONNECTIONS);
			}
			break;

			case BasePlayer::PNS_PLAYER_CONFIRMED_CROSS_CONNECTION_START :
			{
				fprintf(logFile, "Player %d is cross connecting, TOTL %d.\n", i, sessionInfo.players[i].netTOTL);
				sessionInfo.players[i].send(PLAYERS_CAN_START_CROSS_CONNECTIONS);
			}
			break;

			case BasePlayer::PNS_PLAYER_FINISHED_CROSS_CONNECTION :
			{
				fprintf(logFile, "We have to inform player %d that we recieved his crossConnection confirmation.\n", i);
				sessionInfo.players[i].send(SERVER_HEARD_CROSS_CONNECTION_CONFIRMATION);

				sessionInfo.players[i].netState=BasePlayer::PNS_CROSS_CONNECTED;
			}
			break;

			case BasePlayer::PNS_CROSS_CONNECTED :
			{
				fprintf(logFile, "Player %d is cross connected ! Yahoo !, TOTL %d.\n", i, sessionInfo.players[i].netTOTL);
			}
			break;

			case BasePlayer::PNS_SERVER_SEND_START_GAME :
			{
				fprintf(logFile, "We send start game to player %d, TOTL %d.\n", i, sessionInfo.players[i].netTOTL);
				sessionInfo.players[i].send(SERVER_ASK_FOR_GAME_BEGINNING);
			}
			break;

			case BasePlayer::PNS_PLAYER_CONFIRMED_START_GAME :
			{
				// here we could tell other players
				fprintf(logFile, "Player %d plays, TOTL %d.\n", i, sessionInfo.players[i].netTOTL);
			}
			break;

			default:
			{
				fprintf(logFile, "Buggy state for player %d.\n", i);
			}

			}

		}
	}
	
}

void MultiplayersHost::stopHosting(void)
{
	fprintf(logFile, "Every player has one chance to get the server-quit packet.\n");
	send(SERVER_QUIT_NEW_GAME);
	
	if (shareOnYOG)
	{
		yog->unshareGame();
	}
}

void MultiplayersHost::startGame(void)
{
	if(hostGlobalState>=HGS_ALL_PLAYERS_CROSS_CONNECTED_AND_HAVE_FILE)
	{
		fprintf(logFile, "Lets tell all players to start game.\n");
		startGameTimeCounter=SECOND_TIMEOUT*SECONDS_BEFORE_START_GAME;
		{
			for (int i=0; i<sessionInfo.numberOfPlayer; i++)
			{
				sessionInfo.players[i].netState=BasePlayer::PNS_SERVER_SEND_START_GAME;
				sessionInfo.players[i].send(SERVER_ASK_FOR_GAME_BEGINNING, startGameTimeCounter);
			}
		}
		hostGlobalState=HGS_GAME_START_SENDED;

		// let's check if all players are playing
		stepHostGlobalState();
	}
	else
		fprintf(logFile, "can't start now. hostGlobalState=(%d)<(%d)\n", hostGlobalState, HGS_ALL_PLAYERS_CROSS_CONNECTED_AND_HAVE_FILE);

}
