/************************************************************/
/*    NAME: George Loukas                                              */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: PointAssign.cpp                                        */
/*    DATE: December 29th, 1963                             */
/************************************************************/

#include "PointAssign.h"
#include "ACTable.h"
#include "MBUtils.h"
#include <iterator>

using namespace std;

//---------------------------------------------------------
// Constructor()

PointAssign::PointAssign() {
  // I will save the received and through an warning if > 100
  m_points_received = 0;
  // Flags to indicate if i received the first and last point
  m_first_point_flag = false;
  m_last_point_flag = false;
  m_result_sent = false;
  median_x = 0;
}

//---------------------------------------------------------
// Destructor

PointAssign::~PointAssign() {}

//---------------------------------------------------------
// Procedure: OnNewMail()

bool PointAssign::OnNewMail(MOOSMSG_LIST &NewMail) {
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for (p = NewMail.begin(); p != NewMail.end(); p++) {
    CMOOSMsg &msg = *p;
    string key = msg.GetKey();
    string sval  = msg.GetString(); 

#if 0 // Keep these around just for template
    string comm  = msg.GetCommunity();
    double dval  = msg.GetDouble();
    string sval  = msg.GetString(); 
    string msrc  = msg.GetSource();
    double mtime = msg.GetTime();
    bool   mdbl  = msg.IsDouble();
    bool   mstr  = msg.IsString();
#endif

    if (key == "VISIT_POINT") {
      if (sval == "firstpoint")
        m_first_point_flag = true;
      else if (sval == "lastpoint") {
        m_last_point_flag = true;
        Notify("POINTS_RECEIVED", m_points_received);
      } else {

        // handle the incoming points here
        XYPoint new_point = string2Point(sval);
        m_xypoints.push_back(new_point);

        // count the incoming points
        m_points_received++;

        if (m_points_received > m_points_expected) {
          // This should be an error, but for now I will just report a warning
          reportRunWarning(
              "Received more points than expected for lab 07 (" + to_string(m_points_expected) + "): " +
              to_string(m_points_received));
        }
      }
    }

    else if (key != "APPCAST_REQ") // handled by AppCastingMOOSApp
      reportRunWarning("Unhandled Mail: " + key);
  }

  return (true);
}

//---------------------------------------------------------
// Procedure: OnConnectToServer()

bool PointAssign::OnConnectToServer() {
  registerVariables();
  return (true);
}

//---------------------------------------------------------
// Procedure: Iterate()
//            happens AppTick times per second

