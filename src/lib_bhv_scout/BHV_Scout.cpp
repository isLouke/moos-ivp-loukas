/*****************************************************************/
/*    NAME: M.Benjamin                                           */
/*    ORGN: Dept of Mechanical Eng / CSAIL, MIT Cambridge MA     */
/*    FILE: BHV_Scout.cpp                                        */
/*    DATE: April 30th 2022                                      */
/*****************************************************************/

#include <cstdlib>
#include <math.h>
#include "BHV_Scout.h"
#include "MBUtils.h"
#include "AngleUtils.h"
#include "BuildUtils.h"
#include "GeomUtils.h"
#include "ZAIC_PEAK.h"
#include "OF_Coupler.h"
#include "XYFormatUtilsPoly.h"
#include "XYSegList.h"

using namespace std;

//-----------------------------------------------------------
// Constructor()

BHV_Scout::BHV_Scout(IvPDomain gdomain) : 
  IvPBehavior(gdomain)
{
  IvPBehavior::setParam("name", "scout");
 
  // Default values for behavior state variables
  m_osx  = 0;
  m_osy  = 0;

  // All distances are in meters, all speed in meters per second
  // Default values for configuration parameters 
  m_desired_speed  = 1; 
  m_capture_radius = 10;

  m_pt_set = false;
  m_sensor_radius = 3.0; // Updated from 10 to 3 based on user feedback
  m_current_x = 0;
  m_sweep_state = 0;
  m_lane_width = 8.0;
  m_boustro_init = false;
  m_min_dist = 999999;
  
  addInfoVars("NAV_X, NAV_Y");
  addInfoVars("RESCUE_REGION");
  addInfoVars("SCOUTED_SWIMMER");
  addInfoVars("SWIMMER_ALERT");
  addInfoVars("NODE_REPORT");
}

//---------------------------------------------------------------
// Procedure: setParam() - handle behavior configuration parameters

bool BHV_Scout::setParam(string param, string val) 
{
  // Convert the parameter to lower case for more general matching
  param = tolower(param);
  
  bool handled = true;
  if(param == "capture_radius")
    handled = setPosDoubleOnString(m_capture_radius, val);
  else if(param == "desired_speed")
    handled = setPosDoubleOnString(m_desired_speed, val);
  else if(param == "tmate")
    handled = setNonWhiteVarOnString(m_tmate, val);
  else if(param == "sensor_radius")
    handled = setPosDoubleOnString(m_sensor_radius, val);
  else
    handled = false;

  srand(time(NULL));
  
  return(handled);
}

//-----------------------------------------------------------
// Procedure: onEveryState()

void BHV_Scout::onEveryState(string str) 
{
  if(getBufferVarUpdated("SCOUTED_SWIMMER")) {
    string report = getBufferStringVal("SCOUTED_SWIMMER");
    if(report != "") {
      if(m_tmate == "") {
        postWMessage("Mandatory Teammate name is null");
      } else {
        postOffboardMessage(m_tmate, "SWIMMER_ALERT", report);
      }
    }
  }

  if(getBufferVarUpdated("SWIMMER_ALERT")) {
    bool ok_alert;
    vector<string> alerts = getBufferStringVector("SWIMMER_ALERT", ok_alert);
    for(unsigned int i=0; i<alerts.size(); i++) {
      string alert = alerts[i];
      string x_str = tokStringParse(alert, "X", ',', '=');
      string y_str = tokStringParse(alert, "Y", ',', '=');
      if (x_str != "" && y_str != "") {
        double sx = atof(x_str.c_str());
        double sy = atof(y_str.c_str());
        
        // Add to known swimmers if not already there
        bool already_known = false;
        for (unsigned int j=0; j<m_known_swimmers.size(); j++) {
          if (hypot(m_known_swimmers[j].x() - sx, m_known_swimmers[j].y() - sy) < 1.0) {
            already_known = true;
            break;
          }
        }
        if (!already_known) {
          m_known_swimmers.push_back(XYPoint(sx, sy));
          m_pt_set = false; // Force replan so we don't try to drive over the newly discovered buoy
        }
      }
    }
  }
}

//-----------------------------------------------------------
// Procedure: onIdleState()

void BHV_Scout::onIdleState() 
{
}

//-----------------------------------------------------------
// Procedure: onRunState()

