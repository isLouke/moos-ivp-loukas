/************************************************************/
/*    NAME: George Loukas                                   */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: GenRescue.h                                     */
/*    DATE: June 22nd, 2026                                 */
/*                                                          */
/*    Swimmer-aware rescue path planner. Ingestes           */
/*    SWIMMER_ALERT (deduped by id), removes rescued        */
/*    swimmers on FOUND_SWIMMER, and posts an optimal path  */
/*    (SOM TSP) to GEN_PATH for the waypoint behavior.      */
/************************************************************/

#ifndef P_GEN_RESCUE_HEADER
#define P_GEN_RESCUE_HEADER

#include <vector>
#include <string>
#include <map>
#include <set>
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"
#include "XYPoint.h"
#include "XYSegList.h"

class GenRescue : public AppCastingMOOSApp
{
 public:
   GenRescue();
   ~GenRescue() {};

 protected:
   bool OnNewMail(MOOSMSG_LIST &NewMail);
   bool Iterate();
   bool OnConnectToServer();
   bool OnStartUp();
   bool buildReport();
   void RegisterVariables();
   
 protected: // Mail handlers
   bool handleMailNewSwimmer(std::string);
   bool handleMailFoundSwimmer(std::string);

 protected: // Path planning
   bool postPath();
   bool postNullPath();
   std::vector<std::string> computeSOMOrder();
   void clearSwimmers();

 private: // Config variables
   std::string m_vname;
   
 private: // State variables
   double  m_nav_x;
   double  m_nav_y;
   bool    m_nav_x_set;
   bool    m_nav_y_set;
   bool    m_path_needs_update;
   bool    m_path_needs_som;

   // Stored SOM-optimized ordering of swimmer IDs (only includes live swimmers)
   std::vector<std::string> m_ordered_ids;

   // Swimmer tracking: id -> XYPoint position; set of rescued ids
   std::map<std::string, XYPoint> m_swimmers;
   std::set<std::string>          m_rescued;

   // Plan lifecycle (matches example pattern)
   bool             m_plan_pending;
   bool             m_plan_posted;
   bool             m_returned;
   unsigned int     m_plan_size;
   unsigned int     m_prev_swimmer_count;
   unsigned int     m_settle_iters;
   unsigned int     m_alerts_rcvd;
   std::string      m_last_points;   // last posted "points=" spec, for dedup
   XYSegList        m_path;          // last computed path
};

#endif
