/***************************** SCHEDULER.H ***********************************
- SAFARI GROUP
This file contains the different scheduling policies and row policies that the 
memory controller can use to schedule requests.
Current Memory Scheduling Policies:
1) FCFS - First Come First Serve
        This scheduling policy schedules memory requests chronologically
2) FRFCFS - Frist Ready First Come First Serve
        This scheduling policy first checks if a request is READY(meets all 
        timing parameters), if yes then it is prioritized. If multiple requests
        are ready, they they are scheduled chronologically. Otherwise, it 
        behaves the same way as FCFS. 
3) FRFCFS_Cap - First Ready First Come First Serve Cap
       This scheduling policy behaves the same way as FRFCS, except that it has
       a cap on the number of hits you can get in a certain row. The CAP VALUE
       can be altered by changing the number for the "cap" variable in 
       line number 76. 
4) FRFCFS_PriorHit - First Ready First Come First Serve Prioritize Hits
       This scheduling policy behaves the same way as FRFCFS, except that it
       prioritizes row hits more than readiness. 
You can select which scheduler you want to use by changing the value of 
"type" variable on line number 74.
                _______________________________________
Current Row Policies:
1) Closed   - Precharges a row as soon as there are no pending references to 
              the active row.
2) ClosedAP - Closed Auto Precharge
3) Opened   - Precharges a row only if there are pending references to 
              other rows.
4) Timeout  - Precharges a row after X time if there are no pending references.
              'X' time can be changed by changing the variable timeout 
              on line number 221
*****************************************************************************/

#ifndef __SCHEDULER_H
#define __SCHEDULER_H

#include "DRAM.h"
#include "Request.h"
#include "Controller.h"
#include <vector>
#include <map>
#include <list>
#include <functional>
#include <cassert>

using namespace std;

namespace ramulator
{

template <typename T>
class Controller;

typedef list<Request>::iterator ReqIter;

template <typename T>
class Scheduling_Policy
{
public:
    Scheduling_Policy(Controller<T>* ctrl) : ctrl(ctrl) {}
    ReqIter get_head(list<Request>& q)
    {
        //If queue is empty, return end of queue
        if (!q.size())
            return q.end();

        //Else return based on the policy
        auto head = q.begin();
        for (auto itr = next(q.begin(), 1); itr != q.end(); itr++)
            head = this->compare(head, itr);

        return head;
    }

protected:
    Controller<T>* ctrl;
    virtual ReqIter compare(ReqIter req1, ReqIter req2);
};

template <typename T>
class FCFS: public Scheduling_Policy<T> {
public:
    FCFS(Controller<T>* ctrl) : Scheduling_Policy<T>(ctrl) {}

private:
    ReqIter compare(ReqIter req1, ReqIter req2) {
        if (req1->arrive <= req2->arrive) return req1;
        return req2;
    };
};

template <typename T>
class FRFCFS: public Scheduling_Policy<T> {
public:
    FRFCFS(Controller<T>* ctrl) : Scheduling_Policy<T>(ctrl) {}

private:
    ReqIter compare(ReqIter req1, ReqIter req2) {
        bool ready1 = this->ctrl->is_ready(req1);
        bool ready2 = this->ctrl->is_ready(req2);

        if (ready1 ^ ready2) {
            if (ready1) return req1;
            return req2;
        }

        if (req1->arrive <= req2->arrive) return req1;
        return req2;
    };
};

template <typename T>
class FRFCFS_Cap: public Scheduling_Policy<T> {
public:
    FRFCFS_Cap(Controller<T>* ctrl) : Scheduling_Policy<T>(ctrl) {}

private:
    long cap = 16; //Change this line to change cap

    ReqIter compare(ReqIter req1, ReqIter req2) {
        bool ready1 = this->ctrl->is_ready(req1);
        bool ready2 = this->ctrl->is_ready(req2);

        ready1 = ready1 && (this->ctrl->rowtable->get_hits(req1->addr_vec) <= this->cap);
        ready2 = ready2 && (this->ctrl->rowtable->get_hits(req2->addr_vec) <= this->cap);

        if (ready1 ^ ready2) {
            if (ready1) return req1;
            return req2;
        }

        if (req1->arrive <= req2->arrive) return req1;
        return req2;
    };
};

template <typename T>
class FRFCFS_PriorHit: public Scheduling_Policy<T> {
public:
    FRFCFS_PriorHit(Controller<T>* ctrl) : Scheduling_Policy<T>(ctrl) {}

