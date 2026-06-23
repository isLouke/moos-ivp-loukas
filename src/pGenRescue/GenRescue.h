/************************************************************/
/*    NAME: George Loukas                                   */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: GenRescue.h                                     */
/*    DATE: June 22nd, 2026                                 */
/************************************************************/

#ifndef P_GEN_RESCUE_HEADER
#define P_GEN_RESCUE_HEADER

#include <vector>
#include <string>
#include <map>
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"
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
   
  protected:
    bool handleMailNewSwimmer(std::string);
    bool handleMailFoundSwimmer(std::string);
    bool handleMailVisitPoint(std::string);
    void flushVisitPoints();
    bool postPath();
    bool postNullPath();
    void clearSwimmers();
    std::vector<std::string> computeSOMOrder();

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

    // VISIT_POINT accumulation (fallback if SWIMMER_ALERT bridge is delayed)
    bool                        m_collecting_visits;
    std::vector<XYPoint>        m_visit_accumulator;
    bool                        m_visit_points_flushed;

    // True once at least one SWIMMER_ALERT has been received.
    // Used to suppress the VISIT_POINT fallback and to clean up
    // any auto-generated vp_* entries when real alerts arrive.
    bool                        m_swimmer_alert_received;

    // Swimmer tracking: id -> {x, y, rescued}
   std::map<std::string, double> m_swimmer_x;
   std::map<std::string, double> m_swimmer_y;
   std::map<std::string, bool>   m_swimmer_rescued;
};

#endif
