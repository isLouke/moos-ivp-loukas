/*****************************************************************/
/*    NAME: M.Benjamin,                                          */
/*    ORGN: Dept of Mechanical Eng / CSAIL, MIT Cambridge MA     */
/*    FILE: BHV_Scout.h                                          */
/*    DATE: April 30th 2022                                      */
/*                                                               */
/* This program is free software; you can redistribute it and/or */
/* modify it under the terms of the GNU General Public License   */
/* as published by the Free Software Foundation; either version  */
/* 2 of the License, or (at your option) any later version.      */
/*                                                               */
/* This program is distributed in the hope that it will be       */
/* useful, but WITHOUT ANY WARRANTY; without even the implied    */
/* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR       */
/* PURPOSE. See the GNU General Public License for more details. */
/*                                                               */
/* You should have received a copy of the GNU General Public     */
/* License along with this program; if not, write to the Free    */
/* Software Foundation, Inc., 59 Temple Place - Suite 330,       */
/* Boston, MA 02111-1307, USA.                                   */
/*****************************************************************/
 
#ifndef BHV_SCOUT_HEADER
#define BHV_SCOUT_HEADER

#include <string>
#include <vector>
#include "IvPBehavior.h"
#include "ZAIC_PEAK.h"
#include "XYPoint.h"
#include "XYPolygon.h"

class BHV_Scout : public IvPBehavior {
public:
  BHV_Scout(IvPDomain);
  ~BHV_Scout() {};
  
  bool         setParam(std::string, std::string);
  void         onIdleState();
  IvPFunction* onRunState();
  void         onEveryState(std::string);
  
protected: // Local Utility functions
  void         updateScoutPoint();
  void         postViewPoint(bool viewable=true);
  IvPFunction* buildFunction();
  bool         getVerticalIntersections(double x, double& bottom_y, double& top_y);

protected: // Configuration parameters
  std::string m_tmate;
  double      m_capture_radius;
  double      m_desired_speed;
  double      m_sensor_radius;

protected: // State variables
  double      m_osx;
  double      m_osy;
  double      m_ptx;
  double      m_pty;
  bool        m_pt_set;
  XYPolygon   m_rescue_region;
  
  double      m_current_x;
  int         m_sweep_state;
  double      m_lane_width;
  bool        m_boustro_init;
  double      m_min_dist;
  
  std::vector<XYPoint> m_known_swimmers;
};

#define IVP_EXPORT_FUNCTION
extern "C" {
  IVP_EXPORT_FUNCTION IvPBehavior * createBehavior(std::string name, IvPDomain domain) 
  {return new BHV_Scout(domain);}
}
#endif