bool PointAssign::Iterate() {
  AppCastingMOOSApp::Iterate();

  if (m_last_point_flag && !m_result_sent) {

    for (size_t i = 0; i < vname.size(); i++) {
      Notify("VISIT_POINT_" + toupper(vname[i]), "firstpoint");
    }

    if (m_alternating_mode && !vname.empty()) {
    // Alternating Mode: Assign points to N vehicles in an alternating fashion
    for (int i = 0; i < m_xypoints.size(); i++) {
      string vehicle = vname[i % vname.size()];
      string color = (i % 2 == 0) ? "yellow" : "red";
      
      // Explicitly format the xy point instead of using get_spec(), adding id in case behaviors expect it
      string point_str = "x=" + to_string(m_xypoints[i].x()) + ",y=" + to_string(m_xypoints[i].y()) + ",id=" + to_string(i + 1);
      
      Notify("VISIT_POINT_" + toupper(vehicle), point_str);
      postViewPoint(m_xypoints[i].x(), m_xypoints[i].y(), to_string(i + 1), color);
    }
  }

  if (m_regional_mode && !vname.empty()) {
    // Dynamically calculate the average x-coordinate
    double sum_x = 0;
    for (int i = 0; i < m_xypoints.size(); i++) {
      sum_x += m_xypoints[i].x();
    }
    median_x = m_xypoints.size() > 0 ? (sum_x / m_xypoints.size()) : 0;

    // Regional Mode: Assign points based on whether x is less or greater than average
    for (int i = 0; i < m_xypoints.size(); i++) {
      string point_str = "x=" + to_string(m_xypoints[i].x()) + ",y=" + to_string(m_xypoints[i].y()) + ",id=" + to_string(i + 1);

      if (vname.size() >= 2) {
        if (m_xypoints[i].x() < median_x) {
          Notify("VISIT_POINT_" + toupper(vname[0]), point_str);
          postViewPoint(m_xypoints[i].x(), m_xypoints[i].y(), to_string(i + 1), "yellow");
        } else {
          Notify("VISIT_POINT_" + toupper(vname[1]), point_str);
          postViewPoint(m_xypoints[i].x(), m_xypoints[i].y(), to_string(i + 1), "red");
        }
      } else {
        Notify("VISIT_POINT_" + toupper(vname[0]), point_str);
        postViewPoint(m_xypoints[i].x(), m_xypoints[i].y(), to_string(i + 1), "yellow");
      }
    }
  }

    for (size_t i = 0; i < vname.size(); i++) {
      Notify("VISIT_POINT_" + toupper(vname[i]), "lastpoint");
    }
    
    m_result_sent = true;
    
  }

  Notify("PointAssignReady", "true"); // Notify the timer script that we are ready for input

  AppCastingMOOSApp::PostReport();
  return (true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()
//            happens before connection is open

bool PointAssign::OnStartUp() {
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
    } else if (param == "total_points") {
      m_points_expected = stod(value);
      handled = true;
    }
    else if (param == "assignment_mode") {
      if (value == "alternating") {
        m_alternating_mode = true;
        m_regional_mode = false;
      } else if (value == "regional") {
        m_alternating_mode = false;
        m_regional_mode = true;
      } else {
        reportConfigWarning("Invalid assignment_mode: " + value);
      }
      handled = true;
    }
    else if (param == "median_x") {
      median_x = stod(value);
      handled = true;
    }
    else if (param == "vname") {
      vname = parseString(value, ',');
      handled = true;
    }

    else

    if (!handled)
      reportUnhandledConfigWarning(orig);
  }

  registerVariables();
  return (true);
}

//---------------------------------------------------------
// Procedure: registerVariables()

void PointAssign::registerVariables() {
  AppCastingMOOSApp::RegisterVariables();
  Register("VISIT_POINT", 0);
}

//---------------------------------------------------------
// Procedure: postViewPoint()

void PointAssign::postViewPoint(double x, double y, string label, string color)
{
  XYPoint point(x, y);
  point.set_label(label);
  point.set_color("vertex", color);  // yellow is handy on dark screen 
  point.set_param("vertex_size", "4");

  string spec = point.get_spec();    // gets the string representation of a point
  Notify("VIEW_POINT", spec);
}

//------------------------------------------------------------
// Procedure: buildReport()

bool PointAssign::buildReport() {
  m_msgs << "Points received: " << m_points_received << endl;
  m_msgs << "Points expected: " << m_points_expected << endl;
  m_msgs << "Status: " << endl;
  m_msgs << "First point received: " << boolToString(m_first_point_flag) << endl;
  m_msgs << "Last point received: " << boolToString(m_last_point_flag) << endl;
  m_msgs << "Total points received: " << m_points_received << endl;
  m_msgs << "Total points: " << m_xypoints.size() << endl;
  m_msgs << "Assignment mode: " << (m_alternating_mode ? "Alternating" : (m_regional_mode ? "Regional" : "None")) << endl;
  // Print the names of the vehicles
  for (size_t i = 0; i < vname.size(); i++) {
    m_msgs << "Vehicle name " << (i + 1) << ": " << vname[i] << endl;
  }
  // Print the median_x if in regional mode
  if (m_regional_mode) {
    m_msgs << "Median x-coordinate for regional assignment: " << median_x << endl;
  }
  return (true);
}
