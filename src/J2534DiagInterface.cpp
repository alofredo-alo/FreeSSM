/*
 * J2534DiagInterface.cpp - J2534-pass-through diagnostic interface
 *
 * Copyright (C) 2010-2019 Comer352L
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "J2534DiagInterface.h"
#include <cstring>	// strlen()
#include <iomanip>
#include <sstream>


namespace
{
	const char *j2534StatusName(long status)
	{
		switch (status)
		{
		case STATUS_NOERROR: return "STATUS_NOERROR";
		case ERR_NOT_SUPPORTED: return "ERR_NOT_SUPPORTED";
		case ERR_INVALID_CHANNEL_ID: return "ERR_INVALID_CHANNEL_ID";
		case ERR_INVALID_PROTOCOL_ID: return "ERR_INVALID_PROTOCOL_ID";
		case ERR_NULL_PARAMETER: return "ERR_NULL_PARAMETER";
		case ERR_INVALID_IOCTL_VALUE: return "ERR_INVALID_IOCTL_VALUE";
		case ERR_INVALID_FLAGS: return "ERR_INVALID_FLAGS";
		case ERR_FAILED: return "ERR_FAILED";
		case ERR_DEVICE_NOT_CONNECTED: return "ERR_DEVICE_NOT_CONNECTED";
		case ERR_TIMEOUT: return "ERR_TIMEOUT";
		case ERR_INVALID_MSG: return "ERR_INVALID_MSG";
		case ERR_INVALID_TIME_INTERVAL: return "ERR_INVALID_TIME_INTERVAL";
		case ERR_EXCEEDED_LIMIT: return "ERR_EXCEEDED_LIMIT";
		case ERR_INVALID_MSG_ID: return "ERR_INVALID_MSG_ID";
		case ERR_DEVICE_IN_USE: return "ERR_DEVICE_IN_USE";
		case ERR_INVALID_IOCTL_ID: return "ERR_INVALID_IOCTL_ID";
		case ERR_BUFFER_EMPTY: return "ERR_BUFFER_EMPTY";
		case ERR_BUFFER_FULL: return "ERR_BUFFER_FULL";
		case ERR_BUFFER_OVERFLOW: return "ERR_BUFFER_OVERFLOW";
		case ERR_PIN_INVALID: return "ERR_PIN_INVALID";
		case ERR_CHANNEL_IN_USE: return "ERR_CHANNEL_IN_USE";
		case ERR_MSG_PROTOCOL_ID: return "ERR_MSG_PROTOCOL_ID";
		case ERR_INVALID_FILTER_ID: return "ERR_INVALID_FILTER_ID";
		case ERR_NO_FLOW_CONTROL: return "ERR_NO_FLOW_CONTROL";
		case ERR_NOT_UNIQUE: return "ERR_NOT_UNIQUE";
		case ERR_INVALID_BAUDRATE: return "ERR_INVALID_BAUDRATE";
		case ERR_INVALID_DEVICE_ID: return "ERR_INVALID_DEVICE_ID";
		case J2534API_ERROR_FCN_NOT_SUPPORTED: return "FUNCTION_NOT_EXPORTED";
		case J2534API_ERROR_INVALID_LIBRARY: return "INVALID_LIBRARY";
		case J2534API_ERROR_BROKER_TRANSPORT: return "BROKER_TRANSPORT_ERROR";
		default: return "UNKNOWN_J2534_STATUS";
		}
	}
}


J2534DiagInterface::J2534DiagInterface()
{
	_j2534 = NULL;
	_connected = false;
	_DeviceID = 0;
	_ChannelID = 0;
	_numFilters = 0;
	setName("J2534 Pass-Through");
	setVersion("");
	setProtocolBaudrate( 0 );
}


J2534DiagInterface::~J2534DiagInterface()
{
	disconnect();
	close();
}


AbstractDiagInterface::interface_type J2534DiagInterface::interfaceType()
{
	return interface_type::J2534;
}


bool J2534DiagInterface::open( std::string name )
{
	long ret = 0;
	setLastError("");

	if (_j2534 != NULL)
	{
		setLastError("The J2534 interface is already open.");
		return false;
	}
	// Select J2534-library:
	_j2534 = new J2534_API;
	if (!_j2534->selectLibrary(name))
	{
		setLastError(_j2534->lastError());
#ifdef __FSSM_DEBUG__
		std::cout << "Error: invalid library selected\n";
#endif
		delete _j2534;
		_j2534 = NULL;
		return false;
	}
	// Open interface (only 0404-API):
	if (_j2534->libraryAPIversion() != J2534_API_version::v0202)
	{
		_DeviceID = 0;
		ret = _j2534->PassThruOpen(NULL, &_DeviceID);
		if (STATUS_NOERROR != ret)
		{
			rememberError("PassThruOpen", ret);
#ifdef __FSSM_DEBUG__
			printErrorDescription("PassThruOpen() failed: ", ret);
#endif
			delete _j2534;
			_j2534 = NULL;
			return false;
		}
	}
	// Read hardware/software information:
	char FirmwareVersion[80] = {0,};
	char DllVersion[80] = {0,};
	char ApiVersion[80] = {0,};
	if (_j2534->libraryAPIversion() == J2534_API_version::v0202)
		ret = _j2534->PassThruReadVersion(FirmwareVersion, DllVersion, ApiVersion);
	else
		ret = _j2534->PassThruReadVersion(_DeviceID, FirmwareVersion, DllVersion, ApiVersion);
	if (STATUS_NOERROR == ret)
	{
		setVersion(std::string(FirmwareVersion) + " (DLL: " + std::string(DllVersion) + ", API: " + std::string(ApiVersion) +")");
#ifdef __FSSM_DEBUG__
		std::cout << "Interface information:\n";
		std::cout << "   Firmware version: " << FirmwareVersion << '\n';
		std::cout << "   DLL version:      " << DllVersion << '\n';
		std::cout << "   API version:      " << ApiVersion << '\n';
#endif
	}
	else
		rememberError("PassThruReadVersion", ret);
#ifdef __FSSM_DEBUG__
	if (STATUS_NOERROR != ret)
		printErrorDescription("PassThruReadVersion() failed: ", ret);
#endif
	// Get and save library data:
	for (const J2534Library& lib : J2534_API::getAvailableJ2534Libs())
	{
		if (lib.path == name)
		{
			// Interface name
			setName(lib.name);
			// Supported protocols
			std::vector<protocol_type> supportedProtocols;
			const J2534_protocol_flags p = lib.protocols;
			if (bool(p & J2534_protocol_flags::iso9141) ||
				bool(p & J2534_protocol_flags::iso14230))
			{
				supportedProtocols.push_back(protocol_type::SSM2_ISO14230);
			}
			if (bool(p & J2534_protocol_flags::iso15765))
				supportedProtocols.push_back(protocol_type::SSM2_ISO15765);
			if (bool(p & J2534_protocol_flags::iso9141) ||
			    bool(p & J2534_protocol_flags::iso14230))
			{
				supportedProtocols.push_back(protocol_type::SSM3_ISO14230);
			}
			setSupportedProtocols(supportedProtocols);
			break;
		}
	}
	return true;
}


bool J2534DiagInterface::isOpen()
{
	return (_j2534 != NULL);
}


bool J2534DiagInterface::close()
{
	long ret = 0;

	if (_j2534 == NULL)
	{
		setLastError("The J2534 interface is not open.");
		return false;
	}
	if (_connected)
		disconnect();
	// Close interface (only 0404-API):
	if (_j2534->libraryAPIversion() != J2534_API_version::v0202)
	{
		ret = _j2534->PassThruClose(_DeviceID);
		if (STATUS_NOERROR != ret)
		{
			rememberError("PassThruClose", ret);
#ifdef __FSSM_DEBUG__
			printErrorDescription("PassThruClose() failed: ", ret);
#endif
			return false;
		}
	}
	// Clean up:
	delete _j2534;
	_j2534 = NULL;
	setName("J2534 Pass-Through");
	setVersion("");
	return true;
}


bool J2534DiagInterface::connect(AbstractDiagInterface::protocol_type protocol)
{
	unsigned long ProtocolID = 0;
	unsigned long Flags = 0;
	unsigned long BaudRate = 0;
	long ret = 0;
	setLastError("");

	if (_j2534 == NULL)
	{
		setLastError("The J2534 interface is not open.");
		return false;
	}
	if (_connected)
	{
		setLastError("A J2534 protocol channel is already connected.");
		return false;
	}
	// CHECK PROTOCOL AND SET UP PARAMETERS
	if ((protocol == AbstractDiagInterface::protocol_type::SSM2_ISO14230) ||
	    (protocol == AbstractDiagInterface::protocol_type::SSM3_ISO14230))
	{
		ProtocolID = ISO9141; // also: ISO14230
		Flags = ISO9141_NO_CHECKSUM;
		BaudRate =
			(protocol == AbstractDiagInterface::protocol_type::SSM3_ISO14230) ? 10400 : 4800;
	}
	else if (protocol == AbstractDiagInterface::protocol_type::SSM2_ISO15765)
	{
		ProtocolID = ISO15765;
		Flags = 0;
		BaudRate = 500000;
	}
	else
	{
		setLastError("The selected Subaru protocol is not implemented by the J2534 backend.");
#ifdef __FSSM_DEBUG__
		std::cout << "Error: selected protocol is not supported\n";
#endif
		return false;
	}
	// CONNECT CHANNEL:
	if (_j2534->libraryAPIversion() == J2534_API_version::v0202)
		ret = _j2534->PassThruConnect(ProtocolID, Flags, &_ChannelID);
	else
		ret = _j2534->PassThruConnect(_DeviceID, ProtocolID, Flags, BaudRate, &_ChannelID);
	if (STATUS_NOERROR != ret)
	{
		rememberError("PassThruConnect", ret);
#ifdef __FSSM_DEBUG__
		printErrorDescription("PassThruConnect() failed: ", ret);
#endif
		return false;
	}
	/* ----- SET CONFIGURATION ----- */
	SCONFIG_LIST Input;
	SCONFIG CfgItems[1];
	// Echo (MANDATORY):
	CfgItems[0].Parameter = LOOPBACK;   // Echo off/on
	if ((protocol == AbstractDiagInterface::protocol_type::SSM2_ISO14230) ||
	    (protocol == AbstractDiagInterface::protocol_type::SSM3_ISO14230))
		CfgItems[0].Value = ON;
	else
		CfgItems[0].Value = OFF;
	Input.NumOfParams = 1;
	Input.ConfigPtr = CfgItems;
	ret = _j2534->PassThruIoctl(_ChannelID, SET_CONFIG, (void *)&Input, (void *)NULL);
	if (STATUS_NOERROR != ret)
	{
		rememberError("PassThruIoctl(SET_CONFIG/LOOPBACK)", ret);
#ifdef __FSSM_DEBUG__
		printErrorDescription("PassThruIoctl() for parameter LOOPBACK failed: ", ret);
#endif
		goto err_close;
	}
	// Baudrate (MANDATORY for 02.02-API only):
	CfgItems[0].Parameter = DATA_RATE;
	CfgItems[0].Value = BaudRate;
	Input.NumOfParams = 1;
	Input.ConfigPtr = CfgItems;
	ret = _j2534->PassThruIoctl(_ChannelID, SET_CONFIG, (void *)&Input, (void *)NULL);
	if (STATUS_NOERROR != ret)
	{
#ifdef __FSSM_DEBUG__
		printErrorDescription("PassThruIoctl() for parameter DATARATE failed: ", ret);
#endif
		if (_j2534->libraryAPIversion() == J2534_API_version::v0202)
		{
			rememberError("PassThruIoctl(SET_CONFIG/DATA_RATE)", ret);
			goto err_close;
		}
	}
	if ((protocol == AbstractDiagInterface::protocol_type::SSM2_ISO14230) ||
	    (protocol == AbstractDiagInterface::protocol_type::SSM3_ISO14230))
	{
		/* ----- SET CONFIGURATION (ISO-14230 specific) ----- */
		// P1_MIN (min. ECU inter-byte time)
		if (_j2534->libraryAPIversion() == J2534_API_version::v0202)	// 04.04-API: not adjustable, always 0ms
		{
			CfgItems[0].Parameter = P1_MIN;	// ISO-9141, ISO-14230 (normal timing paramter-set): min/def=0ms,
			CfgItems[0].Value = 0;	// [ms]
			Input.NumOfParams = 1;
			Input.ConfigPtr = CfgItems;
			ret = _j2534->PassThruIoctl(_ChannelID, SET_CONFIG, (void *)&Input, (void *)NULL);
#ifdef __FSSM_DEBUG__
			if (STATUS_NOERROR != ret)
				printErrorDescription("PassThruIoctl() for P1_MIN: ", ret);
#endif
		}
		// P1_MAX (max. ECU inter-byte time)
		CfgItems[0].Parameter = P1_MAX;	// ISO-9141, ISO-14230 (normal timing paramter-set): def/max=20ms,
		if (_j2534->libraryAPIversion() == J2534_API_version::v0202)
			CfgItems[0].Value = 5;	// [02.02-API: ms]
		else
			CfgItems[0].Value = 10;	// [04.04-API: *0.5ms]
		Input.NumOfParams = 1;
		Input.ConfigPtr = CfgItems;
		ret = _j2534->PassThruIoctl(_ChannelID, SET_CONFIG, (void *)&Input, (void *)NULL);
#ifdef __FSSM_DEBUG__
		if (STATUS_NOERROR != ret)
			printErrorDescription("PassThruIoctl() for P1_MAX: ", ret);
#endif
		// P2_MIN (min. ECU response time [ms] to a tester request or between ECU responses)
		if (_j2534->libraryAPIversion() == J2534_API_version::v0202)	// 04.04-API: not adjustable, always 0ms
		{
			CfgItems[0].Parameter = P2_MIN;	// ISO-9141, ISO-14230 (normal timing paramter-set): min=0ms, def=25ms
			CfgItems[0].Value = 0;	// [ms]
			Input.NumOfParams = 1;
			Input.ConfigPtr = CfgItems;
			ret = _j2534->PassThruIoctl(_ChannelID, SET_CONFIG, (void *)&Input, (void *)NULL);
#ifdef __FSSM_DEBUG__
			if (STATUS_NOERROR != ret)
				printErrorDescription("PassThruIoctl() for P1_MIN: ", ret);
#endif
		}
		// P2_MAX (max. ECU response time [ms] to a tester request or between ECU responses)
		if (_j2534->libraryAPIversion() == J2534_API_version::v0202)	// 04.04-API: not adjustable, value is ignored (all messages up to P3_min are accepted)
		{
			CfgItems[0].Parameter = P2_MAX;	// ISO-9141, ISO-14230 (normal timing paramter set): def=50ms, max=inf
			CfgItems[0].Value = 3000;
			Input.NumOfParams = 1;
			Input.ConfigPtr = CfgItems;
			ret = _j2534->PassThruIoctl(_ChannelID, SET_CONFIG, (void *)&Input, (void *)NULL);
#ifdef __FSSM_DEBUG__
			if (STATUS_NOERROR != ret)
				printErrorDescription("PassThruIoctl() for P2_MAX: ", ret);
#endif
		}
		// P3_MIN (min. time between end of ECU reponse and next tester request)
		CfgItems[0].Parameter = P3_MIN;     // ISO-9141, ISO-14230: min=0ms, def=55ms
		CfgItems[0].Value = 0;	// [02.02-API: ms, 04.04-API: *0.5ms]
		Input.NumOfParams = 1;
		Input.ConfigPtr = CfgItems;
		ret = _j2534->PassThruIoctl(_ChannelID, SET_CONFIG, (void *)&Input, (void *)NULL);
#ifdef __FSSM_DEBUG__
		if (STATUS_NOERROR != ret)
			printErrorDescription("PassThruIoctl() for P3_MIN: ", ret);
#endif
		// P3_MAX (max. time between end of ECU reponse and next tester request)
		if (_j2534->libraryAPIversion() == J2534_API_version::v0202)	// 04.04-API: not adjustable, tester allows sending message at any time after P3_MIN
		{
			CfgItems[0].Parameter = P3_MAX;     // ISO-9141, ISO-14230: def=5000ms, max=inf
			CfgItems[0].Value = 0xffff; // [ms] => 65535ms = max. value
			Input.NumOfParams = 1;
			Input.ConfigPtr = CfgItems;
			ret = _j2534->PassThruIoctl(_ChannelID, SET_CONFIG, (void *)&Input, (void *)NULL);
#ifdef __FSSM_DEBUG__
			if (STATUS_NOERROR != ret)
				printErrorDescription("PassThruIoctl() for P3_MIN: ", ret);
#endif
		}
		// P4_MIN (min. tester inter-byte time)
		CfgItems[0].Parameter = P4_MIN;	// ISO-9141, ISO-14230: min=0ms, def=5ms
		CfgItems[0].Value = 0;	// [02.02-API: ms, 04.04-API: *0.5ms]
		Input.NumOfParams = 1;
		Input.ConfigPtr = CfgItems;
		ret = _j2534->PassThruIoctl(_ChannelID, SET_CONFIG, (void *)&Input, (void *)NULL);
#ifdef __FSSM_DEBUG__
		if (STATUS_NOERROR != ret)
			printErrorDescription("PassThruIoctl() for P4_MIN: ", ret);
#endif
		// P4_MAX (max. tester inter-byte time)
		if (_j2534->libraryAPIversion() == J2534_API_version::v0202)	// 04.04-API: not adjustable, device always uses P4_MIN
		{
			CfgItems[0].Parameter = P4_MAX;	// ISO-9141, ISO-14230: def/max=50ms
			CfgItems[0].Value = 5;	// [ms]
			Input.NumOfParams = 1;
			Input.ConfigPtr = CfgItems;
			ret = _j2534->PassThruIoctl(_ChannelID, SET_CONFIG, (void *)&Input, (void *)NULL);
#ifdef __FSSM_DEBUG__
			if (STATUS_NOERROR != ret)
				printErrorDescription("PassThruIoctl() for P4_MAX: ", ret);
#endif
		}
		// Data bits:
		if (_j2534->libraryAPIversion() == J2534_API_version::v0404)
		{
			CfgItems[0].Parameter = DATA_BITS;
			CfgItems[0].Value = DATA_BITS_8;	// should be default
			Input.NumOfParams = 1;
			Input.ConfigPtr = CfgItems;
			ret = _j2534->PassThruIoctl(_ChannelID, SET_CONFIG, (void *)&Input, (void *)NULL);
#ifdef __FSSM_DEBUG__
			if (STATUS_NOERROR != ret)
				printErrorDescription("PassThruIoctl() for parameter DATA_BITS failed: ", ret);
#endif
		}
		// Parity:
		CfgItems[0].Parameter = PARITY;
		CfgItems[0].Value = NO_PARITY;
		Input.NumOfParams = 1;	// should be default
		Input.ConfigPtr = CfgItems;
		ret = _j2534->PassThruIoctl(_ChannelID, SET_CONFIG, (void *)&Input, (void *)NULL);
#ifdef __FSSM_DEBUG__
		if (STATUS_NOERROR != ret)
			printErrorDescription("PassThruIoctl() for parameter PARITY failed: ", ret);
#endif
		/* APPLY MESSAGE FILTER (Receive all messages) */
		PASSTHRU_MSG MaskMsg;
		PASSTHRU_MSG PatternMsg;
		memset(&MaskMsg, 0, sizeof(MaskMsg));		// .Data=0-array means "do not examine any bits"
		memset(&PatternMsg, 0, sizeof(PatternMsg));	// .Data must be zero, if no Data bits are examined
		MaskMsg.DataSize = 1;
		MaskMsg.ProtocolID = ISO9141;
		PatternMsg.DataSize = 1;
		PatternMsg.ProtocolID = ISO9141;
		ret = _j2534->PassThruStartMsgFilter(_ChannelID, PASS_FILTER, &MaskMsg, &PatternMsg, NULL, _FilterID);
		if (STATUS_NOERROR != ret)
		{
			rememberError("PassThruStartMsgFilter(ISO9141)", ret);
#ifdef __FSSM_DEBUG__
			printErrorDescription("PassThruStartMsgFilter() for ISO-14230 failed: ", ret);
#endif
			goto err_close;
		}
		_numFilters = 1;
	}
	else if (protocol == AbstractDiagInterface::protocol_type::SSM2_ISO15765)
	{
		// NOTE: also tweak values of ISO15765_BS, ISO15765_STMIN ?
		/* APPLY MESSAGE FILTERS */
		PASSTHRU_MSG MaskMsg;
		PASSTHRU_MSG PatternMsg;
		PASSTHRU_MSG FlowCtrlMsg;
		memset(&MaskMsg, 0, sizeof(MaskMsg));
		memset(&PatternMsg, 0, sizeof(PatternMsg));
		memset(&FlowCtrlMsg, 0, sizeof(FlowCtrlMsg));
		// ECU:
		MaskMsg.Data[0] = '\xFF';
		MaskMsg.Data[1] = '\xFF';
		MaskMsg.Data[2] = '\xFF';
		MaskMsg.Data[3] = '\xFF';
		MaskMsg.DataSize = 4;
		MaskMsg.ProtocolID = ISO15765;
		MaskMsg.TxFlags = ISO15765_FRAME_PAD;
		PatternMsg.Data[0] =  '\x00';
		PatternMsg.Data[1] =  '\x00';
		PatternMsg.Data[2] =  '\x07';
		PatternMsg.Data[3] =  '\xE8';
		PatternMsg.DataSize = 4;
		PatternMsg.ProtocolID = ISO15765;
		PatternMsg.TxFlags = ISO15765_FRAME_PAD;
		FlowCtrlMsg.Data[0] = '\x00';
		FlowCtrlMsg.Data[1] = '\x00';
		FlowCtrlMsg.Data[2] = '\x07';
		FlowCtrlMsg.Data[3] = '\xE0';
		FlowCtrlMsg.DataSize = 4;
		FlowCtrlMsg.ProtocolID = ISO15765;
		FlowCtrlMsg.TxFlags = ISO15765_FRAME_PAD;
		ret = _j2534->PassThruStartMsgFilter(_ChannelID, FLOW_CONTROL_FILTER, &MaskMsg, &PatternMsg, &FlowCtrlMsg, _FilterID + _numFilters);
		if (STATUS_NOERROR != ret)
		{
			rememberError("PassThruStartMsgFilter(ISO15765/ECU)", ret);
#ifdef __FSSM_DEBUG__
			printErrorDescription("PassThruStartMsgFilter() #1 for ISO-15765 failed: ", ret);
#endif
			goto err_close;
		}
		_numFilters = 1;
		// TCU:
		PatternMsg.Data[3] =  '\xE9';
		FlowCtrlMsg.Data[3] = '\xE1';
		ret = _j2534->PassThruStartMsgFilter(_ChannelID, FLOW_CONTROL_FILTER, &MaskMsg, &PatternMsg, &FlowCtrlMsg, _FilterID + _numFilters);
		if (STATUS_NOERROR != ret)
		{
			rememberError("PassThruStartMsgFilter(ISO15765/TCU)", ret);
#ifdef __FSSM_DEBUG__
			printErrorDescription("PassThruStartMsgFilter() #2 for ISO-15765 failed: ", ret);
#endif
			// Clean up configured message filter
			ret = _j2534->PassThruStopMsgFilter(_ChannelID, _FilterID[0]);
#ifdef __FSSM_DEBUG__
			if (STATUS_NOERROR != ret)
				printErrorDescription("PassThruStopMsgFilter() for ISO-15765 failed: ", ret);
#endif
			_numFilters = 0;
			goto err_close;
		}
		_numFilters = 2;
		// TODO: add support for additional addresses (requires layer/API changes)
	}
	_connected = true;
	setProtocolType( protocol );
	setProtocolBaudrate( BaudRate );
	return true;

