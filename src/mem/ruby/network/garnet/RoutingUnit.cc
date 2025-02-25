/*
 * Copyright (c) 2008 Princeton University
 * Copyright (c) 2016 Georgia Institute of Technology
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "mem/ruby/network/garnet/RoutingUnit.hh"

#include "base/cast.hh"
#include "base/compiler.hh"
#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/garnet/InputUnit.hh"
#include "mem/ruby/network/garnet/Router.hh"
#include "mem/ruby/slicc_interface/Message.hh"

namespace gem5
{

namespace ruby
{

namespace garnet
{

bool wireless_free = true;
// wireless_cout is used for testing purposes only
int wireless_count = 0;
int transmission_count = 0;

RoutingUnit::RoutingUnit(Router *router)
{
    m_router = router;
    m_routing_table.clear();
    m_weight_table.clear();
}

void
RoutingUnit::addRoute(std::vector<NetDest>& routing_table_entry)
{
    if (routing_table_entry.size() > m_routing_table.size()) {
        m_routing_table.resize(routing_table_entry.size());
    }
    for (int v = 0; v < routing_table_entry.size(); v++) {
        m_routing_table[v].push_back(routing_table_entry[v]);
    }
}

void
RoutingUnit::addWeight(int link_weight)
{
    m_weight_table.push_back(link_weight);
}

bool
RoutingUnit::supportsVnet(int vnet, std::vector<int> sVnets)
{
    // If all vnets are supported, return true
    if (sVnets.size() == 0) {
        return true;
    }

    // Find the vnet in the vector, return true
    if (std::find(sVnets.begin(), sVnets.end(), vnet) != sVnets.end()) {
        return true;
    }

    // Not supported vnet
    return false;
}

/*
 * This is the default routing algorithm in garnet.
 * The routing table is populated during topology creation.
 * Routes can be biased via weight assignments in the topology file.
 * Correct weight assignments are critical to provide deadlock avoidance.
 */
int
RoutingUnit::lookupRoutingTable(int vnet, NetDest msg_destination)
{
    // First find all possible output link candidates
    // For ordered vnet, just choose the first
    // (to make sure different packets don't choose different routes)
    // For unordered vnet, randomly choose any of the links
    // To have a strict ordering between links, they should be given
    // different weights in the topology file

    int output_link = -1;
    int min_weight = INFINITE_;
    std::vector<int> output_link_candidates;
    int num_candidates = 0;

    // Identify the minimum weight among the candidate output links
    for (int link = 0; link < m_routing_table[vnet].size(); link++) {
        if (msg_destination.intersectionIsNotEmpty(
            m_routing_table[vnet][link])) {

        if (m_weight_table[link] <= min_weight)
            min_weight = m_weight_table[link];
        }
    }

    // Collect all candidate output links with this minimum weight
    for (int link = 0; link < m_routing_table[vnet].size(); link++) {
        if (msg_destination.intersectionIsNotEmpty(
            m_routing_table[vnet][link])) {

            if (m_weight_table[link] == min_weight) {
                num_candidates++;
                output_link_candidates.push_back(link);
            }
        }
    }

    if (output_link_candidates.size() == 0) {
        fatal("Fatal Error:: No Route exists from this Router.");
        exit(0);
    }

    // Randomly select any candidate output link
    int candidate = 0;
    if (!(m_router->get_net_ptr())->isVNetOrdered(vnet))
        candidate = rand() % num_candidates;

    output_link = output_link_candidates.at(candidate);
    return output_link;
}


void
RoutingUnit::addInDirection(PortDirection inport_dirn, int inport_idx)
{
    m_inports_dirn2idx[inport_dirn] = inport_idx;
    m_inports_idx2dirn[inport_idx]  = inport_dirn;
}

void
RoutingUnit::addOutDirection(PortDirection outport_dirn, int outport_idx)
{
    m_outports_dirn2idx[outport_dirn] = outport_idx;
    m_outports_idx2dirn[outport_idx]  = outport_dirn;
}

// outportCompute() is called by the InputUnit
// It calls the routing table by default.
// A template for adaptive topology-specific routing algorithm
// implementations using port directions rather than a static routing
// table is provided here.

