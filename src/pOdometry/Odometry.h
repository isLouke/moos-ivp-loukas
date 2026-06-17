/************************************************************/
/*    NAME: George Loukas                                   */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: Odometry.h                                      */
/*    DATE: December 29th, 1963                             */
/************************************************************/

#ifndef Odometry_HEADER
#define Odometry_HEADER

#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"

class Odometry : public AppCastingMOOSApp {
public:
  Odometry();
  ~Odometry();

protected: // Standard MOOSApp functions to overload
  bool OnNewMail(MOOSMSG_LIST &NewMail);
  bool Iterate();
  bool OnConnectToServer();
  bool OnStartUp();

protected: // Standard AppCastingMOOSApp function to overload
  bool buildReport();

protected:
  void registerVariables();

private: // Configuration variables
  bool m_first_reading;
  double m_current_x;
  double m_current_y;
  double m_previous_x;
  double m_previous_y;
  double m_total_distance;
  double m_depth_thresh;

  // my variables
  double m_incoming_x;
  double m_incoming_y;
  double m_incoming_depth;
  bool m_new_x;
  bool m_new_y;
  double m_tolerance;
  double m_dist_at_depth;

private: // State variables
};

#endif
