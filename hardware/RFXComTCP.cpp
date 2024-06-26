#include "stdafx.h"
#include "RFXComTCP.h"
#include "../main/Logger.h"
#include "../main/Helper.h"
#include "../main/mainworker.h"

#define RETRY_DELAY 30

RFXComTCP::RFXComTCP(const int ID, const std::string &IPAddress, const unsigned short usIPPort, const _eRFXAsyncType AsyncType) :
	m_szIPAddress(IPAddress)
{
	m_HwdID=ID;
	m_usIPPort=usIPPort;
	m_AsyncType = AsyncType;
	m_bReceiverStarted = false;
}

bool RFXComTCP::StartHardware()
{
	RequestStart();

	m_bReceiverStarted = false;

	//force connect the next first time
	m_bIsStarted=true;
	m_rxbufferpos=0;

	//Start worker thread
	m_thread = std::make_shared<std::thread>([this] { Do_Work(); });
	SetThreadNameInt(m_thread->native_handle());
	return (m_thread != nullptr);
}

bool RFXComTCP::StopHardware()
{
	if (m_thread)
	{
		RequestStop();
		m_thread->join();
		m_thread.reset();
	}
	m_bIsStarted = false;
	return true;
}

void RFXComTCP::OnConnect()
{
	Log(LOG_STATUS, "Connected to: %s:%d", m_szIPAddress.c_str(), m_usIPPort);
	m_bIsStarted = true;

	sOnConnected(this);
}

void RFXComTCP::OnDisconnect()
{
	Log(LOG_STATUS, "Disconnected");
}

void RFXComTCP::Do_Work()
{
	int sec_counter = 0;
	connect(m_szIPAddress, m_usIPPort);
	while (!IsStopRequested(1000))
	{
		sec_counter++;

		if (sec_counter  % 12 == 0) {
			m_LastHeartbeat = mytime(nullptr);
		}
	}
	terminate();

	Log(LOG_STATUS,"Worker stopped...");
}

void RFXComTCP::OnData(const unsigned char *pData, size_t length)
{
	std::lock_guard<std::mutex> l(readQueueMutex);
	onInternalMessage(pData, length);
}

void RFXComTCP::OnError(const boost::system::error_code& error)
{
	if (
		(error == boost::asio::error::address_in_use) ||
		(error == boost::asio::error::connection_refused) ||
		(error == boost::asio::error::access_denied) ||
		(error == boost::asio::error::host_unreachable) ||
		(error == boost::asio::error::timed_out)
		)
	{
		Log(LOG_ERROR, "Can not connect to: %s:%d", m_szIPAddress.c_str(), m_usIPPort);
	}
	else if (
		(error == boost::asio::error::eof) ||
		(error == boost::asio::error::connection_reset)
		)
	{
		Log(LOG_STATUS, "Connection reset!");
	}
	else
		Log(LOG_ERROR, "%s", error.message().c_str());
}

bool RFXComTCP::WriteToHardware(const char *pdata, const unsigned char length)
{
	if (!isConnected())
		return false;
	write((const unsigned char*)pdata, length);
	return true;
}
