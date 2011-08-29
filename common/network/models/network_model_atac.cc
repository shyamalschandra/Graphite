#include <math.h>
#include <vector>
using namespace std;

#include "network_model_atac.h"
#include "simulator.h"
#include "tile_manager.h"
#include "tile.h"
#include "config.h"
#include "log.h"
#include "utils.h"

//// Static Variables
// Is Initialized?
bool NetworkModelAtac::_initialized = false;
// ENet
SInt32 NetworkModelAtac::_enet_width;
SInt32 NetworkModelAtac::_enet_height;
// Clusters
SInt32 NetworkModelAtac::_num_clusters;
SInt32 NetworkModelAtac::_cluster_size;
SInt32 NetworkModelAtac::_numX_clusters;
SInt32 NetworkModelAtac::_numY_clusters;
SInt32 NetworkModelAtac::_cluster_width;
SInt32 NetworkModelAtac::_cluster_height;
// Sub Clusters
SInt32 NetworkModelAtac::_num_access_points_per_cluster;
SInt32 NetworkModelAtac::_num_sub_clusters;
SInt32 NetworkModelAtac::_numX_sub_clusters;
SInt32 NetworkModelAtac::_numY_sub_clusters;
SInt32 NetworkModelAtac::_sub_cluster_width;
SInt32 NetworkModelAtac::_sub_cluster_height;
// Cluster Boundaries and Access Points
vector<NetworkModelAtac::ClusterInfo> NetworkModelAtac::_cluster_info_list;
// Type of Receive Network
NetworkModelAtac::ReceiveNetType NetworkModelAtac::_receive_net_type;
// Num Receive Nets
SInt32 NetworkModelAtac::_num_receive_networks_per_cluster;
// Global Routing Strategy
NetworkModelAtac::GlobalRoutingStrategy NetworkModelAtac::_global_routing_strategy;
SInt32 NetworkModelAtac::_unicast_distance_threshold;

NetworkModelAtac::NetworkModelAtac(Network *net, SInt32 network_id):
   NetworkModel(net, network_id)
{
   try
   {
      _frequency = Sim()->getCfg()->getFloat("network/atac/frequency");
      _flit_width = Sim()->getCfg()->getInt("network/atac/flit_width");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read ATAC frequency and flit_width parameters from cfg file");
   }

   // Has Broadcast Capability
   _has_broadcast_capability = true;

   // Initialize ANet topology
   initializeANetTopologyParams();

   // Initialize ENet, ONet and BNet parameters
   createANetRouterAndLinkModels();
}

NetworkModelAtac::~NetworkModelAtac()
{
   // Destroy the Link Models
   destroyANetRouterAndLinkModels();
}

void
NetworkModelAtac::initializeANetTopologyParams()
{
   if (_initialized)
      return;
   _initialized = true;

   // Initialize _total_tiles, _cluster_size, _sqrt_cluster_size, _mesh_width, _mesh_height, _num_clusters
   SInt32 num_application_tiles = Config::getSingleton()->getApplicationTiles();

   try
   {
      _cluster_size = Sim()->getCfg()->getInt("network/atac/cluster_size");
      _num_access_points_per_cluster = Sim()->getCfg()->getInt("network/atac/num_optical_access_points_per_cluster");

      // Topology
      _receive_net_type = parseReceiveNetType(Sim()->getCfg()->getString("network/atac/receive_net_type"));
      _num_receive_networks_per_cluster = Sim()->getCfg()->getInt("network/atac/num_receive_networks_per_cluster");
      
      // Routing
      _global_routing_strategy = parseGlobalRoutingStrategy(Sim()->getCfg()->getString("network/atac/global_routing_strategy"));
      _unicast_distance_threshold = Sim()->getCfg()->getInt("network/atac/unicast_distance_threshold");

   }
   catch (...)
   {
      LOG_PRINT_ERROR("Error reading atac cluster size");
   }

   LOG_ASSERT_ERROR(isPerfectSquare(num_application_tiles),
         "Num Application Tiles(%i) must be a perfect square", num_application_tiles);
   LOG_ASSERT_ERROR(isPower2(num_application_tiles),
         "Num Application Tiles(%i) must be a power of 2", num_application_tiles);
   LOG_ASSERT_ERROR(isPower2(_cluster_size),
         "Cluster Size(%i) must be a power of 2", _cluster_size);
   LOG_ASSERT_ERROR((num_application_tiles % _cluster_size) == 0,
         "Num Application Tiles(%i) must be a multiple of Cluster Size(%i)", num_application_tiles, _cluster_size);

   _num_clusters = num_application_tiles / _cluster_size;
   LOG_ASSERT_ERROR(_num_clusters > 1, "Number of Clusters(%i) must be > 1", _num_clusters);
   
   _num_sub_clusters = _num_access_points_per_cluster;
   LOG_ASSERT_ERROR(isPower2(_num_access_points_per_cluster),
         "Number of Optical Access Points(%i) must be a power of 2", _num_access_points_per_cluster);
   _enet_width = (SInt32) floor(sqrt(num_application_tiles));
   _enet_height = _enet_width;
   
   initializeClusters();
}