    ReqIter get_head(list<Request>& q) {
        //If queue is empty, return end of queue
        if (!q.size())
            return q.end();

        //Else return based on FRFCFS_PriorHit Scheduling Policy
        auto head = q.begin();
        for (auto itr = next(q.begin(), 1); itr != q.end(); itr++) {
            head = compare(head, itr);
        }

        if (this->ctrl->is_ready(head) && this->ctrl->is_row_hit(head)) {
            return head;
        }

        // prepare a list of hit request
        vector<vector<int>> hit_reqs;
        for (auto itr = q.begin() ; itr != q.end() ; ++itr) {
            if (this->ctrl->is_row_hit(itr)) {
                auto begin = itr->addr_vec.begin();
                // TODO Here it assumes all DRAM standards use PRE to close a row
                // It's better to make it more general.
                auto end = begin + int(this->ctrl->channel->spec->scope[int(T::Command::PRE)]) + 1;
                vector<int> rowgroup(begin, end); // bank or subarray
                hit_reqs.push_back(rowgroup);
            }
        }
        // if we can't find proper request, we need to return q.end(),
        // so that no command will be scheduled
        head = q.end();
        for (auto itr = q.begin(); itr != q.end(); itr++) {
            bool violate_hit = false;
            if ((!this->ctrl->is_row_hit(itr)) && this->ctrl->is_row_open(itr)) {
                // so the next instruction to be scheduled is PRE, might violate hit
                auto begin = itr->addr_vec.begin();
                // TODO Here it assumes all DRAM standards use PRE to close a row
                // It's better to make it more general.
                auto end = begin + int(this->ctrl->channel->spec->scope[int(T::Command::PRE)]) + 1;
                vector<int> rowgroup(begin, end); // bank or subarray
                for (const auto& hit_req_rowgroup : hit_reqs) {
                    if (rowgroup == hit_req_rowgroup) {
                        violate_hit = true;
                        break;
                    }
                }
            }
            if (violate_hit) {
                continue;
            }
            // If it comes here, that means it won't violate any hit request
            if (head == q.end()) {
                head = itr;
            } else {
                head = compare(head, itr);
            }
        }

        return head;
    };

private:
    ReqIter compare(ReqIter req1, ReqIter req2) {
        bool ready1 = this->ctrl->is_ready(req1) && this->ctrl->is_row_hit(req1);
        bool ready2 = this->ctrl->is_ready(req2) && this->ctrl->is_row_hit(req2);

        if (ready1 ^ ready2) {
            if (ready1) return req1;
            return req2;
        }

        if (req1->arrive <= req2->arrive) return req1;
        return req2;
    };
};

template <typename T>
class ATLAS: public Scheduling_Policy<T> {
public:
    static const long QUANTUM_LENGTH = 10000000;
    static const long THRESHOLD = 100000;
    static constexpr double ALPHA = 0.875;

    ATLAS(Controller<T>* ctrl) : Scheduling_Policy<T>(ctrl) {}

private:
    ReqIter compare(ReqIter req1, ReqIter req2) {
        // over-threshold-requests-first
        if (req1->arrive + THRESHOLD <= this->ctrl->clk)
            return req1;
        if (req2->arrive + THRESHOLD <= this->ctrl->clk)
            return req2;

        // lower TotalAS first
        double totalAS_1 = this->ctrl->get_total_as(req1->coreid);
        double totalAS_2 = this->ctrl->get_total_as(req2->coreid);
        if (totalAS_1 < totalAS_2)
          return req1;
        if (totalAS_2 < totalAS_1)
          return req2;

        // row-hit-first
        bool ready1 = this->ctrl->is_ready(req1) && this->ctrl->is_row_hit(req1);
        bool ready2 = this->ctrl->is_ready(req2) && this->ctrl->is_row_hit(req2);

        if (ready1 ^ ready2) {
            if (ready1) return req1;
            return req2;
        }

        // oldest-first
        if (req1->arrive <= req2->arrive) return req1;
        return req2;
    };
};

template <typename T>
class BLISS: public Scheduling_Policy<T> {
public:
    static const long CLEARING_INTERVAL = 10000;
    static const long THRESHOLD = 4;

    BLISS(Controller<T>* ctrl) : Scheduling_Policy<T>(ctrl) {}

private:
    ReqIter compare(ReqIter req1, ReqIter req2) {
        // non-blacklisted first
        bool blacklisted1 = this->ctrl->is_blacklisted(req1->coreid);
        bool blacklisted2 = this->ctrl->is_blacklisted(req2->coreid);

        if (blacklisted1 ^ blacklisted2) {
            if (blacklisted1) return req2;
            return req1;
        }

        // row-hit-first
        bool ready1 = this->ctrl->is_ready(req1) && this->ctrl->is_row_hit(req1);
        bool ready2 = this->ctrl->is_ready(req2) && this->ctrl->is_row_hit(req2);

        if (ready1 ^ ready2) {
            if (ready1) return req1;
            return req2;
        }

        // oldest-first
        if (req1->arrive <= req2->arrive) return req1;
        return req2;
    };
};

template <typename T>
class Scheduler
{
public:
    Controller<T>* ctrl;
    Scheduling_Policy<T>* policy;

