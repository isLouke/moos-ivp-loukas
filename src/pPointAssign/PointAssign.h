/************************************************************/
/*    NAME: George Loukas                                   */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: PointAssign.h                                   */
/*    DATE: December 29th, 1963                             */
/************************************************************/

#ifndef PointAssign_HEADER
#define PointAssign_HEADER

#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"
#include "XYPoint.h"
#include "XYFormatUtilsPoint.h"

class PointAssign : public AppCastingMOOSApp {
public:
  PointAssign();
  ~PointAssign();

protected: // Standard MOOSApp functions to overload
  bool OnNewMail(MOOSMSG_LIST &NewMail);
  bool Iterate();
  bool OnConnectToServer();
  bool OnStartUp();

protected: // Standard AppCastingMOOSApp function to overload
  bool buildReport();

protected:
  void registerVariables();
  void postViewPoint(double x, double y, std::string label, std::string color);

private: // Configuration variables
  int m_points_received;
  int m_points_expected;

private: // State variables
  // std::vector<std::string> m_points;
  std::vector<XYPoint> m_xypoints;
  int median_x;
  bool m_first_point_flag;
  bool m_last_point_flag;
  bool m_result_sent;

  bool m_alternating_mode;
  bool m_regional_mode;

  std::vector<std::string> vname;

};

#endif