void
NetworkModelAtac::createANetRouterAndLinkModels()
{
   if (isSystemTile(_tile_id))
      return;

   // ENet Router
   UInt64 enet_router_delay = 0;
   UInt32 num_flits_per_output_buffer_enet_router = 0;

   // Send Hub Router
   UInt64 send_hub_router_delay = 0;
   UInt32 num_flits_per_output_buffer_send_hub_router = 0;

   // Receive Hub Router
   UInt64 receive_hub_router_delay = 0;
   UInt32 num_flits_per_output_buffer_receive_hub_router = 0;

   // Star Net Router
   UInt64 star_net_router_delay = 0;
   UInt32 num_flits_per_output_buffer_star_net_router = 0;

   // Electrical Link Type
   string electrical_link_type;

   // Contention Model
   bool contention_model_enabled = false;
   string contention_model_type;

   try
   {
      // ENet Router
      enet_router_delay = (UInt64) Sim()->getCfg()->getInt("network/atac/enet/router/delay");
      num_flits_per_output_buffer_enet_router = Sim()->getCfg()->getInt("network/atac/enet/router/num_flits_per_port_buffer");

      // Send Hub Router
      send_hub_router_delay = (UInt64) Sim()->getCfg()->getInt("network/atac/onet/send_hub/router/delay");
      num_flits_per_output_buffer_send_hub_router = Sim()->getCfg()->getInt("network/atac/onet/send_hub/router/num_flits_per_port_buffer");

      // Receive Hub Router
      receive_hub_router_delay = (UInt64) Sim()->getCfg()->getInt("network/atac/onet/receive_hub/router/delay");
      num_flits_per_output_buffer_receive_hub_router = Sim()->getCfg()->getInt("network/atac/onet/receive_hub/router/num_flits_per_port_buffer");
      
      // Star Net Router
      star_net_router_delay = (UInt64) Sim()->getCfg()->getInt("network/atac/star_net/router/delay");
      num_flits_per_output_buffer_star_net_router = Sim()->getCfg()->getInt("network/atac/star_net/router/num_flits_per_port_buffer");
      
      // Electrical Link Type
      electrical_link_type = Sim()->getCfg()->getString("network/atac/electrical_link_type");
      
      contention_model_enabled = Sim()->getCfg()->getBool("network/atac/queue_model/enabled");
      contention_model_type = Sim()->getCfg()->getString("network/atac/queue_model/type");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read ANet router and link parameters from the cfg file");
   }
   
   //// Electrical Mesh Router & Link
  
   // Injection Port
   _injection_router = new NetworkRouterModel(this, 1, 1,
                                              4, 0, _flit_width,
                                              contention_model_enabled, contention_model_type);

   // ENet Router
   _num_enet_router_ports = 5;
   
   SInt32 num_enet_router_output_ports = _num_enet_router_ports;
   if (isAccessPoint(_tile_id))
      num_enet_router_output_ports += 1;

   _enet_router = new NetworkRouterModel(this, _num_enet_router_ports, num_enet_router_output_ports,
                                          num_flits_per_output_buffer_enet_router, enet_router_delay, _flit_width,
                                          contention_model_enabled, contention_model_type);
   
   // ENet Link
   volatile double enet_link_length = _tile_width;
   _enet_link_list.resize(num_enet_router_output_ports);
   for (SInt32 i = 0; i < num_enet_router_output_ports; i++)
   {
      _enet_link_list[i] = ElectricalNetworkLinkModel::create(electrical_link_type, this,
                                                              _frequency, enet_link_length, _flit_width);
      assert(_enet_link_list[i]->getDelay() == 1);
   }

   // All on Tiles with Hubs only
   if (_tile_id == getTileIDWithOpticalHub(getClusterID(_tile_id)))
   {
      // Send Hub Router
      // Performance Model
      if (_num_access_points_per_cluster > 1)
      {
         _send_hub_router = new NetworkRouterModel(this, _num_access_points_per_cluster, 1 /* num_output_ports */,
                                                   num_flits_per_output_buffer_send_hub_router, send_hub_router_delay, _flit_width,
                                                   contention_model_enabled, contention_model_type);
      }

      // Optical Network Link Models
      volatile double waveguide_length = computeOpticalLinkLength();   // In mm
      _optical_link = new OpticalNetworkLinkModel(this, _num_clusters,
                                                  _frequency, waveguide_length, _flit_width);
      assert(_optical_link->getDelay() == 2);

      // Receive Hub Router Models
      _receive_hub_router = new NetworkRouterModel(this, _num_clusters, _num_receive_networks_per_cluster,
                                                   num_flits_per_output_buffer_receive_hub_router, receive_hub_router_delay, _flit_width,
                                                   contention_model_enabled, contention_model_type);

         // Receive Net
      if (_receive_net_type == HTREE) // HTree BNet
      {
         // Just the Htree link
         volatile double htree_link_length = _tile_width * _cluster_size;
         
         _htree_link_list.resize(_num_receive_networks_per_cluster);
         for (SInt32 i = 0; i < _num_receive_networks_per_cluster; i++)
         {
            _htree_link_list[i] = ElectricalNetworkLinkModel::create(electrical_link_type, this,
                                                                     _frequency, htree_link_length, _flit_width);
            assert(_htree_link_list[i]->getDelay() == 1);
         }
      }
      else // (_receive_net_type == STAR)
      {
         // Star Net Router
         _star_net_router_list.resize(_num_receive_networks_per_cluster);
         _star_net_link_list.resize(_num_receive_networks_per_cluster);
         for (SInt32 i = 0; i < _num_receive_networks_per_cluster; i++)
         {
            // Star Net Router
            _star_net_router_list[i] = new NetworkRouterModel(this, 1 /* num_input_ports */, _cluster_size,
                                                              num_flits_per_output_buffer_star_net_router, star_net_router_delay, _flit_width,
                                                              contention_model_enabled, contention_model_type);

            // Star Net Link
            vector<tile_id_t> tile_id_list;
            getTileIDListInCluster(getClusterID(_tile_id), tile_id_list);
            assert(_cluster_size == (SInt32) tile_id_list.size());

            _star_net_link_list[i].resize(_cluster_size);
            for (SInt32 j = 0; j < _cluster_size; j++)
            {
               volatile double star_net_link_length = computeNumHopsOnENet(_tile_id, tile_id_list[j]) * _tile_width;
               if (star_net_link_length == 0)
                  star_net_link_length = 0.1;   // A small quantity
               _star_net_link_list[i][j] = ElectricalNetworkLinkModel::create(electrical_link_type, this,
                                                                              _frequency, star_net_link_length, _flit_width);
               assert(_star_net_link_list[i][j]->getDelay() == 1);
            }
         }
      }
   
   } // tile_id with optical hub
}