int
RoutingUnit::outportCompute(RouteInfo route, int inport,
                            PortDirection inport_dirn)
{
    int outport = -1;

    if (route.dest_router == m_router->get_id()) {

        // Multiple NIs may be connected to this router,
        // all with output port direction = "Local"
        // Get exact outport id from table
        outport = lookupRoutingTable(route.vnet, route.net_dest);
        return outport;
    }

    // Routing Algorithm set in GarnetNetwork.py
    // Can be over-ridden from command line using --routing-algorithm = 1
    RoutingAlgorithm routing_algorithm =
        (RoutingAlgorithm) m_router->get_net_ptr()->getRoutingAlgorithm();

    switch (routing_algorithm) {
        case TABLE_:  outport =
            lookupRoutingTable(route.vnet, route.net_dest); break;
        case XY_:     outport =
            outportComputeXY(route, inport, inport_dirn); break;
        // any custom algorithm
        case XYZ_:     outport =
            outportComputeXYZ(route, inport, inport_dirn); break;
        case U_CHIPLETS_: outport =
            outportComputeUChiplets(route, inport, inport_dirn); break;
        case NU_CHIPLETS_: outport =
            outportComputeNUChiplets(route, inport, inport_dirn); break;
        case WIRELESS_: outport =
            outportComputeWireless(route, inport, inport_dirn); break;
        case CUSTOM_: outport =
            outportComputeCustom(route, inport, inport_dirn); break;
        default: outport =
            lookupRoutingTable(route.vnet, route.net_dest); break;
    }

    assert(outport != -1);
    return outport;
}

// XY routing implemented using port directions
// Only for reference purpose in a Mesh
// By default Garnet uses the routing table
int
RoutingUnit::outportComputeXY(RouteInfo route,
                              int inport,
                              PortDirection inport_dirn)
{
    PortDirection outport_dirn = "Unknown";

    [[maybe_unused]] int num_rows = m_router->get_net_ptr()->getNumRows();
    int num_cols = m_router->get_net_ptr()->getNumCols();
    assert(num_rows > 0 && num_cols > 0);

    int my_id = m_router->get_id();
    int my_x = my_id % num_cols;
    int my_y = my_id / num_cols;

    int dest_id = route.dest_router;
    int dest_x = dest_id % num_cols;
    int dest_y = dest_id / num_cols;

    int x_hops = abs(dest_x - my_x);
    int y_hops = abs(dest_y - my_y);

    bool x_dirn = (dest_x >= my_x);
    bool y_dirn = (dest_y >= my_y);

    // already checked that in outportCompute() function
    assert(!(x_hops == 0 && y_hops == 0));

    if (x_hops > 0) {
        if (x_dirn) {
            assert(inport_dirn == "Local" || inport_dirn == "West");
            outport_dirn = "East";
        } else {
            assert(inport_dirn == "Local" || inport_dirn == "East");
            outport_dirn = "West";
        }
    } else if (y_hops > 0) {
        if (y_dirn) {
            // "Local" or "South" or "West" or "East"
            assert(inport_dirn != "North");
            outport_dirn = "North";
        } else {
            // "Local" or "North" or "West" or "East"
            assert(inport_dirn != "South");
            outport_dirn = "South";
        }
    } else {
        // x_hops == 0 and y_hops == 0
        // this is not possible
        // already checked that in outportCompute() function
        panic("x_hops == y_hops == 0");
    }

    return m_outports_dirn2idx[outport_dirn];
}

int
RoutingUnit::outportComputeXYZ(RouteInfo route,
                              int inport,
                              PortDirection inport_dirn)
{
    PortDirection outport_dirn = "Unknown";

    int num_rows = m_router->get_net_ptr()->getNumRows();
    int num_cols = m_router->get_net_ptr()->getNumCols();
    int z_depth = m_router->get_net_ptr()->getZDepth();
    assert(num_rows > 0 && num_cols > 0 && z_depth > 0);

    int my_id = m_router->get_id();
    int my_z = (my_id/(num_rows*num_cols));
    int my_x = (my_id-(my_z*num_rows*num_cols)) % num_cols;
    int my_y = (my_id-(my_z*num_rows*num_cols)) / num_cols;

    int dest_id = route.dest_router;
    int dest_z = (dest_id/(num_rows*num_cols));
    int dest_x = (dest_id-(dest_z*num_rows*num_cols)) % num_cols;
    int dest_y = (dest_id-(dest_z*num_rows*num_cols)) / num_cols;

    int x_hops = abs(dest_x - my_x);
    int y_hops = abs(dest_y - my_y);
    int z_hops = abs(dest_z - my_z);

    bool x_dirn = (dest_x >= my_x); //true if destination is east of current
    bool y_dirn = (dest_y >= my_y); //true if destination is north of current
    bool z_dirn = (dest_z >= my_z); //true if destination is above current

    // already checked that in outportCompute() function
    assert(!(x_hops == 0 && y_hops == 0 && z_hops == 0));

    if (x_hops > 0) {
        if (x_dirn) {
            assert(inport_dirn == "Local" || inport_dirn == "West");
            outport_dirn = "East";
        } else {
            assert(inport_dirn == "Local" || inport_dirn == "East");
            outport_dirn = "West";
        }
    } else if (y_hops > 0) {
        if (y_dirn) {
            assert(inport_dirn != "North");
            outport_dirn = "North";
        } else {
            assert(inport_dirn != "South");
            outport_dirn = "South";
        }
    } else if (z_hops > 0) {
        if (z_dirn) {
            assert(inport_dirn != "Up");
            outport_dirn = "Up";
        } else {
            assert(inport_dirn != "Down");
            outport_dirn = "Down";
        }
    } else {
        // x_hops == 0 and y_hops == 0 and z_hops == 0
        // this is not possible
        // already checked that in outportCompute() function
        panic("x_hops == y_hops == z_hops == 0");
    }

    return m_outports_dirn2idx[outport_dirn];
}

