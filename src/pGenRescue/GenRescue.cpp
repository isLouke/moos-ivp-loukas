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
#include "GenRescue.h"

using namespace std;

//---------------------------------------------------------
// Constructor()

GenRescue::GenRescue()
{
  m_path_generated = false;
  m_pos_x = 0;
  m_pos_y = 0;
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
    if (param == "foo")
    {
      handled = true;
    }
    else if (param == "bar")
    {
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
    if (m_swimmers[i].x < min_x) min_x = m_swimmers[i].x;
    if (m_swimmers[i].x > max_x) max_x = m_swimmers[i].x;
    if (m_swimmers[i].y < min_y) min_y = m_swimmers[i].y;
    if (m_swimmers[i].y > max_y) max_y = m_swimmers[i].y;
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