void
NetworkModelAtac::destroyANetRouterAndLinkModels()
{
   if (isSystemTile(_tile_id))
      return;

   // Injection Port Router
   delete _injection_router;

   // ENet Router and Link Models
   delete _enet_router;
   for (SInt32 i = 0; i < _num_enet_router_ports; i++)
      delete _enet_link_list[i];
   if (isAccessPoint(_tile_id))
      delete _enet_link_list[_num_enet_router_ports];

   if (_tile_id == getTileIDWithOpticalHub(getClusterID(_tile_id)))
   {
      // Send Hub Router
      if (_num_access_points_per_cluster > 1)
         delete _send_hub_router;
      
      // Optical Link
      delete _optical_link;

      // Receive Hub Router
      delete _receive_hub_router;

      // Receive Net
      if (_receive_net_type == HTREE)
      {
         for (SInt32 i = 0; i < _num_receive_networks_per_cluster; i++)
            delete _htree_link_list[i];
      }
      else // (_receive_net_type == STAR)
      {
         for (SInt32 i = 0; i < _num_receive_networks_per_cluster; i++)
         {
            // Star Net Router
            delete _star_net_router_list[i];
            // Star Net Links
            for (SInt32 j = 0; j < _cluster_size; j++)
               delete _star_net_link_list[i][j];
         }
      }
   }
}

void
NetworkModelAtac::routePacket(const NetPacket& pkt, queue<Hop>& next_hops)
{
   tile_id_t pkt_sender = TILE_ID(pkt.sender);
   tile_id_t pkt_receiver = TILE_ID(pkt.receiver);

   if (pkt.node_type == SEND_TILE)
   {
      assert(pkt_sender == _tile_id);

      UInt64 zero_load_delay = 0;
      UInt64 contention_delay = 0;
      _injection_router->processPacket(pkt, 0, zero_load_delay, contention_delay);
      
      Hop hop(pkt, _tile_id, EMESH, zero_load_delay, contention_delay);
      next_hops.push(hop);
   }

   else // (pkt.node_type != SEND_TILE) (In one of the intermediate routers)
   {
      GlobalRoute global_route = computeGlobalRoute(pkt_sender, pkt_receiver);
      if (global_route == GLOBAL_ENET)
      {
         LOG_PRINT("Global Route: ENET");
         routePacketOnENet(pkt, pkt_sender, pkt_receiver, next_hops);
      }
      else if (global_route == GLOBAL_ONET)
      {
         LOG_PRINT("Global Route: ONET");
         routePacketOnONet(pkt, pkt_sender, pkt_receiver, next_hops);
      }
   }
}

void
NetworkModelAtac::routePacketOnENet(const NetPacket& pkt, tile_id_t pkt_sender, tile_id_t pkt_receiver, queue<Hop>& next_hops)
{
   LOG_ASSERT_ERROR(pkt_receiver != NetPacket::BROADCAST, "Cannot broadcast packets on ENet");

   SInt32 cx, cy, dx, dy;
   computePositionOnENet(_tile_id, cx, cy);
   computePositionOnENet(pkt_receiver, dx, dy); 

   NextDest next_dest;

   if (cx > dx)
      next_dest = NextDest(computeTileIDOnENet(cx-1,cy), LEFT, EMESH);
   else if (cx < dx)
      next_dest = NextDest(computeTileIDOnENet(cx+1,cy), RIGHT, EMESH);
   else if (cy > dy)
      next_dest = NextDest(computeTileIDOnENet(cx,cy-1), DOWN, EMESH);
   else if (cy < dy)
      next_dest = NextDest(computeTileIDOnENet(cx,cy+1), UP, EMESH);
   else // (cx == dx) && (cy == dy)
      next_dest = NextDest(_tile_id, SELF, RECEIVE_TILE);

   UInt64 zero_load_delay = 0;
   UInt64 contention_delay = 0;

   // Go through router and link
   assert(0 <= next_dest._output_port && next_dest._output_port < (SInt32) _enet_link_list.size());

   _enet_router->processPacket(pkt, next_dest._output_port, zero_load_delay, contention_delay);
   _enet_link_list[next_dest._output_port]->processPacket(pkt, zero_load_delay);

   Hop hop(pkt, next_dest._tile_id, next_dest._node_type, zero_load_delay, contention_delay);
   next_hops.push(hop);
}

