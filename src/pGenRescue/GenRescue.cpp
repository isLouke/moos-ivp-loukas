/************************************************************/
/*    NAME: George Loukas                                   */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: GenRescue.cpp                                   */
/*    DATE: June 22nd, 2026                                 */
/************************************************************/

#include <iterator>
#include <algorithm>
#include <cmath>
#include <random>
#include <utility>
#include <vector>
#include "GenRescue.h"
#include "MBUtils.h"
#include "ColorParse.h"
#include "XYSegList.h"
#include "XYPoint.h"
#include "GeomUtils.h"
#include "PathUtils.h"
#include "ACTable.h"

using namespace std;

//---------------------------------------------------------
// Constructor()

GenRescue::GenRescue()
{
  m_nav_x = 0;
  m_nav_y = 0;
  m_nav_x_set = false;
  m_nav_y_set = false;
  m_path_needs_update = false;
  m_path_needs_som = false;
  m_collecting_visits = false;
  m_visit_points_flushed = false;
  m_swimmer_alert_received = false;
}

//---------------------------------------------------------
// Procedure: OnNewMail()

bool GenRescue::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for(p = NewMail.begin(); p != NewMail.end(); p++) {
    CMOOSMsg &msg = *p;
    string key  = msg.GetKey();
    string sval = msg.GetString();

    bool handled = true;
    if(key == "SWIMMER_ALERT")
      handled = handleMailNewSwimmer(sval);
    else if(key == "FOUND_SWIMMER")
      handled = handleMailFoundSwimmer(sval);
    else if(key == "NAV_X") {
      m_nav_x = msg.GetDouble();
      m_nav_x_set = true;
    }
    else if(key == "NAV_Y") {
      m_nav_y = msg.GetDouble();
      m_nav_y_set = true;
    }
    else if(key == "VISIT_POINT")
      handled = handleMailVisitPoint(sval);
    else if(key != "APPCAST_REQ")
      handled = false;

    if(!handled)
      reportRunWarning("Unhandled Mail: " + key + "=" + sval);
  }
  return(true);
}

//---------------------------------------------------------
// Procedure: OnConnectToServer()

bool GenRescue::OnConnectToServer()
{
  RegisterVariables();
  return(true);
}

//---------------------------------------------------------
// Procedure: Iterate()

bool GenRescue::Iterate()
{
  AppCastingMOOSApp::Iterate();

  // Recompute and post the path whenever new swimmer info arrives.
  // Only clear the flag when the path was actually computed, so
  // the update isn't lost if NAV_X/NAV_Y aren't ready yet.
  if(m_path_needs_update)
    m_path_needs_update = !postPath();

  AppCastingMOOSApp::PostReport();
  return(true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()

bool GenRescue::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  STRING_LIST sParams;
  m_MissionReader.GetConfiguration(GetAppName(), sParams);

  STRING_LIST::iterator p;
  for(p = sParams.begin(); p != sParams.end(); p++) {
    string sLine  = *p;
    string param  = tolower(biteStringX(sLine, '='));
    string value  = sLine;
    if(param == "vname")
      m_vname = value;
  }

  RegisterVariables();
  return(true);
}

//---------------------------------------------------------
// Procedure: RegisterVariables()

void GenRescue::RegisterVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  Register("SWIMMER_ALERT", 0);
  Register("FOUND_SWIMMER", 0);
  Register("NAV_X", 0);
  Register("NAV_Y", 0);
  Register("VISIT_POINT", 0);
}

//---------------------------------------------------------
// Procedure: handleMailNewSwimmer()
//   Example: SWIMMER_ALERT = x=23, y=54, id=04

bool GenRescue::handleMailNewSwimmer(string str)
{
  string xstr = tokStringParse(str, "x", ',', '=');
  string ystr = tokStringParse(str, "y", ',', '=');
  string id   = tokStringParse(str, "id", ',', '=');

  if(xstr.empty() || ystr.empty() || id.empty()) {
    reportRunWarning("Unhandled SWIMMER_ALERT: " + str);
    return(false);
  }

  // Mark that we've received a real alert. This keeps the VISIT_POINT
  // fallback from flushing again on subsequent lastpoint events (if the
  // timer fires again). We do NOT clear VISIT_POINT-derived entries here:
  // a mid-mission click adds ONE new swimmer, and clearing vp_* entries
  // would lose all the original swimmers until the next broadcast.
  // Coexisting vp_* and real entries at the same coordinates is harmless
  // (duplicate waypoints at the same location — the vehicle visits once).
  m_swimmer_alert_received = true;

  // Ignore swimmers we already know about
  if(m_swimmer_rescued.count(id)) {
    // Update position in case it changes (though spec says it won't)
    m_swimmer_x[id] = stod(xstr);
    m_swimmer_y[id] = stod(ystr);
    return(true);
  }

  double x = stod(xstr);
  double y = stod(ystr);

  m_swimmer_x[id] = x;
  m_swimmer_y[id] = y;
  m_swimmer_rescued[id] = false;
  m_path_needs_update = true;
  m_path_needs_som = true;

  reportEvent("SWM_DEBUG: New swimmer received! id=" + id +
              " (x=" + xstr + ", y=" + ystr + "). Triggering SOM recompute.");
  return(true);
}

