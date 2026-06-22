/****************************************************************/
/*   NAME: George Loukas                                        */
/*   ORGN: MIT, Cambridge MA                                    */
/*   FILE: GenRescue_Info.cpp                                   */
/*   DATE: June 22nd, 2026                                      */
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
  blk("  Generates a waypoint path through known swimmer locations     ");
  blk("  for a rescue vehicle. Ingestes SWIMMER_ALERT messages from   ");
  blk("  the shoreside uFldRescueMgr with swimmer coordinates and     ");
  blk("  FOUND_SWIMMER messages indicating rescues. Uses a Self-     ");
  blk("  Organizing Map (SOM) TSP algorithm to plan the optimal       ");
  blk("  visit order, and publishes the result as a GEN_PATH update   ");
  blk("  for the helm BHV_Waypoint behavior.                          ");
  blk("                                                                ");
}

//----------------------------------------------------------------
// Procedure: showHelpAndExit

void showHelpAndExit()
{
  blu("=============================================================== ");
  blu("Usage: pGenRescue file.moos [OPTIONS]                           ");
  blu("=============================================================== ");
  blk("                                                                ");
  showSynopsis();
  blk("                                                                ");
  blk("Options:                                                        ");
  mag("  --alias","=<ProcessName>                                      ");
  blk("      Launch pGenRescue with the given process                  ");
  blk("      name rather than pGenRescue.                              ");
  mag("  --example, -e                                                 ");
  blk("      Display example MOOS configuration block.                 ");
  mag("  --help, -h                                                    ");
  blk("      Display this help message.                                ");
  mag("  --interface, -i                                               ");
  blk("      Display MOOS publications and subscriptions.              ");
  mag("  --version,-v                                                  ");
  blk("      Display the release version of pGenRescue.                ");
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
  cout << "                                                   " << endl;
  cout << "===================================================" << endl;
  cout << "pGenRescue Example MOOS Configuration              " << endl;
  cout << "===================================================" << endl;
  cout << "                                                   " << endl;
  cout << "ProcessConfig = pGenRescue                         " << endl;
  cout << "{                                                  " << endl;
  cout << "  AppTick   = 4                                    " << endl;
  cout << "  CommsTick = 4                                    " << endl;
  cout << "                                                   " << endl;
  cout << "  vname    = abe           // vehicle name         " << endl;
  cout << "}                                                  " << endl;
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
  blk("  SWIMMER_ALERT = x=23, y=54, id=04                            ");
  blk("      Swimner coordinates from shoreside uFldRescueMgr.         ");
  blk("                                                                ");
  blk("  FOUND_SWIMMER = id=01, finder=abe                             ");
  blk("      Notification that a swimmer has been rescued.             ");
  blk("                                                                ");
  blk("  NAV_X = double                                                ");
  blk("      Ownship X position from the simulator or GPS.             ");
  blk("                                                                ");
  blk("  NAV_Y = double                                                ");
  blk("      Ownship Y position from the simulator or GPS.             ");
  blk("                                                                ");
  blk("PUBLICATIONS:                                                   ");
  blk("------------------------------------                            ");
  blk("  GEN_PATH = points = x1,y1:x2,y2:...                          ");
  blk("      Ordered waypoint path update for BHV_Waypoint.            ");
  blk("                                                                ");
  blk("  VIEW_SEGLIST = spec                                           ");
  blk("      Visualization of the planned path in pMarineViewer.       ");
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