int
RoutingUnit::outportComputeUChiplets(RouteInfo route,
                              int inport,
                              PortDirection inport_dirn)
{
    PortDirection outport_dirn = "Unknown";
    int num_chiplets_x = m_router->get_net_ptr()->getNumChipletsX();
    int num_chiplets_y = m_router->get_net_ptr()->getNumChipletsY();
    int num_rows = m_router->get_net_ptr()->getNumRows();
    int num_cols = m_router->get_net_ptr()->getNumCols();
    int z_depth = m_router->get_net_ptr()->getZDepth();
    assert(num_rows > 0 && num_cols > 0 && z_depth > 0);

    int my_id = m_router->get_id();
    int my_coords[3];
    //(z,y,x) = a[0],a[1],a[2]
    m_router->get_net_ptr()->getCoords(my_id,my_coords);
    int my_sector = m_router->get_net_ptr()->getSectorU(my_id,
                                                        my_coords[0],
                                                        num_chiplets_x,
                                                        num_chiplets_y);

    // std::cout<<"Current Sector: "<<my_sector<<std::endl;

    int dest_id = route.dest_router;
    int dest_coords[3];
    //(z,y,x) = a[0],a[1],a[2]
    m_router->get_net_ptr()->getCoords(dest_id,dest_coords);
    int dest_sector = m_router->get_net_ptr()->getSectorU(dest_id,
                                                          dest_coords[0],
                                                          num_chiplets_x,
                                                          num_chiplets_y);

    // std::cout<<"Destination Sector: "<<dest_sector<<std::endl;

    int x_hops = abs(dest_coords[2] - my_coords[2]);
    int y_hops = abs(dest_coords[1] - my_coords[1]);
    int z_hops = abs(dest_coords[0] - my_coords[0]);

    // dest is east of current
    bool x_dirn = (dest_coords[2] >= my_coords[2]);
    // dest is north of current
    bool y_dirn = (dest_coords[1] >= my_coords[1]);
    // dest is above current
    bool z_dirn = (dest_coords[0] >= my_coords[0]);
    // dest and current router are in the same sectorrant
    bool same_sector = (my_sector == dest_sector);

    // Sector Numbering:
    // _______________
    // |      |      |
    // |  2   |   3  |
    // |______|______|
    // |      |      |
    // |  0   |   1  |
    // |______|______|

    // already checked that in outportCompute() function
    assert(!(x_hops == 0 && y_hops == 0 && z_hops == 0));

    // cur node and dest router are in the same sectorrant, move normally
    if (same_sector){
        if (x_hops > 0) {
            if (x_dirn) {
                // assert(inport_dirn == "Local" || inport_dirn == "West");
                outport_dirn = "East";
            } else {
                // assert(inport_dirn == "Local" || inport_dirn == "East");
                outport_dirn = "West";
            }
        } else if (y_hops > 0) {
            if (y_dirn) {
                // assert(inport_dirn != "North");
                outport_dirn = "North";
            } else {
                // assert(inport_dirn != "South");
                outport_dirn = "South";
            }
        } else if (z_hops > 0) {
            if (z_dirn) {
                // assert(inport_dirn != "Up");
                outport_dirn = "Up";
            } else {
                // assert(inport_dirn != "Down");
                outport_dirn = "Down";
            }
        } else {
            // x_hops == 0 and y_hops == 0 and z_hops == 0
            // this is not possible
            // already checked that in outportCompute() function
            panic("x_hops == y_hops == z_hops == 0");
        }
    } else {
        // router in layer 0, move in xy direction to same sector as router
        if (my_coords[0] == 0) {
            if (x_hops > 0) {
                if (x_dirn) {
                    outport_dirn = "East";
                } else {
                    outport_dirn = "West";
                }
            } else if (y_hops > 0) {
                if (y_dirn) {
                    assert(inport_dirn != "North");
                    outport_dirn = "North";
                } else {
                    assert(inport_dirn != "South");
                    outport_dirn = "South";
                }
            }
        } else { // if router not in layer 0, move to layer 0
            // assert(inport_dirn != "Down");
            outport_dirn = "Down";
        }
    }

    return m_outports_dirn2idx[outport_dirn];
}