void
NetworkModelAtac::routePacketOnONet(const NetPacket& pkt, tile_id_t pkt_sender, tile_id_t pkt_receiver, queue<Hop>& next_hops)
{
   if (pkt.node_type == EMESH)
   {
      assert(getClusterID(pkt_sender) == getClusterID(_tile_id));
      if (isAccessPoint(_tile_id))
      {
         UInt64 zero_load_delay = 0;
         UInt64 contention_delay = 0;
            
         _enet_router->processPacket(pkt, _num_enet_router_ports, zero_load_delay, contention_delay);
         _enet_link_list[_num_enet_router_ports]->processPacket(pkt, zero_load_delay);

         Hop hop(pkt, getTileIDWithOpticalHub(getClusterID(_tile_id)), SEND_HUB, zero_load_delay, contention_delay);
         next_hops.push(hop);
      }
      else // (!isAccessPoint(_tile_id))
      {
         tile_id_t access_point = getNearestAccessPoint(pkt_sender);
         routePacketOnENet(pkt, pkt_sender, access_point, next_hops);
      }
   }

   else if (pkt.node_type == SEND_HUB)
   {
      UInt64 zero_load_delay = 0;
      UInt64 contention_delay = 0;

      if (_num_access_points_per_cluster > 1)
         _send_hub_router->processPacket(pkt, 0, zero_load_delay, contention_delay);

      if (pkt_receiver == NetPacket::BROADCAST)
         _optical_link->processPacket(pkt, OpticalNetworkLinkModel::ENDPOINT_ALL, zero_load_delay);
      else // (pkt_receiver != NetPacket::BROADCAST)
         _optical_link->processPacket(pkt, 1 /* Send to only 1 endpoint */, zero_load_delay);

      if (pkt_receiver == NetPacket::BROADCAST)
      {
         for (SInt32 i = 0; i < _num_clusters; i++)
         {
            Hop hop(pkt, getTileIDWithOpticalHub(i), RECEIVE_HUB, zero_load_delay, contention_delay);
            next_hops.push(hop);
         }
      }
      else // (receiver != NetPacket::BROADCAST)
      {
         Hop hop(pkt, getTileIDWithOpticalHub(getClusterID(pkt_receiver)), RECEIVE_HUB, zero_load_delay, contention_delay);
         next_hops.push(hop);
      }
   }

   else if (pkt.node_type == RECEIVE_HUB)
   {
      vector<tile_id_t> tile_id_list;
      getTileIDListInCluster(getClusterID(_tile_id), tile_id_list);
      assert(_cluster_size == (SInt32) tile_id_list.size());

      // get receive net id
      SInt32 receive_net_id = computeReceiveNetID(pkt_sender);

      UInt64 zero_load_delay = 0;
      UInt64 contention_delay = 0;

      // Receive Hub Router
      // Update router event counters, get delay, update dynamic energy
      _receive_hub_router->processPacket(pkt, receive_net_id, zero_load_delay, contention_delay);

      if (_receive_net_type == HTREE)
      {
         // Update link counters, get delay, update dynamic energy
         _htree_link_list[receive_net_id]->processPacket(pkt, zero_load_delay);
      }
      else // (_receive_net_type == STAR)
      {
         if (pkt_receiver == NetPacket::BROADCAST)
         {
            _star_net_router_list[receive_net_id]->processPacket(pkt, NetworkRouterModel::OUTPUT_PORT_ALL, zero_load_delay, contention_delay);
            // For links, compute max_delay
            UInt64 max_link_delay = 0;
            for (SInt32 i = 0; i < _cluster_size; i++)
            {
               UInt64 link_delay = 0;
               _star_net_link_list[receive_net_id][i]->processPacket(pkt, link_delay);
               max_link_delay = max<UInt64>(max_link_delay, link_delay);
            }
            zero_load_delay += max_link_delay;
         }
         else // (pkt_receiver != NetPacket::BROADCAST)
         {
            SInt32 idx = getIndexInList(pkt_receiver, tile_id_list);
            assert(idx >= 0 && idx < (SInt32) _cluster_size);

            _star_net_router_list[receive_net_id]->processPacket(pkt, idx, zero_load_delay, contention_delay);
            _star_net_link_list[receive_net_id][idx]->processPacket(pkt, zero_load_delay);
         }
      }

      if (pkt_receiver == NetPacket::BROADCAST)
      {
         for (vector<tile_id_t>::iterator it = tile_id_list.begin(); it != tile_id_list.end(); it++)
         {
            Hop hop(pkt, *it, RECEIVE_TILE, zero_load_delay, contention_delay);
            next_hops.push(hop);
         }
      }
      else // (pkt_receiver != NetPacket::BROADCAST)
      {
         Hop hop(pkt, pkt_receiver, RECEIVE_TILE, zero_load_delay, contention_delay);
         next_hops.push(hop);
      }
   }

   else
   {
      LOG_PRINT_ERROR("Unrecognized Node Type(%i)", pkt.node_type);
   }
}

void
NetworkModelAtac::outputSummary(ostream &out)
{
   NetworkModel::outputSummary(out);
   outputEventCountSummary(out);
}

