/*
 * main.cpp - Main file of the FreeSSM software
 *
 * Copyright (C) 2008-2018 Comer352L
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


#define FSSM_VERSION "(devel)"


#include <QtGui>
#include "FreeSSM.h"
#include "CmdLine.h"
#include "DiagnosticSafety.h"
#include <iostream>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <cstdio>

static void attachProbeConsole(int argc, char *argv[])
{
	bool probeRequested = false;
	for (int i = 1; i < argc; ++i)
		probeRequested = probeRequested || (std::strcmp(argv[i], "--j2534-probe") == 0);
	if (!probeRequested)
		return;

	/* Release builds use the Windows GUI subsystem. Attach only for the explicit
	 * probe, so normal GUI launches do not open a console window. */
	if (!AttachConsole(ATTACH_PARENT_PROCESS) && GetLastError() != ERROR_ACCESS_DENIED)
		return;
	std::freopen("CONOUT$", "w", stdout);
	std::freopen("CONOUT$", "w", stderr);
}
#endif


static int runJ2534Probe()
{
	const std::vector<J2534Library> libraries = J2534_API::getAvailableJ2534Libs();
	if (libraries.empty())
	{
		std::cerr << "J2534 probe: no registered drivers found.\n";
		return ERROR_J2534PROBE;
	}

	bool passedAny = false;
	for (const J2534Library& library : libraries)
	{
		std::cout << "J2534 driver: " << library.name << "\n"
		          << "  DLL: " << library.path << "\n"
		          << "  Registry architecture: " << J2534misc::architectureToStr(library.architecture) << "\n"
		          << "  API: " << J2534misc::apiVersionToStr(library.api) << "\n"
		          << "  Protocols:";
		for (const std::string& protocol : J2534misc::protocolsToStrings(library.protocols))
			std::cout << " " << protocol;
		std::cout << "\n";

		if (!library.compatibleWithApplication)
		{
			std::cout << "  RESULT: SKIP - DLL architecture does not match this FreeSSM build.\n";
			continue;
		}

		J2534DiagInterface interface;
		if (!interface.open(library.path))
		{
			std::cout << "  RESULT: FAIL - " << interface.lastError() << "\n";
			continue;
		}

		std::cout << "  Device: " << interface.name() << "\n"
		          << "  Version: " << interface.version() << "\n";
		const std::string openWarning = interface.lastError();
		if (interface.close())
		{
			if (openWarning.empty())
				std::cout << "  RESULT: PASS\n";
			else
				std::cout << "  RESULT: PASS with warning - " << openWarning << "\n";
			passedAny = true;
		}
		else
			std::cout << "  RESULT: FAIL on close - " << interface.lastError() << "\n";
	}
	return passedAny ? NOERROR : ERROR_J2534PROBE;
}



static bool applyCmdLineStartupOptions(FreeSSM *freessm_mainwindow, QStringList cmdline_args)
{
	QStringList option_values;
	QString cu_str;

	// Check if command line option -c/--controlunit is specified and extract corresponding value(s):
	if (!CmdLine::parseForOption(&cmdline_args, "-c", "--controlunit", &option_values))
		return true; // no error, option unused
	// Validate number of specified values:
	if (option_values.size() < 1)
	{
		CmdLine::printError("no control unit specified with option -c/--controlunit");
		return false;
	}
	if (option_values.size() > 1)
	{
		CmdLine::printError("multiple control units specified with option -c/--controlunit");
		return false;
	}
	// Validate control unit string and start specified control unit dialog:
	cu_str = option_values.at(0);
	if (cu_str == "engine")
		freessm_mainwindow->engine(cmdline_args);
	else if (cu_str == "transmission")
		freessm_mainwindow->transmission(cmdline_args);
	else if (cu_str == "tpms")
		freessm_mainwindow->tpms(cmdline_args);
	else if (cu_str == "absvdc")
		freessm_mainwindow->abs(cmdline_args);
	else if (cu_str == "cruisectrl")
		freessm_mainwindow->cruisecontrol(cmdline_args);
	else if (cu_str == "aircon")
		freessm_mainwindow->aircon(cmdline_args);
	else
	{
		CmdLine::printError("invalid control unit specified with option -c/--controlunit");
		return false;
	}

	return true;
}



