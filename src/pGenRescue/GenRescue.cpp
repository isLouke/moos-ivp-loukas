/************************************************************/
/*    NAME: George Loukas                                   */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: GenRescue.cpp                                   */
/*    DATE: June 22nd, 2026                                 */
/*                                                          */
/*    Swimmer-aware rescue path planner. Ingestes           */
/*    SWIMMER_ALERT (deduped by id), removes rescued        */
/*    swimmers on FOUND_SWIMMER, and posts an optimal path  */
/*    (SOM TSP) to GEN_PATH for the waypoint behavior.      */
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

  // Plan lifecycle (matches example pattern)
  m_plan_pending       = false;
  m_plan_posted        = false;
  m_returned           = false;
  m_plan_size          = 0;
  m_prev_swimmer_count = 0;
  m_settle_iters       = 0;
  m_alerts_rcvd        = 0;
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

  // Settle gate: only (re)plan once the swimmer count has been stable
  // for a couple iterations, so the opening burst of N alerts produces
  // ONE plan rather than N replans (matches example pattern).
  if(m_swimmers.size() != m_prev_swimmer_count) {
    m_prev_swimmer_count = (unsigned int)(m_swimmers.size());
    m_settle_iters = 0;
  }
  else if(m_plan_pending)
    m_settle_iters++;

  bool ready = m_nav_x_set && m_nav_y_set && (m_swimmers.size() > 0);

  // Recompute and post the path when plan is pending.
  if(m_plan_pending && ready && (m_settle_iters >= 2))
    m_plan_pending = !postPath();

  // Return-home edge case (matches example pattern): once every swimmer has
  // been collected, post RETURN=true so the helm transitions to the return
  // behavior immediately. Cleared when a fresh swimmer re-engages the survey.
  if(m_swimmers.empty() && m_plan_posted && !m_returned) {
    Notify("RETURN", "true");
    m_returned     = true;
    m_plan_pending = false;
    reportEvent("All swimmers resolved -> RETURN=true (returning home)");
  }

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
}

//---------------------------------------------------------
// Procedure: handleMailNewSwimmer()
//   Example: SWIMMER_ALERT = x=34.0, y=85.0, id=21
//   Parses using the robust approach from the example:
//   parseString + biteStringX + stripBlankEnds + isNumber.

bool GenRescue::handleMailNewSwimmer(string str)
{
  string id;
  double x = 0, y = 0;
  bool   x_set = false, y_set = false;

  vector<string> svector = parseString(str, ',');
  for(unsigned int i=0; i<svector.size(); i++) {
    string param = tolower(biteStringX(svector[i], '='));
    string value = stripBlankEnds(svector[i]);
    if(param == "id")
      id = value;
    else if(param == "x") {
      if(!isNumber(value)) return(false);
      x = atof(value.c_str());
      x_set = true;
    }
    else if(param == "y") {
      if(!isNumber(value)) return(false);
      y = atof(value.c_str());
      y_set = true;
    }
  }
  if((id == "") || !x_set || !y_set) {
    reportRunWarning("Unhandled SWIMMER_ALERT: " + str);
    return(false);
  }

  m_alerts_rcvd++;

  // Already rescued -> ignore
  if(m_rescued.count(id))
    return(true);

  map<string,XYPoint>::iterator it = m_swimmers.find(id);
  if(it == m_swimmers.end()) {
    // New swimmer
    XYPoint pt(x, y);
    pt.set_label(id);
    m_swimmers[id] = pt;
    m_plan_pending = true;
    m_path_needs_som = true;

    // If we had previously returned home, re-engage survey on new swimmer
    if(m_returned) {
      Notify("RETURN", "false");
      m_returned = false;
      reportEvent("New swimmer after return -> re-engaging survey");
    }

    reportEvent("New swimmer: id=" + id +
                " (x=" + doubleToStringX(x,1) +
                ", y=" + doubleToStringX(y,1) + ")");
  }
  else if((it->second.x() != x) || (it->second.y() != y)) {
    // Existing swimmer moved -> update and replan
    XYPoint pt(x, y);
    pt.set_label(id);
    it->second = pt;
    m_plan_pending = true;
    m_path_needs_som = true;
    reportEvent("Swimmer moved: id=" + id);
  }
  // else: duplicate with same coordinates -> no action needed

  return(true);
}

//---------------------------------------------------------
// Procedure: handleMailFoundSwimmer()
//   Example: FOUND_SWIMMER = id=21, finder=abe
//   Parses using the same robust approach as the example.

