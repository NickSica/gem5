# Copyright (c) 2010 Advanced Micro Devices, Inc.
#               2016 Georgia Institute of Technology
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Authors: Brad Beckmann
#          Tushar Krishna

from __future__ import print_function
from __future__ import absolute_import

from m5.params import *
from m5.objects import *

from common import FileSystemConfig

from .BaseTopology import SimpleTopology
import random

# Creates a generic 3D Mesh assuming an equal number of cache
# and directory controllers.
# XYZ routing is enforced (using link weights)
# to guarantee deadlock freedom.

class Wireless_NUChiplets(SimpleTopology):
    description='Wireless_NUChiplets'

    def __init__(self, controllers):
        self.nodes = controllers #includes directories and caches

        # Makes a MeshXYZChiplet topology with the following composition:

        # layer 2: routers and caches(L1)
        # layer 1: routers, directories(corners only) and caches(L1)
        # layer 0: routers only (not included in num_cpus)

    def makeTopology(self, options, network, IntLink, ExtLink, Router):
        print("File: Wireless_NUChiplets.py")
        nodes = self.nodes
        print("len(nodes) (includes caches and directories): ", len(nodes))

        user_routers = options.num_cpus # only the user specified number of routers (no layer 0)

        link_latency = options.link_latency # used by simple and garnet
        router_latency = options.router_latency # only used by garnet

        x_depth = options.mesh_cols

        if (options.z_depth>0):
            z_depth=options.z_depth
        else:
            z_depth = int(user_routers/x_depth/x_depth)

        if (options.mesh_rows>0):
            y_depth=options.mesh_rows
        else:
            y_depth = int(user_routers/x_depth/z_depth)

        true_z_depth = z_depth+1

        assert(z_depth > 0 and z_depth <= (user_routers))
        assert(y_depth > 0 and y_depth <= (user_routers))
        assert(x_depth > 0 and x_depth <= (user_routers))

        num_routers = true_z_depth*x_depth*y_depth # total number of routers in the build (all layers)
        assert(z_depth * y_depth * x_depth < (num_routers))
        assert(true_z_depth * y_depth * x_depth == num_routers)
        assert(options.nu_chiplets_input)
        assert(options.wireless_input)
        assert(options.wireless_input_pattern)

        if (options.wireless_width):
            wirelessWidth = options.wireless_width
        else:
            wirelessWidth = 4
        if (options.wired_width):
            regularWidth = options.wired_width
        else:
            regularWidth = 32

        wirelessInputPattern = options.wireless_input_pattern
        wirelessInput = [int(x) for x in options.wireless_input.split(',') if x.strip().isdigit()]
        wirelessRouters = []
        availableRouters = [x for x in range(y_depth*x_depth, num_routers)]
        print("wirelessInput: ", wirelessInput)
        assert(all(i >= 0 for i in wirelessInput))

        if (wirelessInputPattern == 'r'):
            print("randomly placed wireless")
            assert(all(router < x_depth*y_depth/2 for router in wirelessInput))
            for i in range(len(wirelessInput)):
                layer_routers = [x for x in range((i+1)*x_depth*y_depth, (i+2)*x_depth*y_depth)]
                layer_set = set(layer_routers)
                for x in range(wirelessInput[i]):
                    repeat = True
                    while(repeat):
                        # make sure there are enough routers in the layer that can be designated as wireless
                        assert(len(layer_set.intersection(set(availableRouters))) >= wirelessInput[i]-x)
                        router = random.randint((i+1)*x_depth*y_depth, (i+2)*x_depth*y_depth-1)
                        if(router not in wirelessRouters and router in availableRouters):
                            # only add to the array if it does not already exist in array
                            wirelessRouters.append(router)
                            # add an additional layer to the router value to account for addition of layer 0
                            # remove wireless router and its adjacent routers from availableRouters list
                            availableRouters.remove(router)
                            if (router+1 in availableRouters and router%x_depth != x_depth-1):
                                availableRouters.remove(router+1)
                            if (router-1 in availableRouters and router%x_depth != 0):
                                availableRouters.remove(router-1)
                            if (router+x_depth in availableRouters and (router%(x_depth*y_depth))/x_depth < y_depth-1):
                                availableRouters.remove(router+x_depth)
                            if (router-x_depth in availableRouters and (router%(x_depth*y_depth))/x_depth > 0):
                                availableRouters.remove(router-x_depth)
                            if (router+x_depth*y_depth in availableRouters):
                                availableRouters.remove(router+x_depth*y_depth)
                            if (router-x_depth*y_depth in availableRouters):
                                availableRouters.remove(router-x_depth*y_depth)
                            repeat = False
        elif (wirelessInputPattern == 'u'):
            print("user-designated wireless")
            for i in range(0, len(wirelessInput), 3):
                router = wirelessInput[i]*x_depth+wirelessInput[i+1]+wirelessInput[i+2]*x_depth*y_depth+(x_depth*y_depth)

                assert(router in availableRouters)
                print(router)

                wirelessRouters.append(router)
                availableRouters.remove(router)
                if (router+1 in availableRouters and router%x_depth != x_depth-1):
                    availableRouters.remove(router+1)
                if (router-1 in availableRouters and router%x_depth != 0):
                    availableRouters.remove(router-1)
                if (router+x_depth in availableRouters and (router%(x_depth*y_depth))/x_depth < y_depth-1):
                    availableRouters.remove(router+x_depth)
                if (router-x_depth in availableRouters and (router%(x_depth*y_depth))/x_depth > 0):
                    availableRouters.remove(router-x_depth)
                if (router+x_depth*y_depth in availableRouters):
                    availableRouters.remove(router+x_depth*y_depth)
                if (router-x_depth*y_depth in availableRouters):
                    availableRouters.remove(router-x_depth*y_depth)

        print("wirelessRouters: ", wirelessRouters)

        widthArr = []
        serDesArr = []

        for router in range(num_routers):
            if router in wirelessRouters:
                widthArr.append(wirelessWidth)
                serDesArr.append(True)
            else:
                widthArr.append(regularWidth)
                serDesArr.append(False)

        # print("wirelessInputPattern: ", wirelessInputPattern)
        # print("widthArr", widthArr)
        # print("serDesArr", serDesArr)

        for layer in range(z_depth, -1, -1):
            for row in range(y_depth-1, -1, -1):
                for col in range(x_depth):
                    print ("%3d" % (widthArr[row*x_depth+col+layer*y_depth*x_depth]), end=' ')
                print("")
            print("")

        print("number of user specified routers: ", (user_routers))
        print("number of routers in layer 0: ", (x_depth*y_depth))
        print("total number of routers: ", num_routers)
        print("x_depth: ", x_depth)
        print("y_depth: ", y_depth)
        print("true_z_depth", true_z_depth)

        # Create the routers in the mesh (all layers including layer 0)
        routers = [Router(router_id=i, latency = router_latency, width = widthArr[i]) \
            for i in range(num_routers)]
        network.routers = routers
        print("total routers created: ", len(routers))

        assert(len(routers)==num_routers)

        # link counter to set unique link ids
        link_count = 0

        # Determine which nodes are cache cntrls vs. dirs
        cache_nodes = []
        dir_nodes = []
        for node in nodes:
            if node.type == 'L1Cache_Controller':
                cache_nodes.append(node)
            elif node.type == 'Directory_Controller':
                dir_nodes.append(node)

        print("cache_nodes: ", len(cache_nodes))
        print("dir_nodes: ", len(dir_nodes))
        assert(len(cache_nodes) == user_routers)

        # Connect each cache controller to the appropriate router
        ext_links = []
        for (i, n) in enumerate(dir_nodes):
            cntrl_level, router_id = divmod(i, user_routers)
            # print("router_id: ", router_id)
            # assert(cntrl_level < caches_per_router)
            ext_links.append(ExtLink(link_id=link_count, ext_node=n,
                                    int_node=routers[router_id+x_depth*y_depth],
                                    width = widthArr[router_id+x_depth*y_depth],
                                    latency = link_latency))
            link_count += 1

        for (i, n) in enumerate(cache_nodes):
            cntrl_level, router_id = divmod(i, user_routers)
            # print("router_id: ", router_id)
            # assert(cntrl_level < caches_per_router)
            ext_links.append(ExtLink(link_id=link_count, ext_node=n,
                                    int_node=routers[router_id+x_depth*y_depth],
                                    width = widthArr[router_id+x_depth*y_depth],
                                    latency = link_latency))
            link_count += 1

        network.ext_links = ext_links

        # Create the mesh links.
        int_links = []
        total=link_count
        # East output to West input links
        for z in range(true_z_depth):
            for x in range(x_depth):
                for y in range(y_depth):
                    if (x + 1 < x_depth):
                        east_out = x + (y * x_depth) + (z * y_depth * x_depth)
                        west_in = (x + 1) + (y * x_depth) + (z * y_depth * x_depth)
                        int_links.append(IntLink(link_id=link_count,
                                                src_node=routers[east_out],
                                                dst_node=routers[west_in],
                                                src_outport="East",
                                                dst_inport="West",
                                                width = regularWidth,
                                                src_serdes = serDesArr[east_out],
                                                dst_serdes = serDesArr[west_in],
                                                latency = link_latency,
                                                weight=1))
                        link_count += 1
        print("\nNUM EAST-WEST LINKS = ", link_count-total)
        total=link_count
        # West output to East input links
        for z in range(true_z_depth):
            for x in range(x_depth):
                for y in range(y_depth):
                    if (x + 1 < x_depth):
                        east_in = x + (y * x_depth) + (z * y_depth * x_depth)
                        west_out = (x + 1) + (y * x_depth) + (z * y_depth * x_depth)
                        int_links.append(IntLink(link_id=link_count,
                                                src_node=routers[west_out],
                                                dst_node=routers[east_in],
                                                src_outport="West",
                                                dst_inport="East",
                                                width = regularWidth,
                                                src_serdes = serDesArr[west_out],
                                                dst_serdes = serDesArr[east_in],
                                                latency = link_latency,
                                                weight=1))
                        link_count += 1
        print("NUM WEST-EAST LINKS = ", link_count-total)
        total=link_count
        # North output to South input links
        for z in range(true_z_depth):
            for x in range(x_depth):
                for y in range(y_depth):
                    if (y + 1 < y_depth):
                        north_out = x + (y * x_depth) + (z * y_depth * x_depth)
                        south_in = x + ((y + 1) * x_depth) + (z * y_depth * x_depth)
                        int_links.append(IntLink(link_id=link_count,
                                                src_node=routers[north_out],
                                                dst_node=routers[south_in],
                                                src_outport="North",
                                                dst_inport="South",
                                                width = regularWidth,
                                                src_serdes = serDesArr[north_out],
                                                dst_serdes = serDesArr[south_in],
                                                latency = link_latency,
                                                weight=1))
                        link_count += 1
        print("NUM NORTH-SOUTH LINKS = ", link_count-total)
        total=link_count
        # South output to North input links
        for z in range(true_z_depth):
            for x in range(x_depth):
                for y in range(y_depth):
                    if (y + 1 < y_depth):
                        north_in = x + (y * x_depth) + (z * y_depth * x_depth)
                        south_out = x + ((y + 1) * x_depth) + (z * y_depth * x_depth)
                        int_links.append(IntLink(link_id=link_count,
                                                src_node=routers[south_out],
                                                dst_node=routers[north_in],
                                                src_outport="South",
                                                dst_inport="North",
                                                width = regularWidth,
                                                src_serdes = serDesArr[south_out],
                                                dst_serdes = serDesArr[north_in],
                                                latency = link_latency,
                                                weight=1))
                        link_count += 1
        print("NUM SOUTH-NORTH LINKS = ", link_count-total)
        total=link_count
        # Up output to Down input links
        for z in range(true_z_depth):
            for y in range(y_depth):
                for x in range(x_depth):
                    if (z + 1 < true_z_depth):
                        up_out = x + (y * x_depth) + (z * y_depth * x_depth)
                        down_in = x + (y * x_depth) + ((z + 1) * y_depth * x_depth)
                        int_links.append(IntLink(link_id=link_count,
                                                src_node=routers[up_out],
                                                dst_node=routers[down_in],
                                                src_outport="Up",
                                                dst_inport="Down",
                                                width = regularWidth,
                                                src_serdes = serDesArr[up_out],
                                                dst_serdes = serDesArr[down_in],
                                                latency = link_latency,
                                                weight=1))
                        link_count += 1
        print("NUM UP-DOWN LINKS = ", link_count-total)
        total=link_count
        # Down output to Up input links
        for z in range(true_z_depth):
            for y in range(y_depth):
                for x in range(x_depth):
                    if (z + 1 < true_z_depth):
                        up_in = x + (y * x_depth) + (z * y_depth * x_depth)
                        down_out = x + (y * x_depth) + ((z + 1) * y_depth * x_depth)
                        int_links.append(IntLink(link_id=link_count,
                                                src_node=routers[down_out],
                                                dst_node=routers[up_in],
                                                src_outport="Down",
                                                dst_inport="Up",
                                                width = regularWidth,
                                                src_serdes = serDesArr[down_out],
                                                dst_serdes = serDesArr[up_in],
                                                latency = link_latency,
                                                weight=1))
                        link_count += 1
        print("NUM DOWN-UP LINKS = ", link_count-total)
        total=link_count
        # All wireless input/output links
        for src in range(len(wirelessRouters)):
            for dest in range(len(wirelessRouters)):
                if (src != dest):
                    # print("src: %d, dest: %d" % (src, dest))
                    int_links.append(IntLink(link_id=link_count,
                                            src_node=routers[wirelessRouters[src]],
                                            dst_node=routers[wirelessRouters[dest]],
                                            src_outport="Transmit_" + str(wirelessRouters[dest]),
                                            dst_inport="Receive_" + str(wirelessRouters[src]),
                                            width = wirelessWidth,
                                            latency = link_latency,
                                            weight=1))
                    link_count += 1
        print("WIRELESS LINKS = ", link_count-total)
        total=link_count

        print("TOTAL NUM LINKS = ", len(int_links), "\n")
        network.int_links = int_links

    # Register nodes with filesystem
    def registerTopology(self, options):
        for i in range(options.num_cpus):
            FileSystemConfig.register_node([i],
                    MemorySize(options.mem_size) // options.num_cpus, i)