IvPFunction *BHV_Scout::onRunState() 
{
  // Part 1: Get vehicle position from InfoBuffer and post a 
  // warning if problem is encountered
  bool ok1, ok2;
  m_osx = getBufferDoubleVal("NAV_X", ok1);
  m_osy = getBufferDoubleVal("NAV_Y", ok2);
  if(!ok1 || !ok2) {
    postWMessage("No ownship X/Y info in info_buffer.");
    return(0);
  }

  // Part 2: Determine if the vehicle has reached the destination 
  // point and if so, declare completion.
  updateScoutPoint();
  double dist = hypot((m_ptx-m_osx), (m_pty-m_osy));
  
  if (dist < m_min_dist) {
    m_min_dist = dist;
  }

  bool point_reached = false;
  if (dist <= m_capture_radius) { 
    point_reached = true;
  } else if (dist > m_min_dist + 3.0 && m_min_dist < 25.0) {
    // Slipped past it (missed by up to 25m, but distance is now increasing by 3m)
    point_reached = true;
  }

  if (point_reached) { 
    m_pt_set = false;
    postViewPoint(false);
    return(0);
  }

  // Part 3: Post the waypoint as a string for consumption by 
  // a viewer application.
  postViewPoint(true);

  // Part 4: Build the IvP function 
  IvPFunction *ipf = buildFunction();
  if(ipf == 0) 
    postWMessage("Problem Creating the IvP Function");
  
  return(ipf);
}

//-----------------------------------------------------------
// Procedure: updateScoutPoint()

bool BHV_Scout::getVerticalIntersections(double x, double& bottom_y, double& top_y)
{
  bottom_y = 999999;
  top_y = -999999;
  bool found = false;

  unsigned int vsize = m_rescue_region.size();
  for(unsigned int i=0; i<vsize; i++) {
    unsigned int j = (i + 1) % vsize;
    double x1 = m_rescue_region.get_vx(i);
    double y1 = m_rescue_region.get_vy(i);
    double x2 = m_rescue_region.get_vx(j);
    double y2 = m_rescue_region.get_vy(j);

    if ((x1 <= x && x2 >= x) || (x2 <= x && x1 >= x)) {
      if (x1 != x2) {
        double y_int = y1 + (y2 - y1) * (x - x1) / (x2 - x1);
        if (y_int < bottom_y) bottom_y = y_int;
        if (y_int > top_y) top_y = y_int;
        found = true;
      } else if (x1 == x) {
        if (y1 < bottom_y) bottom_y = y1;
        if (y1 > top_y) top_y = y1;
        if (y2 < bottom_y) bottom_y = y2;
        if (y2 > top_y) top_y = y2;
        found = true;
      }
    }
  }
  
  if (found) {
    bottom_y += 5.0; // Inner buffer from absolute bottom
    top_y -= 5.0;    // Inner buffer from absolute top
    if (bottom_y > top_y) {
      double mid = (bottom_y + top_y) / 2.0;
      bottom_y = mid;
      top_y = mid;
    }
  }
  
  return found;
}