    Scheduler(Controller<T>* ctrl, const Config& configs) : ctrl(ctrl) {
        /*
         * Change class name to one of the following:
         *
         * - FCFS
         * - FRFCFS
         * - FRFCFS_Cap
         * - FRFCFS_PriorHit
         * - ATLAS
         * - BLISS
         */
        auto default_policy = new FCFS<T>(ctrl);

        if (configs.contains("scheduling_policy")) {
            std::string policy_str = configs["scheduling_policy"];

            if (policy_str.compare("FCFS") == 0) {
                printf("Scheduling Policy: FCFS\n");
                policy = new FCFS<T>(ctrl);
            } else if (policy_str.compare("FRFCFS") == 0) {
                printf("Scheduling Policy: FRFCFS\n");
                policy = new FRFCFS<T>(ctrl);
            } else if (policy_str.compare("FRFCFS_Cap") == 0) {
                printf("Scheduling Policy: FRFCFS_Cap\n");
                policy = new FRFCFS_Cap<T>(ctrl);
            } else if (policy_str.compare("FRFCFS_PriorHit") == 0) {
                printf("Scheduling Policy: FRFCFS_PriorHit\n");
                policy = new FRFCFS_PriorHit<T>(ctrl);
            } else if (policy_str.compare("ATLAS") == 0) {
                printf("Scheduling Policy: ATLAS\n");
                policy = new ATLAS<T>(ctrl);
            } else if (policy_str.compare("BLISS") == 0) {
                printf("Scheduling Policy: BLISS\n");
                policy = new BLISS<T>(ctrl);
            }
        } else {
            policy = default_policy;
        }
    }

    list<Request>::iterator get_head(list<Request>& q)
    {
        return this->policy->get_head(q);
    }
};

// Row Precharge Policy
template <typename T>
class RowPolicy
{
public:
    Controller<T>* ctrl;

    enum class Type {
        Closed, ClosedAP, Opened, Timeout, MAX
    } type = Type::Opened;

    int timeout = 50;

    RowPolicy(Controller<T>* ctrl) : ctrl(ctrl) {}

    vector<int> get_victim(typename T::Command cmd)
    {
        return policy[int(type)](cmd);
    }

private:
    function<vector<int>(typename T::Command)> policy[int(Type::MAX)] = {
        // Closed
        [this] (typename T::Command cmd) -> vector<int> {
            for (auto& kv : this->ctrl->rowtable->table) {
                if (!this->ctrl->is_ready(cmd, kv.first))
                    continue;
                return kv.first;
            }
            return vector<int>();},

        // ClosedAP
        [this] (typename T::Command cmd) -> vector<int> {
            for (auto& kv : this->ctrl->rowtable->table) {
                if (!this->ctrl->is_ready(cmd, kv.first))
                    continue;
                return kv.first;
            }
            return vector<int>();},

        // Opened
        [this] (typename T::Command cmd) {
            return vector<int>();},

        // Timeout
        [this] (typename T::Command cmd) -> vector<int> {
            for (auto& kv : this->ctrl->rowtable->table) {
                auto& entry = kv.second;
                if (this->ctrl->clk - entry.timestamp < timeout)
                    continue;
                if (!this->ctrl->is_ready(cmd, kv.first))
                    continue;
                return kv.first;
            }
            return vector<int>();}
    };

};


template <typename T>
class RowTable
{
public:
    Controller<T>* ctrl;

    struct Entry {
        int row;
        int hits;
        long timestamp;
    };

    map<vector<int>, Entry> table;

    RowTable(Controller<T>* ctrl) : ctrl(ctrl) {}

    void update(typename T::Command cmd, const vector<int>& addr_vec, long clk)
    {
        auto begin = addr_vec.begin();
        auto end = begin + int(T::Level::Row);
        vector<int> rowgroup(begin, end); // bank or subarray
        int row = *end;

        T* spec = ctrl->channel->spec;

        if (spec->is_opening(cmd))
            table.insert({rowgroup, {row, 0, clk}});

        if (spec->is_accessing(cmd)) {
            // we are accessing a row -- update its entry
            auto match = table.find(rowgroup);
            assert(match != table.end());
            assert(match->second.row == row);
            match->second.hits++;
            match->second.timestamp = clk;
        } /* accessing */

        if (spec->is_closing(cmd)) {
          // we are closing one or more rows -- remove their entries
          int n_rm = 0;
          int scope;
          if (spec->is_accessing(cmd))
            scope = int(T::Level::Row) - 1; //special condition for RDA and WRA
          else
            scope = int(spec->scope[int(cmd)]);

          for (auto it = table.begin(); it != table.end();) {
            if (equal(begin, begin + scope + 1, it->first.begin())) {
              n_rm++;
              it = table.erase(it);
            }
            else
              it++;
          }

          assert(n_rm > 0);
        } /* closing */
    }

    int get_hits(const vector<int>& addr_vec, const bool to_opened_row = false)
    {
        auto begin = addr_vec.begin();
        auto end = begin + int(T::Level::Row);

        vector<int> rowgroup(begin, end);
        int row = *end;

        auto itr = table.find(rowgroup);
        if (itr == table.end())
            return 0;

        if(!to_opened_row && (itr->second.row != row))
            return 0;

        return itr->second.hits;
    }

    int get_open_row(const vector<int>& addr_vec) {
        auto begin = addr_vec.begin();
        auto end = begin + int(T::Level::Row);

        vector<int> rowgroup(begin, end);

        auto itr = table.find(rowgroup);
        if(itr == table.end())
            return -1;

        return itr->second.row;
    }
};

} /*namespace ramulator*/

#endif /*__SCHEDULER_H*/