int main(int argc, char *argv[])
{
	int ret = NOERROR;
	QStringList cmdline_args;
	QStringList option_values;

#ifdef _WIN32
	attachProbeConsole(argc, argv);
#endif
	QApplication app(argc, argv);
	// Get command line arguments and check if help message is requested
	cmdline_args = QCoreApplication::arguments();
	if (CmdLine::parseForOption(&cmdline_args, "-h", "--help", &option_values))
	{
		CmdLine::printHelp();
		return NOERROR;
	}
	if (CmdLine::parseForOption(&cmdline_args, "", "--j2534-probe", &option_values))
	{
		if (!option_values.empty())
		{
			CmdLine::printError("option --j2534-probe does not accept values");
			return ERROR_BADCMDLINEARGS;
		}
		return runJ2534Probe();
	}
	if (CmdLine::parseForOption(&cmdline_args, "", "--read-only", &option_values))
	{
		if (!option_values.empty())
		{
			CmdLine::printError("option --read-only does not accept values");
			return ERROR_BADCMDLINEARGS;
		}
		DiagnosticSafety::setReadOnly(true);
	}
	// Validate command line option dependencies:
	if (!CmdLine::validateOptionDependencies(cmdline_args))
		return ERROR_BADCMDLINEARGS;
	// Ensure that only one instance of FreeSSM is started:
	QSharedMemory fssm_lock("FreeSSM_smo-key_1234567890ABCDEFGHIJKLMNOPQRSTUVW");
	if ( fssm_lock.attach() )
	{
		/* NOTE: With the current implementation of QSharedMemory, shared memory objects
		 * survive on UNIX-systems if the attached process is killed (or crashed) !    */
		fssm_lock.detach();	// trick: this also detaches all dead processes (why ?!) !
		if ( fssm_lock.attach() ) // another instance of FreeSSM is still running
		{
			CmdLine::printError("an instance of FreeSSM is already running");
			return ERROR_APPRUNNING;
		}
	}
	fssm_lock.create(1, QSharedMemory::ReadOnly);
	// Set application version:
	app.setApplicationVersion(FSSM_VERSION);
	// Set window icon:
	app.setWindowIcon( QIcon(":/icons/freessm/32x32/FreeSSM.png") );
#if QT_VERSION < 0x050000
	// Use Unicode UTF-8:
	QTextCodec::setCodecForCStrings( QTextCodec::codecForName("UTF-8") );
#endif
	// Get installed (system) fonts:
	QFontDatabase fdb;
	QStringList fontfamilies = fdb.families ( QFontDatabase::Any );
	// Check if font "Liberation Sans" is installed:
	if (!fontfamilies.contains("Liberation Sans"))
	{
		// Load font "Liberation Sans" (no installation !)
		QString AppsPath( QCoreApplication::applicationDirPath() );
		QFontDatabase::addApplicationFont (AppsPath + "/LiberationSans-Regular.ttf");
		QFontDatabase::addApplicationFont (AppsPath + "/LiberationSans-Bold.ttf");
		QFontDatabase::addApplicationFont (AppsPath + "/LiberationSans-Italic.ttf");
		QFontDatabase::addApplicationFont (AppsPath + "/LiberationSans-BoldItalic.ttf");
	}
	// Select and use font:
	QFont appfont = app.font();
	appfont.setFamily("Liberation Sans");
	app.setFont( appfont );
	// Open main window:
	FreeSSM *freessm_mainwindow = new FreeSSM(&app);
#ifdef SMALL_RESOLUTION
	freessm_mainwindow->showFullScreen();
#else
	freessm_mainwindow->show();
#endif
	// Start control unit dialog if specified via command line:
	if (applyCmdLineStartupOptions(freessm_mainwindow, cmdline_args))
		// Wait until main window is closed by the user:
		ret = app.exec();
	else
		ret = ERROR_BADCMDLINEARGS;

	delete freessm_mainwindow;
	return ret;
}