err_close:
#ifdef __FSSM_DEBUG__
	ret = _j2534->PassThruDisconnect(_ChannelID);
	if (STATUS_NOERROR != ret)
		printErrorDescription("PassThruDisconnect() failed: ", ret);
#else
	_j2534->PassThruDisconnect(_ChannelID);
#endif
	return false;
}


bool J2534DiagInterface::isConnected()
{
	return _connected;
}


bool J2534DiagInterface::disconnect()
{
	long ret = 0;

	if (!_connected)
	{
		setLastError("No J2534 protocol channel is connected.");
		return false;
	}
	// Remove filters:
	for (unsigned char k=0; k<_numFilters; k++)
	{
		ret = _j2534->PassThruStopMsgFilter(_ChannelID, _FilterID[k]);
#ifdef __FSSM_DEBUG__
		if (STATUS_NOERROR != ret)
			printErrorDescription("PassThruStopMsgFilter() failed: ", ret);
#endif
	}
	_numFilters = 0;
	// Disconnect channel:
	ret = _j2534->PassThruDisconnect(_ChannelID);
	if (STATUS_NOERROR != ret)
	{
		rememberError("PassThruDisconnect", ret);
#ifdef __FSSM_DEBUG__
		printErrorDescription("PassThruDisconnect() failed: ", ret);
#endif
		return false;
	}
	_connected = false;
	setProtocolType( protocol_type::NONE );
	setProtocolBaudrate( 0 );
	return true;
}