//---------------------------------------------------------
// Procedure: handleMailFoundSwimmer()
//   Example: FOUND_SWIMMER = id=01, finder=abe

bool GenRescue::handleMailFoundSwimmer(string str)
{
  string id     = tokStringParse(str, "id", ',', '=');
  string finder = tokStringParse(str, "finder", ',', '=');

  if(id.empty()) {
    reportRunWarning("Unhandled FOUND_SWIMMER: " + str);
    return(false);
  }

  // If swimmer not yet in our map, create an entry but mark rescued.
  // No path update needed since the un-rescued count is unchanged.
  if(!m_swimmer_rescued.count(id)) {
    m_swimmer_x[id] = 0;
    m_swimmer_y[id] = 0;
    m_swimmer_rescued[id] = true;
    reportEvent("Found swimmer (unknown location): id=" + id +
                ", finder=" + finder);
    return(true);
  }

  // If already marked rescued, ignore
  if(m_swimmer_rescued[id])
    return(true);

  m_swimmer_rescued[id] = true;
  m_path_needs_update = true;

  reportEvent("Swimmer found: id=" + id +
              ", finder=" + finder);
  return(true);
}

//---------------------------------------------------------
// Procedure: handleMailVisitPoint()
//   Purpose: Accumulate VISIT_POINT coordinates as a fallback
//            in case SWIMMER_ALERT was missed (e.g., vehicle
//            connected after the initial alert burst).
//   Example: VISIT_POINT = firstpoint
//            VISIT_POINT = x=23,y=54
//            VISIT_POINT = lastpoint
//
//   On "firstpoint" we start accumulating. On each coordinate
//   we store it. On "lastpoint", if no SWIMMER_ALERT swimmer
//   data has arrived yet, the accumulated points are flushed
//   into the swimmer map with auto-generated IDs.

bool GenRescue::handleMailVisitPoint(string str)
{
  if(str == "firstpoint") {
    m_visit_accumulator.clear();
    m_collecting_visits = true;
    return(true);
  }

  if(str == "lastpoint") {
    m_collecting_visits = false;
    // Only flush if we still have received no real SWIMMER_ALERT
    // (vehicle may have connected after the initial alert burst).
    if(!m_swimmer_alert_received && !m_visit_accumulator.empty())
      flushVisitPoints();
    return(true);
  }

  // Parse coordinate: x=<x>,y=<y>
  if(m_collecting_visits) {
    string xstr = tokStringParse(str, "x", ',', '=');
    string ystr = tokStringParse(str, "y", ',', '=');
    if(!xstr.empty() && !ystr.empty()) {
      m_visit_accumulator.push_back(
        XYPoint(stod(xstr), stod(ystr)));
    }
  }
  return(true);
}

//---------------------------------------------------------
// Procedure: flushVisitPoints()
//   Purpose: Convert accumulated VISIT_POINT data into
//            swimmer entries with auto-generated IDs.

void GenRescue::flushVisitPoints()
{
  for(size_t i = 0; i < m_visit_accumulator.size(); ++i) {
    string id = "vp_" + to_string(i);
    m_swimmer_x[id] = m_visit_accumulator[i].x();
    m_swimmer_y[id] = m_visit_accumulator[i].y();
    m_swimmer_rescued[id] = false;
  }

  reportEvent("Flushed " + to_string(m_visit_accumulator.size()) +
              " VISIT_POINT coordinates as swimmer locations");

  m_visit_points_flushed = true;
  m_path_needs_update = true;
  m_path_needs_som = true;
}

//---------------------------------------------------------
// Procedure: postPath()
//   Purpose: Build a path through all un-rescued swimmers,
//            starting from current ownship position.
//            Runs the full SOM only when a NEW swimmer is
//            added (swimmer set changed). On rescue, simply
//            removes the rescued swimmer from the stored
//            SOM order — no expensive recomputation needed.
//   Returns: true if the path was computed and posted.
//            false if NAV_X/NAV_Y are not yet available.

