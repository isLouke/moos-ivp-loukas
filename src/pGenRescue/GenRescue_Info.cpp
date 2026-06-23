/****************************************************************/
/*   NAME: George Loukas                                             */
/*   ORGN: MIT, Cambridge MA                                    */
/*   FILE: GenRescue_Info.cpp                               */
/*   DATE: December 29th, 1963                                  */
/****************************************************************/

#include <cstdlib>
#include <iostream>
#include "GenRescue_Info.h"
#include "ColorParse.h"
#include "ReleaseInfo.h"

using namespace std;

//----------------------------------------------------------------
// Procedure: showSynopsis

void showSynopsis()
{
  blk("SYNOPSIS:                                                       ");
  blk("------------------------------------                            ");
  blk("  The pGenRescue application plans an optimal path to visit     ");
  blk("  and rescue swimmers during a rescue mission. It receives      ");
  blk("  SWIMMER_ALERT messages from the shoreside (x=..., y=...,      ");
  blk("  id=...), tracks which swimmers have been rescued via           ");
  blk("  FOUND_SWIMMER messages, and uses a Self-Organizing Map (SOM)  ");
  blk("  to generate an efficient TSP tour. The resulting waypoint     ");
  blk("  path is published via SURVEY_UPDATE to the helm's BHV_Waypoint");
  blk("  behavior, enabling dynamic re-planning as new alerts arrive.  ");
  blk("                                                                ");
}

//----------------------------------------------------------------
// Procedure: showHelpAndExit

void showHelpAndExit()
{
  blk("                                                                ");
  blu("=============================================================== ");
  blu("Usage: pGenRescue file.moos [OPTIONS]                   ");
  blu("=============================================================== ");
  blk("                                                                ");
  showSynopsis();
  blk("                                                                ");
  blk("Options:                                                        ");
  mag("  --alias","=<ProcessName>                                      ");
  blk("      Launch pGenRescue with the given process name         ");
  blk("      rather than pGenRescue.                           ");
  mag("  --example, -e                                                 ");
  blk("      Display example MOOS configuration block.                 ");
  mag("  --help, -h                                                    ");
  blk("      Display this help message.                                ");
  mag("  --interface, -i                                               ");
  blk("      Display MOOS publications and subscriptions.              ");
  mag("  --version,-v                                                  ");
  blk("      Display the release version of pGenRescue.        ");
  blk("                                                                ");
  blk("Note: If argv[2] does not otherwise match a known option,       ");
  blk("      then it will be interpreted as a run alias. This is       ");
  blk("      to support pAntler launching conventions.                 ");
  blk("                                                                ");
  exit(0);
}

//----------------------------------------------------------------
// Procedure: showExampleConfigAndExit

void showExampleConfigAndExit()
{
  blk("                                                                ");
  blu("=============================================================== ");
  blu("pGenRescue Example MOOS Configuration                   ");
  blu("=============================================================== ");
  blk("                                                                ");
  blk("ProcessConfig = pGenRescue                              ");
  blk("{                                                               ");
  blk("  AppTick   = 4                                                 ");
  blk("  CommsTick = 4                                                 ");
  blk("                                                                ");
  blk("}                                                               ");
  blk("                                                                ");
  exit(0);
}


//----------------------------------------------------------------
// Procedure: showInterfaceAndExit

void showInterfaceAndExit()
{
  blk("                                                                ");
  blu("=============================================================== ");
  blu("pGenRescue INTERFACE                                    ");
  blu("=============================================================== ");
  blk("                                                                ");
  showSynopsis();
  blk("                                                                ");
  blk("SUBSCRIPTIONS:                                                  ");
  blk("------------------------------------                            ");
  blk("  SWIMMER_ALERT  = x=23, y=54, id=04                            ");
  blk("  FOUND_SWIMMER  = id=01, finder=abe                            ");
  blk("  NAV_X          = double (vehicle X position)                  ");
  blk("  NAV_Y          = double (vehicle Y position)                  ");
  blk("                                                                ");
  blk("PUBLICATIONS:                                                   ");
  blk("------------------------------------                            ");
  blk("  SURVEY_UPDATE  = points=0,0:23,54:-4,95 (XYSegList spec)     ");
  blk("                                                                ");
  exit(0);
}

//----------------------------------------------------------------
// Procedure: showReleaseInfoAndExit

void showReleaseInfoAndExit()
{
  showReleaseInfo("pGenRescue", "gpl");
  exit(0);
}
