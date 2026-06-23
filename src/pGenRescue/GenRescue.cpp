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
  m_path_generated = false;
  m_pos_x = 0;
  m_pos_y = 0;
  m_last_predict_time = 0;
  m_own_speed = 1.2;
  m_rival_default_speed = 1.2;
  m_update_interval = 15.0;
  m_max_rival_age = 30.0;
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

    if (key == "NAV_X")
    {
      m_pos_x = dval;
    }
    else if (key == "NAV_Y")
    {
      m_pos_y = dval;
    }
    else if (key == "SWIMMER_ALERT")
    {
      // Format: x=23, y=54, id=04
      string id = tokStringParse(sval, "id", ',', '=');
      string xpos = tokStringParse(sval, "x", ',', '=');
      string ypos = tokStringParse(sval, "y", ',', '=');

      if (id.empty() || xpos.empty() || ypos.empty())
      {
        reportRunWarning("Malformed SWIMMER_ALERT: " + sval);
        continue;
      }

      // Skip if this swimmer has already been rescued
      if (m_rescued_ids.count(id))
        continue;

      // Skip duplicate alerts for swimmers we already know about
      bool already_known = false;
      for (size_t i = 0; i < m_swimmers.size(); ++i)
      {
        if (m_swimmers[i].id == id)
        {
          already_known = true;
          break;
        }
      }
      if (already_known)
        continue;

      // New swimmer alert – add to active list
      Swimmer s;
      s.id = id;
      s.x = stod(xpos);
      s.y = stod(ypos);

      // Check if swimmer is within rescue region
      if (m_rescue_region.is_convex() && !m_rescue_region.contains(s.x, s.y))
      {
        reportEvent("Ignoring swimmer id=" + id + " outside rescue region");
        continue;
      }

      m_swimmers.push_back(s);
      m_path_generated = false;

      reportEvent("New swimmer alert: id=" + id + ", x=" + xpos + ", y=" + ypos);
    }
    else if (key == "FOUND_SWIMMER")
    {
      // Format: id=01, finder=abe
      string id = tokStringParse(sval, "id", ',', '=');
      if (id.empty())
      {
        reportRunWarning("Malformed FOUND_SWIMMER: " + sval);
        continue;
      }

      // Mark as rescued (to ignore future alerts) and remove from active list
      m_rescued_ids.insert(id);

      bool removed = false;
      for (size_t i = 0; i < m_swimmers.size();)
      {
        if (m_swimmers[i].id == id)
        {
          m_swimmers.erase(m_swimmers.begin() + i);
          removed = true;
        }
        else
        {
          ++i;
        }
      }

      if (removed)
      {
        m_path_generated = false;
        reportEvent("Swimmer found/rescued: id=" + id);
      }
    }
    else if (key == "NODE_REPORT")
    {
      // Parse competitor vehicle report
      // Format: NAME=alpha,TYPE=UUV,X=51.71,Y=-35.50,SPD=2.0,HDG=118.8,...
      string vname = tokStringParse(sval, "NAME", ',', '=');
      string xstr = tokStringParse(sval, "X", ',', '=');
      string ystr = tokStringParse(sval, "Y", ',', '=');
      string spdstr = tokStringParse(sval, "SPD", ',', '=');

      // Skip own reports and malformed messages
      if (vname == GetAppName() || xstr.empty() || ystr.empty())
        continue;

      double x = strtod(xstr.c_str(), NULL);
      double y = strtod(ystr.c_str(), NULL);
      double spd = spdstr.empty() ? m_rival_default_speed : strtod(spdstr.c_str(), NULL);

      Rival rival;
      rival.vname = vname;
      rival.x = x;
      rival.y = y;
      rival.spd = spd;
      rival.timestamp = MOOSTime();

      m_rivals[vname] = rival;
    }
    else if (key == "RESCUE_REGION")
    {
      XYPolygon poly = string2Poly(sval);
      if (poly.is_convex())
        m_rescue_region = poly;
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

  // Regenerate path whenever the swimmer set changes
  if (!m_path_generated && !m_swimmers.empty())
    generatePath();

  // Periodic adaptive re-planning based on rival positions
  double elapsed = MOOSTime() - m_last_predict_time;
  if (elapsed >= m_update_interval)
  {
    m_last_predict_time = MOOSTime();
    predictiveSweep();
    m_path_generated = false;
  }

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
    if (param == "own_speed")
    {
      m_own_speed = atof(value.c_str());
      handled = true;
    }
    else if (param == "rival_default_speed")
    {
      m_rival_default_speed = atof(value.c_str());
      handled = true;
    }
    else if (param == "update_interval")
    {
      m_update_interval = atof(value.c_str());
      handled = true;
    }
    else if (param == "max_rival_age")
    {
      m_max_rival_age = atof(value.c_str());
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
  Register("SWIMMER_ALERT", 0);
  Register("FOUND_SWIMMER", 0);
  Register("NAV_X", 0);
  Register("NAV_Y", 0);
  Register("NODE_REPORT", 0);
  Register("RESCUE_REGION", 0);
}

//------------------------------------------------------------
// Procedure: buildReport()

bool GenRescue::buildReport()
{
  m_msgs << "============================================" << endl;
  m_msgs << "Active Swimmers: " << m_swimmers.size() << endl;
  m_msgs << "Rescued Swimmers: " << m_rescued_ids.size() << endl;
  m_msgs << "Vehicle Position: (" << m_pos_x << ", " << m_pos_y << ")" << endl;
  m_msgs << "Path Generated: " << boolToString(m_path_generated) << endl;
  m_msgs << "============================================" << endl;

  for (size_t i = 0; i < m_swimmers.size(); ++i)
  {
    m_msgs << "Swimmer " << i + 1 << ": id=" << m_swimmers[i].id
           << ", x=" << m_swimmers[i].x << ", y=" << m_swimmers[i].y << endl;
  }

  m_msgs << "============================================" << endl;
  m_msgs << "Competitive State:" << endl;
  m_msgs << "  Update Interval: " << m_update_interval << " s" << endl;
  m_msgs << "  Last Predict: " << (m_last_predict_time > 0 ? doubleToString(m_last_predict_time, 1) : "never") << endl;
  m_msgs << "  Rescue Region: " << (m_rescue_region.is_convex() ? "active" : "none") << endl;
  m_msgs << "  Active Rivals: " << uintToString(m_rivals.size()) << endl;

  for (map<string, Rival>::iterator it = m_rivals.begin(); it != m_rivals.end(); ++it)
  {
    double age = MOOSTime() - it->second.timestamp;
    m_msgs << "    " << it->first
           << " x=" << doubleToString(it->second.x, 1)
           << ", y=" << doubleToString(it->second.y, 1)
           << ", spd=" << doubleToString(it->second.spd, 2)
           << ", age=" << doubleToString(age, 1) << "s" << endl;
  }

  return (true);
}

//------------------------------------------------------------
// Procedure: generatePath()
//            Uses a Self-Organizing Map (SOM) to solve the
//            Traveling Salesman Problem over active swimmers,
//            then publishes the resulting path via SURVEY_UPDATE.

void GenRescue::generatePath()
{
  int n_cities = m_swimmers.size();

  // SOM Parameters
  int n_neurons = n_cities * 8;
  int max_iter = 50000;
  double learning_rate = 0.8;
  double radius = (double)n_neurons / 10.0;

  // 1. Determine bounding box and center of all swimmer points
  double min_x = m_swimmers[0].x, max_x = m_swimmers[0].x;
  double min_y = m_swimmers[0].y, max_y = m_swimmers[0].y;
  for (int i = 1; i < n_cities; ++i)
  {
    if (m_swimmers[i].x < min_x)
      min_x = m_swimmers[i].x;
    if (m_swimmers[i].x > max_x)
      max_x = m_swimmers[i].x;
    if (m_swimmers[i].y < min_y)
      min_y = m_swimmers[i].y;
    if (m_swimmers[i].y > max_y)
      max_y = m_swimmers[i].y;
  }

  double center_x = (max_x + min_x) / 2.0;
  double center_y = (max_y + min_y) / 2.0;
  double circle_radius = std::max(max_x - min_x, max_y - min_y) / 2.0;
  if (circle_radius < 1.0)
    circle_radius = 10.0; // Avoid degenerate circle for co-located points

  struct Neuron
  {
    double x, y;
  };
  std::vector<Neuron> neurons(n_neurons);
  for (int i = 0; i < n_neurons; ++i)
  {
    double angle = 2.0 * M_PI * i / n_neurons;
    neurons[i].x = center_x + circle_radius * std::cos(angle);
    neurons[i].y = center_y + circle_radius * std::sin(angle);
  }

  // Setup RNG for picking random cities
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dist(0, n_cities - 1);

  // 2. Train the SOM
  for (int iter = 0; iter < max_iter; ++iter)
  {
    int city_idx = dist(gen);
    double cx = m_swimmers[city_idx].x;
    double cy = m_swimmers[city_idx].y;

    // Find the winning neuron (closest to the chosen city)
    int winner = 0;
    double min_dist = std::numeric_limits<double>::max();
    for (int i = 0; i < n_neurons; ++i)
    {
      double d = std::hypot(neurons[i].x - cx, neurons[i].y - cy);
      if (d < min_dist)
      {
        min_dist = d;
        winner = i;
      }
    }

    // Update the winning neuron and its neighbors
    for (int i = 0; i < n_neurons; ++i)
    {
      int dist_i = std::abs(i - winner);
      dist_i = std::min(dist_i, n_neurons - dist_i);

      double influence = std::exp(-(dist_i * dist_i) / (2.0 * radius * radius));

      neurons[i].x += learning_rate * influence * (cx - neurons[i].x);
      neurons[i].y += learning_rate * influence * (cy - neurons[i].y);
    }

    // Decay learning rate and radius
    learning_rate *= 0.99997;
    radius *= 0.99997;
  }

  // 3. Map swimmers to their closest winning neurons to extract the tour
  std::vector<std::pair<int, int>> mapped_cities(n_cities);
  for (int c = 0; c < n_cities; ++c)
  {
    int winner = 0;
    double min_dist = std::numeric_limits<double>::max();
    for (int n = 0; n < n_neurons; ++n)
    {
      double d = std::hypot(neurons[n].x - m_swimmers[c].x,
                            neurons[n].y - m_swimmers[c].y);
      if (d < min_dist)
      {
        min_dist = d;
        winner = n;
      }
    }
    mapped_cities[c] = {winner, c};
  }

  // Sort swimmers by their assigned neuron index to form the continuous path
  std::sort(mapped_cities.begin(), mapped_cities.end());

  // 4. Align the cyclic tour to start at the swimmer closest to our vehicle
  int start_idx = 0;
  double min_start_dist = std::numeric_limits<double>::max();
  for (int i = 0; i < n_cities; ++i)
  {
    int c_idx = mapped_cities[i].second;
    double d = std::hypot(m_swimmers[c_idx].x - m_pos_x,
                          m_swimmers[c_idx].y - m_pos_y);
    if (d < min_start_dist)
    {
      min_start_dist = d;
      start_idx = i;
    }
  }

  // 5. Construct the XYSegList and publish via SURVEY_UPDATE
  XYSegList path;
  path.add_vertex(m_pos_x, m_pos_y);

  for (int i = 0; i < n_cities; ++i)
  {
    int seq = (start_idx + i) % n_cities;
    int c_idx = mapped_cities[seq].second;
    path.add_vertex(m_swimmers[c_idx].x, m_swimmers[c_idx].y);
  }

  Notify("SURVEY_UPDATE", "points=" + path.get_spec());
  m_path_generated = true;

  reportEvent("Published SURVEY_UPDATE with " + uintToString(n_cities) + " swimmer waypoints");
}

//------------------------------------------------------------
// Procedure: predictiveSweep()
//            Periodically evaluates rival vehicle positions
//            and prunes swimmers that a rival can reach
//            before we would in our tour order.

void GenRescue::predictiveSweep()
{
  double now = MOOSTime();

  for (map<string, Rival>::iterator rit = m_rivals.begin();
       rit != m_rivals.end(); ++rit)
  {
    Rival &rival = rit->second;
    double age = now - rival.timestamp;
    if (age > m_max_rival_age)
      continue; // stale rival, skip

    // Step 1: Compute rival's greedy nearest-neighbor TTT to each swimmer
    // Make working copies of remaining swimmer positions
    vector<pair<double, double>> remaining;
    vector<string> remaining_ids;
    for (size_t i = 0; i < m_swimmers.size(); i++)
    {
      remaining.push_back(make_pair(m_swimmers[i].x, m_swimmers[i].y));
      remaining_ids.push_back(m_swimmers[i].id);
    }

    double rspd = rival.spd;
    if (rspd <= 0)
      rspd = m_rival_default_speed;

    double rpos_x = rival.x;
    double rpos_y = rival.y;

    map<string, double> rival_ttt_map;
    double rival_ttt = 0;

    while (!remaining.empty())
    {
      // Find nearest remaining swimmer
      int nearest_idx = 0;
      double nearest_dist = numeric_limits<double>::max();
      for (size_t i = 0; i < remaining.size(); i++)
      {
        double d = hypot(remaining[i].first - rpos_x,
                         remaining[i].second - rpos_y);
        if (d < nearest_dist)
        {
          nearest_dist = d;
          nearest_idx = i;
        }
      }

      rival_ttt += nearest_dist / rspd;
      rival_ttt_map[remaining_ids[nearest_idx]] = rival_ttt;

      // Move rival position to this swimmer
      rpos_x = remaining[nearest_idx].first;
      rpos_y = remaining[nearest_idx].second;

      // Remove from working set
      remaining.erase(remaining.begin() + nearest_idx);
      remaining_ids.erase(remaining_ids.begin() + nearest_idx);
    }

    // Step 2: Compute our own TTT using the m_swimmers tour order
    double opos_x = m_pos_x;
    double opos_y = m_pos_y;
    double ospd = m_own_speed;
    double own_ttt = 0;
    map<string, double> own_ttt_map;

    for (size_t i = 0; i < m_swimmers.size(); i++)
    {
      double d = hypot(m_swimmers[i].x - opos_x, m_swimmers[i].y - opos_y);
      own_ttt += d / ospd;
      own_ttt_map[m_swimmers[i].id] = own_ttt;
      opos_x = m_swimmers[i].x;
      opos_y = m_swimmers[i].y;
    }

    // Step 3: Prune swimmers where rival reaches first
    vector<string> to_remove;
    for (size_t i = 0; i < m_swimmers.size(); i++)
    {
      if (rival_ttt_map.count(m_swimmers[i].id) &&
          own_ttt_map.count(m_swimmers[i].id))
      {
        if (rival_ttt_map[m_swimmers[i].id] < own_ttt_map[m_swimmers[i].id])
        {
          to_remove.push_back(m_swimmers[i].id);
          reportEvent("Dropping swimmer " + m_swimmers[i].id + " — rival " + rival.vname + " reaches it " + doubleToString(own_ttt_map[m_swimmers[i].id] - rival_ttt_map[m_swimmers[i].id], 1) + "s faster");
        }
      }
    }

    // Remove marked swimmers from m_swimmers
    if (!to_remove.empty())
    {
      for (size_t i = 0; i < to_remove.size(); i++)
      {
        for (size_t j = 0; j < m_swimmers.size(); j++)
        {
          if (m_swimmers[j].id == to_remove[i])
          {
            m_swimmers.erase(m_swimmers.begin() + j);
            break;
          }
        }
      }
      m_path_generated = false;
    }
  }
}
