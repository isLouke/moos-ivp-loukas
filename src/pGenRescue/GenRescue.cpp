/************************************************************/
/*    NAME: George Loukas                                              */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: GenRescue.cpp                                        */
/*    DATE: December 29th, 1963                             */
/************************************************************/

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <random>
#include <set>
#include <utility>
#include <vector>
#include "MBUtils.h"
#include "ACTable.h"
#include "XYFormatUtilsPoly.h"
#include "PathUtils.h"
#include "AngleUtils.h"
#include "GenRescue.h"

using namespace std;

//---------------------------------------------------------
// Constructor()

GenRescue::GenRescue()
{
  // My Vehicle State
  m_my_state.x = 0;
  m_my_state.y = 0;
  m_my_state.speed = 0;
  m_my_state.heading = 0;
  m_my_state.timestamp = 0;
  m_my_state.valid = false;
  m_my_state.friendly = true;
  m_my_state.name = "luke";
}

//---------------------------------------------------------
// Destructor

GenRescue::~GenRescue()
{
}

//---------------------------------------------------------
// Procedure: OnNewMail()

bool GenRescue::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for (p = NewMail.begin(); p != NewMail.end(); p++)
  {
    CMOOSMsg &msg = *p;
    string key = msg.GetKey();
    string sval = msg.GetString();
    double dval = msg.GetDouble();

    // uFldRescueMgr usefull publications
    //
    // FOUND_SWIMMER = id=01, finder=abe                              DEPRECATED. Use RESCUED_SWIMMER
    // RESCUED_SWIMMER = id=07, finder=cal                            We update the found swimmer's
    // RESCUE_REGION = pts={-215,-2:-16,6:-76,-86},edge_color=gray90,vertex_color=dodger_blue,vertex_size=5
    // SCOUTED_SWIMMER_BEN = id=18, x=-150, y=-50
    // SWIMMER_ALERT_ABE = x=23, y=54, id=04
    // SWIMMER_ALERT_BEN = x=23, y=54, id=04
    // UFRM_LEADER = abe                                              We can track who is winning the competition and adjust our strategy accordingly.
    //
    // uFldNodeComms usefull publications
    // NODE_REPORT_ABE = NAME=abe,X=-100,Y=-10,SPD=2.0,HDG=100.0 ...  We get the position of other vehicles.

    if (key == "NAV_X") // Checked OK
    {
      m_my_state.x = dval;
    }
    else if (key == "NAV_Y") // Checked OK
    {
      m_my_state.y = dval;
    }
    else if (key == "NAV_SPEED") // Checked OK
    {
      m_my_state.speed = dval;
    }
    else if (key == "NAV_HEADING") // Checked OK
    {
      m_my_state.heading = dval;
    }
    else if (key == "RESCUED_SWIMMER") // RESCUED_SWIMMER = id=07, finder=cal
    {
      // Format: id=07, finder=cal
      string id = tokStringParse(sval, "id", ',', '=');
      string finder = tokStringParse(sval, "finder", ',', '=');

      if (!m_swimmers.empty())
      {
        bool known_swimmer = false;
        for (int i = 0; i < m_swimmers.size(); ++i)
        {
          if (m_swimmers[i].id == stoi(id))
          {
            m_swimmers[i].rescued = true;
            m_swimmers[i].finder = finder;
            known_swimmer = true;
            reportEvent("Swimmer rescued: id=" + id + ", finder=" + finder + ". Good Job!");
            break;
          }
        }
        if (!known_swimmer)
        {
          reportEvent("RESCUED_SWIMMER for unknown id=" + id + ", finder=" + finder + ". Good Job!");
        }
      }
    }
    else if (key == "RESCUE_REGION") // Checked OK
    {
      XYPolygon poly = string2Poly(sval);
      if (poly.is_convex())
        m_rescue_region = poly;
    }
    else if (key == "SCOUTED_SWIMMER")
    {
      // Format: id=18, x=-150, y=-50
    }
    else if (key == "SWIMMER_ALERT")
    {
      // Format: x=23, y=54, id=04
      string id = tokStringParse(sval, "id", ',', '=');
      string xpos = tokStringParse(sval, "x", ',', '=');
      string ypos = tokStringParse(sval, "y", ',', '=');

      // Validate the parsed values

      if (id.empty() || xpos.empty() || ypos.empty())
      {
        reportRunWarning("Malformed SWIMMER_ALERT: " + sval);
        continue;
      }

      Swimmer s;
      s.id = stoi(id);
      s.x = stod(xpos);
      s.y = stod(ypos);
      s.rescued = false;
      s.ignored = false;
      s.target_weight = 1.0; // Default weight

      // Add the first swimmer
      // RUNS ONLY ONCE TO POPULATE THE SWIMMER LIST.
      if (m_swimmers.empty())
      {
        m_swimmers.push_back(s);
      }

      // 1.   Check if the swimmer is already in the list.
      // 1.1    If not add him.
      // 1.2    If he exists
      // 2.   Additional Check will be implemented in Iterate()

      if (!m_swimmers.empty()) // Add if statement for clarity, it is not needed
      {
        bool found = false;
        for (int i = 0; i < m_swimmers.size(); ++i) // Loop the swimmer list
        {
          // Check if the swimmer exists
          if (m_swimmers[i].id == s.id)
          {
            found = true;
          }
        }

        if (!found)
        {
          m_swimmers.push_back(s);
          reportEvent("New swimmer alert: id=" + id + ", x=" + xpos + ", y=" + ypos);
        }
      }
    }
    else if (key == "UFRM_LEADER")
    {
      // Format: abe
    }
    else if (key == "NODE_REPORT")
    {
      // Format : NAME=abe,X=-100,Y=-10,SPD=2.0,HDG=100.0 ...

      string vname = tokStringParse(sval, "NAME", ',', '=');
      string xstr = tokStringParse(sval, "X", ',', '=');
      string ystr = tokStringParse(sval, "Y", ',', '=');
      string spdstr = tokStringParse(sval, "SPD", ',', '=');
      string hdgstr = tokStringParse(sval, "HDG", ',', '=');
      string typestr = tokStringParse(sval, "TYPE", ',', '=');

      // Skip own reports and malformed messages
      if (vname.empty() || vname == GetAppName() || xstr.empty() || ystr.empty())
        continue;

      double x = stod(xstr);
      double y = stod(ystr);
      double spd = spdstr.empty() ? 0.0 : stod(spdstr);
      double hdg = hdgstr.empty() ? 0.0 : stod(hdgstr);

      State rival;
      rival.name = vname;
      rival.x = x;
      rival.y = y;
      rival.speed = spd;
      rival.heading = hdg;
      rival.timestamp = MOOSTime();
      rival.valid = true;
      rival.friendly = (typestr == "heron") ? true : false;

      // Check if rival exists, update if so, add if new
      bool found = false;
      for (int i = 0; i < m_rivals.size(); ++i)
      {
        if (m_rivals[i].name == rival.name)
        {
          m_rivals[i] = rival; // Update existing rival
          found = true;
          break;
        }
      }

      if (!found)
        m_rivals.push_back(rival);

      reportEvent("Rival updated: " + vname + " x=" + doubleToString(x, 1) + " y=" + doubleToString(y, 1) + " spd=" + doubleToString(spd, 1));
    }
    else if (key != "APPCAST_REQ") // handled by AppCastingMOOSApp
      reportRunWarning("Unhandled Mail: " + key);
  }
  return (true);
}

