#pragma once

#include <string>
#include <vector>
using std::string;
using std::vector;

#include "fixed_types.h"
#include "queue_model.h"

class NetworkModel;
class NetPacket;

class NetworkRouterModel
{
public:
   NetworkRouterModel(NetworkModel* model, SInt32 num_input_ports, SInt32 num_output_ports,
                      SInt32 num_flits_per_port_buffer, UInt64 delay, SInt32 flit_width,
                      bool contention_model_enabled, string& contention_model_type);
   ~NetworkRouterModel();

   void processPacket(const NetPacket& pkt, SInt32 output_port,
                      UInt64& zero_load_delay, UInt64& contention_delay);
   void processPacket(const NetPacket& pkt, vector<SInt32>& output_port_list,
                      UInt64& zero_load_delay, UInt64& contention_delay);
   
   UInt64 getTotalBufferWrites()                            { return _total_buffer_writes; }
   UInt64 getTotalBufferReads()                             { return _total_buffer_reads; }
   UInt64 getTotalSwitchAllocatorRequests()                 { return _total_switch_allocator_requests; }
   UInt64 getTotalCrossbarTraversals(SInt32 multicast_idx)  { return _total_crossbar_traversals[multicast_idx-1]; }

   static const SInt32 OUTPUT_PORT_ALL = -1;

private:
   NetworkModel* _model;
   SInt32 _num_input_ports;
   SInt32 _num_output_ports;
   SInt32 _flit_width;
   UInt64 _delay;
   bool _contention_model_enabled;
   vector<QueueModel*> _contention_model_list;

   // Event Counters
   UInt64 _total_buffer_writes;
   UInt64 _total_buffer_reads;
   UInt64 _total_switch_allocator_requests;
   vector<UInt64> _total_crossbar_traversals;

   // Initialize Event Counters
   void initializeEventCounters();
};
