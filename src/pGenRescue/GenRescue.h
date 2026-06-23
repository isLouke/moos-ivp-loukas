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

private: // Configuration variables

private: // State variables
  struct Swimmer {
    std::string id;
    double      x;
    double      y;
  };

  std::vector<Swimmer>  m_swimmers;
  std::set<std::string> m_rescued_ids;
  bool                  m_path_generated;

  double m_pos_x;
  double m_pos_y;
};

#endif
