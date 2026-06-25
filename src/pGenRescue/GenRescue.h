/************************************************************/
/*    NAME: George Loukas                                              */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: GenRescue.h                                          */
/*    DATE: December 29th, 1963                             */
/************************************************************/

#ifndef GenRescue_HEADER
#define GenRescue_HEADER

#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"
#include "XYPoint.h"
#include "XYFormatUtilsPoint.h"
#include "XYSegList.h"
#include "XYPolygon.h"
#include <map>
#include <set>

class GenRescue : public AppCastingMOOSApp
{
public:
  GenRescue();
  ~GenRescue();

protected: // Standard MOOSApp functions to overload
  bool OnNewMail(MOOSMSG_LIST &NewMail);
  bool Iterate();
  bool OnConnectToServer();
  bool OnStartUp();

protected: // Standard AppCastingMOOSApp function to overload
  bool buildReport();

protected:
  void registerVariables();
  void generatePath();
  bool predictiveSweep();

private: // Configuration variables
private: // State variables
  struct Swimmer
  {
    int id;
    double x;
    double y;
    double target_weight;
    bool rescued;
    bool ignored;
    std::string finder;
  };

  struct State
  {
    std::string name;
    double x;
    double y;
    double speed;
    double heading;
    double timestamp;
    bool valid;
    bool friendly;
  };

  // Swimmers tracking
  std::vector<Swimmer> m_swimmers;

  // States
  State m_my_state;
  std::vector<State> m_rivals;

  std::string m_scout_name;
  std::string m_scout_rival;

  XYPolygon m_rescue_region;
};

#endif