int
RoutingUnit::outportComputeNUChiplets(RouteInfo route,
                              int inport,
                              PortDirection inport_dirn)
{
    PortDirection outport_dirn = "Unknown";
    int num_rows = m_router->get_net_ptr()->getNumRows();
    int num_cols = m_router->get_net_ptr()->getNumCols();
    int z_depth = m_router->get_net_ptr()->getZDepth();
    assert(num_rows > 0 && num_cols > 0 && z_depth > 0);

    int my_id = m_router->get_id();
    int my_coords[3];
    //(z,y,x) = a[0],a[1],a[2]
    m_router->get_net_ptr()->getCoords(my_id,my_coords);

    int my_sector = m_router->get_net_ptr()->getSectorNU(my_id, my_coords[0]);

    int dest_id = route.dest_router;
    int dest_coords[3];
    //(z,y,x) = a[0],a[1],a[2]
    m_router->get_net_ptr()->getCoords(dest_id,dest_coords);

    int dest_sector = m_router->get_net_ptr()->getSectorNU(dest_id,
                                                           dest_coords[0]);

    int x_hops = abs(dest_coords[2] - my_coords[2]);
    int y_hops = abs(dest_coords[1] - my_coords[1]);
    int z_hops = abs(dest_coords[0] - my_coords[0]);

    // dest is east of current
    bool x_dirn = (dest_coords[2] >= my_coords[2]);
    // dest is north of current
    bool y_dirn = (dest_coords[1] >= my_coords[1]);
    // dest is above current
    bool z_dirn = (dest_coords[0] >= my_coords[0]);
    // dest and current router are in the same sectorrant
    bool same_sector = (my_sector == dest_sector);

    // already checked that in outportCompute() function
    assert(!(x_hops == 0 && y_hops == 0 && z_hops == 0));

    // if cur node and dest router are in the same sector, move normally
    if (same_sector){
        if (x_hops > 0) {
            if (x_dirn) {
                // assert(inport_dirn == "Local" || inport_dirn == "West");
                outport_dirn = "East";
            } else {
                // assert(inport_dirn == "Local" || inport_dirn == "East");
                outport_dirn = "West";
            }
        } else if (y_hops > 0) {
            if (y_dirn) {
                // assert(inport_dirn != "North");
                outport_dirn = "North";
            } else {
                // assert(inport_dirn != "South");
                outport_dirn = "South";
            }
        } else if (z_hops > 0) {
            if (z_dirn) {
                // assert(inport_dirn != "Up");
                outport_dirn = "Up";
            } else {
                // assert(inport_dirn != "Down");
                outport_dirn = "Down";
            }
        } else {
            // x_hops == 0 and y_hops == 0 and z_hops == 0
            // this is not possible
            // already checked that in outportCompute() function
            panic("x_hops == y_hops == z_hops == 0");
        }
    } else {
        //if router in layer 0, move in xy direction to same sector as router
        if (my_coords[0] == 0) {
            if (x_hops > 0) {
                if (x_dirn) {
                    outport_dirn = "East";
                } else {
                    outport_dirn = "West";
                }
            } else if (y_hops > 0) {
                if (y_dirn) {
                    assert(inport_dirn != "North");
                    outport_dirn = "North";
                } else {
                    assert(inport_dirn != "South");
                    outport_dirn = "South";
                }
            }
        } else { // if router not in layer 0, move to layer 0
            // assert(inport_dirn != "Down");
            outport_dirn = "Down";
        }
    }
    // std::cout<<"outport_dirn: "<<outport_dirn<<std::endl;

    return m_outports_dirn2idx[outport_dirn];
}