volatile double
NetworkModelAtac::computeOpticalLinkLength()
{
   // Note that number of clusters must be 'positive' and 'power of 2'
   // 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024

   if (_num_clusters == 2)
   {
      // Assume that optical link connects the mid-point of the clusters
      return (_cluster_height * _tile_width);
   }
   else if (_num_clusters == 4)
   {
      // Assume that optical link passes through mid-point of the clusters
      return (_cluster_width * _tile_width) * (_cluster_height * _tile_width);
   }
   else if (_num_clusters == 8)
   {
      return (_cluster_width * _tile_width) * (2 * _cluster_height * _tile_width);
   }
   else
   {
      // Assume that optical link passes through the edges of the clusters
      double length_rectangle = (_numX_clusters-2) * _cluster_width * _tile_width;
      double height_rectangle = (_cluster_height*2) * _tile_width;
      SInt32 num_rectangles = _numY_clusters/4;
      return (num_rectangles * 2 * (length_rectangle + height_rectangle));
   }
}
void
NetworkModelAtac::initializeClusters()
{
   // Cluster -> Boundary and Access Point List
   _cluster_info_list.resize(_num_clusters);

   // Clusters
   // _numX_clusters, _numY_clusters, _cluster_width, _cluster_height
   if (isEven(floorLog2(_num_clusters)))
   {
      _numX_clusters = sqrt(_num_clusters);
      _numY_clusters = sqrt(_num_clusters);
   }
   else // (isOdd(floorLog2(_num_clusters)))
   {
      _numX_clusters = sqrt(_num_clusters/2);
      _numY_clusters = sqrt(_num_clusters*2);
   }
   _cluster_width = _enet_width / _numX_clusters;
   _cluster_height = _enet_height / _numY_clusters;

   // Initialize Cluster Boundaries
   for (SInt32 y = 0; y < _numY_clusters; y++)
   {
      for (SInt32 x = 0; x < _numX_clusters; x++)
      {
         SInt32 cluster_id = (y * _numX_clusters) + x;
         ClusterInfo::Boundary boundary(x * _cluster_width, (x+1) * _cluster_width,
                                        y * _cluster_height, (y+1) * _cluster_height);
         _cluster_info_list[cluster_id]._boundary = boundary;
      }
   }

   // Sub Clusters
   // _numX_sub_clusters, _numY_sub_clusters, _sub_cluster_width, _sub_cluster_height
   if (isEven(floorLog2(_num_sub_clusters)))
   {
      _numX_sub_clusters = sqrt(_num_sub_clusters);
      _numY_sub_clusters = sqrt(_num_sub_clusters);
   }
   else // (isOdd(floorLog2(_num_sub_clusters)))
   {
      _numX_sub_clusters = sqrt(_num_sub_clusters*2);
      _numY_sub_clusters = sqrt(_num_sub_clusters/2);
   }
   _sub_cluster_width = _cluster_width / _numX_sub_clusters;
   _sub_cluster_height = _cluster_height / _numY_sub_clusters;

   // Initialize Access Point List
   for (SInt32 i = 0; i < _num_clusters; i++)
   {
      initializeAccessPointList(i);
   }
}

void
NetworkModelAtac::initializeAccessPointList(SInt32 cluster_id)
{
   ClusterInfo::Boundary& boundary = _cluster_info_list[cluster_id]._boundary;
   // Access Points
   // _access_point_list
   for (SInt32 y = 0; y < _numY_sub_clusters; y++)
   {
      for (SInt32 x = 0; x < _numX_sub_clusters; x++)
      {
         SInt32 access_point_x = boundary.minX + (x * _sub_cluster_width) + (_sub_cluster_width/2);
         SInt32 access_point_y = boundary.minY + (y * _sub_cluster_height) + (_sub_cluster_height/2);
         tile_id_t access_point_id = access_point_y * _enet_width + access_point_x;
         _cluster_info_list[cluster_id]._access_point_list.push_back(access_point_id);
      }
   }
}

SInt32
NetworkModelAtac::getClusterID(tile_id_t tile_id)
{
   // Consider a mesh formed by the clusters
   SInt32 cluster_mesh_width;
   cluster_mesh_width = _enet_width / _cluster_width;

   SInt32 tile_x, tile_y;
   computePositionOnENet(tile_id, tile_x, tile_y);

   SInt32 cluster_pos_x, cluster_pos_y;
   cluster_pos_x = tile_x / _cluster_width;
   cluster_pos_y = tile_y / _cluster_height;

   return (cluster_pos_y * cluster_mesh_width + cluster_pos_x);
}

SInt32
NetworkModelAtac::getSubClusterID(tile_id_t tile_id)
{
   SInt32 cx, cy;
   computePositionOnENet(tile_id, cx, cy);

   SInt32 cluster_id = getClusterID(tile_id);
   // Get the cluster boundary
   ClusterInfo::Boundary& boundary = _cluster_info_list[cluster_id]._boundary;
   SInt32 pos_x = (cx - boundary.minX) / _sub_cluster_width;
   SInt32 pos_y = (cy - boundary.minY) / _sub_cluster_height;
   return (pos_y * _numX_sub_clusters) + pos_x;
}

tile_id_t
NetworkModelAtac::getNearestAccessPoint(tile_id_t tile_id)
{
   SInt32 cluster_id = getClusterID(tile_id);
   SInt32 sub_cluster_id = getSubClusterID(tile_id);
   return _cluster_info_list[cluster_id]._access_point_list[sub_cluster_id];
}

bool
NetworkModelAtac::isAccessPoint(tile_id_t tile_id)
{
   return (tile_id == getNearestAccessPoint(tile_id));
}

