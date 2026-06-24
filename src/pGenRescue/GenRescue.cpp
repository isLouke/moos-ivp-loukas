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
#include "GenRescue.h"

using namespace std;

//---------------------------------------------------------
// Constructor()

GenRescue::GenRescue()
{
  // Flag to trigger path generation
  m_generate_path = true;

  // My Vehicle State
  m_my_state.x = 0;
  m_my_state.y = 0;
  m_my_state.speed = 0;
  m_my_state.heading = 0;
  m_my_state.timestamp = 0;
  m_my_state.valid = false;
  m_my_state.friendly = true;
  m_my_state.name = "Luke Skywalker";
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

      // Skip own reports and malformed messages
      if (vname == GetAppName() || xstr.empty() || ystr.empty())
        continue;

      double x = stod(xstr);
      double y = stod(ystr);
      double spd = stod(spdstr);
      double hdg = stod(hdgstr);

      State rival;
      rival.name = vname;
      rival.x = x;
      rival.y = y;
      rival.speed = spd;
      rival.heading = hdg;
      rival.timestamp = MOOSTime();
      rival.valid = true;
      rival.friendly = false;

      // Check if rival exists
      if (m_rivals.empty())
      {
        m_rivals.push_back(rival);
      }
      else
      {
        for (int i = 0; i < m_rivals.size(); ++i)
        {
          if (m_rivals[i].name == rival.name)
          {
            m_rivals[i] = rival; // Update existing rival
            break;
          }
          else if (i == m_rivals.size() - 1)
          {
            m_rivals.push_back(rival); // Add new rival
            break;
          }
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

  // 1.2.1    Check the position. Update if changed.
  // 1.2.2    Check if the swimmer is within the rescue region. If not, ignore.
  // 1.2.3    Make sure the swimmer is not already rescued or ignored.

  // // Check if the swimmer is within the rescue region
  //   if (m_rescue_region.is_convex() && !m_rescue_region.contains(s.x, s.y))
  //   {
  //     reportEvent("Ignoring swimmer id=" + id + " outside rescue region");
  //     s.ignored = true;
  //     continue;
  //   }

  // // Regenerate path whenever the swimmer set changes
  // if (!m_path_generated && !m_swimmers.empty())
  //   generatePath();

  // // Periodic adaptive re-planning based on rival positions
  // double elapsed = MOOSTime() - m_last_predict_time;
  // if (elapsed >= m_update_interval)
  // {
  //   m_last_predict_time = MOOSTime();
  //   predictiveSweep();
  //   m_path_generated = false;
  // }

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

  // Report My State
  m_msgs << "==============================================" << endl;
  m_msgs << "                   My State                   " << endl;
  m_msgs << "==============================================" << endl;

  // Report Vehicles
  // #, Name, X, Y, Speed, Heading, Timestamp, Valid, Friendly

  // Report Swimmers (Table)
  // id, X, Y, Rescued, Ignored, Finder

  return (true);
}

// //------------------------------------------------------------
// // Procedure: generatePath()
// //            Uses a Self-Organizing Map (SOM) to solve the
// //            Traveling Salesman Problem over active swimmers,
// //            then publishes the resulting path via SURVEY_UPDATE.

// void GenRescue::generatePath()
// {
//   int n_cities = m_swimmers.size();

//   // SOM Parameters
//   int n_neurons = n_cities * 8;
//   int max_iter = 50000;
//   double learning_rate = 0.8;
//   double radius = (double)n_neurons / 10.0;

//   // 1. Determine bounding box and center of all swimmer points
//   double min_x = m_swimmers[0].x, max_x = m_swimmers[0].x;
//   double min_y = m_swimmers[0].y, max_y = m_swimmers[0].y;
//   for (int i = 1; i < n_cities; ++i)
//   {
//     if (m_swimmers[i].x < min_x)
//       min_x = m_swimmers[i].x;
//     if (m_swimmers[i].x > max_x)
//       max_x = m_swimmers[i].x;
//     if (m_swimmers[i].y < min_y)
//       min_y = m_swimmers[i].y;
//     if (m_swimmers[i].y > max_y)
//       max_y = m_swimmers[i].y;
//   }

//   double center_x = (max_x + min_x) / 2.0;
//   double center_y = (max_y + min_y) / 2.0;
//   double circle_radius = std::max(max_x - min_x, max_y - min_y) / 2.0;
//   if (circle_radius < 1.0)
//     circle_radius = 10.0; // Avoid degenerate circle for co-located points

//   struct Neuron
//   {
//     double x, y;
//   };
//   std::vector<Neuron> neurons(n_neurons);
//   for (int i = 0; i < n_neurons; ++i)
//   {
//     double angle = 2.0 * M_PI * i / n_neurons;
//     neurons[i].x = center_x + circle_radius * std::cos(angle);
//     neurons[i].y = center_y + circle_radius * std::sin(angle);
//   }

//   // Setup RNG for picking random cities
//   std::random_device rd;
//   std::mt19937 gen(rd());
//   std::uniform_int_distribution<> dist(0, n_cities - 1);

//   // 2. Train the SOM
//   for (int iter = 0; iter < max_iter; ++iter)
//   {
//     int city_idx = dist(gen);
//     double cx = m_swimmers[city_idx].x;
//     double cy = m_swimmers[city_idx].y;

//     // Find the winning neuron (closest to the chosen city)
//     int winner = 0;
//     double min_dist = std::numeric_limits<double>::max();
//     for (int i = 0; i < n_neurons; ++i)
//     {
//       double d = std::hypot(neurons[i].x - cx, neurons[i].y - cy);
//       if (d < min_dist)
//       {
//         min_dist = d;
//         winner = i;
//       }
//     }

//     // Update the winning neuron and its neighbors
//     for (int i = 0; i < n_neurons; ++i)
//     {
//       int dist_i = std::abs(i - winner);
//       dist_i = std::min(dist_i, n_neurons - dist_i);

//       double influence = std::exp(-(dist_i * dist_i) / (2.0 * radius * radius));

//       neurons[i].x += learning_rate * influence * (cx - neurons[i].x);
//       neurons[i].y += learning_rate * influence * (cy - neurons[i].y);
//     }

//     // Decay learning rate and radius
//     learning_rate *= 0.99997;
//     radius *= 0.99997;
//   }

//   // 3. Map swimmers to their closest winning neurons to extract the tour
//   std::vector<std::pair<int, int>> mapped_cities(n_cities);
//   for (int c = 0; c < n_cities; ++c)
//   {
//     int winner = 0;
//     double min_dist = std::numeric_limits<double>::max();
//     for (int n = 0; n < n_neurons; ++n)
//     {
//       double d = std::hypot(neurons[n].x - m_swimmers[c].x,
//                             neurons[n].y - m_swimmers[c].y);
//       if (d < min_dist)
//       {
//         min_dist = d;
//         winner = n;
//       }
//     }
//     mapped_cities[c] = {winner, c};
//   }

//   // Sort swimmers by their assigned neuron index to form the continuous path
//   std::sort(mapped_cities.begin(), mapped_cities.end());

//   // 4. Align the cyclic tour to start at the swimmer closest to our vehicle
//   int start_idx = 0;
//   double min_start_dist = std::numeric_limits<double>::max();
//   for (int i = 0; i < n_cities; ++i)
//   {
//     int c_idx = mapped_cities[i].second;
//     double d = std::hypot(m_swimmers[c_idx].x - m_pos_x,
//                           m_swimmers[c_idx].y - m_pos_y);
//     if (d < min_start_dist)
//     {
//       min_start_dist = d;
//       start_idx = i;
//     }
//   }

//   // 5. Construct the XYSegList and publish via SURVEY_UPDATE
//   XYSegList path;
//   path.add_vertex(m_pos_x, m_pos_y);

//   for (int i = 0; i < n_cities; ++i)
//   {
//     int seq = (start_idx + i) % n_cities;
//     int c_idx = mapped_cities[seq].second;
//     path.add_vertex(m_swimmers[c_idx].x, m_swimmers[c_idx].y);
//   }

//   Notify("SURVEY_UPDATE", "points=" + path.get_spec());
//   m_path_generated = true;

//   reportEvent("Published SURVEY_UPDATE with " + uintToString(n_cities) + " swimmer waypoints");
// }

// //------------------------------------------------------------
// // Procedure: predictiveSweep()
// //            Periodically evaluates rival vehicle positions
// //            and prunes swimmers that a rival can reach
// //            before we would in our tour order.

// bool GenRescue::predictiveSweep()
// {
//   double now = MOOSTime();

//   for (map<string, Rival>::iterator rit = m_rivals.begin(); rit != m_rivals.end(); ++rit)
//   {
//     Rival &rival = rit->second;
//     if (now - rival.timestamp > m_max_rival_age)
//       continue;

//     double opos_x = m_pos_x;
//     double opos_y = m_pos_y;
//     double own_ttt = 0.0;

//     // C++11 lambda requires the explicit type (Swimmer), not 'auto'
//     vector<Swimmer>::iterator remove_it = std::remove_if(
//         m_swimmers.begin(),
//         m_swimmers.end(),
//         [&](const Swimmer &s)
//         {
//           // Our accumulated tour time
//           own_ttt += hypot(s.x - opos_x, s.y - opos_y) / m_own_speed;
//           opos_x = s.x;
//           opos_y = s.y;

//           // Rival's direct straight-line time
//           double rival_ttt = hypot(s.x - rival.x, s.y - rival.y) / rival.spd;

//           if (rival_ttt < own_ttt)
//           {
//             reportEvent("Dropping swimmer " + s.id);
//             return true;
//           }
//           return false;
//         });

//     if (remove_it != m_swimmers.end())
//     {
//       m_swimmers.erase(remove_it, m_swimmers.end());
//       m_path_generated = false;
//     }
//   }
// }