bool GenRescue::postPath()
{
  if(!m_nav_x_set || !m_nav_y_set)
    return(false);

  // Count un-rescued swimmers
  unsigned int unrescued = 0;
  for(auto it = m_swimmer_rescued.begin(); it != m_swimmer_rescued.end(); ++it) {
    if(!it->second)
      unrescued++;
  }

  // If no un-rescued swimmers, post a null path
  if(unrescued == 0) {
    postNullPath();
    return(true);
  }

  // Only run the expensive SOM when a new swimmer has been added
  // (i.e., the set of swimmer positions grew). On rescue, the
  // stored SOM order is still valid — just filter out the rescued.
  if(m_path_needs_som || m_ordered_ids.empty()) {
    reportEvent("SWM_DEBUG: Recomputing SOM path for " + to_string(unrescued) + " swimmers...");
    m_ordered_ids = computeSOMOrder();
    m_path_needs_som = false;
  }

  // Build path from the stored SOM order, skipping any now-rescued
  XYSegList path;
  path.add_vertex(m_nav_x, m_nav_y);
  for(const auto& id : m_ordered_ids) {
    if(!m_swimmer_rescued[id]) {
      path.add_vertex(m_swimmer_x[id], m_swimmer_y[id]);
    }
  }

  // Post visualization
  path.set_label("swimmer_path");
  Notify("VIEW_SEGLIST", path.get_spec());

  // Post update for the waypoint behavior (BHV_Waypoint)
  string update_str = "points = " + path.get_spec_pts();
  Notify("GEN_PATH", update_str);
  reportEvent("GEN_PATH=" + update_str);

  return(true);
}

//---------------------------------------------------------
// Procedure: computeSOMOrder()
//   Purpose: Run the Self-Organizing Map TSP algorithm on
//            all un-rescued swimmers and return their IDs
//            in SOM-optimized visit order. This is the same
//            algorithm used in pGenPath.
//   Returns: Vector of swimmer IDs in optimal tour order.

std::vector<std::string> GenRescue::computeSOMOrder()
{
  // Collect un-rescued swimmers with their IDs
  std::vector<std::pair<std::string, XYPoint>> swimmers;
  for(auto it = m_swimmer_rescued.begin(); it != m_swimmer_rescued.end(); ++it) {
    if(!it->second) {
      string id = it->first;
      swimmers.push_back({id, XYPoint(m_swimmer_x[id], m_swimmer_y[id])});
    }
  }

  int n_cities = swimmers.size();

  // SOM Parameters (Derived from som-tsp logic in GenPath.cpp)
  int n_neurons = n_cities * 8;
  int max_iter = 50000;
  double learning_rate = 0.8;
  double radius = (double)n_neurons / 10.0;

  // 1. Initialize Neurons in a circle around the bounding box center
  double min_x = swimmers[0].second.x(), max_x = swimmers[0].second.x();
  double min_y = swimmers[0].second.y(), max_y = swimmers[0].second.y();
  for(int i = 1; i < n_cities; ++i) {
    if(swimmers[i].second.x() < min_x) min_x = swimmers[i].second.x();
    if(swimmers[i].second.x() > max_x) max_x = swimmers[i].second.x();
    if(swimmers[i].second.y() < min_y) min_y = swimmers[i].second.y();
    if(swimmers[i].second.y() > max_y) max_y = swimmers[i].second.y();
  }

  double center_x = (max_x + min_x) / 2.0;
  double center_y = (max_y + min_y) / 2.0;
  double circle_radius = std::max(max_x - min_x, max_y - min_y) / 2.0;
  if(circle_radius < 0.1) circle_radius = 1.0;

  struct Neuron { double x, y; };
  std::vector<Neuron> neurons(n_neurons);
  for(int i = 0; i < n_neurons; ++i) {
    double angle = 2.0 * M_PI * i / n_neurons;
    neurons[i].x = center_x + circle_radius * std::cos(angle);
    neurons[i].y = center_y + circle_radius * std::sin(angle);
  }

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dist(0, n_cities - 1);

  // 2. Train the SOM
  for(int iter = 0; iter < max_iter; ++iter) {
    int city_idx = dist(gen);
    double cx = swimmers[city_idx].second.x();
    double cy = swimmers[city_idx].second.y();

    int winner = 0;
    double min_dist = std::numeric_limits<double>::max();
    for(int i = 0; i < n_neurons; ++i) {
      double d = std::hypot(neurons[i].x - cx, neurons[i].y - cy);
      if(d < min_dist) { min_dist = d; winner = i; }
    }

    for(int i = 0; i < n_neurons; ++i) {
      int dist_i = std::abs(i - winner);
      dist_i = std::min(dist_i, n_neurons - dist_i);
      double influence = std::exp(-(dist_i * dist_i) / (2.0 * radius * radius));
      neurons[i].x += learning_rate * influence * (cx - neurons[i].x);
      neurons[i].y += learning_rate * influence * (cy - neurons[i].y);
    }

    learning_rate *= 0.99997;
    radius *= 0.99997;
  }

  // 3. Map cities to closest winning neurons
  std::vector<std::pair<int, int>> mapped_cities(n_cities);
  for(int c = 0; c < n_cities; ++c) {
    int winner = 0;
    double min_dist = std::numeric_limits<double>::max();
    for(int n = 0; n < n_neurons; ++n) {
      double d = std::hypot(neurons[n].x - swimmers[c].second.x(),
                           neurons[n].y - swimmers[c].second.y());
      if(d < min_dist) { min_dist = d; winner = n; }
    }
    mapped_cities[c] = {winner, c};
  }

  std::sort(mapped_cities.begin(), mapped_cities.end());

  // 4. Align to start at city closest to vehicle
  int start_idx = 0;
  double min_start_dist = std::numeric_limits<double>::max();
  for(int i = 0; i < n_cities; ++i) {
    int c_idx = mapped_cities[i].second;
    double d = std::hypot(swimmers[c_idx].second.x() - m_nav_x,
                         swimmers[c_idx].second.y() - m_nav_y);
    if(d < min_start_dist) { min_start_dist = d; start_idx = i; }
  }

  // 5. Build ordered ID list
  std::vector<std::string> ordered_ids;
  for(int i = 0; i < n_cities; ++i) {
    int seq = (start_idx + i) % n_cities;
    int c_idx = mapped_cities[seq].second;
    ordered_ids.push_back(swimmers[c_idx].first);
  }

  return ordered_ids;
}

