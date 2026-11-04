/*
 * CUcontent_DCs_abstract.cpp - Abstract widget for Diagnostic Codes Reading
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

#include "CUcontent_DCs_abstract.h"
#include "VehicleReportPdf.h"
#include "DiagnosticSafety.h"
#include <QDateTime>
#include <QFileDialog>
#include <QInputDialog>
#include <QSaveFile>


CUcontent_DCs_abstract::CUcontent_DCs_abstract(QWidget *parent) : QWidget(parent)
{
	_SSMPdev = NULL;
	_supportedDCgroups = 0;
	_reportButton = NULL;
}


CUcontent_DCs_abstract::~CUcontent_DCs_abstract()
{
	if (!_SSMPdev) return;	// avoid NULL-pointer-warning-message
	disconnect(_SSMPdev, SIGNAL( stoppedDCreading() ), this, SLOT( callStop() )); // this must be done BEFORE calling _SSMP2dev->stopDCreading() !
	_SSMPdev->stopDCreading();
	disconnect(_SSMPdev, SIGNAL( startedDCreading() ), this, SLOT( callStart() ));
	disconnect(_SSMPdev, SIGNAL( stoppedDCreading() ), this, SLOT( callStop() ));
}


void CUcontent_DCs_abstract::callStart()
{
	if (!startDCreading())
		communicationError(tr("Couldn't start Diagnostic Codes Reading."));
}


void CUcontent_DCs_abstract::callStop()
{
	if (!stopDCreading())
		communicationError(tr("Couldn't stop Diagnostic Codes Reading."));
}


bool CUcontent_DCs_abstract::startDCreading()
{
	SSMprotocol::state_dt state = SSMprotocol::state_needSetup;
	int selDCgroups = 0;
	if (!_SSMPdev) return false;
	// Check if DC-group(s) selected:
	if (_supportedDCgroups == SSMprotocol::noDCs_DCgroup)
		return false;
	// Check if DC-reading is startable or already in progress:
	state = _SSMPdev->state();
	if (state == SSMprotocol::state_normal)
	{
		disconnect(_SSMPdev, SIGNAL( startedDCreading() ), this, SLOT( callStart() ));
		// Start DC-reading:
		if (!_SSMPdev->startDCreading( _supportedDCgroups ))
		{
			connect(_SSMPdev, SIGNAL( startedDCreading() ), this, SLOT( callStart() ));
			return false;
		}
	}
	else if (state == SSMprotocol::state_DCreading)
	{
		// Verify consistency:
		if (!_SSMPdev->getLastDCgroupsSelection(&selDCgroups))
			return false;
		if (selDCgroups != _supportedDCgroups)  // inconsistency detected !
		{
			// Stop DC-reading:
			stopDCreading();
			return false;
		}
	}
	else
		return false;
	// Enable notification about external DC-reading-stops:
	connect(_SSMPdev, SIGNAL( stoppedDCreading() ), this, SLOT( callStop() ));
	// Connect DC-table and print-button slots:
	connectGUIelements();
	CUtype cu;
	if (_SSMPdev->CUtype(&cu) && (cu == CUtype::Engine || cu == CUtype::Transmission))
	{
		const bool current = _supportedDCgroups & (SSMprotocol::currentDTCs_DCgroup | SSMprotocol::temporaryDTCs_DCgroup);
		const bool history = _supportedDCgroups & (SSMprotocol::historicDTCs_DCgroup | SSMprotocol::memorizedDTCs_DCgroup);
		_reportCollector.reset(new VehicleReport::Collector(cu == CUtype::Engine ? "motor" : "transmision", "SSM", current, history));
		VehicleReport::Reading rom;
		rom.value = _SSMPdev->getROMID();
		rom.status = rom.value.empty() ? VehicleReport::QueryStatus::NotQueried : VehicleReport::QueryStatus::Complete;
		rom.source = "SSM";
		_reportCollector->setRomId(rom);
		_reportCollector->setUnverifiedDtcDefinitions(_SSMPdev->hasUnverifiedDtcDefinitions());
		if (!_reportButton && layout()) {
			_reportButton = new QPushButton(tr("Save status report (JSON)"), this);
			layout()->addWidget(_reportButton);
			connect(_reportButton, SIGNAL(clicked()), this, SLOT(saveVehicleReport()));
		}
		if (_reportButton) _reportButton->setEnabled(true);
		connect(_SSMPdev, SIGNAL(currentOrTemporaryDTCs(QStringList,QStringList,bool,bool)), this, SLOT(captureCurrentDTCs(QStringList,QStringList,bool,bool)), Qt::UniqueConnection);
		connect(_SSMPdev, SIGNAL(historicOrMemorizedDTCs(QStringList,QStringList)), this, SLOT(captureHistoryDTCs(QStringList,QStringList)), Qt::UniqueConnection);
		connect(_SSMPdev, SIGNAL(commError()), this, SLOT(reportCommunicationError()), Qt::UniqueConnection);
	}
	return true;
}


bool CUcontent_DCs_abstract::stopDCreading()
{
	if (_reportButton) _reportButton->setEnabled(false);
	if (_SSMPdev) {
		disconnect(_SSMPdev, SIGNAL(currentOrTemporaryDTCs(QStringList,QStringList,bool,bool)), this, SLOT(captureCurrentDTCs(QStringList,QStringList,bool,bool)));
		disconnect(_SSMPdev, SIGNAL(historicOrMemorizedDTCs(QStringList,QStringList)), this, SLOT(captureHistoryDTCs(QStringList,QStringList)));
		disconnect(_SSMPdev, SIGNAL(commError()), this, SLOT(reportCommunicationError()));
	}
	_reportCollector.reset();
	if (_SSMPdev)
	{
		disconnect(_SSMPdev, SIGNAL( stoppedDCreading() ), this, SLOT( callStop() )); // this must be done BEFORE calling _SSMP2dev->stopDCreading() !
		if (_SSMPdev->state() == SSMprotocol::state_DCreading)
		{
			if (!_SSMPdev->stopDCreading())
			{
				connect(_SSMPdev, SIGNAL( stoppedDCreading() ), this, SLOT( callStop() )); // this must be done BEFORE calling _SSMP2dev->stopDCreading() !
				return false;
			}
		}
		connect(_SSMPdev, SIGNAL( startedDCreading() ), this, SLOT( callStart() ));
	}
	disconnectGUIelements();
	return true;
}


void CUcontent_DCs_abstract::setDCtableContent(QTableWidget *tableWidget, QStringList DCs, QStringList DCdescriptions)
{
	int k = 0;
	QTableWidgetItem *tableelement;
	// Delete table content:
	tableWidget->clearContents();
	// Set number of rows:
	setNrOfTableRows(tableWidget, DCdescriptions.size());
	// Fill Table:
	for (k=0; k<DCs.size(); k++)
	{
		// Output DC:
		tableelement = new QTableWidgetItem(DCs.at(k));
		tableelement->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		tableWidget->setItem(k, 0, tableelement);
		// Output DC description:
		tableelement = new QTableWidgetItem(DCdescriptions.at(k));
		tableWidget->setItem(k, 1, tableelement);
	}
}


void CUcontent_DCs_abstract::setNrOfTableRows(QTableWidget *tablewidget, unsigned int nrofUsedRows)
{
	// NOTE: this function doesn't change the table's content ! Min. 1 row !
	int rowheight = 0;
	int vspace = 0;
	unsigned int minnrofrows = 0;
	// Get available vertical space (for rows) and height per row:
	if (tablewidget->rowCount() < 1)
		tablewidget->setRowCount(1); // temporary create a row to get the row hight
	rowheight = tablewidget->rowHeight(0);
	//vspace = tablewidget->viewport()->height(); // NOTE: Sometimes doesn't work as expected ! (Qt-Bug ?)
	vspace = tablewidget->height() - tablewidget->horizontalHeader()->viewport()->height() - 4;
	// Temporary switch to "Scroll per Pixel"-mode to ensure auto-scroll (prevent white space between bottom of the last row and the lower table border)
	tablewidget->setVerticalScrollMode( QAbstractItemView::ScrollPerPixel );
	// Calculate and set nr. of rows:
	minnrofrows = static_cast<unsigned int>(trunc((vspace-1)/rowheight) + 1);
	if (minnrofrows < nrofUsedRows)
		minnrofrows = nrofUsedRows;
	tablewidget->setRowCount(minnrofrows);
	// Set vertical scroll bar policy:
	if (minnrofrows > nrofUsedRows)
		tablewidget->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
	else
		tablewidget->setVerticalScrollBarPolicy( Qt::ScrollBarAsNeeded );
	// Switch back to "Scroll per Item"-mode:
	tablewidget->setVerticalScrollMode( QAbstractItemView::ScrollPerItem ); // auto-scroll is triggered; Maybe this is a Qt-Bug, we don't want that here...
}


#ifndef SMALL_RESOLUTION
void CUcontent_DCs_abstract::printDCprotocol()
{
	QString datetime;
	CUtype cu_type;
	QString CU;
	QString systype;
	std::string ROM_ID;
	QString VIN = tr("not supported by ECU");
	bool VINsup = false;
	bool ok = false;
	QString errstr = "";

	if (!_SSMPdev) return;
	// Create Printer:
	QPrinter printer(QPrinter::ScreenResolution);
	// Show print dialog:
	QPrintDialog printDialog(&printer, this);
	if (printDialog.exec() != QDialog::Accepted)
		return;
	// Show print status message:
	QMessageBox printmbox( QMessageBox::NoIcon, tr("Printing..."), tr("Printing... Please wait !    "), QMessageBox::NoButton, this);
	printmbox.setStandardButtons(QMessageBox::NoButton);
	QPixmap printicon(QString::fromUtf8(":/icons/chrystal/32x32/fileprint"));
	printmbox.setIconPixmap(printicon);
	QFont printmboxfont = printmbox.font();
	printmboxfont.setPointSize(10);	// 10pts
	printmboxfont.setBold(true);
	printmbox.setFont( printmboxfont );
	printmbox.show();
	// ##### GATHER CU-INFORMATIONS #####
	datetime = QDateTime::currentDateTime().toString("dddd, dd. MMMM yyyy, h:mm") + ":";
	ok = true;
	if (!_SSMPdev->getSystemDescription(&systype))
	{
		std::string SYS_ID = "";
		SYS_ID = _SSMPdev->getSysID();
		ok = SYS_ID.length();
		if (ok)
			systype = tr("Unknown") + " (" + QString::fromStdString(SYS_ID) + ")";  // NOTE: SYS_ID is always available, if CU is initialized/connection is alive
		else
			errstr = tr("Query of the System-ID failed.");
	}
	if (ok)
	{
		ROM_ID = _SSMPdev->getROMID();	// NOTE: ROM_ID is always available, if CU is initialized/connection is alive
		ok = ROM_ID.length();
		if (ok)
		{
			ok = _SSMPdev->CUtype(&cu_type);
			if (ok)
			{
				switch (cu_type)
				{
					case CUtype::Engine:
						CU = tr("Engine");
						ok = _SSMPdev->hasVINsupport(&VINsup);
						if (ok)
						{
							if (VINsup)
							{
								// Temporary stop DC-reading for VIN-Query:
								disconnect(_SSMPdev, SIGNAL( stoppedDCreading() ), this, SLOT( callStop() ));
								ok = _SSMPdev->stopDCreading();
								if (ok)
								{
									// Query VIN:
									ok = _SSMPdev->getVIN(&VIN);
									if (ok)
									{
										if (VIN.size() == 0)
											VIN = tr("not programmed yet");
										// Restart DC-reading:
										ok = _SSMPdev->restartDCreading();
										if (ok)
											connect(_SSMPdev, SIGNAL( stoppedDCreading() ), this, SLOT( callStop() ));
										else
											errstr = tr("Couldn't restart Diagnostic Codes Reading.");
									}
									else
										errstr = tr("Query of the VIN failed.");
								}
								else
									errstr = tr("Couldn't stop Diagnostic Codes Reading.");
							}
						}
						else
							errstr = tr("Couldn't determine if VIN-registration is supported.");
						break;
					case CUtype::Transmission:
						CU = tr("Transmission");
						break;
					case CUtype::CruiseControl:
						CU = tr("Cruise Control");
						break;
					case CUtype::AirCon:
						CU = tr("Air Conditioning");
						break;
					case CUtype::TPMS:
						CU = tr("Tire Pressure Monitoring System");
						break;
					case CUtype::FourWheelSteering:
						CU = tr("4 Wheel Steering");
						break;
					default:
						CU = tr("UNKNOWN");
				}
			}
			else
				errstr = tr("Couldn't determine Control Unit type.");
		}
		else
			errstr = tr("Query of the ROM-ID failed.");
	}
	// Check for communication error:
	if (!ok)
	{
		printmbox.close();
		communicationError(errstr);
		return;
	}
	// ##### CREATE TEXT DOCUMENT #####
	// Create text document and get cursor position:
	QTextDocument textDocument;
	QTextCursor cursor(&textDocument);
	// Define formats:
	QTextBlockFormat blockFormat;
	QTextCharFormat charFormat;
	QTextTableFormat tableFormat;
	// --- TITLE ---
	// Set title format:
	blockFormat.setAlignment(Qt::AlignHCenter);
	charFormat.setFontPointSize(18);
	charFormat.setFontWeight(QFont::Normal);
	charFormat.setFontUnderline(true);
	// Put title into text document:
	cursor.setBlockFormat(blockFormat);
	cursor.setCharFormat(charFormat);
	cursor.insertText("FreeSSM " + QApplication::applicationVersion());
	cursor.insertBlock();
	// --- DATE, TIME ---
	// Format date + time:
	blockFormat.setAlignment(Qt::AlignLeft);
	blockFormat.setTopMargin(15);
	charFormat.setFontPointSize(14);
	charFormat.setFontWeight(QFont::Normal);
	charFormat.setFontUnderline(false);
	cursor.setBlockFormat(blockFormat);
	cursor.setCharFormat(charFormat);
	// Put date + time into the text document:
	cursor.insertText(datetime);
	// ----- CU-INFORMATIONS -----
	// -- Create frame for CU-Informations:
	QTextFrameFormat infoFrame;
	infoFrame.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
	infoFrame.setBorder(1);
	infoFrame.setBorderBrush(QBrush(QColor("black"), Qt::SolidPattern));
	infoFrame.setTopMargin(10);
	cursor.insertFrame(infoFrame);
	// -- TABLE OF CU-INFORMATIONS:
	// Set minimal columns widths:
	QVector<QTextLength> widthconstraints(2);
	widthconstraints[0] = QTextLength(QTextLength::PercentageLength, 50);
	widthconstraints[1] = QTextLength(QTextLength::PercentageLength, 50);
	// Set format:
	tableFormat.setBorderStyle(QTextFrameFormat::BorderStyle_None);
	tableFormat.setColumnWidthConstraints(widthconstraints);
	tableFormat.setLeftMargin(20);
	// Create and insert table:
	if (cu_type == CUtype::Engine)
		cursor.insertTable(4, 2, tableFormat);
	else
		cursor.insertTable(3, 2, tableFormat);
	// Set font format:
	charFormat.setFontPointSize(12);
	charFormat.setFontUnderline(false);
	// Fill table cells:
	// Row 1 - Column 1:
	charFormat.setFontWeight(QFont::Bold);
	cursor.setCharFormat(charFormat);
	cursor.insertText(tr("Control Unit:"));
	cursor.movePosition(QTextCursor::NextBlock,QTextCursor::MoveAnchor,1);
	// Row 1 - Column 2:
	charFormat.setFontWeight(QFont::Normal);
	cursor.setCharFormat(charFormat);
	cursor.insertText(CU);
	cursor.movePosition(QTextCursor::NextBlock,QTextCursor::MoveAnchor,1);
	// Row 2 - Column 1:
	charFormat.setFontWeight(QFont::Bold);
	cursor.setCharFormat(charFormat);
	cursor.insertText(tr("System Type:"));
	cursor.movePosition(QTextCursor::NextBlock,QTextCursor::MoveAnchor,1);
	// Row 2 - Column 2:
	charFormat.setFontWeight(QFont::Normal);
	cursor.setCharFormat(charFormat);
	cursor.insertText(systype);
	cursor.movePosition(QTextCursor::NextBlock,QTextCursor::MoveAnchor,1);
	// Row 3 - Column 1:
	charFormat.setFontWeight(QFont::Bold);
	cursor.setCharFormat(charFormat);
	cursor.insertText(tr("ROM-ID:"));
	cursor.movePosition(QTextCursor::NextBlock,QTextCursor::MoveAnchor,1);
	// Row 3 - Column 2:
	charFormat.setFontWeight(QFont::Normal);
	cursor.setCharFormat(charFormat);
	cursor.insertText( QString::fromStdString(ROM_ID) );
	if (cu_type == CUtype::Engine)
	{
		cursor.movePosition(QTextCursor::NextBlock,QTextCursor::MoveAnchor,1);
		// Row 4 - Column 1:
		charFormat.setFontWeight(QFont::Bold);
		cursor.setCharFormat(charFormat);
		cursor.insertText(tr("Registered VIN:"));
		cursor.movePosition(QTextCursor::NextBlock,QTextCursor::MoveAnchor,1);
		// Row 4 - Column 2:
		charFormat.setFontWeight(QFont::Normal);
		cursor.setCharFormat(charFormat);
		cursor.insertText(VIN);
	}
	// Move (out of the table) to the next block:
	cursor.movePosition(QTextCursor::NextBlock,QTextCursor::MoveAnchor,1);
	// Move (out of the frame) to the next block:
	cursor.movePosition(QTextCursor::NextBlock,QTextCursor::MoveAnchor,1);
	// Create DC-tables:
	createDCprintTables(cursor);
	// Print created text-document:
	textDocument.print(&printer);
	// Delete status message:
	printmbox.close();
}
#endif	// !SMALL_RESOLUTION


#ifndef SMALL_RESOLUTION
void CUcontent_DCs_abstract::insertDCprintTable(QTextCursor cursor, QString title, QStringList codes, QStringList descriptions)
{
	QTextTableFormat tableFormat;
	QTextCharFormat charFormat;
	QVector<QTextLength> widthconstraints(2);
	int k = 0;
	// Insert space line:
	cursor.insertBlock();
	// Set minimal column widths:
	widthconstraints.clear();
	widthconstraints.resize(2);
	widthconstraints[0] = QTextLength(QTextLength::PercentageLength,12);
	widthconstraints[1] = QTextLength(QTextLength::PercentageLength,88);
	// Set table format:
	tableFormat.setBorderStyle(QTextFrameFormat::BorderStyle_None);
	tableFormat.setColumnWidthConstraints(widthconstraints);
	tableFormat.setLeftMargin(20);
	// Set title:
	charFormat.setFontPointSize(14);
	charFormat.setFontWeight(QFont::Bold);
	charFormat.setFontUnderline(false);
	cursor.setCharFormat(charFormat);
	cursor.insertText( title );
	// Create and insert table:
	cursor.insertTable(codes.size(), 2, tableFormat); // n rows, 2 columns
	// Set font format for DCs:
	charFormat.setFontPointSize(12);
	charFormat.setFontWeight(QFont::Normal);
	charFormat.setFontUnderline(false);
	// Fill table cells:
	for (k=0; k<codes.size(); k++)
	{
		//   Column 1: Code
		cursor.setCharFormat(charFormat);
		cursor.insertText( codes.at(k) );
		cursor.movePosition(QTextCursor::NextBlock,QTextCursor::MoveAnchor,1);
		//   Column 2: Description
		cursor.setCharFormat(charFormat);
		cursor.insertText( descriptions.at(k) );
		cursor.movePosition(QTextCursor::NextBlock,QTextCursor::MoveAnchor,1);
	}
}
#endif


void CUcontent_DCs_abstract::communicationError(QString errstr)
{
	QMessageBox msg( QMessageBox::Critical, tr("Communication Error"), tr("Communication Error:") + ('\n') + errstr, QMessageBox::Ok, this);
	QFont msgfont = msg.font();
	msgfont.setPointSize(9); // 9pts
	msg.setFont( msgfont );
	msg.show();
	msg.exec();
	msg.close();
	emit error();
}



namespace {
std::vector<VehicleReport::DiagnosticCode> reportCodes(const QStringList& codes, const QStringList& descriptions, VehicleReport::CodeCategory category)
{
	std::vector<VehicleReport::DiagnosticCode> result;
	for (int i = 0; i < codes.size(); ++i) {
		VehicleReport::DiagnosticCode entry;
		entry.code = codes.at(i).toUtf8().constData();
		entry.description = descriptions.at(i).toUtf8().constData();
		entry.category = category;
		result.push_back(entry);
	}
	return result;
}
}

void CUcontent_DCs_abstract::captureCurrentDTCs(QStringList codes, QStringList descriptions, bool, bool dCheckActive)
{
	if (!_reportCollector || !(_supportedDCgroups & (SSMprotocol::currentDTCs_DCgroup | SSMprotocol::temporaryDTCs_DCgroup))) return;
	if (dCheckActive || codes.size() != descriptions.size()) {
		_reportCollector->fail(dCheckActive ? "System check incomplete" : "Invalid DTC response");
		return;
	}
	if (!_reportCollector->accept(VehicleReport::CodeCategory::CurrentOrTemporary,
		reportCodes(codes, descriptions, VehicleReport::CodeCategory::CurrentOrTemporary)))
		_reportCollector->fail("Invalid DTC response");
}

void CUcontent_DCs_abstract::captureHistoryDTCs(QStringList codes, QStringList descriptions)
{
	if (!_reportCollector || !(_supportedDCgroups & (SSMprotocol::historicDTCs_DCgroup | SSMprotocol::memorizedDTCs_DCgroup))) return;
	if (codes.size() != descriptions.size()) {
		_reportCollector->fail("Invalid DTC response");
		return;
	}
	if (!_reportCollector->accept(VehicleReport::CodeCategory::HistoricOrMemorized,
		reportCodes(codes, descriptions, VehicleReport::CodeCategory::HistoricOrMemorized)))
		_reportCollector->fail("Invalid DTC response");
}

void CUcontent_DCs_abstract::reportCommunicationError()
{
	if (_reportCollector) _reportCollector->fail("Communication error");
}

void CUcontent_DCs_abstract::saveVehicleReport()
{
	if (!_reportCollector) return;
	VehicleReport::Report report;
	report.simulated = DiagnosticSafety::isDemo();
	report.applicationVersion = QCoreApplication::applicationVersion().toStdString();
	report.capturedAt = QDateTime::currentDateTime().toString(Qt::ISODate).toStdString();
	const VehicleReport::Module currentModule = _reportCollector->result();
	if (currentModule.name == "motor") {
		QString vin;
		bool supported = false;
		if (_SSMPdev->getCachedVIN(&vin)) {
			report.vin.status = VehicleReport::QueryStatus::Complete;
			report.vin.value = vin.toUtf8().constData();
			report.vin.source = "SSM";
		} else if (_SSMPdev->hasVINsupport(&supported) && !supported) {
			report.vin.status = VehicleReport::QueryStatus::Unsupported;
			report.vin.source = "SSM";
		}
	}
	report.modules.push_back(currentModule);
	const char* other = report.modules.front().name == "motor" ? "transmision" : "motor";
	for (const char* name : {other, "abs_vdc", "airbag", "tablero"}) {
		VehicleReport::Module module;
		module.name = name;
		report.modules.push_back(module);
	}
	bool accepted = false;
	const int km = QInputDialog::getInt(this, tr("Vehicle report"),
		tr("Dashboard mileage in km (-1 if unknown):"), -1, -1, 2000000000, 1, &accepted);
	if (!accepted) return;
	report.dashboardKm = km;
	std::string json, error;
	if (!VehicleReport::toJson(report, &json, &error)) {
		QMessageBox::critical(this, tr("Error"), QString::fromStdString(error));
		return;
	}
	const QString filename = QFileDialog::getSaveFileName(this, tr("Save status report"), QString(), tr("JSON files (*.json)"));
	if (filename.isEmpty()) return;
	QSaveFile file(filename);
	if (!file.open(QIODevice::WriteOnly) || file.write(json.data(), static_cast<qint64>(json.size())) != static_cast<qint64>(json.size()) || !file.commit()) {
		QMessageBox::critical(this, tr("Error"), tr("Could not save the report."));
		return;
	}
	const QString pdfName = QFileDialog::getSaveFileName(this, tr("Save PDF report"),
		QFileInfo(filename).absolutePath() + "/" + QFileInfo(filename).completeBaseName() + ".pdf", tr("PDF files (*.pdf)"));
	if (!pdfName.isEmpty()) {
		const QFileInfo jsonInfo(filename), pdfInfo(pdfName);
		if (jsonInfo.absoluteFilePath() == pdfInfo.absoluteFilePath() ||
		    (!jsonInfo.canonicalFilePath().isEmpty() && jsonInfo.canonicalFilePath() == pdfInfo.canonicalFilePath())) {
			QMessageBox::warning(this, tr("Vehicle report"), tr("JSON and PDF must use different files."));
			return;
		}
		QString pdfError;
		if (!VehicleReport::writePdf(report, pdfName, &pdfError)) {
			QMessageBox::warning(this, tr("Vehicle report"), tr("JSON saved, but PDF generation failed:") + "\n" + pdfError);
			return;
		}
	}
	QMessageBox::information(this, tr("Vehicle report"), tr("Report saved. Other modules were not queried."));
}
