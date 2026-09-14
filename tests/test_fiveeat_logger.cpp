#include "src/FiveEATLoggerDefinitions.h"

#include <cstdlib>
#include <iostream>
#include <string>


namespace
{

void require(bool condition, const std::string& message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << std::endl;
		std::exit(1);
	}
}


const FiveEATLoggerDefinitions::Parameter *findParameter(
	const FiveEATLoggerDefinitions& definitions, const std::string& id)
{
	const std::vector<FiveEATLoggerDefinitions::Parameter>& parameters = definitions.parameters();
	for (std::size_t i = 0; i < parameters.size(); ++i)
	{
		if (parameters.at(i).id == id)
			return &parameters.at(i);
	}
	return NULL;
}


void checkKnownUnit(const std::string& filename, const std::string& unitIdentifier)
{
	FiveEATLoggerDefinitions definitions;
	std::string error;
	require(definitions.load(filename, unitIdentifier, &error), error);
	require(definitions.unitIdentifier() == unitIdentifier, "normalized unit identifier mismatch");
	require(definitions.definitionVersion() == "5eat-tcu", "definition version mismatch");
	require(definitions.parameters().size() >= 40, "too few parameters selected");
	require(definitions.diagnosticBlocks().size() == 12, "expected twelve complete DTC groups");

	const FiveEATLoggerDefinitions::Parameter *engineSpeed = findParameter(definitions, "P001");
	require(engineSpeed != NULL, "Engine Speed is missing");
	require(engineSpeed->address == 0x0E, "Engine Speed address mismatch");
	require(engineSpeed->length == 2, "Engine Speed must be two bytes");
	require(engineSpeed->formula == "/4", "Engine Speed formula mismatch");

	const FiveEATLoggerDefinitions::Parameter *atfTemperature = findParameter(definitions, "P009");
	require(atfTemperature != NULL, "ATF Temperature is missing");
	require(atfTemperature->address == 0x56, "ATF Temperature address mismatch");
	require(atfTemperature->formula == "-50", "ATF Temperature formula mismatch");

	const FiveEATLoggerDefinitions::DiagnosticBlock& first = definitions.diagnosticBlocks().front();
	const FiveEATLoggerDefinitions::DiagnosticBlock& last = definitions.diagnosticBlocks().back();
	require((first.group == 0) && (first.currentAddress == 0x09C) &&
	        (first.confirmedAddress == 0x0BC), "DTC group 0 mismatch");
	require((last.group == 11) && (last.currentAddress == 0x162) &&
	        (last.confirmedAddress == 0x167), "DTC group 11 mismatch");

	for (std::size_t i = 0; i < definitions.parameters().size(); ++i)
		require(definitions.parameters().at(i).address < 0x200,
		        "direct RAM parameter escaped the safety filter");
}


} // namespace


int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		std::cerr << "Usage: test_fiveeat_logger <5eat_tcu_logger.xml>" << std::endl;
		return 2;
	}

	const std::string filename = argv[1];
	const char *supportedUnitIdentifiers[] = {
		"8AF0237300", "8CEB207000", "8EFB206000", "91A0217300",
		"91A0217400", "91A1207300", "91D0207500", "91D1206000",
		"91D1207900", "91F0217100", "91FE207100", "91FE216300",
		"99DE206000", "A3DE207100", "AAD1A06000", "AAD1A07100",
		"ABD1207000", "ABD1A03100", "ACD1207000", "ACD1A06000",
		"ADE0236000", "AED1207100", "AED1A0A100", "AFDE217000",
		"B1AE207100", "C1D1207000"
	};
	for (std::size_t index = 0;
	     index < sizeof(supportedUnitIdentifiers) / sizeof(supportedUnitIdentifiers[0]);
	     ++index)
		checkKnownUnit(filename, supportedUnitIdentifiers[index]);

	checkKnownUnit(filename, "ACD1A06000"); // Hitachi M32R
	checkKnownUnit(filename, "A3DE207100"); // Denso SH705x

	FiveEATLoggerDefinitions unknown;
	std::string error;
	require(!unknown.load(filename, "00 00 00 00 00", &error),
	        "unknown unit identifier must not match");
	require(unknown.parameters().empty(), "unknown unit must expose no parameters");
	require(error.find("not found") != std::string::npos, "unknown unit error is not actionable");

	FiveEATLoggerDefinitions invalid;
	require(invalid.load(filename, "ACD1A06000", &error), error);
	const std::size_t pathSeparator = filename.find_last_of("/\\");
	const std::string definitionDirectory =
		pathSeparator == std::string::npos ? "" : filename.substr(0, pathSeparator + 1);
	require(!invalid.load(definitionDirectory + "5eat_tcu_logger.LICENSE.txt",
	                      "ACD1A06000", &error),
	        "non-XML input must fail");
	require(invalid.parameters().empty(), "failed reload must expose no stale parameters");
	require(invalid.diagnosticBlocks().empty(), "failed reload must expose no stale DTC blocks");

	require(FiveEATLoggerDefinitions::normalizeUnitIdentifier("ac d1-a0:60 00") ==
	        "ACD1A06000", "unit identifier normalization failed");

	std::cout << "PASS: 5EAT logger parser" << std::endl;
	return 0;
}
