/*
 * DiagnosticSafety.h - Process-wide diagnostic safety policy
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef DIAGNOSTICSAFETY_H
#define DIAGNOSTICSAFETY_H


class DiagnosticSafety
{
public:
	static void setReadOnly(bool enabled)
	{
		readOnlyStorage() = enabled || demoStorage();
	}

	static void setDemo(bool enabled)
	{
		demoStorage() = enabled;
		if (enabled) readOnlyStorage() = true;
	}

	static bool isDemo() { return demoStorage(); }

	static bool isReadOnly()
	{
		return readOnlyStorage();
	}

private:
	static bool& demoStorage()
	{
		static bool enabled = false;
		return enabled;
	}

	static bool& readOnlyStorage()
	{
		static bool enabled = true;
		return enabled;
	}
};


#endif