tile_id_t
NetworkModelAtac::getTileIDWithOpticalHub(SInt32 cluster_id)
{
   // Consider a mesh formed by the clusters
   SInt32 cluster_mesh_width;
   cluster_mesh_width = _enet_width / _cluster_width;

   SInt32 cluster_pos_x, cluster_pos_y;
   cluster_pos_x = (cluster_id % cluster_mesh_width);
   cluster_pos_y = (cluster_id / cluster_mesh_width);

   SInt32 optical_hub_x, optical_hub_y;
   optical_hub_x = (cluster_pos_x * _cluster_width) + (_cluster_width/2);
   optical_hub_y = (cluster_pos_y * _cluster_height) + (_cluster_height/2);
   
   return (optical_hub_y * _enet_width + optical_hub_x);
}

void
NetworkModelAtac::getTileIDListInCluster(SInt32 cluster_id, vector<tile_id_t>& tile_id_list)
{
   SInt32 cluster_mesh_width;
   cluster_mesh_width = _enet_width / _cluster_width;

   SInt32 cluster_pos_x, cluster_pos_y;
   cluster_pos_x = cluster_id % cluster_mesh_width;
   cluster_pos_y = cluster_id / cluster_mesh_width;

   SInt32 core_x, core_y; 
   core_x = cluster_pos_x * _cluster_width;
   core_y = cluster_pos_y * _cluster_height;

   for (SInt32 i = core_x; i < core_x + _cluster_width; i++)
   {
      for (SInt32 j = core_y; j < core_y + _cluster_height; j++)
      {
         SInt32 tile_id = j * _enet_width + i;
         assert (tile_id < (SInt32) Config::getSingleton()->getApplicationTiles());
         tile_id_list.push_back(tile_id);
      }
   }
}

SInt32
NetworkModelAtac::getIndexInList(tile_id_t tile_id, vector<tile_id_t>& tile_id_list)
{
   SInt32 idx = 0;
   for (vector<tile_id_t>::iterator it = tile_id_list.begin(); it != tile_id_list.end(); it++, idx++)
   {
      if (tile_id == *it)
         return idx;
   }
   LOG_PRINT_ERROR("Could not find (%i)", tile_id);
   return -1;
}

SInt32
NetworkModelAtac::computeNumHopsOnENet(tile_id_t sender, tile_id_t receiver)
{
   SInt32 sx, sy, dx, dy;
   computePositionOnENet(sender, sx, sy);
   computePositionOnENet(receiver, dx, dy);
   return (abs(sx-dx) + abs(sy-dy));
}

void
NetworkModelAtac::computePositionOnENet(tile_id_t tile_id, SInt32& x, SInt32& y)
{
   x = tile_id % _enet_width;
   y = tile_id / _enet_width;
}

tile_id_t
NetworkModelAtac::computeTileIDOnENet(SInt32 x, SInt32 y)
{
   if ((x < 0) || (y < 0) || (x >= _enet_width) || (y >= _enet_height))
      return INVALID_TILE_ID;
   else
      return (y * _enet_width + x);
}

NetworkModelAtac::GlobalRoutingStrategy
NetworkModelAtac::parseGlobalRoutingStrategy(string strategy)
{
   if (strategy == "cluster_based")
      return CLUSTER_BASED;
   else if (strategy == "distance_based")
      return DISTANCE_BASED;
   else
      LOG_PRINT_ERROR("Unrecognized Global Routing Strategy (%s)", strategy.c_str());
   return (GlobalRoutingStrategy) -1;
}

NetworkModelAtac::GlobalRoute
NetworkModelAtac::computeGlobalRoute(tile_id_t sender, tile_id_t receiver)
{
   if (receiver == NetPacket::BROADCAST)
      return GLOBAL_ONET;

   if (getClusterID(sender) == getClusterID(receiver))
   {
      return GLOBAL_ENET;
   }
   else // (getClusterID(sender) != getClusterID(receiver))
   {
      if (_global_routing_strategy == CLUSTER_BASED)
      {
         return GLOBAL_ONET;
      }
      else // (_global_routing_strategy == DISTANCE_BASED)
      {
         SInt32 num_hops_on_enet = computeNumHopsOnENet(sender, receiver);
         return (num_hops_on_enet <= _unicast_distance_threshold) ? GLOBAL_ENET : GLOBAL_ONET;
      }
   }
}

NetworkModelAtac::ReceiveNetType
NetworkModelAtac::parseReceiveNetType(string str)
{
   if (str == "htree")
      return HTREE;
   else if (str == "star")
      return STAR;
   else
   {
      LOG_PRINT_ERROR("Unrecognized Receive Net Type(%s)", str.c_str());
      return (ReceiveNetType) -1;
   }
}

SInt32
NetworkModelAtac::computeReceiveNetID(tile_id_t sender)
{
   // This can be made random also if needed
   SInt32 sending_cluster_id = getClusterID(sender);
   return (sending_cluster_id % _num_receive_networks_per_cluster);
}

bool
NetworkModelAtac::isTileCountPermissible(SInt32 tile_count)
{
   // Same Calculations as Electrical Mesh Model
   SInt32 enet_width = (SInt32) floor (sqrt(tile_count));
   SInt32 enet_height = (SInt32) ceil (1.0 * tile_count / enet_width);

   if (tile_count != (enet_width * enet_height))
   {
      fprintf(stderr, "Can't form a mesh with tile count(%i)", tile_count);
      return false;
   }
   return true;
}

