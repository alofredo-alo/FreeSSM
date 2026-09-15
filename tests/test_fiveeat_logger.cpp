#include "src/FiveEATLoggerDefinitions.h"

#include <cstdlib>
#include <iostream>
#include <string>


namespace
{

const unsigned int expectedCurrentDTCAddresses[] = {
	0x09C, 0x09D, 0x09E, 0x0A6, 0x0F0, 0x0F1,
	0x0F2, 0x0F3, 0x123, 0x124, 0x125, 0x162,
	0x174, 0x175, 0x176, 0x177, 0x178, 0x179, 0x17A
};
const unsigned int expectedConfirmedDTCAddresses[] = {
	0x0BC, 0x0BD, 0x0BE, 0x0C6, 0x0F4, 0x0F5,
	0x0F6, 0x0F7, 0x12B, 0x12C, 0x12D, 0x167,
	0x17B, 0x17C, 0x17D, 0x17E, 0x17F, 0x180, 0x181
};


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
	require(!definitions.isDiagnosticProbe(), "exact logger definition marked as probe");
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

	for (std::size_t index = 0; index < definitions.diagnosticBlocks().size(); ++index)
	{
		const FiveEATLoggerDefinitions::DiagnosticBlock& block =
			definitions.diagnosticBlocks().at(index);
		require((block.group == index) &&
		        (block.currentAddress == expectedCurrentDTCAddresses[index]) &&
		        (block.confirmedAddress == expectedConfirmedDTCAddresses[index]),
		        "DTC group address mismatch");
	}

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
	require(!unknown.isDiagnosticProbe(), "unknown unit marked as probe");

	FiveEATLoggerDefinitions probe;
	require(probe.loadDiagnosticProbe("b1 ab-21:71 00", &error), error);
	require(probe.isDiagnosticProbe(), "field probe not marked as unverified probe");
	require(probe.unitIdentifier() == "B1AB217100", "probe unit identifier mismatch");
	require(probe.definitionVersion() == "5eat-dtc-field-probe-v2",
	        "probe definition version mismatch");
	require(probe.parameters().size() == 38, "probe must expose all 38 raw DTC bytes");
	require(probe.diagnosticBlocks().size() == 19, "probe must expose nineteen DTC pairs");
	require(probe.parameters().front().address == 0x09C, "first probe address mismatch");
	require(probe.parameters().back().address == 0x181, "last probe address mismatch");
	for (std::size_t index = 0; index < probe.diagnosticBlocks().size(); ++index)
	{
		const FiveEATLoggerDefinitions::DiagnosticBlock& block =
			probe.diagnosticBlocks().at(index);
		require((block.group == index) &&
		        (block.currentAddress == expectedCurrentDTCAddresses[index]) &&
		        (block.confirmedAddress == expectedConfirmedDTCAddresses[index]),
		        "probe DTC group address mismatch");
	}
	for (std::size_t index = 0; index < probe.parameters().size(); ++index)
	{
		const FiveEATLoggerDefinitions::Parameter& parameter = probe.parameters().at(index);
		require(parameter.address < 0x200, "probe contains a direct RAM address");
		require(parameter.length == 1, "probe values must be single bytes");
		require(parameter.formula == "*1", "probe must preserve raw byte values");
	}
	require(!probe.loadDiagnosticProbe("B1AE207100", &error),
	        "probe must not activate for a different ROM");
	require(probe.parameters().empty(), "failed probe selection exposed stale parameters");
	require(probe.diagnosticBlocks().empty(), "failed probe selection exposed stale DTC blocks");
	require(!probe.isDiagnosticProbe(), "failed probe selection remained active");

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
