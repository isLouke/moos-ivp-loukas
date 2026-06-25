/*****************************************************************/
/*    NAME: George Chrysanidis                                   */
/*    ORGN: Dept of Mechanical Eng / CSAIL, MIT Cambridge MA     */
/*    FILE: BHV_Scout.cpp                                        */
/*    DATE: June 24th, 2026                                      */
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
  m_capture_radius = 3.0;

  m_pt_set = false;
  m_grid_generated = false;
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
  else if(param == "adv_scout")
    handled = true; // Ignore but accept
  else if(param == "adv_rescuer")
    handled = true; // Ignore but accept
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
      string val = alerts[i];
      double sx = 0, sy = 0;
      bool okx = false, oky = false;
      
      vector<string> svector = parseString(val, ',');
      for (unsigned int j=0; j<svector.size(); j++) {
        string param = stripBlankEnds(biteStringX(svector[j], '='));
        string value = stripBlankEnds(svector[j]);
        if (param == "x") {
           sx = atof(value.c_str());
           okx = true;
        }
        else if (param == "y") {
           sy = atof(value.c_str());
           oky = true;
        }
      }
      
      if (okx && oky) {
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
          
          // Update any existing cells immediately so they turn green
          for (unsigned int k=0; k<m_cells.size(); k++) {
             if (abs(sx - m_cells[k].x) <= 5.0 && abs(sy - m_cells[k].y) <= 5.0) {
                m_cells[k].min_dist = 0.0;
                m_cells[k].weight = 0.0;
             }
          }
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

  // Track all boats
  vector<XYPoint> boats;
  boats.push_back(XYPoint(m_osx, m_osy)); // scout itself
  
  if(getBufferVarUpdated("NODE_REPORT")) {
    bool ok_rep;
    vector<string> reports = getBufferStringVector("NODE_REPORT", ok_rep);
    for(unsigned int i=0; i<reports.size(); i++) {
      string report = reports[i];
      string x_str = tokStringParse(report, "X", ',', '=');
      string y_str = tokStringParse(report, "Y", ',', '=');
      string type_str = tokStringParse(report, "TYPE", ',', '=');
      
      bool is_heron = false;
      if (type_str == "heron" || type_str == "HERON" || type_str == "Heron") {
          is_heron = true;
      }
      
      if (is_heron) continue;

      if (x_str != "" && y_str != "") {
         double bx = atof(x_str.c_str());
         double by = atof(y_str.c_str());
         boats.push_back(XYPoint(bx, by));
      }
    }
  }

  // Update cell weights
  for (unsigned int i=0; i<m_cells.size(); i++) {
     for (auto& b : boats) {
        double d = hypot(m_cells[i].x - b.x(), m_cells[i].y - b.y());
        if (d < m_cells[i].min_dist) {
           m_cells[i].min_dist = d;
        }
     }
     
     // Calculate new weight
     if (m_cells[i].min_dist <= 5.0) {
        m_cells[i].weight = 0.0;
     } else if (m_cells[i].min_dist < 10.0) {
        double w = (m_cells[i].min_dist - 5.0) / 5.0;
        if (w < m_cells[i].weight) m_cells[i].weight = w;
     }
     
     // Visualization update if weight changed
     if (abs(m_cells[i].weight - m_cells[i].last_drawn_weight) > 0.05) {
        m_cells[i].last_drawn_weight = m_cells[i].weight;
        string color = "red";
        if (m_cells[i].weight < 0.2) color = "green";
        else if (m_cells[i].weight < 0.6) color = "yellow";
        else if (m_cells[i].weight < 0.9) color = "orange";
        
        XYPolygon poly;
        poly.add_vertex(m_cells[i].x - 5, m_cells[i].y - 5);
        poly.add_vertex(m_cells[i].x + 5, m_cells[i].y - 5);
        poly.add_vertex(m_cells[i].x + 5, m_cells[i].y + 5);
        poly.add_vertex(m_cells[i].x - 5, m_cells[i].y + 5);
        poly.set_label("cell_" + m_us_name + "_" + intToString(i));
        poly.set_color("edge", "gray");
        poly.set_color("fill", color);
        postMessage("VIEW_POLYGON", poly.get_spec());
     }
  }

  // Part 2: Determine if the vehicle has reached the destination 
  // point and if so, declare completion.
  updateScoutPoint();
  double dist = hypot((m_ptx-m_osx), (m_pty-m_osy));
  
  if (dist < m_min_dist) {
    m_min_dist = dist;
  }

  bool point_reached = false;
  if (dist <= 3.0) { 
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

void BHV_Scout::updateScoutPoint()
{
  if(m_pt_set) {
    // Check if the current target cell has already been searched
    for (unsigned int i=0; i<m_cells.size(); i++) {
       if (m_cells[i].x == m_ptx && m_cells[i].y == m_pty) {
          if (m_cells[i].weight == 0.0) {
             m_pt_set = false; // Force repick
          }
          break;
       }
    }
  }

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

  if (!m_grid_generated) {
    m_grid_generated = true;
    m_cells.clear();
    
    // Seed points based on known swimmers (if any)
    double seed_x = m_rescue_region.get_min_x();
    double seed_y = m_rescue_region.get_min_y();
    if (!m_known_swimmers.empty()) {
      seed_x = m_known_swimmers[0].x();
      seed_y = m_known_swimmers[0].y();
    }
    
    // Grid alignment offsets
    double off_x = fmod(seed_x, 10.0);
    double off_y = fmod(seed_y, 10.0);
    if (off_x < 0) off_x += 10.0;
    if (off_y < 0) off_y += 10.0;

    double min_x = m_rescue_region.get_min_x() - 10.0;
    double max_x = m_rescue_region.get_max_x() + 10.0;
    double min_y = m_rescue_region.get_min_y() - 10.0;
    double max_y = m_rescue_region.get_max_y() + 10.0;
    
    for (double x = min_x; x <= max_x; x += 10.0) {
      for (double y = min_y; y <= max_y; y += 10.0) {
        // align
        double cx = floor((x - off_x)/10.0)*10.0 + off_x;
        double cy = floor((y - off_y)/10.0)*10.0 + off_y;
        
          if (m_rescue_region.contains(cx, cy)) {
          // ensure no duplicates
          bool duplicate = false;
          for (auto& c : m_cells) {
            if (hypot(c.x - cx, c.y - cy) < 1.0) duplicate = true;
          }
          
          bool near_buoy = false;
          double buoy_x[] = {-35.0, -62.5, -95.0};
          double buoy_y[] = {-6.0, -17.9, -28.0};
          for (int b=0; b<3; b++) {
             if (abs(cx - buoy_x[b]) <= 5.0 && abs(cy - buoy_y[b]) <= 5.0) {
                 near_buoy = true;
                 break;
             }
          }
          if (near_buoy) continue;

          if (!duplicate) {
             SearchCell cell;
             cell.x = cx;
             cell.y = cy;
             cell.min_dist = 999999.0;
             cell.weight = 1.0;
             
             for(auto& ks : m_known_swimmers) {
                if (abs(ks.x() - cx) <= 5.0 && abs(ks.y() - cy) <= 5.0) {
                   cell.min_dist = 0.0;
                   cell.weight = 0.0;
                }
             }
             
             cell.last_drawn_weight = -1.0; // Force draw
             m_cells.push_back(cell);
          }
        }
      }
    }
  }

  // Path Planning: Find cell with highest weight, breaking ties by closest distance
  double max_weight = -1.0;
  double best_dist = 999999.0;
  int best_index = -1;
  
  for (unsigned int i=0; i<m_cells.size(); i++) {
     if (m_cells[i].weight > 0.0) {
        double dist = hypot(m_cells[i].x - m_osx, m_cells[i].y - m_osy);
        
        if (m_cells[i].weight > max_weight + 0.01) {
           max_weight = m_cells[i].weight;
           best_dist = dist;
           best_index = i;
        } else if (abs(m_cells[i].weight - max_weight) <= 0.01) {
           if (dist < best_dist) {
              best_dist = dist;
              best_index = i;
           }
        }
     }
  }
  
  if (best_index != -1) {
     m_ptx = m_cells[best_index].x;
     m_pty = m_cells[best_index].y;
     m_pt_set = true;
     m_min_dist = 999999;
     postViewPoint(true);
  } else {
     // No cells left to search, just hover or random point
     randPointInPoly(m_rescue_region, m_ptx, m_pty);
     m_pt_set = true;
     m_min_dist = 999999;
     postViewPoint(true);
  }
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