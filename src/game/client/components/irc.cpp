#include <base/system.h>
#include <engine/shared/config.h>

#if defined(CONF_FAMILY_WINDOWS)
	#include <windows.h>
#endif

#include "irc.h"

int end_of_motd(char* params, irc_reply_data* hostd, void* conn, void* user) // our callback function
{
	IRC* irc_conn=(IRC*)conn;
	CIRC *pData = (CIRC *)user;

	if(g_Config.m_ClIRCQAuthName && g_Config.m_ClIRCQAuthPass)
		irc_conn->auth(g_Config.m_ClIRCQAuthName, g_Config.m_ClIRCQAuthPass);

	if(g_Config.m_ClIRCModes)
		irc_conn->mode(g_Config.m_ClIRCModes);

	irc_conn->join((char *)"#AllTheHaxx"); // join the channel #AllTheHaxx

	pData->GameClient()->Console()->Print(0, "AS=DJAS)=DA)S(DJ", "ASDU)DASDNJAS(D)ASD", true);
	return 0;
}

CIRC::CIRC()
{
	m_pIRCThread = 0;
	OnReset();
}

void CIRC::OnRender()
{
	// update stuff first, doesn't render anything
	{
		const int64 Now = time_get();
		static int64 LastServerChange = 0;
		static char *pLastServer = g_Config.m_UiServerAddress;

		if(str_comp(pLastServer, g_Config.m_UiServerAddress) != 0)
			LastServerChange = Now;

		if(LastServerChange && Now > LastServerChange + 30*time_freq())
		{
			// TODO: IMPORTANT: send an update about the server we are currently on!
			LastServerChange = 0;
		}
	}

	// render stuff
	{

	}
}

void CIRC::ListenIRCThread(void *pUser)
{
	CIRC *pData = (CIRC *)pUser;

	#if defined(CONF_FAMILY_WINDOWS)
		WSADATA wsaData; /* winsock stuff, linux/unix/bsd users need not worry about this */
	
		if (WSAStartup(MAKEWORD(1, 1), &wsaData)) /* more winsock rubbish */
		{
			printf("Failed to initialise winsock!\n");
		}
	#endif

	pData->m_Connection.hook_irc_command((char *)"376", &end_of_motd, pUser); // hook the end of MOTD message
	pData->m_Connection.start((char *)"irc.quakenet.org", 6668,
			g_Config.m_ClIRCNick, g_Config.m_ClIRCUser, g_Config.m_ClIRCRealname, g_Config.m_ClIRCPass); // connect to the server
	pData->m_Connection.message_loop();

	#if defined(CONF_FAMILY_WINDOWS)
		WSACleanup(); /* more winsock stuff */
	#endif
}

void CIRC::SendChat(const char* pMsg)
{
	char aBuf[510];
	str_format(aBuf, sizeof(aBuf), "PRIVMSG #AllTheHaxx :%s", pMsg);
	m_Connection.raw(aBuf);
}

void CIRC::SendRaw(const char* pMsg)
{
	char aBuf[510];
	str_format(aBuf, sizeof(aBuf), "%s", pMsg+1);
	m_Connection.raw(aBuf);
}

void CIRC::SendNickChange(const char *pNewNick)
{
	char aBuf[32];
	str_format(aBuf, sizeof(aBuf), "%s", pNewNick);
	m_Connection.nick(const_cast<char*>(aBuf));
}

void CIRC::OnConsoleInit()
{
	m_pIRCThread = thread_init(ListenIRCThread, this);
}

void CIRC::OnReset()
{
}