int
RoutingUnit::outportComputeWireless(RouteInfo route,
                              int inport,
                              PortDirection inport_dirn)
{
    PortDirection outport_dirn = "Unknown";
    assert(wireless_count < 2);
    if (inport_dirn.find("Receive_") == 0) {
        // free token if packet comes from wireless transmission
        wireless_free = true;
        wireless_count--;
    }

    int num_rows = m_router->get_net_ptr()->getNumRows();
    int num_cols = m_router->get_net_ptr()->getNumCols();
    int z_depth = m_router->get_net_ptr()->getZDepth();
    assert(num_rows > 0 && num_cols > 0 && z_depth > 0);

    int my_id = m_router->get_id();
    int my_coords[3];
    //(z,y,x) = a[0],a[1],a[2]
    m_router->get_net_ptr()->getCoords(my_id, my_coords);

    int my_sector = m_router->get_net_ptr()->getSectorNU(my_id, my_coords[0]);

    int dest_id = route.dest_router;
    int dest_coords[3];
    //(z,y,x) = a[0],a[1],a[2]
    m_router->get_net_ptr()->getCoords(dest_id, dest_coords);

    int dest_sector = m_router->get_net_ptr()->getSectorNU(dest_id,
                                                           dest_coords[0]);

    int x_hops = abs(dest_coords[2] - my_coords[2]);
    int y_hops = abs(dest_coords[1] - my_coords[1]);
    int z_hops = abs(dest_coords[0] - my_coords[0]);

    // dest is east of current
    bool x_dirn = (dest_coords[2] >= my_coords[2]);
    // dest is north of current
    bool y_dirn = (dest_coords[1] >= my_coords[1]);
    // dest is above current
    bool z_dirn = (dest_coords[0] >= my_coords[0]);
    // dest and current router are in the same sectorrant
    bool same_sector = (my_sector == dest_sector);
    bool wirelessCapability = m_router->get_net_ptr()->getRouterType(my_id);
    // std::cout<<"wirelessCapability: "<<wirelessCapability<<std::endl;

    // already checked that in outportCompute() function
    assert(!(x_hops == 0 && y_hops == 0 && z_hops == 0));
    int best_router = m_router->get_net_ptr()->getBestWirelessRouter(my_id,
                                                                     dest_id);

    // best route is wireless and the token is free,
    // transmit using wireless and take token
    if (wirelessCapability && best_router != my_id && wireless_free) {
        outport_dirn = "Transmit_" + std::to_string(best_router);
        wireless_free = false;
        wireless_count++;
        transmission_count++;
        std::cout<<"transmission_count: "<<transmission_count<<std::endl;
    }
    // if cur node and dest router are in the same sector, move normally
    else if (same_sector){
        if (x_hops > 0) {
            if (x_dirn) {
                // assert(inport_dirn == "Local" || inport_dirn == "West");
                outport_dirn = "East";
            } else {
                // assert(inport_dirn == "Local" || inport_dirn == "East");
                outport_dirn = "West";
            }
        } else if (y_hops > 0) {
            if (y_dirn) {
                // assert(inport_dirn != "North");
                outport_dirn = "North";
            } else {
                // assert(inport_dirn != "South");
                outport_dirn = "South";
            }
        } else if (z_hops > 0) {
            if (z_dirn) {
                // assert(inport_dirn != "Up");
                outport_dirn = "Up";
            } else {
                // assert(inport_dirn != "Down");
                outport_dirn = "Down";
            }
        } else {
            // x_hops == 0 and y_hops == 0 and z_hops == 0
            // this is not possible
            // already checked that in outportCompute() function
            panic("x_hops == y_hops == z_hops == 0");
        }
    } else {
        //if router in layer 0, move in xy direction to same sector as router
        if (my_coords[0] == 0) {
            if (x_hops > 0) {
                if (x_dirn) {
                    outport_dirn = "East";
                } else {
                    outport_dirn = "West";
                }
            } else if (y_hops > 0) {
                if (y_dirn) {
                    assert(inport_dirn != "North");
                    outport_dirn = "North";
                } else {
                    assert(inport_dirn != "South");
                    outport_dirn = "South";
                }
            }
        } else {
            // if router not in layer 0, move to layer 0
            outport_dirn = "Down";
        }
    }
    // std::cout<<"outport_dirn: "<<outport_dirn<<std::endl;

    return m_outports_dirn2idx[outport_dirn];
}

// Template for implementing custom routing algorithm
// using port directions. (Example adaptive)
int
RoutingUnit::outportComputeCustom(RouteInfo route,
                                 int inport,
                                 PortDirection inport_dirn)
{
    panic("%s placeholder executed", __FUNCTION__);
}

} // namespace garnet
} // namespace ruby
} // namespace gem5
