/************************************************************/
/*    NAME: George Loukas                                              */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: GenPath.cpp                                        */
/*    DATE: December 29th, 1963                             */
/************************************************************/

#include <algorithm>
#include <cmath>
#include <iterator>
#include "MBUtils.h"
#include "ACTable.h"
#include "GenPath.h"
#include <random>
#include <utility>
#include <vector>

using namespace std;

//---------------------------------------------------------
// Constructor()

GenPath::GenPath()
{
  m_first_point_received = false;
  m_last_point_received = false;
  m_path_generated = false;
  m_pos_x = 0;
  m_pos_y = 0;
}

//---------------------------------------------------------
// Destructor

GenPath::~GenPath()
{
}

//---------------------------------------------------------
// Procedure: OnNewMail()

bool GenPath::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for (p = NewMail.begin(); p != NewMail.end(); p++)
  {
    CMOOSMsg &msg = *p;
    string key = msg.GetKey();
    string sval = msg.GetString();
    double dval = msg.GetDouble();

#if 0 // Keep these around just for template
    string comm  = msg.GetCommunity();
    double dval  = msg.GetDouble();
    string msrc  = msg.GetSource();
    double mtime = msg.GetTime();
    bool   mdbl  = msg.IsDouble();
    bool   mstr  = msg.IsString();
#endif

    if (key == "FOO")
    {
      cout << "great!";
    }
    else if (key == "NAV_X")
    {
      m_pos_x = dval;
    }
    else if (key == "NAV_Y")
    {
      m_pos_y = dval;
    }
    else if (key == "VISIT_POINT")
    {
      if (sval == "firstpoint")
      {
        m_first_point_received = true;
      }
      else if (sval == "lastpoint")
      {
        m_last_point_received = true;
      }
      else
      {
        std::string xpos = tokStringParse(sval, "x", ',', '=');
        std::string ypos = tokStringParse(sval, "y", ',', '=');

        if (!xpos.empty() && !ypos.empty())
        {
          double xcoord = std::stod(xpos.c_str());
          double ycoord = std::stod(ypos.c_str());

          XYPoint new_point(xcoord, ycoord);
          m_points.push_back(new_point);
        }
      }
    }
    else if (key == "VISIT_POINT")
    {
      if (sval == "firstpoint")
      {
        m_first_point_received = true;
      }
      else if (sval == "lastpoint")
      {
        m_last_point_received = true;
      }
      else
      {

        std::string xpos = tokStringParse(sval, "x", ',', '=');
        std::string ypos = tokStringParse(sval, "y", ',', '=');

        if (!xpos.empty() && !ypos.empty())
        {
          double xcoord = std::stod(xpos.c_str());
          double ycoord = std::stod(ypos.c_str());

          XYPoint new_point(xcoord, ycoord);
          m_points.push_back(new_point);
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

bool GenPath::OnConnectToServer()
{
  registerVariables();
  return (true);
}

//---------------------------------------------------------
// Procedure: Iterate()
//            happens AppTick times per second

//---------------------------------------------------------
// Procedure: Iterate()
//            happens AppTick times per second

bool GenPath::Iterate()
{
  AppCastingMOOSApp::Iterate();

  if (!m_path_generated && m_first_point_received && m_last_point_received && !m_points.empty())
  {
    int n_cities = m_points.size();

    // SOM Parameters (Derived from som-tsp logic)
    int n_neurons = n_cities * 8; // Convention: 8 neurons per city
    int max_iter = 50000;         // 50k iterations is fast in C++ and robust for typical MOOS point clouds
    double learning_rate = 0.8;
    double radius = (double)n_neurons / 10.0;

    // 1. Initialize Neurons in a circle around the bounding box center of the points
    double min_x = m_points[0].x(), max_x = m_points[0].x();
    double min_y = m_points[0].y(), max_y = m_points[0].y();
    for (int i = 1; i < n_cities; ++i)
    {
      if (m_points[i].x() < min_x)
        min_x = m_points[i].x();
      if (m_points[i].x() > max_x)
        max_x = m_points[i].x();
      if (m_points[i].y() < min_y)
        min_y = m_points[i].y();
      if (m_points[i].y() > max_y)
        max_y = m_points[i].y();
    }

    double center_x = (max_x + min_x) / 2.0;
    double center_y = (max_y + min_y) / 2.0;
    double circle_radius = std::max(max_x - min_x, max_y - min_y) / 2.0;

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
      double cx = m_points[city_idx].x();
      double cy = m_points[city_idx].y();

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
        // Shortest distance in the circular array
        int dist_i = std::abs(i - winner);
        dist_i = std::min(dist_i, n_neurons - dist_i);

        // Gaussian neighborhood function
        double influence = std::exp(-(dist_i * dist_i) / (2.0 * radius * radius));

        neurons[i].x += learning_rate * influence * (cx - neurons[i].x);
        neurons[i].y += learning_rate * influence * (cy - neurons[i].y);
      }

      // Decay learning rate and radius
      learning_rate *= 0.99997;
      radius *= 0.99997;
    }

    // 3. Map cities to their closest winning neurons to extract the tour
    std::vector<std::pair<int, int>> mapped_cities(n_cities);
    for (int c = 0; c < n_cities; ++c)
    {
      int winner = 0;
      double min_dist = std::numeric_limits<double>::max();
      for (int n = 0; n < n_neurons; ++n)
      {
        double d = std::hypot(neurons[n].x - m_points[c].x(), neurons[n].y - m_points[c].y());
        if (d < min_dist)
        {
          min_dist = d;
          winner = n;
        }
      }
      mapped_cities[c] = {winner, c};
    }

    // Sort cities by their assigned neuron index to form the continuous path
    std::sort(mapped_cities.begin(), mapped_cities.end());

    // 4. Align the cyclic tour to start at the city closest to our vehicle
    int start_idx = 0;
    double min_start_dist = std::numeric_limits<double>::max();
    for (int i = 0; i < n_cities; ++i)
    {
      int c_idx = mapped_cities[i].second;
      double d = std::hypot(m_points[c_idx].x() - m_pos_x, m_points[c_idx].y() - m_pos_y);
      if (d < min_start_dist)
      {
        min_start_dist = d;
        start_idx = i;
      }
    }

    // 5. Construct the final MOOS XYSegList
    XYSegList path;
    path.add_vertex(m_pos_x, m_pos_y); // Always start from current nav position

    for (int i = 0; i < n_cities; ++i)
    {
      // Wrap around the sorted array to ensure continuous loop from start index
      int seq = (start_idx + i) % n_cities;
      int c_idx = mapped_cities[seq].second;
      path.add_vertex(m_points[c_idx].x(), m_points[c_idx].y());
    }

    Notify("GEN_PATH", "points=" + path.get_spec());
    m_path_generated = true;
  }

  AppCastingMOOSApp::PostReport();
  return (true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()
//            happens before connection is open

bool GenPath::OnStartUp()
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

void GenPath::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  // Register("FOOBAR", 0);
  Register("VISIT_POINT", 0);
  Register("NAV_X", 0);
  Register("NAV_Y", 0);
}

//------------------------------------------------------------
// Procedure: buildReport()

bool GenPath::buildReport()
{
  m_msgs << "============================================" << endl;
  m_msgs << "Points Received: " << m_points.size() << endl;
  m_msgs << "First Point Received: " << boolToString(m_first_point_received) << endl;
  m_msgs << "Last Point Received: " << boolToString(m_last_point_received) << endl;
  m_msgs << "============================================" << endl;

  for (size_t i = 0; i < m_points.size(); ++i)
  {
    m_msgs << "Point " << i + 1 << ": x=" << m_points[i].x() << "\t y=" << m_points[i].y() << endl;
  }

  return (true);
}