bool J2534DiagInterface::read(std::vector<char> *buffer)
{
	const unsigned long num_PTMSGS = 8; // Nr. of PASSTHRU_MSGs per PassThruReadMsgs() call
	std::vector<char> readbuffer;
	long ret = 0;

	if (!_connected)
	{
		setLastError("No J2534 protocol channel is connected.");
		return false;
	}
	// Setup message-container:
	PASSTHRU_MSG *rx_msgs = new(std::nothrow) PASSTHRU_MSG[num_PTMSGS];
	if (rx_msgs == NULL)
		return false;
	memset(rx_msgs, 0, num_PTMSGS * sizeof(PASSTHRU_MSG));
	for (unsigned long i=0; i<num_PTMSGS; i++)
	{
		if ((protocolType() == AbstractDiagInterface::protocol_type::SSM2_ISO14230) ||
		    (protocolType() == AbstractDiagInterface::protocol_type::SSM3_ISO14230))
		{
			rx_msgs[i].ProtocolID = ISO9141;
		}
		else if (protocolType() == AbstractDiagInterface::protocol_type::SSM2_ISO15765)
		{
			rx_msgs[i].ProtocolID = ISO15765;
		}
		else
		{
			delete[] rx_msgs;
			return false;
		}
	}
	// Read all available messages:
	unsigned long rxNumMsgs;
	const unsigned long timeout = 0;	// return immediately
	do
	{
		rxNumMsgs = num_PTMSGS;
		ret = _j2534->PassThruReadMsgs(_ChannelID, rx_msgs, &rxNumMsgs, timeout);
		// NOTE: even with timeout=0 ERR_TIMEOUT can be returned (if at least 1 but less messages than requested have been read)
		if (((STATUS_NOERROR == ret) || (ERR_TIMEOUT == ret)) && rxNumMsgs)
		{
#ifdef __FSSM_DEBUG__
			std::cout << "PassThruReadMsgs(): received " << rxNumMsgs << " J2534-messages:" << std::endl;
#endif
			// Process received messages:
			for (unsigned long i=0; i<rxNumMsgs; i++)
			{
				if (rx_msgs[i].DataSize > sizeof(rx_msgs[i].Data))
				{
					setLastError("PassThruReadMsgs returned a message larger than the J2534 buffer.");
					delete[] rx_msgs;
					return false;
				}
#ifdef __FSSM_DEBUG__
				std::cout << "  PASSTHRU_MSG #" << i
						<< ": protocol id 0x" << std::hex << rx_msgs[i].ProtocolID << ", rx status 0x" << rx_msgs[i].RxStatus
						<< ", extra data index " << std::dec << rx_msgs[i].ExtraDataIndex << ":\n"
						<< libFSSM::StrToMultiLineHexstr(rx_msgs[i].Data, rx_msgs[i].DataSize, 16, "  ");
#endif
				if (rx_msgs[i].RxStatus & TX_MSG_TYPE)
				{
#ifdef __FSSM_DEBUG__
					if (rx_msgs[i].RxStatus & TX_DONE)	// SAE J2534-1 (dec 2004): ISO-15765 only
					{
						std::cout << "  => message is transmit confirmation message\n";
						continue;
					}
					else
						std::cout << "  => message is loopback message\n";
#endif
				}
				else
				{
					if (rx_msgs[i].RxStatus & START_OF_MESSAGE)
					{
						/* NOTE:
						* - incoming (multi-frame) msg transfer has commenced (ISO-15765) /
						*   first byte of an incoming message has been received (ISO-9141 / ISO-14230)
						* - ISO-15765: message contains CAN-ID only                                   */
#ifdef __FSSM_DEBUG__
						std::cout << "  => message indicates that an incoming message transfer has commenced.\n";
#endif
						continue;
					}
					if ((protocolType() == protocol_type::SSM2_ISO15765) && (rx_msgs[i].RxStatus & ISO15765_PADDING_ERROR))
					{
						// NOTE: ISO-15765 CAN frame was received with less than 8 data bytes
#ifdef __FSSM_DEBUG__
						std::cout << "  => message indicates ISO15765 padding error.\n";
#endif
						continue;
					}
					// NOTE: all other flags do not affect the transferred data or are not defined for the ISO-protocols
				}
				// Extract data:
				if (rx_msgs[i].DataSize > 0)
				{
#ifdef __FSSM_DEBUG__
					if ((rx_msgs[i].ExtraDataIndex == 0) && (_j2534->libraryAPIversion() == J2534_API_version::v0404))
					{
						std::cout << "  WARNING: ExtraDataIndex is 0, which should be the case only for pure status messages !\n";
					}
					else if (rx_msgs[i].ExtraDataIndex < rx_msgs[i].DataSize)
					{
						if ((_j2534->libraryAPIversion() == J2534_API_version::v0404) ||
							((protocolType() != protocol_type::SSM2_ISO14230) &&
							 (protocolType() != protocol_type::SSM3_ISO14230)) ||
							(((protocolType() == protocol_type::SSM2_ISO14230) ||
							  (protocolType() == protocol_type::SSM3_ISO14230)) &&
							 (rx_msgs[i].ExtraDataIndex < (rx_msgs[i].DataSize - 1))))
							std::cout << "  WARNING: ExtraDataIndex is smaller than expected !\n";
						/* NOTE:
						* - 04.04-API: (SAE-J2534-1, dec 2004): ExtraDataIndex only used with J1850 PWM
						* - 02.02-API: (SAE-J2534, feb 2002):   ExtraDataIndex also used with J1850 VPW, ISO-9141, ISO-14230 */
					}
#endif
					readbuffer.insert(readbuffer.end(), rx_msgs[i].Data, rx_msgs[i].Data + rx_msgs[i].DataSize);
					/* NOTE: at least for SSM2 via ISO-14230 we can't assume that a message from the control unit
						* is delivered in a single PASSTHRU_MSG. The used timings exceed the limits defined in
						* ISO-14230, so interfaces can't always detect message ends/starts via timeouts properly.
						*/
				}
			}
		}
	} while ((STATUS_NOERROR == ret) && rxNumMsgs);
	if ((STATUS_NOERROR == ret) || (ERR_BUFFER_EMPTY == ret) || (ERR_TIMEOUT == ret))
	{
		buffer->assign(readbuffer.begin(), readbuffer.end());
		delete[] rx_msgs;
		return true;
	}
#ifdef __FSSM_DEBUG__
	else
		printErrorDescription("PassThruReadMsgs() failed: ", ret);
#endif
	rememberError("PassThruReadMsgs", ret);
	delete[] rx_msgs;
	return false;
}


