/************************************************************/
/*    NAME: George Loukas                                   */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: main.cpp                                        */
/*    DATE: June 22nd, 2026                                 */
/************************************************************/

#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>
#include <string>
#include "MBUtils.h"
#include "GenRescue_Info.h"
#include "GenRescue.h"

using namespace std;

int main(int argc, char *argv[])
{
  string mission_file = "";
  string run_command  = argv[0];

  for(int i=1; i<argc; i++) {
    string argi = argv[i];
    if((argi=="-v") || (argi=="--version") || (argi=="-version"))
      showReleaseInfoAndExit();
    else if((argi=="-e") || (argi=="--example") || (argi=="-example"))
      showExampleConfigAndExit();
    else if((argi == "--help")||(argi=="-h"))
      showHelpAndExit();
    else if((argi == "--interface")||(argi=="-i"))
      showInterfaceAndExit();
    else if(strEnds(argi, ".moos") || strEnds(argi, ".moos++"))
      mission_file = argv[i];
    else if(strBegins(argi, "--alias="))
      run_command = argi.substr(8);
    else if(i==2)
     run_command = argi;
  }
  
  if(mission_file == "")
    showHelpAndExit();
  
  cout << "pGenRescue running as: " << run_command << endl;

  GenRescue GenRescue;

  GenRescue.Run(run_command.c_str(), mission_file.c_str());
  
  return(0);
}

