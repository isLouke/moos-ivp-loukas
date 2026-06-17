/************************************************************/
/*    NAME: George Loukas                                   */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: Odometry.cpp                                    */
/*    DATE: December 29th, 1963                             */
/************************************************************/

#include "Odometry.h"
#include "ACTable.h"
#include "MBUtils.h"
#include <iterator>

using namespace std;

//---------------------------------------------------------
// Constructor()

Odometry::Odometry() {
  m_first_reading = true;
  m_current_x = 0.;
  m_current_y = 0.;
  m_previous_x = 0.;
  m_previous_y = 0.;
  m_total_distance = 0.;

  // my variables
  m_incoming_x = 0.;
  m_incoming_y = 0.;
  m_incoming_depth = 0.;
  m_new_x = false;
  m_new_y = false;
  m_tolerance = 0.1; // Minimum distance to consider a new reading
  m_dist_at_depth = 0.;
}

//---------------------------------------------------------
// Destructor

Odometry::~Odometry() {}

//---------------------------------------------------------
// Procedure: OnNewMail()

bool Odometry::OnNewMail(MOOSMSG_LIST &NewMail) {
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for (p = NewMail.begin(); p != NewMail.end(); p++) {
    CMOOSMsg &msg = *p;
    string key = msg.GetKey();

#if 0 // Keep these around just for template
    string comm  = msg.GetCommunity();
    double dval  = msg.GetDouble();
    string sval  = msg.GetString(); 
    string msrc  = msg.GetSource();
    double mtime = msg.GetTime();
    bool   mdbl  = msg.IsDouble();
    bool   mstr  = msg.IsString();
#endif

    // Check if the mail is for NAV_X
    if (key == "NAV_X") {
      if (msg.IsDouble()) {
        m_incoming_x = msg.GetDouble();
        m_new_x = true;
      }
    }
    // Check if the mail is for NAV_Y
    else if (key == "NAV_Y") {
      if (msg.IsDouble()) {
        m_incoming_y = msg.GetDouble();
        m_new_y = true;
      }
    } else if (key == "NAV_DEPTH") {
      if (msg.IsDouble()) {
        m_incoming_depth = msg.GetDouble();
      }
    } else if (key != "APPCAST_REQ") { // handled by AppCastingMOOSApp
      reportRunWarning("Unhandled Mail: " + key);
    }
  }

  return (true);
}

//---------------------------------------------------------
// Procedure: OnConnectToServer()

bool Odometry::OnConnectToServer() {
  registerVariables();
  return (true);
}

//---------------------------------------------------------
// Procedure: Iterate()
//            happens AppTick times per second

bool Odometry::Iterate() {
  AppCastingMOOSApp::Iterate();
  // Do your thing here!

  // Initialize on the very first valid X and Y reading
  if (m_first_reading && m_new_x && m_new_y) {
    m_previous_x = m_incoming_x;
    m_previous_y = m_incoming_y;

    m_new_x = false;
    m_new_y = false;
    m_first_reading = false;
    Notify("ODOMETRY_DIST", 0.0); // Publish initial distance
    Notify("ODOMETRY_DIST_AT_DEPTH",
           0.0); // Publish initial distance at depth
  }
  // For all subsequent readings
  else if (!m_first_reading && m_new_x && m_new_y) {

    // Calculate the distance moved since the LAST reading
    // Same as  sqrt((x2 - x1)^2 + (y2 - y1)^2)
    double dist_delta =
        std::hypot(m_incoming_x - m_previous_x, m_incoming_y - m_previous_y);

    // Only update and publish if the movement exceeds the noise tolerance
    if (dist_delta > m_tolerance) {
      m_total_distance += dist_delta;

      if (m_incoming_depth > m_depth_thresh) {
        m_dist_at_depth += dist_delta;
      }

      // Update previous coordinates for the next iteration
      m_previous_x = m_incoming_x;
      m_previous_y = m_incoming_y;

      // Publish the total distance to the MOOSDB
      Notify("ODOMETRY_DIST", m_total_distance);
      Notify("ODOMETRY_DIST_AT_DEPTH", m_dist_at_depth);
    }

    // Reset flags
    m_new_x = false;
    m_new_y = false;
  }

  AppCastingMOOSApp::PostReport();
  return (true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()
//            happens before connection is open

bool Odometry::OnStartUp() {
  AppCastingMOOSApp::OnStartUp();

  STRING_LIST sParams;
  m_MissionReader.EnableVerbatimQuoting(false);
  if (!m_MissionReader.GetConfiguration(GetAppName(), sParams))
    reportConfigWarning("No config block found for " + GetAppName());

  STRING_LIST::iterator p;
  for (p = sParams.begin(); p != sParams.end(); p++) {
    string orig = *p;
    string line = *p;
    string param = tolower(biteStringX(line, '='));
    string value = line;

    bool handled = false;
    if (param == "foo") {
      handled = true;
    } else if (param == "depth_thresh") {
      m_depth_thresh = stod(value);
      handled = true;
    } else if (param == "bar") {
      handled = true;
    }

    if (!handled)
      reportUnhandledConfigWarning(orig);
  }

  registerVariables();
  return (true);
}

//---------------------------------------------------------
// Procedure: registerVariables()

void Odometry::registerVariables() {
  AppCastingMOOSApp::RegisterVariables();
  // Register("FOOBAR", 0);
  Register("NAV_X", 0);
  Register("NAV_Y", 0);
  Register("NAV_DEPTH", 0);
}

//------------------------------------------------------------
// Procedure: buildReport()

bool Odometry::buildReport() {
  m_msgs << "============================================" << endl;
  m_msgs << "File:                                       " << endl;
  m_msgs << "============================================" << endl;

  m_msgs << "Total Distance Traveled: " << m_total_distance << endl;
  m_msgs << "Distance Traveled at Depth: " << m_dist_at_depth << endl;
  return (true);
}
