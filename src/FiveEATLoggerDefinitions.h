/*
 * FiveEATLoggerDefinitions.h - Read-only RomRaider logger definition loader
 *
 * Copyright (C) 2026 Alonso Lopez
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef FIVEEATLOGGERDEFINITIONS_H
#define FIVEEATLOGGERDEFINITIONS_H


#include <cstddef>
#include <string>
#include <vector>


class FiveEATLoggerDefinitions
{
public:
	class Parameter
	{
	public:
		std::string id;
		std::string name;
		std::string description;
		std::string unit;
		std::string formula;
		unsigned int address = 0;
		unsigned int length = 1;
		unsigned int precision = 0;
	};

	class DiagnosticBlock
	{
	public:
		unsigned int group = 0;
		unsigned int currentAddress = 0;
		unsigned int confirmedAddress = 0;
	};

	bool load(const std::string& filename, const std::string& unitIdentifier,
	          std::string *errorMessage = NULL);

	const std::vector<Parameter>& parameters() const { return _parameters; }
	const std::vector<DiagnosticBlock>& diagnosticBlocks() const { return _diagnosticBlocks; }
	const std::string& unitIdentifier() const { return _unitIdentifier; }
	const std::string& definitionVersion() const { return _definitionVersion; }

	static std::string normalizeUnitIdentifier(const std::string& value);

private:
	std::vector<Parameter> _parameters;
	std::vector<DiagnosticBlock> _diagnosticBlocks;
	std::string _unitIdentifier;
	std::string _definitionVersion;
};


#endif // FIVEEATLOGGERDEFINITIONS_H