pair<bool, vector<tile_id_t> >
NetworkModelAtac::computeMemoryControllerPositions(SInt32 num_memory_controllers, SInt32 tile_count)
{
   // Initialize the topology parameters in case called by an external model
   // Initialize _total_tiles, _cluster_size, _sqrt_cluster_size, _mesh_width, _mesh_height, _num_clusters
   initializeANetTopologyParams();

   // Initialization should be done by now
   LOG_ASSERT_ERROR(num_memory_controllers <= (SInt32) _num_clusters,
         "num_memory_controllers(%i), num_clusters(%u)", num_memory_controllers, _num_clusters);

   vector<tile_id_t> tile_id_list_with_memory_controllers;
   for (SInt32 i = 0; i < num_memory_controllers; i++)
   {
      tile_id_list_with_memory_controllers.push_back(getTileIDWithOpticalHub(i));
   }

   return (make_pair(true, tile_id_list_with_memory_controllers));
}

pair<bool, vector<Config::TileList> >
NetworkModelAtac::computeProcessToTileMapping()
{
   // Initialize _total_tiles, _cluster_size, _sqrt_cluster_size, _mesh_width, _mesh_height, _num_clusters
   initializeANetTopologyParams();

   SInt32 process_count = (SInt32) Config::getSingleton()->getProcessCount();
   vector<Config::TileList> process_to_tile_mapping(process_count);
  
   LOG_ASSERT_WARNING(_num_clusters >= process_count,
        "Number of Clusters(%u) < Total Process in Simulation(%u)",
        _num_clusters, process_count);
        
   UInt32 process_num = 0;
   for (SInt32 i = 0; i < _num_clusters; i++)
   {
      Config::TileList tile_id_list;
      Config::TileList::iterator tile_it;
      getTileIDListInCluster(i, tile_id_list);
      for (tile_it = tile_id_list.begin(); tile_it != tile_id_list.end(); tile_it ++)
      {
         process_to_tile_mapping[process_num].push_back(*tile_it);
      }
      process_num = (process_num + 1) % process_count;
   }

   return (make_pair(true, process_to_tile_mapping));
}