void BHV_Scout::updateScoutPoint()
{
  if(m_pt_set)
    return;

  string region_str = getBufferStringVal("RESCUE_REGION");
  if(region_str == "") {
    postWMessage("Unknown RESCUE_REGION");
    return;
  }
  else {
    postRetractWMessage("Unknown RESCUE_REGION");
  }

  m_rescue_region = string2Poly(region_str);
  if(!m_rescue_region.is_convex()) {
    postWMessage("Badly formed RESCUE_REGION");
    return;
  }

  if (m_rescue_region.size() == 0) return;

  if (!m_boustro_init) {
    m_current_x = m_rescue_region.get_min_x() + 15.0;
    m_sweep_state = 0; // 0: Top, 1: Bottom
    m_lane_width = 8.0;
    m_boustro_init = true;
    
    // Plot the full zig-zag pattern once
    XYSegList pattern_seglist;
    double sim_x = m_current_x;
    int sim_state = m_sweep_state;
    
    while (true) {
      double b_y = 0, t_y = 0;
      bool intersects = getVerticalIntersections(sim_x, b_y, t_y);
      if (!intersects || sim_x > m_rescue_region.get_max_x() - 15.0) {
        break;
      }
      
      double ty = (sim_state == 0 || sim_state == 3 || sim_state == 4) ? t_y : b_y;
      pattern_seglist.add_vertex(sim_x, ty);
      
      if (sim_state == 0) {
        sim_state = 1;
      } else if (sim_state == 1) {
        sim_state = 2;
        sim_x += m_lane_width;
      } else if (sim_state == 2) {
        sim_state = 3;
        sim_x += m_lane_width;
      } else if (sim_state == 3) {
        sim_state = 4;
        sim_x += m_lane_width;
      } else if (sim_state == 4) {
        sim_state = 1;
      }
    }
    
    pattern_seglist.set_label("scout_planned_pattern_" + m_us_name);
    pattern_seglist.set_color("edge", "cyan");
    pattern_seglist.set_color("vertex", "cyan");
    pattern_seglist.set_edge_size(1);
    pattern_seglist.set_vertex_size(2);
    postMessage("VIEW_SEGLIST", pattern_seglist.get_spec());
  }

  bool found_point = false;
  int attempts = 0;
  
  while (!found_point && attempts < 8) {
    double bottom_y = 0, top_y = 0;
    
    bool intersects = getVerticalIntersections(m_current_x, bottom_y, top_y);
    
    if (!intersects || m_current_x > m_rescue_region.get_max_x() - 15.0) {
      // Reached far right of polygon. Reset to top-left.
      m_current_x = m_rescue_region.get_min_x() + 15.0;
      m_sweep_state = 0;
      attempts++;
      continue;
    }

    double tx = m_current_x, ty = 0;
    if (m_sweep_state == 0 || m_sweep_state == 3 || m_sweep_state == 4) ty = top_y;
    else ty = bottom_y;

    bool valid = true;
    for(unsigned int j=0; j<m_known_swimmers.size(); j++) {
      if(hypot(tx - m_known_swimmers[j].x(), ty - m_known_swimmers[j].y()) <= 3.0) {
        valid = false;
        break;
      }
    }
    
    if (valid) {
      m_ptx = tx;
      m_pty = ty;
      found_point = true;
    }
    
    // Z-amboni State Machine
    if (m_sweep_state == 0) {
      m_sweep_state = 1;
    } else if (m_sweep_state == 1) {
      m_sweep_state = 2;
      m_current_x += m_lane_width;
    } else if (m_sweep_state == 2) {
      m_sweep_state = 3;
      m_current_x += m_lane_width;
    } else if (m_sweep_state == 3) {
      m_sweep_state = 4;
      m_current_x += m_lane_width;
    } else if (m_sweep_state == 4) {
      m_sweep_state = 1;
    }
    
    attempts++;
  }
  if(!found_point) {
    // Failsafe
    randPointInPoly(m_rescue_region, m_ptx, m_pty);
  }
  m_pt_set = true;
  m_min_dist = 999999; // Reset slip capture distance for the new point

  postViewPoint(true);
}

//-----------------------------------------------------------
// Procedure: postViewPoint()

void BHV_Scout::postViewPoint(bool viewable) 
{

  XYPoint pt(m_ptx, m_pty);
  pt.set_vertex_size(5);
  pt.set_vertex_color("orange");
  pt.set_label(m_us_name + "'s next waypoint");
  
  string point_spec;
  if(viewable)
    point_spec = pt.get_spec("active=true");
  else
    point_spec = pt.get_spec("active=false");
  postMessage("VIEW_POINT", point_spec);
}


//-----------------------------------------------------------
// Procedure: buildFunction()

IvPFunction *BHV_Scout::buildFunction() 
{
  if(!m_pt_set)
    return(0);
  
  ZAIC_PEAK spd_zaic(m_domain, "speed");
  spd_zaic.setSummit(m_desired_speed);
  spd_zaic.setPeakWidth(0.5);
  spd_zaic.setBaseWidth(1.0);
  spd_zaic.setSummitDelta(0.8);  
  if(spd_zaic.stateOK() == false) {
    string warnings = "Speed ZAIC problems " + spd_zaic.getWarnings();
    postWMessage(warnings);
    return(0);
  }
  
  double rel_ang_to_wpt = relAng(m_osx, m_osy, m_ptx, m_pty);
  ZAIC_PEAK crs_zaic(m_domain, "course");
  crs_zaic.setSummit(rel_ang_to_wpt);
  crs_zaic.setPeakWidth(0);
  crs_zaic.setBaseWidth(180.0);
  crs_zaic.setSummitDelta(0);  
  crs_zaic.setValueWrap(true);
  if(crs_zaic.stateOK() == false) {
    string warnings = "Course ZAIC problems " + crs_zaic.getWarnings();
    postWMessage(warnings);
    return(0);
  }

  IvPFunction *spd_ipf = spd_zaic.extractIvPFunction();
  IvPFunction *crs_ipf = crs_zaic.extractIvPFunction();

  OF_Coupler coupler;
  IvPFunction *ivp_function = coupler.couple(crs_ipf, spd_ipf, 50, 50);

  return(ivp_function);
}