//---------------------------------------------------------
// Procedure: OnConnectToServer()

bool GenRescue::OnConnectToServer()
{
  registerVariables();
  return (true);
}

//---------------------------------------------------------
// Procedure: Iterate()
//            happens AppTick times per second

bool GenRescue::Iterate()
{
  AppCastingMOOSApp::Iterate();

  // 1 Check the position. Update if changed.
  // 2 Check if the swimmer is within the rescue region. If not, ignore.
  // 3 Make sure the swimmer is not already rescued or ignored.

  if (m_swimmers.size() > 0)
  {
    for (int i = 0; i < m_swimmers.size(); ++i)
    {
      // Check if the swimmer is already rescued or should be ignored.
      if (m_swimmers[i].rescued || m_swimmers[i].ignored)
        continue;

      // Check if the swimmer is within the rescue region
      if (m_rescue_region.is_convex() && !m_rescue_region.contains(m_swimmers[i].x, m_swimmers[i].y))
      {
        reportEvent("Ignoring swimmer id=" + intToString(m_swimmers[i].id) + " outside rescue region");
        m_swimmers[i].ignored = true;
      }
    }
  }

  // Calculate Target Weights based on TTT and Heading Intent
  for (int i = 0; i < m_swimmers.size(); ++i)
  {
    if (m_swimmers[i].rescued || m_swimmers[i].ignored)
    {
      m_swimmers[i].target_weight = -9999.0; // Sink completed/ignored swimmers to the bottom
      continue;
    }

    // Calculate MY Time To Target & Heading Intent for Swimmer i
    //
    double dist_mine = hypot(m_my_state.x - m_swimmers[i].x, m_my_state.y - m_swimmers[i].y);
    double ttt_mine = (m_my_state.speed > 0) ? dist_mine / m_my_state.speed : 9999.0;

    double my_angle_to_swimmer = relAng(m_my_state.x, m_my_state.y,
                                        m_swimmers[i].x, m_swimmers[i].y);
    double my_angle_diff = angleDiff(my_angle_to_swimmer, m_my_state.heading);

    // My base score: High if TTT is low. Penalized slightly if I have to turn around.
    // heading_factor ranges from 1.0 (perfectly aligned) to 0.0 (pointed opposite way)
    double my_heading_factor = 1.0 - (my_angle_diff / 180.0);
    double my_base_score = (1000.0 / (ttt_mine + 1.0)) * (0.5 + 0.5 * my_heading_factor);

    // Evaluate ALL Rivals to find the Maximum Threat
    //
    double max_rival_threat = 0.0;

    for (int j = 0; j < m_rivals.size(); ++j)
    {

      double dist_rival = hypot(m_rivals[j].x - m_swimmers[i].x, m_rivals[j].y - m_swimmers[i].y);
      double ttt_rival = (m_rivals[j].speed > 0) ? dist_rival / m_rivals[j].speed : 9999.0;

      double rival_angle_to_swimmer = relAng(m_rivals[j].x, m_rivals[j].y,
                                             m_swimmers[i].x, m_swimmers[i].y);
      double rival_angle_diff = angleDiff(rival_angle_to_swimmer, m_rivals[j].heading);

      // Rival threat: High if TTT is low AND they are pointed directly at it
      double rival_heading_factor = 1.0 - (rival_angle_diff / 180.0);
      double current_rival_threat = (1000.0 / (ttt_rival + 1.0)) * rival_heading_factor;

      if (m_rivals[j].friendly)
        current_rival_threat = 0.0; // Ignore friendly rivals
      continue;

      // Keep the highest threat score among all rivals for this specific swimmer
      if (current_rival_threat > max_rival_threat)
      {
        max_rival_threat = current_rival_threat;
      }
    }

    // Final Combined Weight
    m_swimmers[i].target_weight = my_base_score - max_rival_threat;
  }

  // Sort swimmers by target_weight in descending order (highest threat/priority first)
  std::sort(m_swimmers.begin(), m_swimmers.end(),
            [](const Swimmer &a, const Swimmer &b)
            {
              return a.target_weight > b.target_weight;
            });

  // Regenerate path based on flag
  generatePath();

  AppCastingMOOSApp::PostReport();
  return (true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()
//            happens before connection is open

bool GenRescue::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  STRING_LIST sParams;
  m_MissionReader.EnableVerbatimQuoting(false);
  if (!m_MissionReader.GetConfiguration(GetAppName(), sParams))
    reportConfigWarning("No config block found for " + GetAppName());

  STRING_LIST::iterator p;
  for (p = sParams.begin(); p != sParams.end(); p++)
  {
    string orig = *p;
    string line = *p;
    string param = tolower(biteStringX(line, '='));
    string value = line;

    bool handled = false;
    if (param == "scout_name")
    {
      m_scout_name = value;
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

void GenRescue::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  Register("NAV_X", 0);
  Register("NAV_Y", 0);
  Register("NAV_SPEED", 0);
  Register("NAV_HEADING", 0);
  Register("RESCUED_SWIMMER", 0);
  Register("RESCUE_REGION", 0);
  Register("SCOUTED_SWIMMER", 0);
  Register("SWIMMER_ALERT", 0);
  Register("UFRM_LEADER", 0);
  Register("NODE_REPORT", 0);
}

//------------------------------------------------------------
// Procedure: buildReport()

bool GenRescue::buildReport()
{
  // Report App Configuration
  m_msgs << "==============================================" << endl;
  m_msgs << "                 Configuration                " << endl;
  m_msgs << "==============================================" << endl;
  m_msgs << "  scout_name: " << m_scout_name << endl;
  m_msgs << endl;

  // Report My State
  m_msgs << "==============================================" << endl;
  m_msgs << "                   My State                   " << endl;
  m_msgs << "==============================================" << endl;
  m_msgs << "  Name:     " << m_my_state.name << endl;
  m_msgs << "  X:        " << doubleToString(m_my_state.x, 2) << endl;
  m_msgs << "  Y:        " << doubleToString(m_my_state.y, 2) << endl;
  m_msgs << "  Speed:    " << doubleToString(m_my_state.speed, 2) << endl;
  m_msgs << "  Heading:  " << doubleToString(m_my_state.heading, 2) << endl;
  m_msgs << endl;

  // Report Rivals
  m_msgs << "==============================================" << endl;
  m_msgs << "                    Others                    " << endl;
  m_msgs << "==============================================" << endl;

  if (m_rivals.size() < 1)
  {
    m_msgs << "None" << endl;
  }
  else
  {
    for (int i = 0; i < m_rivals.size(); ++i)
    {
      m_msgs << "  Name:     " << m_rivals[i].name << endl;
      m_msgs << "  X:        " << doubleToString(m_rivals[i].x, 2) << endl;
      m_msgs << "  Y:        " << doubleToString(m_rivals[i].y, 2) << endl;
      m_msgs << "  Speed:    " << doubleToString(m_rivals[i].speed, 2) << endl;
      m_msgs << "  Heading:  " << doubleToString(m_rivals[i].heading, 2) << endl;
      m_msgs << "  Friendly: " << (m_rivals[i].friendly ? "yes" : "no") << endl;
      m_msgs << "-------------------------------------" << endl;
    }
  }
  // Report Swimmers
  m_msgs << "==============================================" << endl;
  m_msgs << "                   Swimmers                   " << endl;
  m_msgs << "==============================================" << endl;
  if (m_swimmers.empty())
  {
    m_msgs << "  none" << endl;
    m_msgs << endl;
  }
  else
  {
    ACTable actab(7);
    actab << "id" << "X" << "Y" << "Rescued" << "Ignored"
          << "Finder" << "Target Weight";
    actab.addHeaderLines();
    for (unsigned int i = 0; i < m_swimmers.size(); i++)
    {
      actab << intToString(m_swimmers[i].id)
            << doubleToString(m_swimmers[i].x, 2)
            << doubleToString(m_swimmers[i].y, 2)
            << (m_swimmers[i].rescued ? "yes" : "no")
            << (m_swimmers[i].ignored ? "yes" : "no")
            << m_swimmers[i].finder
            << doubleToString(m_swimmers[i].target_weight, 2);
    }
    m_msgs << actab.getFormattedString();
  }

  return (true);
}

//------------------------------------------------------------
// Procedure: generatePath()
//            Builds a greedy nearest-neighbor tour over active
//            (non-rescued, non-ignored) swimmers using the
//            greedyPath() utility from lib_geometry, then
//            publishes the path via SURVEY_UPDATE.

void GenRescue::generatePath()
{
  // Collect active swimmer positions into an XYSegList
  XYSegList segl;
  for (int i = 0; i < m_swimmers.size(); i++)
  {
    if (!m_swimmers[i].rescued && !m_swimmers[i].ignored)
    {
      segl.add_vertex(m_swimmers[i].x, m_swimmers[i].y);
    }
  }

  if (segl.size() == 0)
  {
    reportEvent("generatePath: no active swimmers to visit");
    return;
  }

  // Start greedy tour from the vehicle's current position
  XYSegList tour = greedyPath(segl, m_my_state.x, m_my_state.y);

  Notify("SURVEY_UPDATE", "points=" + tour.get_spec());
}