void
NetworkModelAtac::outputEventCountSummary(ostream& out)
{
   out << "   Event Counters:" << endl;

   if (isApplicationTile(_tile_id))
   {
      // ENet Router
      out << "    ENet Router Buffer Writes: " << _enet_router->getTotalBufferWrites() << endl;
      out << "    ENet Router Buffer Reads: " << _enet_router->getTotalBufferReads() << endl;
      out << "    ENet Router Switch Allocator Requests: " << _enet_router->getTotalSwitchAllocatorRequests() << endl;
      out << "    ENet Router Crossbar Traversals: " << _enet_router->getTotalCrossbarTraversals(1) << endl;
      
      // ENet Link
      UInt64 total_enet_link_traversals = 0;
      for (SInt32 i = 0; i < _num_enet_router_ports; i++)
         total_enet_link_traversals += _enet_link_list[i]->getTotalTraversals();
      out << "    ENet Router To ENet Router Link Traversals: " << total_enet_link_traversals << endl;
      
      // Access Point
      if (isAccessPoint(_tile_id))
         out << "    ENet Router To Send Hub Router Link Traversals: " << _enet_link_list[_num_enet_router_ports]->getTotalTraversals() << endl;
      else
         out << "    ENet Router To Send Hub Router Link Traversals: NA" << endl;

      // Send Hub Router
      if (_num_access_points_per_cluster > 1)
      {
         if (_tile_id == getTileIDWithOpticalHub(getClusterID(_tile_id)))
         {
            out << "    Send Hub Router Buffer Writes: " << _send_hub_router->getTotalBufferWrites() << endl;
            out << "    Send Hub Router Buffer Reads: " << _send_hub_router->getTotalBufferReads() << endl;
            out << "    Send Hub Router Switch Allocator Requests: " << _send_hub_router->getTotalSwitchAllocatorRequests() << endl;
            out << "    Send Hub Router Crossbar Traversals: " << _send_hub_router->getTotalCrossbarTraversals(1) << endl;
         }
         else
         {
            out << "    Send Hub Router Buffer Writes: NA" << endl;
            out << "    Send Hub Router Buffer Reads: NA" << endl;
            out << "    Send Hub Router Switch Allocator Requests: NA" << endl;
            out << "    Send Hub Router Crossbar Traversals: NA" << endl;
         }
      }

      // Optical Link
      if (_tile_id == getTileIDWithOpticalHub(getClusterID(_tile_id)))
      {
         out << "    Optical Link Unicasts: " << _optical_link->getTotalUnicasts() << endl;
         out << "    Optical Link Broadcasts: " << _optical_link->getTotalBroadcasts() << endl;
      }
      else
      {
         out << "    Optical Link Unicasts: NA" << endl;
         out << "    Optical Link Broadcasts: NA" << endl;
      }

      // Receive Hub Router
      if (_tile_id == getTileIDWithOpticalHub(getClusterID(_tile_id)))
      {
         out << "    Receive Hub Router Buffer Writes: " << _receive_hub_router->getTotalBufferWrites() << endl;
         out << "    Receive Hub Router Buffer Reads: " << _receive_hub_router->getTotalBufferReads() << endl;
         out << "    Receive Hub Router Switch Allocator Requests: " << _receive_hub_router->getTotalSwitchAllocatorRequests() << endl;
         out << "    Receive Hub Router Crossbar Traversals: " << _receive_hub_router->getTotalCrossbarTraversals(1) << endl;
      }
      else
      {
         out << "    Receive Hub Router Buffer Writes: NA" << endl;
         out << "    Receive Hub Router Buffer Reads: NA" << endl;
         out << "    Receive Hub Router Switch Allocator Requests: NA" << endl;
         out << "    Receive Hub Router Crossbar Traversals: NA" << endl;
      }
     
      // Receive Net
      if (_tile_id == getTileIDWithOpticalHub(getClusterID(_tile_id)))
      {
         if (_receive_net_type == HTREE)
         {
            UInt64 total_htree_link_traversals = 0;
            for (SInt32 i = 0; i < _num_receive_networks_per_cluster; i++)
               total_htree_link_traversals += _htree_link_list[i]->getTotalTraversals();
            out << "    HTree Link Traversals: " << total_htree_link_traversals << endl;
         }
         else // (_receive_net_type == STAR)
         {
            UInt64 total_star_net_router_buffer_writes = 0;
            UInt64 total_star_net_router_buffer_reads = 0;
            UInt64 total_star_net_router_switch_allocator_requests = 0;
            UInt64 total_star_net_router_crossbar_unicasts = 0;
            UInt64 total_star_net_router_crossbar_broadcasts = 0;
            vector<UInt64> total_star_net_link_traversals(_cluster_size, 0);
            
            for (SInt32 i = 0; i < _num_receive_networks_per_cluster; i++)
            {
               total_star_net_router_buffer_writes += _star_net_router_list[i]->getTotalBufferWrites();
               total_star_net_router_buffer_reads += _star_net_router_list[i]->getTotalBufferReads();
               total_star_net_router_switch_allocator_requests += _star_net_router_list[i]->getTotalSwitchAllocatorRequests();
               total_star_net_router_crossbar_unicasts += _star_net_router_list[i]->getTotalCrossbarTraversals(1);
               total_star_net_router_crossbar_broadcasts += _star_net_router_list[i]->getTotalCrossbarTraversals(_cluster_size);
               for (SInt32 j = 0; j < _cluster_size; j++)
                  total_star_net_link_traversals[j] += _star_net_link_list[i][j]->getTotalTraversals();
            }

            out << "    Star Net Router Buffer Writes: " << total_star_net_router_buffer_writes << endl;
            out << "    Star Net Router Buffer Reads: " << total_star_net_router_buffer_reads << endl;
            out << "    Star Net Router Switch Allocator Requests: " << total_star_net_router_switch_allocator_requests << endl;
            out << "    Star Net Router Crossbar Unicasts: " << total_star_net_router_crossbar_unicasts << endl;
            out << "    Star Net Router Crossbar Broadcasts: " << total_star_net_router_crossbar_broadcasts << endl;
            for (SInt32 j = 0; j < _cluster_size; j++)
               out << "    Star Net Link Traversals[" << j << "]: " << total_star_net_link_traversals[j] << endl;
         }
      }
      else
      {
         if (_receive_net_type == HTREE)
         {
            out << "    HTree Link Traversals: NA" << endl;
         }
         else // (_receive_net_type == STAR)
         {
            out << "    Star Net Router Buffer Writes: NA" << endl;
            out << "    Star Net Router Buffer Reads: NA" << endl;
            out << "    Star Net Router Switch Allocator Requests: NA" << endl;
            out << "    Star Net Router Crossbar Unicasts: NA" << endl;
            out << "    Star Net Router Crossbar Broadcasts: NA" << endl;
            for (SInt32 j = 0; j < _cluster_size; j++)
               out << "    Star Net Link Traversals[" << j << "]: NA" << endl;
         }
      }
   }

   else if (isSystemTile(_tile_id))
   {
      // ENet Router
      out << "    ENet Router Buffer Writes: NA" << endl;
      out << "    ENet Router Buffer Reads: NA" << endl;
      out << "    ENet Router Switch Allocator Requests: NA" << endl;
      out << "    ENet Router Crossbar Traversals: NA" << endl;
      
      // ENet Link
      out << "    ENet Router To ENet Router Link Traversals: NA" << endl;
      
      // Access Point
      out << "    ENet Router To Send Hub Router Link Traversals: NA" << endl;

      // Send Hub Router
      if (_num_access_points_per_cluster > 1)
      {
         out << "    Send Hub Router Buffer Writes: NA" << endl;
         out << "    Send Hub Router Buffer Reads: NA" << endl;
         out << "    Send Hub Router Switch Allocator Requests: NA" << endl;
         out << "    Send Hub Router Crossbar Traversals: NA" << endl;
      }

      // Optical Link
      out << "    Optical Link Unicasts: NA" << endl;
      out << "    Optical Link Broadcasts: NA" << endl;

      // Receive Hub Router
      out << "    Receive Hub Router Buffer Writes: NA" << endl;
      out << "    Receive Hub Router Buffer Reads: NA" << endl;
      out << "    Receive Hub Router Switch Allocator Requests: NA" << endl;
      out << "    Receive Hub Router Crossbar Traversals: NA" << endl;
     
      // Receive Net
      if (_receive_net_type == HTREE)
      {
         out << "    HTree Link Traversals: NA" << endl;
      }
      else // (_receive_net_type == STAR)
      {
         out << "    Star Net Router Buffer Writes: NA" << endl;
         out << "    Star Net Router Buffer Reads: NA" << endl;
         out << "    Star Net Router Switch Allocator Requests: NA" << endl;
         out << "    Star Net Router Crossbar Unicasts: NA" << endl;
         out << "    Star Net Router Crossbar Broadcasts: NA" << endl;
         for (SInt32 j = 0; j < _cluster_size; j++)
            out << "    Star Net Link Traversals[" << j << "]: NA" << endl;
      }
   }

   else
   {
      LOG_PRINT_ERROR("Unrecognized Tile ID(%i)", _tile_id);
   }
}
