/*
 * FiveEATLoggerDefinitions.cpp - Read-only RomRaider logger definition loader
 *
 * Copyright (C) 2026 Alonso Lopez
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "FiveEATLoggerDefinitions.h"

#include "tinyxml2/tinyxml2.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>


namespace
{

using namespace tinyxml2;


void setError(std::string *errorMessage, const std::string& message)
{
	if (errorMessage != NULL)
		*errorMessage = message;
}


bool parseUnsigned(const char *text, unsigned int *value)
{
	if ((text == NULL) || (value == NULL) || (*text == '\0'))
		return false;

	errno = 0;
	char *end = NULL;
	const unsigned long parsed = std::strtoul(text, &end, 0);
	if ((errno != 0) || (end == text) || (*end != '\0') ||
	    (parsed > std::numeric_limits<unsigned int>::max()))
		return false;

	*value = static_cast<unsigned int>(parsed);
	return true;
}


bool matchesUnitIdentifier(const char *identifiers, const std::string& expected)
{
	if (identifiers == NULL)
		return false;

	std::stringstream values(identifiers);
	std::string value;
	while (std::getline(values, value, ','))
	{
		if (FiveEATLoggerDefinitions::normalizeUnitIdentifier(value) == expected)
			return true;
	}
	return false;
}


bool convertExpression(const char *expression, std::string *formula)
{
	if ((expression == NULL) || (formula == NULL))
		return false;

	const std::string input(expression);
	if (input.empty() || (input.at(0) != 'x'))
		return false;

	std::size_t pos = 1;
	while (pos < input.size())
	{
		const char operation = input.at(pos++);
		if ((operation != '+') && (operation != '-') &&
		    (operation != '*') && (operation != '/'))
			return false;

		const std::size_t numberStart = pos;
		bool decimalPointSeen = false;
		while (pos < input.size())
		{
			const char current = input.at(pos);
			if (std::isdigit(static_cast<unsigned char>(current)))
			{
				++pos;
				continue;
			}
			if ((current == '.') && !decimalPointSeen)
			{
				decimalPointSeen = true;
				++pos;
				continue;
			}
			break;
		}
		if ((pos == numberStart) || (input.at(pos - 1) == '.'))
			return false;

		if (operation == '/')
		{
			const double divisor = std::strtod(input.substr(numberStart, pos - numberStart).c_str(), NULL);
			if (divisor == 0.0)
				return false;
		}
	}

	*formula = input.size() == 1 ? "*1" : input.substr(1);
	return true;
}


unsigned int precisionFromFormat(const char *format)
{
	if (format == NULL)
		return 0;
	const std::string value(format);
	const std::size_t decimalPoint = value.find('.');
	if (decimalPoint == std::string::npos)
		return 0;
	return std::min<unsigned int>(3, value.size() - decimalPoint - 1);
}


bool diagnosticParameter(const std::string& name, unsigned int *group, bool *confirmed)
{
	const std::string prefix = "DTC Group ";
	if (name.compare(0, prefix.size(), prefix) != 0)
		return false;

	const std::size_t suffixStart = name.find(' ', prefix.size());
	if (suffixStart == std::string::npos)
		return false;

	unsigned int parsedGroup = 0;
	if (!parseUnsigned(name.substr(prefix.size(), suffixStart - prefix.size()).c_str(), &parsedGroup))
		return false;

	const std::string suffix = name.substr(suffixStart + 1);
	if (suffix == "Current Bits")
		*confirmed = false;
	else if (suffix == "Confirmed Bits")
		*confirmed = true;
	else
		return false;

	*group = parsedGroup;
	return true;
}


} // namespace


std::string FiveEATLoggerDefinitions::normalizeUnitIdentifier(const std::string& value)
{
	std::string normalized;
	for (std::size_t i = 0; i < value.size(); ++i)
	{
		const unsigned char current = static_cast<unsigned char>(value.at(i));
		if (std::isxdigit(current))
			normalized.push_back(static_cast<char>(std::toupper(current)));
		else if (!std::isspace(current) && (current != ':') && (current != '-'))
			return "";
	}
	return normalized.size() == 10 ? normalized : "";
}


bool FiveEATLoggerDefinitions::load(const std::string& filename,
	                                 const std::string& unitIdentifier,
	                                 std::string *errorMessage)
{
	_parameters.clear();
	_diagnosticBlocks.clear();
	_definitionVersion.clear();
	_diagnosticProbe = false;
	_unitIdentifier = normalizeUnitIdentifier(unitIdentifier);
	if (_unitIdentifier.empty())
	{
		setError(errorMessage, "invalid 5EAT unit identifier");
		return false;
	}

	XMLDocument document;
	const XMLError loadResult = document.LoadFile(filename.c_str());
	if (loadResult != XML_SUCCESS)
	{
		setError(errorMessage, std::string("cannot load 5EAT logger XML: ") + document.ErrorStr());
		return false;
	}

	const XMLElement *logger = document.FirstChildElement("logger");
	if ((logger == NULL) || (logger->Attribute("version") == NULL))
	{
		setError(errorMessage, "invalid 5EAT logger root element");
		return false;
	}
	_definitionVersion = logger->Attribute("version");
	if (_definitionVersion != "5eat-tcu")
	{
		setError(errorMessage, "unsupported 5EAT logger version: " + _definitionVersion);
		return false;
	}

	const XMLElement *protocol = logger->FirstChildElement("protocol");
	if ((protocol == NULL) || (protocol->Attribute("id") == NULL) ||
	    (std::string(protocol->Attribute("id")) != "SSM"))
	{
		setError(errorMessage, "5EAT logger does not contain an SSM protocol");
		return false;
	}

	std::set<std::string> parameterIds;
	std::vector<Parameter> parameters;
	std::map<unsigned int, DiagnosticBlock> diagnosticBlocks;
	for (const XMLElement *element = protocol->FirstChildElement("ecuparam");
	     element != NULL; element = element->NextSiblingElement("ecuparam"))
	{
		const char *target = element->Attribute("target");
		if ((target != NULL) && (std::string(target) != "2"))
			continue;

		const XMLElement *matchedEcu = NULL;
		for (const XMLElement *ecu = element->FirstChildElement("ecu");
		     ecu != NULL; ecu = ecu->NextSiblingElement("ecu"))
		{
			if (matchesUnitIdentifier(ecu->Attribute("id"), _unitIdentifier))
			{
				matchedEcu = ecu;
				break;
			}
		}
		if (matchedEcu == NULL)
			continue;

		const char *id = element->Attribute("id");
		const char *name = element->Attribute("name");
		const XMLElement *addressElement = matchedEcu->FirstChildElement("address");
		const XMLElement *conversionElement = element->FirstChildElement("conversions");
		if (conversionElement != NULL)
			conversionElement = conversionElement->FirstChildElement("conversion");
		if ((id == NULL) || (name == NULL) || (addressElement == NULL) ||
		    (addressElement->GetText() == NULL) || (conversionElement == NULL))
		{
			setError(errorMessage, "incomplete 5EAT parameter for unit " + _unitIdentifier);
			return false;
		}

		Parameter parameter;
		parameter.id = id;
		parameter.name = name;
		const char *description = element->Attribute("desc");
		parameter.description = description == NULL ? "" : description;
		const char *unit = conversionElement->Attribute("units");
		parameter.unit = unit == NULL ? "" : unit;
		if (!parseUnsigned(addressElement->GetText(), &parameter.address))
		{
			setError(errorMessage, "invalid address in 5EAT parameter " + parameter.id);
			return false;
		}
		parameter.length = addressElement->UnsignedAttribute("length", 1);
		if ((parameter.length < 1) || (parameter.length > 2))
			continue;
		/* Direct RAM reads are intentionally excluded from this first, read-only
		 * integration. Only the standard Select Monitor table is accepted. */
		if ((parameter.address >= 0x200) ||
		    (parameter.address + parameter.length > 0x200))
			continue;
		if (!convertExpression(conversionElement->Attribute("expr"), &parameter.formula))
		{
			setError(errorMessage, "unsupported conversion in 5EAT parameter " + parameter.id);
			return false;
		}
		parameter.precision = precisionFromFormat(conversionElement->Attribute("format"));

		if (!parameterIds.insert(parameter.id).second)
		{
			setError(errorMessage, "duplicate 5EAT parameter id " + parameter.id);
			return false;
		}
		parameters.push_back(parameter);

		unsigned int group = 0;
		bool confirmed = false;
		if (diagnosticParameter(parameter.name, &group, &confirmed))
		{
			DiagnosticBlock& block = diagnosticBlocks[group];
			block.group = group;
			if (confirmed)
				block.confirmedAddress = parameter.address;
			else
				block.currentAddress = parameter.address;
		}
	}

	if (parameters.empty())
	{
		setError(errorMessage, "5EAT unit identifier not found: " + _unitIdentifier);
		return false;
	}

	for (std::map<unsigned int, DiagnosticBlock>::const_iterator block = diagnosticBlocks.begin();
	     block != diagnosticBlocks.end(); ++block)
	{
		if ((block->second.currentAddress != 0) && (block->second.confirmedAddress != 0))
			_diagnosticBlocks.push_back(block->second);
	}
	if (_diagnosticBlocks.empty())
	{
		setError(errorMessage, "5EAT logger has no complete diagnostic blocks for " + _unitIdentifier);
		return false;
	}

	_parameters.swap(parameters);
	setError(errorMessage, "");
	return true;
}