bool GenRescue::handleMailFoundSwimmer(string str)
{
  string id;
  vector<string> svector = parseString(str, ',');
  for(unsigned int i=0; i<svector.size(); i++) {
    string param = tolower(biteStringX(svector[i], '='));
    string value = stripBlankEnds(svector[i]);
    if(param == "id")
      id = value;
  }
  if(id.empty()) {
    reportRunWarning("Unhandled FOUND_SWIMMER: " + str);
    return(false);
  }

  m_rescued.insert(id);
  if(m_swimmers.count(id)) {
    m_swimmers.erase(id);
    m_plan_pending = true;   // target gone -> re-optimize the rest
  }
  // If swimmer wasn't in our map, we still record it as rescued (prevents
  // a late SWIMMER_ALERT from adding it back).

  reportEvent("Swimmer rescued: id=" + id +
              "  (" + uintToString((unsigned int)m_swimmers.size()) +
              " remaining)");
  return(true);
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

  int unrescued = (int)m_swimmers.size();

  // If no un-rescued swimmers, post a null path
  if(unrescued == 0) {
    postNullPath();
    return(true);
  }

  // Only run the expensive SOM when a new swimmer has been added
  // (i.e., the set of swimmer positions grew). On rescue, the
  // stored SOM order is still valid — just filter out the rescued.
  if(m_path_needs_som || m_ordered_ids.empty()) {
    reportEvent("SWM_DEBUG: Recomputing SOM path for " +
                uintToString(unrescued) + " swimmers...");
    m_ordered_ids = computeSOMOrder();
    m_path_needs_som = false;
  }

  // Build path from the stored SOM order, skipping any now-rescued
  XYSegList path;
  path.add_vertex(m_nav_x, m_nav_y);
  for(const auto& id : m_ordered_ids) {
    if(m_swimmers.count(id)) {
      path.add_vertex(m_swimmers[id].x(), m_swimmers[id].y());
    }
  }

  path.set_label("swimmer_path");
  string spec = path.get_spec_pts();

  // Only (re)post when the waypoint list actually changed, to avoid
  // resetting the BHV_Waypoint index unnecessarily.
  bool changed = (spec != m_last_points);
  if(changed) {
    // Post visualization
    Notify("VIEW_SEGLIST", path.get_spec());

    // Post update for the waypoint behavior (BHV_Waypoint)
    string update_str = "points = " + spec;
    Notify("GEN_PATH", update_str);
    reportEvent("GEN_PATH=" + update_str);

    m_last_points = spec;
    m_path = path;
  }

  m_plan_posted = true;
  m_plan_size   = (unsigned int)path.size();

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
  for(auto it = m_swimmers.begin(); it != m_swimmers.end(); ++it) {
    swimmers.push_back({it->first, it->second});
  }

  int n_cities = (int)swimmers.size();
  if(n_cities == 0)
    return {};

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

  string spec = segl.get_spec_pts();
  string update_str = "points = " + spec;
  Notify("GEN_PATH", update_str);
  reportEvent("GEN_PATH=" + update_str + " (all rescued)");

  m_last_points = spec;
  m_path = segl;
  m_plan_posted = true;
  m_plan_size   = 0;

  return(true);
}

//---------------------------------------------------------
// Procedure: clearSwimmers()
//   Purpose: Reset swimmer tracking (useful for testing/reinit).

void GenRescue::clearSwimmers()
{
  m_swimmers.clear();
  m_rescued.clear();
  m_ordered_ids.clear();
  m_path_needs_update = false;
  m_path_needs_som = false;
  m_plan_pending = false;
  m_plan_posted  = false;
  m_returned     = false;
  m_plan_size    = 0;
  m_prev_swimmer_count = 0;
  m_settle_iters = 0;
  m_alerts_rcvd  = 0;
  m_last_points.clear();
}

//---------------------------------------------------------
// Procedure: buildReport()

bool GenRescue::buildReport()
{
  m_msgs << "Vehicle Name: " << m_vname << endl;
  m_msgs << "Nav Status:   x=" << doubleToStringX(m_nav_x, 1)
         << " y=" << doubleToStringX(m_nav_y, 1) << endl;
  m_msgs << "Alerts rcvd:  " << uintToString(m_alerts_rcvd) << endl;
  m_msgs << "Swimmers:     " << uintToString((unsigned int)m_swimmers.size())
         << " active, " << uintToString((unsigned int)m_rescued.size())
         << " rescued" << endl;
  m_msgs << "Plan:         " << (m_plan_posted ? "posted" : "none")
         << ", " << uintToString(m_plan_size) << " waypoints"
         << (m_plan_pending ? "  [replan pending]" : "") << endl;
  m_msgs << "Returned:     " << boolToString(m_returned) << endl;
  m_msgs << endl;

  if(m_swimmers.size() == 0 && m_rescued.size() == 0) {
    m_msgs << "  No swimmers yet." << endl;
    return(true);
  }

  if(m_rescued.size() > 0) {
    m_msgs << "Rescued Swimmers:" << endl;
    m_msgs << "--------------------------------" << endl;
    for(set<string>::iterator it = m_rescued.begin(); it != m_rescued.end(); it++)
      m_msgs << "  id=" << *it << endl;
    m_msgs << endl;
  }

  if(m_swimmers.size() > 0) {
    m_msgs << "Active Swimmers:" << endl;
    m_msgs << "--------------------------------" << endl;
    map<string, XYPoint>::iterator it;
    for(it = m_swimmers.begin(); it != m_swimmers.end(); it++) {
      string id = it->first;
      double sx = it->second.x();
      double sy = it->second.y();
      m_msgs << "  id=" << id
             << "  x=" << doubleToStringX(sx, 1)
             << "  y=" << doubleToStringX(sy, 1) << endl;
    }
    m_msgs << endl;
  }

  return(true);
}
