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
  blk("  and rescue swimmers during a cooperative rescue mission. It   ");
  blk("  receives SWIMMER_ALERT messages from the shoreside, tracks    ");
  blk("  rescued swimmers via RESCUED_SWIMMER, and uses a greedy nearest-");
  blk("  neighbor tour (greedyPath() from lib_geometry).              ");
  blk("                                                                ");
  blk("  Competitive adaptive logic runs predictive sweeps every N     ");
  blk("  seconds, estimating rival time-to-target (TTT) via greedy     ");
  blk("  nearest-neighbor tours from NODE_REPORT data. Swimmers where  ");
  blk("  a rival reaches first are pruned, ensuring efficient division ");
  blk("  of labour. A RESCUE_REGION polygon can further filter alerts. ");
  blk("  The resulting path is published via SURVEY_UPDATE.            ");
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
  blk("  own_speed           = 1.5                                     ");
  blk("  rival_default_speed = 1.5                                     ");
  blk("  update_interval     = 15.0                                    ");
  blk("  max_rival_age       = 30.0                                    ");
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
  blk("  NAV_X           = double (vehicle X position)                 ");
  blk("  NAV_Y           = double (vehicle Y position)                 ");
  blk("  NAV_SPEED       = double (vehicle speed in m/s)               ");
  blk("  NAV_HEADING     = double (vehicle heading in degrees)         ");
  blk("  RESCUED_SWIMMER = id=07, finder=cal                           ");
  blk("  RESCUE_REGION   = pts={-215,-2:-76,-86:-16,6:-79,4}          ");
  blk("  SCOUTED_SWIMMER = id=18, x=-150, y=-50                        ");
  blk("  SWIMMER_ALERT   = x=23, y=54, id=04                           ");
  blk("  UFRM_LEADER     = abe (name of current competition leader)    ");
  blk("  NODE_REPORT     = NAME=alpha,TYPE=UUV,X=51.71,Y=-35.50,SPD=2.0");
  blk("                                                                ");
  blk("PUBLICATIONS:                                                   ");
  blk("------------------------------------                            ");
  blk("  SURVEY_UPDATE  = points=0,0:23,54:-4,95 (XYSegList spec)     ");
  blk("                   (generated via greedyPath() from lib_geometry) ");
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