bool FiveEATLoggerDefinitions::loadDiagnosticProbe(const std::string& unitIdentifier,
	                                                std::string *errorMessage)
{
	_parameters.clear();
	_diagnosticBlocks.clear();
	_definitionVersion.clear();
	_diagnosticProbe = false;
	_unitIdentifier = normalizeUnitIdentifier(unitIdentifier);
	if (_unitIdentifier.empty())
	{
		setError(errorMessage, "invalid 5EAT unit identifier");
		return false;
	}

	/* This is deliberately an exact, single-ROM field probe. Scan the complete
	 * standard SSM2 OBD DTC bank already described by FreeSSM's flag-byte
	 * definitions: 61 current/confirmed pairs in six address ranges. RomRaider
	 * logger definition v370 independently uses the same ranges through D488.
	 * They remain unverified for this ROM, so every byte is also exposed as a
	 * raw measuring block and all reads stay below address 0x200. */
	if (_unitIdentifier != "B1AB217100")
	{
		setError(errorMessage, "no 5EAT DTC probe defined for unit identifier: " +
		         _unitIdentifier);
		return false;
	}

	struct DiagnosticRange
	{
		unsigned int firstCurrentAddress;
		unsigned int lastCurrentAddress;
		unsigned int confirmedOffset;
	};
	static const DiagnosticRange ranges[] = {
		{0x08E, 0x0AD, 0x20},
		{0x0F0, 0x0F3, 0x04},
		{0x123, 0x12A, 0x08},
		{0x150, 0x154, 0x05},
		{0x160, 0x164, 0x05},
		{0x174, 0x17A, 0x07}
	};

	std::size_t index = 0;
	for (std::size_t rangeIndex = 0;
	     rangeIndex < sizeof(ranges) / sizeof(ranges[0]); ++rangeIndex)
	{
		for (unsigned int currentAddress = ranges[rangeIndex].firstCurrentAddress;
		     currentAddress <= ranges[rangeIndex].lastCurrentAddress;
		     ++currentAddress, ++index)
		{
			DiagnosticBlock block;
			block.group = static_cast<unsigned int>(index);
			block.currentAddress = currentAddress;
			block.confirmedAddress = currentAddress + ranges[rangeIndex].confirmedOffset;
			_diagnosticBlocks.push_back(block);

			const char *states[] = {"current", "confirmed"};
			const unsigned int addresses[] = {block.currentAddress, block.confirmedAddress};
			for (std::size_t state = 0; state < 2; ++state)
			{
				std::ostringstream groupText;
				groupText << std::setfill('0') << std::setw(2) << index;
				std::ostringstream addressText;
				addressText << "0x" << std::uppercase << std::hex << std::setfill('0')
				            << std::setw(3) << addresses[state];

				Parameter parameter;
				parameter.id = "DTC_PROBE_G" + groupText.str() + "_" + states[state];
				parameter.name = "5EAT DTC probe G" + groupText.str() + " " +
				                 states[state] + " raw [" + addressText.str() + "]";
				parameter.description =
					"Unverified read-only DTC byte; preserve the raw value before interpretation";
				parameter.unit = "raw byte (dec)";
				parameter.formula = "*1";
				parameter.address = addresses[state];
				parameter.length = 1;
				parameter.precision = 0;
				_parameters.push_back(parameter);
			}
		}
	}

	_definitionVersion = "5eat-dtc-field-probe-v3";
	_diagnosticProbe = true;
	setError(errorMessage, "");
	return true;
}
