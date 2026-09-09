/*
 * J2534_API.cpp - API for accessing SAE-J2534 compliant interfaces
 *
 * Copyright (C) 2009-2010 Comer352l
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

#include "J2534_API.h"
#include <algorithm>
#include <cctype>
#include <sstream>


namespace
{
	template<typename T>
	T resolve(HINSTANCE library, const char *name)
	{
		FARPROC raw = GetProcAddress(library, name);
		T result = NULL;
		static_assert(sizeof(result) == sizeof(raw), "unexpected Windows function pointer size");
		memcpy(&result, &raw, sizeof(result));
		return result;
	}

	std::string windowsErrorMessage(DWORD error)
	{
		char *buffer = NULL;
		const DWORD size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		                                  NULL, error, 0, reinterpret_cast<char *>(&buffer), 0, NULL);
		std::string message = size && buffer ? std::string(buffer, size) : std::string();
		if (buffer)
			LocalFree(buffer);
		while (!message.empty() && ((message.back() == '\r') || (message.back() == '\n') || (message.back() == ' ')))
			message.pop_back();
		return message;
	}

	std::string registryString(const unsigned char *data, DWORD dataSize, DWORD dataType)
	{
		size_t length = 0;
		while ((length < dataSize) && data[length])
			++length;
		const std::string value(reinterpret_cast<const char *>(data), length);
		if ((dataType != REG_EXPAND_SZ) || value.empty())
			return value;

		const DWORD required = ExpandEnvironmentStringsA(value.c_str(), NULL, 0);
		if (!required)
			return value;
		std::vector<char> expanded(required, '\0');
		return ExpandEnvironmentStringsA(value.c_str(), expanded.data(), required) ? std::string(expanded.data()) : value;
	}

	void addProtocolsFromString(const std::string& value, J2534_protocol_flags& protocols)
	{
		std::string token;
		for (size_t i = 0; i <= value.size(); ++i)
		{
			const unsigned char c = (i < value.size()) ? static_cast<unsigned char>(value[i]) : 0;
			if (std::isalnum(c) || (c == '_'))
				token.push_back(static_cast<char>(c));
			else if (!token.empty())
			{
				protocols = protocols | J2534misc::parseProtocol(token);
				token.clear();
			}
		}
	}

	void addLibraryIfUnique(const J2534Library& candidate, std::vector<J2534Library>& libraries)
	{
		for (J2534Library& existing : libraries)
		{
			if (_stricmp(existing.path.c_str(), candidate.path.c_str()) == 0)
			{
				existing.protocols = existing.protocols | candidate.protocols;
				if (existing.name.empty())
					existing.name = candidate.name;
				if (candidate.api == J2534_API_version::v0404)
					existing.api = candidate.api;
				if (candidate.compatibleWithApplication)
				{
					existing.architecture = candidate.architecture;
					existing.compatibleWithApplication = true;
				}
				return;
			}
		}
		libraries.push_back(candidate);
	}
}



J2534_API::J2534_API()
{
	_J2534LIB = NULL;
	_api_version = J2534_API_version::v0404;
	_PassThruOpen = NULL;
	_PassThruClose = NULL;
	_PassThruConnect_0202 = NULL;
	_PassThruConnect_0404 = NULL;
	_PassThruDisconnect = NULL;
	_PassThruReadVersion_0202 = NULL;
	_PassThruReadVersion_0404 = NULL;
	_PassThruGetLastError = NULL;
	_PassThruReadMsgs = NULL;
	_PassThruStartMsgFilter = NULL;
	_PassThruStopMsgFilter = NULL;
	_PassThruWriteMsgs = NULL;
	_PassThruStartPeriodicMsg = NULL;
	_PassThruStopPeriodicMsg = NULL;
	_PassThruIoctl = NULL;
	_PassThruSetProgrammingVoltage_0202 = NULL;
	_PassThruSetProgrammingVoltage_0404 = NULL;
	_last_error.clear();
}


J2534_API::~J2534_API()
{
	if (_J2534LIB)
	{
#ifdef __J2534_API_DEBUG__
		if (!FreeLibrary( _J2534LIB ))
			std::cout << "J2534interface::~J2534interface(): FreeLibrary() failed with error " << GetLastError() << "\n";
#else
		FreeLibrary( _J2534LIB );
#endif
	}
}


bool J2534_API::selectLibrary(std::string libPath)
{
	_last_error.clear();
	if (!libPath.size())
	{
		_last_error = "No J2534 library path was supplied.";
		return false;
	}
	HINSTANCE newJ2534LIB = NULL;
	SetLastError(ERROR_SUCCESS);
	newJ2534LIB = LoadLibraryA( libPath.c_str() );
	if (newJ2534LIB)
	{
		// Check if library is a valid J2534-library:
		const char *requiredFunctions[] = {
			"PassThruConnect", "PassThruDisconnect", "PassThruReadVersion", "PassThruGetLastError",
			"PassThruReadMsgs", "PassThruStartMsgFilter", "PassThruStopMsgFilter", "PassThruWriteMsgs", "PassThruIoctl"
		};
		std::string missingFunctions;
		for (const char *functionName : requiredFunctions)
		{
			if (!GetProcAddress(newJ2534LIB, functionName))
			{
				if (!missingFunctions.empty())
					missingFunctions += ", ";
				missingFunctions += functionName;
			}
		}
		if (!missingFunctions.empty())
		{
			_last_error = "The selected DLL is not a usable J2534 library. Missing exports: " + missingFunctions;
#ifdef __J2534_API_DEBUG__
			std::cout << "J2534interface::selectLibrary(): " << _last_error << "\n";
			if (!FreeLibrary( newJ2534LIB ))
				std::cout << "J2534interface::selectLibrary(): FreeLibrary() failed with error " << GetLastError() << "\n";
#else
			FreeLibrary( newJ2534LIB );
#endif
			return false;
		}
		// Check API-version of the library:
		if (!GetProcAddress( newJ2534LIB, "PassThruOpen" ) || !GetProcAddress( newJ2534LIB, "PassThruClose" ))
			_api_version = J2534_API_version::v0202;
		else
			_api_version = J2534_API_version::v0404;
		// Close old library:
		if (_J2534LIB)
		{
#ifdef __J2534_API_DEBUG__
			if (!FreeLibrary( _J2534LIB ))
				std::cout << "J2534interface::selectLibrary(): FreeLibrary() failed with error " << GetLastError() << "\n";
#else
			FreeLibrary( _J2534LIB );
#endif
		}
		// Save data:
		_J2534LIB = newJ2534LIB;
		_lib_path = libPath;
		assignJ2534fcns();
	}
	else
	{
		const DWORD error = GetLastError();
		std::ostringstream message;
		message << "LoadLibrary failed for '" << libPath << "' (Windows error " << error << ")";
		const std::string systemMessage = windowsErrorMessage(error);
		if (!systemMessage.empty())
			message << ": " << systemMessage;
		if (error == ERROR_BAD_EXE_FORMAT)
			message << ". The J2534 DLL architecture does not match this " << (sizeof(void *) == 8 ? "64-bit" : "32-bit") << " FreeSSM build.";
		_last_error = message.str();
	}
#ifdef __J2534_API_DEBUG__
	if (!newJ2534LIB)
		std::cout << "J2534interface::selectLibrary(): " << _last_error << "\n";
#endif
	return newJ2534LIB != NULL;
}


std::string J2534_API::library()
{
	if (_J2534LIB)
		return _lib_path;
	else
		return "";
}


J2534_API_version J2534_API::libraryAPIversion()
{
	return _api_version;
}


std::string J2534_API::lastError()
{
	return _last_error;
}


void J2534_API::assignJ2534fcns()
{
	_PassThruOpen = resolve<J2534_PassThruOpen>(_J2534LIB, "PassThruOpen");
	_PassThruClose = resolve<J2534_PassThruClose>(_J2534LIB, "PassThruClose");
	if (_api_version == J2534_API_version::v0202)
	{
		_PassThruConnect_0202 = resolve<J2534_PassThruConnect_0202>(_J2534LIB, "PassThruConnect");
		_PassThruReadVersion_0202 = resolve<J2534_PassThruReadVersion_0202>(_J2534LIB, "PassThruReadVersion");
		_PassThruSetProgrammingVoltage_0202 = resolve<J2534_PassThruSetProgrammingVoltage_0202>(_J2534LIB, "PassThruSetProgrammingVoltage");
		_PassThruConnect_0404 = NULL;
		_PassThruReadVersion_0404 = NULL;
		_PassThruSetProgrammingVoltage_0404 = NULL;
	}
	else
	{
		_PassThruConnect_0202 = NULL;
		_PassThruReadVersion_0202 = NULL;
		_PassThruSetProgrammingVoltage_0202 = NULL;
		_PassThruConnect_0404 = resolve<J2534_PassThruConnect_0404>(_J2534LIB, "PassThruConnect");
		_PassThruReadVersion_0404 = resolve<J2534_PassThruReadVersion_0404>(_J2534LIB, "PassThruReadVersion");
		_PassThruSetProgrammingVoltage_0404 = resolve<J2534_PassThruSetProgrammingVoltage_0404>(_J2534LIB, "PassThruSetProgrammingVoltage");
	}
	_PassThruDisconnect = resolve<J2534_PassThruDisconnect>(_J2534LIB, "PassThruDisconnect");
	_PassThruGetLastError = resolve<J2534_PassThruGetLastError>(_J2534LIB, "PassThruGetLastError");
	_PassThruReadMsgs = resolve<J2534_PassThruReadMsgs>(_J2534LIB, "PassThruReadMsgs");
	_PassThruStartMsgFilter = resolve<J2534_PassThruStartMsgFilter>(_J2534LIB, "PassThruStartMsgFilter");
	_PassThruStopMsgFilter = resolve<J2534_PassThruStopMsgFilter>(_J2534LIB, "PassThruStopMsgFilter");
	_PassThruWriteMsgs = resolve<J2534_PassThruWriteMsgs>(_J2534LIB, "PassThruWriteMsgs");
	_PassThruStartPeriodicMsg = resolve<J2534_PassThruStartPeriodicMsg>(_J2534LIB, "PassThruStartPeriodicMsg");
	_PassThruStopPeriodicMsg = resolve<J2534_PassThruStopPeriodicMsg>(_J2534LIB, "PassThruStopPeriodicMsg");
	_PassThruIoctl = resolve<J2534_PassThruIoctl>(_J2534LIB, "PassThruIoctl");
}


std::vector<J2534Library> J2534_API::getAvailableJ2534Libs()
{
	std::vector<J2534Library> PTlibraries;
	// J2534 drivers are frequently 32-bit even on 64-bit Windows. Search
	// both registry views and list entries compatible with this process first.
	if (sizeof(void *) == 8)
	{
		searchRegistryView(KEY_WOW64_64KEY, J2534_library_architecture::x64, PTlibraries);
		searchRegistryView(KEY_WOW64_32KEY, J2534_library_architecture::x86, PTlibraries);
	}
	else
	{
		searchRegistryView(KEY_WOW64_32KEY, J2534_library_architecture::x86, PTlibraries);
		searchRegistryView(KEY_WOW64_64KEY, J2534_library_architecture::x64, PTlibraries);
	}
#ifdef __J2534_API_DEBUG__
	J2534misc::printLibraryInfo(PTlibraries);
#endif
	return PTlibraries;
}


void J2534_API::searchRegistryView(REGSAM viewFlag, J2534_library_architecture architecture, std::vector<J2534Library>& PTlibs)
{
	HKEY softwareKey = NULL;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE", 0, KEY_READ | viewFlag, &softwareKey) != ERROR_SUCCESS)
		return;

	DWORD subKeyCount = 0;
	DWORD maxSubKeyLength = 0;
	if (RegQueryInfoKeyA(softwareKey, NULL, NULL, NULL, &subKeyCount, &maxSubKeyLength, NULL,
	                     NULL, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
	{
		RegCloseKey(softwareKey);
		return;
	}

	std::vector<char> keyName(maxSubKeyLength + 2, '\0');
	for (DWORD index = 0; index < subKeyCount; ++index)
	{
		DWORD keyNameLength = static_cast<DWORD>(keyName.size() - 1);
		if (RegEnumKeyExA(softwareKey, index, keyName.data(), &keyNameLength, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
			continue;
		keyName[keyNameLength] = '\0';
		if (_strnicmp(keyName.data(), "PassThruSupport", 15) != 0)
			continue;

		HKEY passThruKey = NULL;
		if (RegOpenKeyExA(softwareKey, keyName.data(), 0, KEY_READ | viewFlag, &passThruKey) == ERROR_SUCCESS)
		{
			searchLibValuesRecursive(passThruKey, viewFlag, architecture, PTlibs);
			RegCloseKey(passThruKey);
		}
	}
	RegCloseKey(softwareKey);
}


void J2534_API::searchLibValuesRecursive(HKEY hKey, REGSAM viewFlag, J2534_library_architecture architecture, std::vector<J2534Library>& PTlibs)
{
	DWORD subKeyCount = 0;
	DWORD maxSubKeyLength = 0;
	DWORD valueCount = 0;
	DWORD maxValueNameLength = 0;
	DWORD maxValueDataLength = 0;
	if (RegQueryInfoKeyA(hKey, NULL, NULL, NULL, &subKeyCount, &maxSubKeyLength, NULL,
	                     &valueCount, &maxValueNameLength, &maxValueDataLength, NULL, NULL) != ERROR_SUCCESS)
		return;

	J2534Library PTlib;
	PTlib.api = J2534_API_version::v0404;
	PTlib.architecture = architecture;
	PTlib.compatibleWithApplication = ((sizeof(void *) == 8) && (architecture == J2534_library_architecture::x64)) ||
	                                  ((sizeof(void *) == 4) && (architecture == J2534_library_architecture::x86));

	std::vector<char> valueName(maxValueNameLength + 2, '\0');
	std::vector<unsigned char> data(maxValueDataLength + 2, 0);
	for (DWORD index = 0; index < valueCount; ++index)
	{
		DWORD valueNameLength = static_cast<DWORD>(valueName.size() - 1);
		DWORD dataSize = static_cast<DWORD>(data.size() - 1);
		DWORD dataType = REG_NONE;
		if (RegEnumValueA(hKey, index, valueName.data(), &valueNameLength, NULL,
		                  &dataType, data.data(), &dataSize) != ERROR_SUCCESS)
			continue;
		valueName[valueNameLength] = '\0';
		data[std::min<DWORD>(dataSize, static_cast<DWORD>(data.size() - 1))] = 0;

		if ((dataType == REG_SZ) || (dataType == REG_EXPAND_SZ))
		{
			const std::string value = registryString(data.data(), dataSize, dataType);
			if (_stricmp(valueName.data(), "FunctionLibrary") == 0)
				PTlib.path = value;
			else if (_stricmp(valueName.data(), "Name") == 0)
				PTlib.name = value;
			else if (_stricmp(valueName.data(), "ProtocolsSupported") == 0)
			{
				PTlib.api = J2534_API_version::v0202;
				addProtocolsFromString(value, PTlib.protocols);
			}
		}
		else if ((dataType == REG_DWORD) && (dataSize >= sizeof(DWORD)))
		{
			DWORD enabled = 0;
			memcpy(&enabled, data.data(), sizeof(enabled));
			if (enabled)
				PTlib.protocols = PTlib.protocols | J2534misc::parseProtocol(valueName.data());
		}
	}

	if (!PTlib.path.empty())
	{
		if (PTlib.name.empty())
			PTlib.name = PTlib.path;
		addLibraryIfUnique(PTlib, PTlibs);
	}

	std::vector<char> keyName(maxSubKeyLength + 2, '\0');
	for (DWORD index = 0; index < subKeyCount; ++index)
	{
		DWORD keyNameLength = static_cast<DWORD>(keyName.size() - 1);
		if (RegEnumKeyExA(hKey, index, keyName.data(), &keyNameLength, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
			continue;
		keyName[keyNameLength] = '\0';

		HKEY childKey = NULL;
		if (RegOpenKeyExA(hKey, keyName.data(), 0, KEY_READ | viewFlag, &childKey) == ERROR_SUCCESS)
		{
			searchLibValuesRecursive(childKey, viewFlag, architecture, PTlibs);
			RegCloseKey(childKey);
		}
	}
}


long J2534_API::PassThruOpen(void* pName, unsigned long *pDeviceID)	// 0404-API only
{
	if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
	if (!_PassThruOpen) return J2534API_ERROR_FCN_NOT_SUPPORTED;
	return _PassThruOpen(pName, pDeviceID);
}


long J2534_API::PassThruClose(unsigned long DeviceID)			// 0404-API only
{
	if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
	if (!_PassThruClose) return J2534API_ERROR_FCN_NOT_SUPPORTED;
	return _PassThruClose(DeviceID);
}


long J2534_API::PassThruConnect(unsigned long ProtocolID, unsigned long Flags, unsigned long *pChannelID)	// 0202-API
{
	if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
	if (!_PassThruConnect_0202) return J2534API_ERROR_FCN_NOT_SUPPORTED;
	return _PassThruConnect_0202(ProtocolID, Flags, pChannelID);
}


long J2534_API::PassThruConnect(unsigned long DeviceID, unsigned long ProtocolID, unsigned long Flags, unsigned long BaudRate, unsigned long *pChannelID)	// 0404-API
{
	if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
	if (!_PassThruConnect_0404) return J2534API_ERROR_FCN_NOT_SUPPORTED;
	return _PassThruConnect_0404(DeviceID, ProtocolID, Flags, BaudRate, pChannelID);
}


long J2534_API::PassThruDisconnect(unsigned long ChannelID)
{
	if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
	if (!_PassThruDisconnect) return J2534API_ERROR_FCN_NOT_SUPPORTED;
	return _PassThruDisconnect(ChannelID);
}


long J2534_API::PassThruReadVersion(char *pFirmwareVersion, char * pDllVersion, char *pApiVersion)	// 0202-API
{
	if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
	if (!_PassThruReadVersion_0202) return J2534API_ERROR_FCN_NOT_SUPPORTED;
	return _PassThruReadVersion_0202(pFirmwareVersion, pDllVersion, pApiVersion);
}


long J2534_API::PassThruReadVersion(unsigned long DeviceID, char *pFirmwareVersion, char * pDllVersion, char *pApiVersion)	// 0404-API
{
	if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
	if (!_PassThruReadVersion_0404) return J2534API_ERROR_FCN_NOT_SUPPORTED;
	return _PassThruReadVersion_0404(DeviceID, pFirmwareVersion, pDllVersion, pApiVersion);
}


long J2534_API::PassThruGetLastError(char *pErrorDescription)
{
	if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
	if (!_PassThruGetLastError) return J2534API_ERROR_FCN_NOT_SUPPORTED;
	return _PassThruGetLastError(pErrorDescription);
}


long J2534_API::PassThruReadMsgs(unsigned long ChannelID, PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs, unsigned long Timeout)
{
	if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
	if (!_PassThruReadMsgs) return J2534API_ERROR_FCN_NOT_SUPPORTED;
	return _PassThruReadMsgs(ChannelID, pMsg, pNumMsgs, Timeout);
}


long J2534_API::PassThruStartMsgFilter(unsigned long ChannelID, unsigned long FilterType, PASSTHRU_MSG *pMaskMsg, PASSTHRU_MSG *pPatternMsg, PASSTHRU_MSG *pFlowControlMsg, unsigned long *pMsgID)
{
	if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
	if (!_PassThruStartMsgFilter) return J2534API_ERROR_FCN_NOT_SUPPORTED;
	return _PassThruStartMsgFilter(ChannelID, FilterType, pMaskMsg, pPatternMsg, pFlowControlMsg, pMsgID);
}


long J2534_API::PassThruStopMsgFilter(unsigned long ChannelID, unsigned long MsgID)
{
	if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
	if (!_PassThruStopMsgFilter) return J2534API_ERROR_FCN_NOT_SUPPORTED;
	return _PassThruStopMsgFilter(ChannelID, MsgID);
}


long J2534_API::PassThruWriteMsgs(unsigned long ChannelID, PASSTHRU_MSG *pMsg, unsigned long *pNumMsgs, unsigned long Timeout)
{
	if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
	if (!_PassThruWriteMsgs) return J2534API_ERROR_FCN_NOT_SUPPORTED;
	return _PassThruWriteMsgs(ChannelID, pMsg, pNumMsgs, Timeout);
}


long J2534_API::PassThruStartPeriodicMsg(unsigned long ChannelID, PASSTHRU_MSG *pMsg, unsigned long *pMsgID, unsigned long TimeInterval)
{
	if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
	if (!_PassThruStartPeriodicMsg) return J2534API_ERROR_FCN_NOT_SUPPORTED;
	return _PassThruStartPeriodicMsg(ChannelID, pMsg, pMsgID, TimeInterval);
}


long J2534_API::PassThruStopPeriodicMsg(unsigned long ChannelID, unsigned long MsgID)
{
	if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
	if (!_PassThruStopPeriodicMsg) return J2534API_ERROR_FCN_NOT_SUPPORTED;
	return _PassThruStopPeriodicMsg(ChannelID, MsgID);
}


long J2534_API::PassThruIoctl(unsigned long ChannelID, unsigned long IoctlID, void *pInput, void *pOutput)
{
	if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
	if (!_PassThruIoctl) return J2534API_ERROR_FCN_NOT_SUPPORTED;
	return _PassThruIoctl(ChannelID, IoctlID, pInput, pOutput);
}


long J2534_API::PassThruSetProgrammingVoltage(unsigned long PinNumber, unsigned long Voltage)	// 0202-API
{
	if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
	if (!_PassThruSetProgrammingVoltage_0202) return J2534API_ERROR_FCN_NOT_SUPPORTED;
	return _PassThruSetProgrammingVoltage_0202(PinNumber, Voltage);
}


long J2534_API::PassThruSetProgrammingVoltage(unsigned long DeviceID, unsigned long PinNumber, unsigned long Voltage)	// 0404-API
{
	if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
	if (!_PassThruSetProgrammingVoltage_0404) return J2534API_ERROR_FCN_NOT_SUPPORTED;
	return _PassThruSetProgrammingVoltage_0404(DeviceID, PinNumber, Voltage);
}
