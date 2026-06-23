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
  void predictiveSweep();

private: // Configuration variables

private: // State variables
  struct Swimmer {
    std::string id;
    double      x;
    double      y;
  };

  struct Rival {
    std::string vname;
    double      x;
    double      y;
    double      spd;
    double      timestamp;  // MOOSTime when last NODE_REPORT received
  };

  std::vector<Swimmer>  m_swimmers;
  std::set<std::string> m_rescued_ids;
  bool                  m_path_generated;

  XYPolygon                        m_rescue_region;
  std::map<std::string, Rival>     m_rivals;
  double                           m_last_predict_time;
  double                           m_own_speed;
  double                           m_rival_default_speed;
  double                           m_update_interval;
  double                           m_max_rival_age;

  double m_pos_x;
  double m_pos_y;
};

#endif