//---------------------------------------------------------
// Procedure: postNullPath()
//   Purpose: When all swimmers have been rescued, post a
//            minimal path (current position) to the behavior.
//            This prevents stale waypoints from lingering.
//   Returns: true if the path was posted.

bool GenRescue::postNullPath()
{
  if(!m_nav_x_set || !m_nav_y_set)
    return(false);

  XYSegList segl;
  segl.add_vertex(m_nav_x, m_nav_y);
  segl.set_label("swimmer_path");
  Notify("VIEW_SEGLIST", segl.get_spec());

  string update_str = "points = " + segl.get_spec_pts();
  Notify("GEN_PATH", update_str);
  reportEvent("GEN_PATH=" + update_str + " (all rescued)");

  return(true);
}

//---------------------------------------------------------
// Procedure: clearSwimmers()
//   Purpose: Reset swimmer tracking (useful for testing/reinit).

void GenRescue::clearSwimmers()
{
  m_swimmer_x.clear();
  m_swimmer_y.clear();
  m_swimmer_rescued.clear();
  m_ordered_ids.clear();
  m_visit_accumulator.clear();
  m_collecting_visits = false;
  m_visit_points_flushed = false;
  m_swimmer_alert_received = false;
  m_path_needs_update = false;
  m_path_needs_som = false;
}

//---------------------------------------------------------
// Procedure: buildReport()

bool GenRescue::buildReport()
{
  m_msgs << "Vehicle Name: " << m_vname << endl;
  m_msgs << "Nav Status:   x=" << doubleToStringX(m_nav_x, 1)
         << " y=" << doubleToStringX(m_nav_y, 1) << endl;
  m_msgs << "VISIT_ACCUM:  " << m_visit_accumulator.size()
         << " (coll=" << boolToString(m_collecting_visits)
         << " flushed=" << boolToString(m_visit_points_flushed)
         << ")" << endl;
  m_msgs << "SWIMMER_ALERT received: "
         << boolToString(m_swimmer_alert_received) << endl;
  m_msgs << "Total tracked swimmers: "
         << m_swimmer_rescued.size() << endl;
  m_msgs << endl;
  m_msgs << "Swimmer Tracking:" << endl;
  m_msgs << "--------------------------------" << endl;

  if(m_swimmer_rescued.size() == 0) {
    m_msgs << "  No swimmers yet." << endl;
    return(true);
  }

  unsigned int rescued_count = 0;
  map<string, bool>::iterator it;
  for(it = m_swimmer_rescued.begin(); it != m_swimmer_rescued.end(); it++) {
    string id = it->first;
    bool rescued = it->second;
    string status = rescued ? "RESCUED" : "pending";
    m_msgs << "  id=" << id
           << "  x=" << doubleToStringX(m_swimmer_x[id], 1)
           << "  y=" << doubleToStringX(m_swimmer_y[id], 1)
           << "  [" << status << "]" << endl;
    if(rescued)
      rescued_count++;
  }
  m_msgs << endl;
  m_msgs << "Total: " << m_swimmer_rescued.size()
         << "  Rescued: " << rescued_count
         << "  Pending: " << (m_swimmer_rescued.size() - rescued_count)
         << endl;

  return(true);
}