bool J2534DiagInterface::write(std::vector<char> buffer)
{
	unsigned long txNumMsgs = 1;
	unsigned long timeout = 1000;	// wait until message has been transmitted
	long ret = 0;

	if (!_connected)
	{
		setLastError("No J2534 protocol channel is connected.");
		return false;
	}
	// Setup message:
	PASSTHRU_MSG tx_msg;
	memset(&tx_msg, 0, sizeof(tx_msg));
	switch(protocolType())
	{
		case AbstractDiagInterface::protocol_type::SSM2_ISO14230:
		case AbstractDiagInterface::protocol_type::SSM3_ISO14230:
			tx_msg.ProtocolID = ISO9141;
			break;
		case AbstractDiagInterface::protocol_type::SSM2_ISO15765:
			tx_msg.ProtocolID = ISO15765;
			tx_msg.TxFlags = ISO15765_FRAME_PAD;
			break;
		default:
			setLastError("The active J2534 protocol cannot transmit this message.");
			return false;
	}
	if (buffer.size() > sizeof(tx_msg.Data))
	{
		setLastError("The outgoing J2534 message exceeds the PassThru buffer size.");
		return false;
	}
	std::copy(buffer.begin(), buffer.end(), tx_msg.Data);
	tx_msg.DataSize = buffer.size();
	// Send message:
	ret = _j2534->PassThruWriteMsgs(_ChannelID, &tx_msg, &txNumMsgs, timeout);
	if (ret != STATUS_NOERROR)
	{
		rememberError("PassThruWriteMsgs", ret);
#ifdef __FSSM_DEBUG__
		printErrorDescription("PassThruWriteMsgs() failed: ", ret);
#endif
		return false;
	}

	return true;
}


bool J2534DiagInterface::clearSendBuffer()
{
	if (!_connected)
	{
		setLastError("No J2534 protocol channel is connected.");
		return false;
	}
	long ret = _j2534->PassThruIoctl(_ChannelID, CLEAR_TX_BUFFER, (void *)NULL, (void *)NULL);
	if (STATUS_NOERROR != ret)
	{
		rememberError("PassThruIoctl(CLEAR_TX_BUFFER)", ret);
#ifdef __FSSM_DEBUG__
		printErrorDescription("PassThruIoctl() for parameter CLEAR_TX_BUFFER failed: ", ret);
#endif
		return false;
	}
	return true;
}


bool J2534DiagInterface::clearReceiveBuffer()
{
	if (!_connected)
	{
		setLastError("No J2534 protocol channel is connected.");
		return false;
	}
	long ret = _j2534->PassThruIoctl(_ChannelID, CLEAR_RX_BUFFER, (void *)NULL, (void *)NULL);
	if (STATUS_NOERROR != ret)
	{
		rememberError("PassThruIoctl(CLEAR_RX_BUFFER)", ret);
#ifdef __FSSM_DEBUG__
		printErrorDescription("PassThruIoctl() for parameter CLEAR_RX_BUFFER failed: ", ret);
#endif
		return false;
	}
	return true;
}

// Private

std::string J2534DiagInterface::errorDescription(const std::string& operation, long ret)
{
	std::ostringstream message;
	message << operation << " failed: " << j2534StatusName(ret) << " (" << std::dec << ret;
	if (ret >= 0)
		message << "/0x" << std::uppercase << std::hex << ret;
	message << ")";

	if (((ret > 0) || (ret == J2534API_ERROR_BROKER_TRANSPORT)) && _j2534)
	{
		char driverDescription[256] = {0,};
		_j2534->PassThruGetLastError(driverDescription);
		if (driverDescription[0])
			message << ": " << driverDescription;
	}
	return message.str();
}


void J2534DiagInterface::rememberError(const std::string& operation, long ret)
{
	setLastError(errorDescription(operation, ret));
}

#ifdef __FSSM_DEBUG__
void J2534DiagInterface::printErrorDescription(std::string title, long ret)
{
	std::cout << errorDescription(title, ret) << std::endl;
}
#endif
