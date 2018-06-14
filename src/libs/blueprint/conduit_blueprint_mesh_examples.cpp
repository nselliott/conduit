//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// Copyright (c) 2014-2018, Lawrence Livermore National Security, LLC.
// 
// Produced at the Lawrence Livermore National Laboratory
// 
// LLNL-CODE-666778
// 
// All rights reserved.
// 
// This file is part of Conduit. 
// 
// For details, see: http://software.llnl.gov/conduit/.
// 
// Please also read conduit/LICENSE
// 
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met:
// 
// * Redistributions of source code must retain the above copyright notice, 
//   this list of conditions and the disclaimer below.
// 
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the disclaimer (as noted below) in the
//   documentation and/or other materials provided with the distribution.
// 
// * Neither the name of the LLNS/LLNL nor the names of its contributors may
//   be used to endorse or promote products derived from this software without
//   specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL LAWRENCE LIVERMORE NATIONAL SECURITY,
// LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
// DAMAGES  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
// POSSIBILITY OF SUCH DAMAGE.
// 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

//-----------------------------------------------------------------------------
///
/// file: conduit_blueprint_mesh_examples.cpp
///
//-----------------------------------------------------------------------------

#if defined(CONDUIT_PLATFORM_WINDOWS)
#define NOMINMAX
#undef min
#undef max
#include "Windows.h"
#endif

//-----------------------------------------------------------------------------
// std lib includes
//-----------------------------------------------------------------------------
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <algorithm>
#include <cassert>
#include <map>
#include <set>

//-----------------------------------------------------------------------------
// conduit includes
//-----------------------------------------------------------------------------
#include "conduit_blueprint_mesh_examples.hpp"


//-----------------------------------------------------------------------------
// -- begin conduit:: --
//-----------------------------------------------------------------------------
namespace conduit
{


//-----------------------------------------------------------------------------
// -- begin conduit::blueprint:: --
//-----------------------------------------------------------------------------
namespace blueprint
{


//-----------------------------------------------------------------------------
// -- begin conduit::blueprint::mesh --
//-----------------------------------------------------------------------------
namespace mesh
{


//-----------------------------------------------------------------------------
// -- begin conduit::blueprint::mesh::examples --
//-----------------------------------------------------------------------------
namespace examples
{

//---------------------------------------------------------------------------//
const float64 PI_VALUE = 3.14159265359;


//---------------------------------------------------------------------------//
struct point
{
    point(float64 px = 0.0, float64 py = 0.0, float64 pz = 0.0) : x(px), y(py), z(pz)
    {
    };

    bool operator<(const point& other) const
    {
        float64 mvals[3] = {this->x, this->y, this->z};
        float64 ovals[3] = {other.x, other.y, other.z};

        for(index_t i = 0; i < 3; i++)
        {
            if(fabs(mvals[i] - ovals[i]) >= 1e-6)
            {
                return mvals[i] < ovals[i];
            }
        }
        return false;
    };

    float64 x, y, z;
};


//---------------------------------------------------------------------------//
void braid_init_example_state(Node &res)
{
    res["state/time"]   = (float64)3.1415;
    res["state/cycle"]  = (uint64) 100;
}


//---------------------------------------------------------------------------//
void braid_init_example_point_scalar_field(index_t npts_x,
                                           index_t npts_y,
                                           index_t npts_z,
                                           Node &res)
{
    index_t npts = npts_x * npts_y * npts_z;
    
    res["association"] = "vertex";
    res["type"] = "scalar";
    res["topology"] = "mesh";
    res["values"].set(DataType::float64(npts));
    
    float64 *vals = res["values"].value();

    float64 dx = (float) (4.0 * PI_VALUE) / float64(npts_x - 1);
    float64 dy = (float) (2.0 * PI_VALUE) / float64(npts_y-1);
    float64 dz = (float) (3.0 * PI_VALUE) / float64(npts_z-1);
    
    index_t idx = 0;

    for(index_t k = 0; k < npts_z ; k++)
    {
        float64 cz =  (k * dz) - (1.5 * PI_VALUE);

        for(index_t j = 0; j < npts_y ; j++)
        {
            float64 cy =  (j * dy) - ( PI_VALUE);
            for(index_t i = 0; i < npts_x ; i++)
            {
            
                float64 cx =  (i * dx) + (2.0 * PI_VALUE);
                
                float64 cv =  sin( cx ) + 
                              sin( cy ) + 
                              2 * cos(sqrt( (cx*cx)/2.0 +cy*cy) / .75) +
                              4 * cos( cx*cy / 4.0);
                                  
                if(npts_z > 1)
                {
                    cv += sin( cz ) + 
                          1.5 * cos(sqrt(cx*cx + cy*cy + cz*cz) / .75);
                }
                
                vals[idx] = cv;
                idx++;
            }
        }
    }
}

//---------------------------------------------------------------------------//
void braid_init_example_point_vector_field(index_t npts_x,
                                           index_t npts_y,
                                           index_t npts_z,
                                           Node &res)
{
    index_t npts = npts_x * npts_y * npts_z;
    
    res["association"] = "vertex";
    res["type"] = "vector";
    res["topology"] = "mesh";
    res["values/u"].set(DataType::float64(npts));
    res["values/v"].set(DataType::float64(npts));

    float64 *u_vals = res["values/u"].value();
    float64 *v_vals = res["values/v"].value();
    float64 *w_vals = NULL;
    
    if(npts_z > 1)
    {
        res["values/w"].set(DataType::float64(npts));
        w_vals = res["values/w"].value();
    }

    // this logic is from the explicit coord set setup function
    // we are using the coords (distance from origin)
    // to create an example vector field
    
    float64 dx = 20.0 / float64(npts_x - 1);
    float64 dy =  20.0 / float64(npts_y-1);
    float64 dz = 0.0;

    if(npts_z > 1)
    {
        dz = 20.0 / float64(npts_z-1);
    }

    index_t idx = 0;
    for(index_t k = 0; k < npts_z ; k++)
    {
        float64 cz = -10.0 + k * dz;
        
        for(index_t j = 0; j < npts_y ; j++)
        {
            float64 cy =  -10.0 + j * dy;
            
            for(index_t i = 0; i < npts_x ; i++)
            {
                float64 cx =  -10.0 + i * dx;

                u_vals[idx] = cx;
                v_vals[idx] = cy;

                if(npts_z > 1)
                {
                    w_vals[idx] = cz;
                }
                
                idx++;
            }
        
        }
    }
}


//---------------------------------------------------------------------------//
void braid_init_example_element_scalar_field(index_t nele_x,
                                             index_t nele_y,
                                             index_t nele_z,
                                             Node &res,
                                             index_t prims_per_ele=1)
{
    index_t nele = nele_x*nele_y;
    
    if(nele_z > 0)
    {
        nele = nele * nele_z;
    }

    res["association"] = "element";
    res["type"] = "scalar";
    res["topology"] = "mesh";
    res["values"].set(DataType::float64(nele*prims_per_ele));

    float64 *vals = res["values"].value();

    float64 dx = 20.0 / float64(nele_x);
    float64 dy = 20.0 / float64(nele_y);
    float64 dz = 0.0f;
    
    if(nele_z > 0 )
    {
        dz = 20.0 / float64(nele_z);
    }
    
    index_t idx = 0;
    for(index_t k = 0; (idx == 0 || k < nele_z); k++)
    {
        float64 cz =  (k * dz) + -10.0;

        for(index_t j = 0; (idx == 0 || j < nele_y) ; j++)
        {
            float64 cy =  (j * dy) + -10.0;
            
            for(index_t i = 0; (idx == 0 || i < nele_x) ; i++)
            {
                float64 cx =  (i * dx) + -10.0;

                float64 cv = 10.0 * sqrt( cx*cx + cy*cy );
                
                if(nele_z != 0)
                {
                    cv = 10.0 * sqrt( cx*cx + cy*cy +cz*cz );
                }

                for(index_t ppe = 0; ppe < prims_per_ele; ppe++ )
                {
                    vals[idx] = cv;
                    idx++;
                }
            }
        }
    }
}


//---------------------------------------------------------------------------//
void braid_init_example_matset(index_t nele_x,
                               index_t nele_y,
                               index_t nele_z,
                               Node &res)
{
    index_t nele = nele_x * nele_y * ((nele_z > 0) ? nele_z : 1);

    res["topology"] = "mesh";

    Node &vfs = res["volume_fractions"];
    vfs["mat1"].set(DataType::float64(nele));
    vfs["mat2"].set(DataType::float64(nele));

    float64 *mat1_vals = vfs["mat1"].value();
    float64 *mat2_vals = vfs["mat2"].value();

    for(index_t k = 0, idx = 0; (idx == 0 || k < nele_z); k++)
    {
        for(index_t j = 0; (idx == 0 || j < nele_y) ; j++)
        {
            for(index_t i = 0; (idx == 0 || i < nele_x) ; i++, idx++)
            {
                float64 mv = (nele_x == 1) ? 0.5 : i / (nele_x - 1.0);

                mat1_vals[idx] = mv;
                mat2_vals[idx] = 1.0 - mv;
            }
        }
    }
}


//---------------------------------------------------------------------------//
void braid_init_uniform_coordset(index_t npts_x,
                                 index_t npts_y,
                                 index_t npts_z,
                                 Node &coords)
{
    coords["type"] = "uniform";
    Node &dims = coords["dims"];
    dims["i"] = npts_x;
    dims["j"] = npts_y;

    if(npts_z > 1)
    {
        dims["k"] = npts_z;
    }
        
    // -10 to 10 in each dim, 
    Node &origin = coords["origin"];
    origin["x"] = -10.0;
    origin["y"] = -10.0;
    
    if(npts_z > 1)
    {
        origin["z"] = -10.0;
    }
    
    Node &spacing = coords["spacing"];
    spacing["dx"] = 20.0 / (float64)(npts_x-1);
    spacing["dy"] = 20.0 / (float64)(npts_y-1);

    if(npts_z > 1 )
    {
        spacing["dz"] = 20.0 / (float64)(npts_z-1);
    }
}


//---------------------------------------------------------------------------//
void braid_init_rectilinear_coordset(index_t npts_x,
                                     index_t npts_y,
                                     index_t npts_z,
                                     Node &coords)
{
    coords["type"] = "rectilinear";
    Node &coord_vals = coords["values"];
    coord_vals["x"].set(DataType::float64(npts_x));
    coord_vals["y"].set(DataType::float64(npts_y));
    
    if(npts_z > 1)
    {
        coord_vals["z"].set(DataType::float64(npts_z));
    }

    float64 *x_vals = coord_vals["x"].value();
    float64 *y_vals = coord_vals["y"].value();
    float64 *z_vals = NULL;

    if(npts_z > 1)
    {
        z_vals = coord_vals["z"].value();
    }


    float64 dx = 20.0 / (float64)(npts_x-1);
    float64 dy = 20.0 / (float64)(npts_y-1);
    float64 dz = 0.0;
    
    if(npts_z > 1)
    {
        dz = 20.0 / (float64)(npts_z-1);
    }

    for(int i=0; i < npts_x; i++)
    {
        x_vals[i] = -10.0 + i * dx;
    }
    
    for(int j=0; j < npts_y; j++)
    {
        y_vals[j] = -10.0 + j * dy;
    }
    
    if(npts_z > 1)
    {
        for(int k=0; k < npts_z; k++)
        {
            z_vals[k] = -10.0 + k * dz;
        }
    }
}

//---------------------------------------------------------------------------//
void
braid_init_explicit_coordset(index_t npts_x,
                             index_t npts_y,
                             index_t npts_z,
                             Node &coords)
{
    coords["type"] = "explicit";
    
    index_t npts = npts_x * npts_y * npts_z;

    // also support interleaved
    Node &coord_vals = coords["values"];
    coord_vals["x"].set(DataType::float64(npts));
    coord_vals["y"].set(DataType::float64(npts));

    if(npts_z > 1)
    {
        coord_vals["z"].set(DataType::float64(npts));
    }

    float64 *x_vals = coord_vals["x"].value();
    float64 *y_vals = coord_vals["y"].value();
    float64 *z_vals = NULL;
    
    if(npts_z > 1)
    {
        z_vals = coord_vals["z"].value();
    }

    float64 dx = 20.0 / float64(npts_x-1);
    float64 dy = 20.0 / float64(npts_y-1);

    float64 dz = 0.0;

    if(npts_z > 1)
    {
        dz = 20.0 / float64(npts_z-1);
    }

    index_t idx = 0;
    for(index_t k = 0; k < npts_z ; k++)
    {
        float64 cz = -10.0 + k * dz;
        
        for(index_t j = 0; j < npts_y ; j++)
        {
            float64 cy =  -10.0 + j * dy;
            
            for(index_t i = 0; i < npts_x ; i++)
            {
                x_vals[idx] = -10.0 + i * dx;
                y_vals[idx] = cy;
                
                if(npts_z > 1)
                {
                    z_vals[idx] = cz;
                }
                
                idx++;
            }
        
        }
    }
}


//---------------------------------------------------------------------------//
void braid_init_example_adjset(Node &mesh)
{
    typedef std::map< point, std::map<index_t, index_t> > point_doms_map;
    typedef std::map<std::set<index_t>, std::vector<std::vector<index_t> > > group_idx_map;

    const std::string dim_names[3] = {"x", "y", "z"};
    const bool mesh_3d = mesh.child(0)["coordsets/coords/values"].has_child("z");
    const index_t dim_count = mesh_3d ? 3 : 2;

    // From mesh data, create a map from domain combination tuple to point list.
    // These domain combination tuples represent groups and the point lists contain
    // the points that lie on the shared boundary between these domains.
    point_doms_map mesh_point_doms_map;

    conduit::NodeConstIterator doms_it = mesh.children();
    while(doms_it.has_next())
    {
        doms_it.next();
        const conduit::Node& dom_node = doms_it.node();
        const conduit::Node& dom_coords = dom_node["coordsets/coords/values"];
        const index_t dom_id = dom_node["state/domain_id"].to_uint64();

        conduit::float64_array dom_dim_coords[3];
        for(index_t d = 0; d < dim_count; d++)
        {
            dom_dim_coords[d] = dom_coords[dim_names[d]].as_float64_array();
        }

        for(index_t i = 0; i < dom_dim_coords[0].number_of_elements(); i++)
        {
            point coord(dom_dim_coords[0][i], dom_dim_coords[1][i]);
            if(mesh_3d)
            {
                coord.z = dom_dim_coords[2][i];
            }
            mesh_point_doms_map[coord][dom_id] = i;
        }
    }

    group_idx_map groups_map;

    point_doms_map::const_iterator pm_itr;
    for(pm_itr = mesh_point_doms_map.begin();
        pm_itr != mesh_point_doms_map.end(); ++pm_itr)
    {
        const std::map<index_t, index_t>& point_dom_idx_map = pm_itr->second;
        if(point_dom_idx_map.size() > 1)
        {
            std::set<index_t> point_group;

            std::map<index_t, index_t>::const_iterator pg_itr;
            for(pg_itr = point_dom_idx_map.begin();
                pg_itr != point_dom_idx_map.end(); ++pg_itr)
            {
                point_group.insert(pg_itr->first);
            }

            std::vector<std::vector<index_t> >& group_indices = groups_map[point_group];
            if(group_indices.empty())
            {
                group_indices.resize(point_group.size());
            }

            std::set<index_t>::const_iterator gd_itr;
            std::set<index_t>::size_type gi = 0;
            for(gd_itr = point_group.begin();
                gd_itr != point_group.end(); ++gd_itr, ++gi)
            {
                index_t g_idx = static_cast<index_t>(point_dom_idx_map.find(*gd_itr)->second);
                group_indices[gi].push_back(g_idx);
            }
        }
    }

    group_idx_map::const_iterator gm_itr;
    index_t gid = 0;
    for(gm_itr = groups_map.begin();
        gm_itr != groups_map.end(); ++gm_itr, ++gid)
    {
        const std::set<index_t>& group_doms = gm_itr->first;
        const std::vector<std::vector<index_t> >& group_indices = gm_itr->second;

        std::ostringstream oss;
        oss << "group" << gid;
        const std::string group_name = oss.str();

        std::set<index_t>::const_iterator dg_itr;
        std::set<index_t>::size_type d = 0;
        for(dg_itr = group_doms.begin();
            dg_itr != group_doms.end(); ++dg_itr, ++d)
        {
          const index_t& dom_id = *dg_itr;
          const std::vector<index_t>& dom_idxs = group_indices[d];

          oss.str("");
          oss << "domain" << dom_id;
          const std::string dom_name = oss.str();

          std::vector<index_t> dom_neighbors(group_doms.begin(), group_doms.end());
          dom_neighbors.erase(dom_neighbors.begin()+d);

          conduit::Node& dom_node = mesh[dom_name]["adjsets"]["mesh_adj"];
          dom_node["association"].set("vertex");
          dom_node["topology"].set("mesh");
          dom_node["groups"][group_name]["neighbors"].set(
            const_cast<index_t*>(dom_neighbors.data()), dom_neighbors.size());
          dom_node["groups"][group_name]["values"].set(
            const_cast<index_t*>(dom_idxs.data()), dom_idxs.size());
        }
    }
}


//---------------------------------------------------------------------------//
void braid_init_example_nestset(Node &mesh)
{
    // TODO(JRC): Implement this function using transforms to different spaces
    // to transform uniform/rectilinear grids into unstructured grids.
    typedef std::map<point, index_t> point_id_map;
    typedef std::pair<index_t, index_t> window;

    // NOTE(JRC): This function currently assumes that the given mesh contains
    // exclusively domains created with "braid_rectilinear"
    const std::string cartesian_dims[3] = {"x", "y", "z"};
    const std::string logical_dims[3] = {"i", "j", "k"};
    const bool mesh_3d = mesh.child(0)["coordsets/coords/values"].has_child("z");
    const index_t dim_count = mesh_3d ? 3 : 2;

    // initialize data to easily index domains by id/level //

    std::map<index_t, const Node*> mesh_id_map;
    index_t max_dom_id = 0, max_level_id = 0;
    {
        conduit::NodeConstIterator doms_it = mesh.children();
        while(doms_it.has_next())
        {
            const conduit::Node& dom_node = doms_it.next();
            const index_t dom_id = dom_node["state/domain_id"].to_uint64();
            mesh_id_map[dom_id] = &dom_node;
            max_dom_id = std::max(dom_id, max_dom_id);

            const index_t dom_level = dom_node["state/level_id"].to_uint64();
            max_level_id = std::max(dom_level, max_level_id);
        }
    }

    // transform rectilinear input data into unstructured data //

    std::vector<point_id_map> mesh_point_maps(max_dom_id + 1);
    std::vector< std::vector<const Node*> > mesh_level_map(max_level_id + 1);
    {
        conduit::NodeConstIterator doms_it = mesh.children();
        while(doms_it.has_next())
        {
            const conduit::Node &dom_node = doms_it.next();
            const index_t dom_id = dom_node["state/domain_id"].to_uint64();
            const index_t level_id = dom_node["state/level_id"].to_uint64();
            const conduit::Node &dom_coords = dom_node["coordsets/coords/values"];

            point_id_map &dom_point_map = mesh_point_maps[dom_id];
            {
                std::vector<point> dom_points(1);
                for(index_t d = 0; d < dim_count; d++)
                {
                    index_t dim_offset = d * (sizeof(float64) / sizeof(uint8));

                    std::vector<point> prev_dom_points = dom_points;
                    dom_points.clear();

                    float64_array dim_coords = dom_coords[cartesian_dims[d]].as_float64_array();
                    for(index_t i = 0; i < dim_coords.number_of_elements(); i++)
                    {
                        for(index_t p = 0; p < (index_t)prev_dom_points.size(); p++)
                        {
                            point new_point = prev_dom_points[p];
                            memcpy((uint8*)&new_point + dim_offset, &dim_coords[i],
                                sizeof(float64));
                            dom_points.push_back(new_point);
                        }
                    }
                }

                for(index_t i = 0; i < (index_t)dom_points.size(); i++)
                {
                    dom_point_map[dom_points[i]] = i;
                }
            }

            mesh_level_map[level_id].push_back(&dom_node);
        }
    }

    // NOTE(JRC): 'mesh_window_maps' maps a given domain ID to all of the windows
    // for that domain, which are returned via a map from other domain ID to
    // the indices of the extents in the primary domain.
    std::map< index_t, std::map<index_t, window> > mesh_window_maps;
    {
        for(index_t l = 0; l < (index_t)mesh_level_map.size() - 1; l++)
        {
            const std::vector<const Node*> &hi_nodes = mesh_level_map[l];
            const std::vector<const Node*> &lo_nodes = mesh_level_map[l+1];
            for(index_t hi = 0; hi < (index_t)hi_nodes.size(); hi++)
            {
                for(index_t lo = 0; lo < (index_t)lo_nodes.size(); lo++)
                {
                    const Node &hi_node = *hi_nodes[hi];
                    const Node &lo_node = *lo_nodes[lo];

                    const index_t hi_dom_id = hi_node["state/domain_id"].to_uint64();
                    const index_t lo_dom_id = lo_node["state/domain_id"].to_uint64();
                    const point_id_map &hi_point_map = mesh_point_maps[hi_dom_id];
                    const point_id_map &lo_point_map = mesh_point_maps[lo_dom_id];

                    std::vector<point> point_intx_list;
                    point_id_map::const_iterator hi_pt_itr = hi_point_map.begin();
                    point_id_map::const_iterator lo_pt_itr = lo_point_map.begin();
                    while(hi_pt_itr != hi_point_map.end() && lo_pt_itr != lo_point_map.end())
                    {
                        if(hi_pt_itr->first < lo_pt_itr->first)
                        {
                            ++hi_pt_itr;
                        }
                        else if(lo_pt_itr->first < hi_pt_itr->first)
                        {
                            ++lo_pt_itr;

                        }
                        else
                        {
                            point_intx_list.push_back(hi_pt_itr->first);
                            ++hi_pt_itr;
                            ++lo_pt_itr;
                        }
                    }
                    // TODO(JRC): Handle cases wherein the low mesh doesn't
                    // have a sensible window with the high mesh.
                    const point min_intx_point = point_intx_list.front();
                    const point max_intx_point = point_intx_list.back();

                    window &hi_window = mesh_window_maps[hi_dom_id][lo_dom_id];
                    hi_window.first = hi_point_map.at(min_intx_point);
                    hi_window.second = hi_point_map.at(max_intx_point);

                    window &lo_window = mesh_window_maps[lo_dom_id][hi_dom_id];
                    lo_window.first = lo_point_map.at(min_intx_point);
                    lo_window.second = lo_point_map.at(max_intx_point);
                }
            }
        }
    }

    conduit::NodeIterator doms_it = mesh.children();
    while(doms_it.has_next())
    {
        conduit::Node &dom_node = doms_it.next();
        index_t dom_id = dom_node["state/domain_id"].to_uint64();
        index_t dom_level = dom_node["state/level_id"].to_uint64();

        index_t dom_dims[3] = {0, 0, 0}; // needed for 1d to 3d xform per domain
        {
            const conduit::Node &dom_coords = dom_node["coordsets/coords/values"];
            for(index_t d = 0; d < 3; d++)
            {
                dom_dims[d] = !dom_coords.has_child(cartesian_dims[d]) ? 1 :
                    dom_coords[cartesian_dims[d]].dtype().number_of_elements();
            }
        }

        conduit::Node &dom_nestset = dom_node["nestsets/mesh_nest"];
        dom_nestset["association"].set("element");
        dom_nestset["topology"].set("mesh");

        std::map<index_t, window>::const_iterator dom_window_itr;
        for(dom_window_itr = mesh_window_maps[dom_id].begin();
            dom_window_itr != mesh_window_maps[dom_id].end(); ++dom_window_itr)
        {
            index_t odom_id = dom_window_itr->first;
            const conduit::Node &odom_node = *mesh_id_map[odom_id];
            index_t odom_level = odom_node["state/level_id"].to_uint64();

            window window_extrema = dom_window_itr->second;
            std::string window_name;
            {
                index_t min_dom_id = std::min(dom_id, odom_id);
                index_t max_dom_id = std::max(dom_id, odom_id);

                std::ostringstream oss;
                oss << "window_" <<  min_dom_id << "_" << max_dom_id;
                window_name = oss.str();
            }

            conduit::Node &dom_window = dom_nestset["windows"][window_name];
            dom_window["domain_id"].set(odom_id);
            dom_window["domain_type"].set(dom_level < odom_level ? "child" : "parent");

            index_t window_extents[2][3] = {{0, 0, 0}, {0, 0, 0}};
            for(index_t e = 0; e < 2; e++)
            {
                index_t window_extreme = e == 0 ? window_extrema.first : window_extrema.second;
                index_t *window_extent = &window_extents[e][0];

                index_t dim_remainder = window_extreme;
                for(index_t d = 3; d-- > 0;)
                {
                    index_t dim_stride = 1;
                    for(index_t dd = 0; dd < d; dd++)
                    {
                        dim_stride *= dom_dims[dd];
                    }

                    window_extent[d] = dim_remainder / dim_stride;
                    dim_remainder = dim_remainder % dim_stride;
                }
            }

            for(index_t d = 0; d < dim_count; d++)
            {
                // NOTE(JRC): These values may seem incorrect since they're relative
                // to point space, but they actually work out to calculate the proper
                // values because the coordinate indices for an element will always
                // match its minimum point indices and h-l points is number of elements.
                dom_window["origin"][logical_dims[d]].set(window_extents[0][d]);
                dom_window["dims"][logical_dims[d]].set(
                    window_extents[1][d] - window_extents[0][d]);
            }
        }
    }

    doms_it = mesh.children();
    while(doms_it.has_next())
    {
        conduit::Node &dom_node = doms_it.next();
        conduit::Node &dom_windows_node = dom_node["nestsets/mesh_nest/windows"];
        conduit::NodeIterator windows_it = dom_windows_node.children();
        while(windows_it.has_next())
        {
            conduit::Node &dom_window_node = windows_it.next();
            const std::string dom_window_name = windows_it.name();
            index_t odom_id = dom_window_node["domain_id"].to_uint64();

            const conduit::Node &odom_node = *mesh_id_map[odom_id];
            const conduit::Node &odom_window_node =
                odom_node["nestsets/mesh_nest/windows"][dom_window_name];

            const conduit::Node *parent_window_node, *child_window_node;
            if(dom_window_node["domain_type"].as_string() == "child")
            {
                parent_window_node = &dom_window_node;
                child_window_node = &odom_window_node;
            }
            else
            {
                parent_window_node = &odom_window_node;
                child_window_node = &dom_window_node;
            }

            for(index_t d = 0; d < dim_count; d++)
            {
                dom_window_node["ratio"][logical_dims[d]].set(
                    (*child_window_node)["dims"][logical_dims[d]].to_uint64() /
                    (*parent_window_node)["dims"][logical_dims[d]].to_uint64());
            }
        }
    }
}


//---------------------------------------------------------------------------//
void
braid_uniform(index_t npts_x,
              index_t npts_y,
              index_t npts_z,
              Node &res)
{
    res.reset();
    
    index_t nele_x = npts_x -1;
    index_t nele_y = npts_y -1;
    index_t nele_z = npts_z -1;
    
    braid_init_example_state(res);
    braid_init_uniform_coordset(npts_x,
                                npts_y,
                                npts_z,
                                res["coordsets/coords"]);

    res["topologies/mesh/type"] = "uniform";
    res["topologies/mesh/coordset"] = "coords"; 
    
    Node &fields = res["fields"];

    braid_init_example_point_scalar_field(npts_x,
                                          npts_y,
                                          npts_z,
                                          fields["braid"]);

    braid_init_example_element_scalar_field(nele_x,
                                            nele_y,
                                            nele_z,
                                            fields["radial"]);

    braid_init_example_point_vector_field(npts_x,
                                          npts_y,
                                          npts_z,
                                          fields["vel"]);

}



//---------------------------------------------------------------------------//
void
braid_rectilinear(index_t npts_x,
                  index_t npts_y,
                  index_t npts_z,
                  Node &res)
{
    res.reset();
    
    index_t nele_x = npts_x -1;
    index_t nele_y = npts_y -1;
    index_t nele_z = npts_z -1;
    
    braid_init_example_state(res);
    braid_init_rectilinear_coordset(npts_x,
                                    npts_y,
                                    npts_z,
                                    res["coordsets/coords"]);
    
    res["topologies/mesh/type"] = "rectilinear";
    res["topologies/mesh/coordset"] = "coords"; 
    
    Node &fields = res["fields"];

    braid_init_example_point_scalar_field(npts_x,
                                          npts_y,
                                          npts_z,
                                          fields["braid"]);

    braid_init_example_element_scalar_field(nele_x,
                                            nele_y,
                                            nele_z,
                                            fields["radial"]);

    braid_init_example_point_vector_field(npts_x,
                                          npts_y,
                                          npts_z,
                                          fields["vel"]);

}

//---------------------------------------------------------------------------//
void
braid_structured(index_t npts_x,
                 index_t npts_y,
                 index_t npts_z,
                 Node &res)
{
    res.reset();
    
    index_t nele_x = npts_x -1;
    index_t nele_y = npts_y -1;
    index_t nele_z = npts_z -1;
    
    braid_init_example_state(res);
    braid_init_explicit_coordset(npts_x,
                                 npts_y,
                                 npts_z,
                                 res["coordsets/coords"]);
  
    res["topologies/mesh/type"] = "structured";
    res["topologies/mesh/coordset"] = "coords";
    res["topologies/mesh/elements/dims/i"] = nele_x;
    res["topologies/mesh/elements/dims/j"] = nele_y;
    
    if(nele_z > 0)
    {
        res["topologies/mesh/elements/dims/k"] = nele_z; 
    }

    Node &fields = res["fields"];

    braid_init_example_point_scalar_field(npts_x,
                                          npts_y,
                                          npts_z,
                                          fields["braid"]);
                                          
    braid_init_example_element_scalar_field(nele_x,
                                            nele_y, 
                                            nele_z,
                                            fields["radial"]);

    braid_init_example_point_vector_field(npts_x,
                                          npts_y,
                                          npts_z,
                                          fields["vel"]);

}


//---------------------------------------------------------------------------//
void
braid_points_explicit(index_t npts_x,
                      index_t npts_y,
                      index_t npts_z,
                      Node &res)
{
    res.reset();
    index_t npts_total = npts_x * npts_y * npts_z;
    
    braid_init_example_state(res);
    braid_init_explicit_coordset(npts_x,
                                 npts_y,
                                 npts_z,
                                 res["coordsets/coords"]);
    
    res["topologies/mesh/type"] = "unstructured";
    res["topologies/mesh/coordset"] = "coords";
    res["topologies/mesh/elements/shape"] = "point";
    res["topologies/mesh/elements/connectivity"].set(DataType::int32(npts_total));
    int32 *conn = res["topologies/mesh/elements/connectivity"].value();

    for(int32 i = 0; i < (int32)npts_total ; i++)
    {
        conn[i] = i;
    }

    Node &fields = res["fields"];
    
    braid_init_example_point_scalar_field(npts_x,
                                          npts_y,
                                          npts_z,
                                          fields["braid"]);
    
    braid_init_example_element_scalar_field(npts_x,
                                            npts_y, 
                                            npts_z,
                                            fields["radial"]);

    braid_init_example_point_vector_field(npts_x,
                                          npts_y,
                                          npts_z,
                                          fields["vel"]);

}


//---------------------------------------------------------------------------//
void
braid_quads(index_t npts_x,
            index_t npts_y,
            Node &res)
{
    res.reset();
    
    int32 nele_x = (int32)(npts_x - 1);
    int32 nele_y = (int32)(npts_y - 1);
    int32 nele = nele_x * nele_y;
    
    braid_init_example_state(res);
    braid_init_explicit_coordset(npts_x,
                                 npts_y,
                                 1,
                                 res["coordsets/coords"]);
  
    res["topologies/mesh/type"] = "unstructured";
    res["topologies/mesh/coordset"] = "coords";
    res["topologies/mesh/elements/shape"] = "quad";
    res["topologies/mesh/elements/connectivity"].set(DataType::int32(nele*4));
    int32 *conn = res["topologies/mesh/elements/connectivity"].value();

    int32 idx = 0;
    for(int32 j = 0; j < nele_x ; j++)
    {
        int32 yoff = j * (nele_x+1);
        for(int32 i = 0; i < nele_y; i++)
        {
            conn[idx+0] = yoff + i;
            conn[idx+1] = yoff + i + (nele_x+1);
            conn[idx+2] = yoff + i + 1 + (nele_x+1);
            conn[idx+3] = yoff + i + 1;

            idx+=4;
        }
    }


    Node &fields = res["fields"];

    braid_init_example_point_scalar_field(npts_x,
                                          npts_y,
                                          1,
                                          fields["braid"]);

    braid_init_example_element_scalar_field(nele_x,
                                            nele_y,
                                            0,
                                            fields["radial"]);

    braid_init_example_point_vector_field(npts_x,
                                          npts_y,
                                          1,
                                          fields["vel"]);
}

//---------------------------------------------------------------------------//
void
braid_quads_and_tris(index_t npts_x,
            index_t npts_y,
            Node &res)
{
    res.reset();
    
    int32 nele_x = (int32)(npts_x - 1);
    int32 nele_y = (int32)(npts_y - 1);
    
    braid_init_example_state(res);
    braid_init_explicit_coordset(npts_x,
                                 npts_y,
                                 1,
                                 res["coordsets/coords"]);
  
    res["topologies/mesh/type"] = "unstructured";
    res["topologies/mesh/coordset"] = "coords";

    Node &elems = res["topologies/mesh/elements"];
    elems["element_types/quads/stream_id"] = 9; // VTK_QUAD
    elems["element_types/quads/shape"]     = "quad";
    elems["element_types/tris/stream_id"]  = 5; // VTK_TRIANGLE
    elems["element_types/tris/shape"]      = "tri";

    // Fill in stream IDs and calculate size of the connectivity array
    int32 count   = 0;
    int32 ielem   = 0;
    std::vector< int32 > stream_ids_buffer;
    std::vector< int32 > stream_lengths;

    for(int32 j = 0; j < nele_x ; j++)
    {
        for(int32 i = 0; i < nele_y; i++)
        {
             if ( ielem % 2 == 0 )
             {
                 // QUAD
                 stream_ids_buffer.push_back( 9 );
                 stream_lengths.push_back( 1 );
                 count += 4;
             }
             else
             {
                 // TRIANGLE
                 stream_ids_buffer.push_back( 5 );
                 count += 6;
                 stream_lengths.push_back( 2 );
             }

             ++ielem;

        } // END for all i

    } // END for all j


    elems["element_index/stream_ids"].set(stream_ids_buffer);
    elems["element_index/element_counts"].set(stream_lengths);

    // Allocate connectivity array
    elems["stream"].set(DataType::int32(count));
    int32* conn = elems["stream"].value();

    // Fill in connectivity array
    int32 idx = 0;
    int32 elem  = 0;
    for(int32 j = 0; j < nele_x ; j++)
    {
        int32 yoff = j * (nele_x+1);

        for(int32 i = 0; i < nele_y; i++)
        {
            int32 n1 = yoff + i;
            int32 n2 = n1 + (nele_x+1);
            int32 n3 = n1 + 1 + (nele_x+1);
            int32 n4 = n1 + 1;

            if ( elem % 2 == 0 )
            {
                conn[idx  ] = n1;
                conn[idx+1] = n2;
                conn[idx+2] = n3;
                conn[idx+3] = n4;
                idx+=4;
            }
            else
            {
               conn[idx   ] = n1;
               conn[idx+1 ] = n2;
               conn[idx+2 ] = n4;
               idx+=3;

               conn[idx   ] = n2;
               conn[idx+1 ] = n3;
               conn[idx+2 ] = n4;
               idx+=3;
            }

            ++elem;

        } // END for all i

    } // END for all j


    Node &fields = res["fields"];

    braid_init_example_point_scalar_field(npts_x,
                                          npts_y,
                                          1,
                                          fields["braid"]);

    braid_init_example_point_vector_field(npts_x,
                                          npts_y,
                                          1,
                                          fields["vel"]);

    // braid_init_example_element_scalar_field(nele_x,
    //                                         nele_y,
    //                                         0,
    //                                         fields["radial"]);
}

//---------------------------------------------------------------------------//
void
braid_quads_and_tris_offsets(index_t npts_x,
                             index_t npts_y,
                             Node &res)
{

    res.reset();

    int32 nele_x = (int32)(npts_x - 1);
    int32 nele_y = (int32)(npts_y - 1);

    braid_init_example_state(res);
    braid_init_explicit_coordset(npts_x,
                                 npts_y,
                                 1,
                                 res["coordsets/coords"]);

    res["topologies/mesh/type"] = "unstructured";
    res["topologies/mesh/coordset"] = "coords";
    
    Node &elems = res["topologies/mesh/elements"];
    elems["element_types/quads/stream_id"] = 9; // VTK_QUAD
    elems["element_types/quads/shape"]     = "quad";
    elems["element_types/tris/stream_id"]  = 5; // VTK_TRIANGLE
    elems["element_types/tris/shape"]      = "tri";

    // Fill in stream IDs and calculate size of the connectivity array
    int32 count   = 0;
    int32 ielem   = 0;
    std::vector< int32 > stream_ids;
    std::vector< int32 > stream_offsets;
    stream_offsets.push_back( 0 );

    for(int32 j = 0; j < nele_x ; j++)
    {
        for(int32 i = 0; i < nele_y; i++)
        {
            int32 next = stream_offsets.back();

             if ( ielem % 2 == 0 )
             {
                 // QUAD
                 stream_offsets.push_back( next+4 );
                 stream_ids.push_back( 9 );
                 count += 4;
             }
             else
             {
                 // TRIANGLE
                 stream_offsets.push_back( next+3 );
                 stream_offsets.push_back( next+6 );
                 stream_ids.push_back( 5 );
                 stream_ids.push_back( 5 );
                 count += 6;
             }

             ++ielem;

        } // END for all i

    } // END for all j


    elems["element_index/stream_ids"].set(stream_ids);
    elems["element_index/offsets"].set(stream_offsets);

    // Allocate connectivity array
    elems["stream"].set(DataType::int32(count));
    int32* conn = elems["stream"].value();

    // Fill in connectivity array
    int32 idx = 0;
    int32 elem  = 0;
    for(int32 j = 0; j < nele_x ; j++)
    {
        int32 yoff = j * (nele_x+1);

        for(int32 i = 0; i < nele_y; i++)
        {
            int32 n1 = yoff + i;
            int32 n2 = n1 + (nele_x+1);
            int32 n3 = n1 + 1 + (nele_x+1);
            int32 n4 = n1 + 1;

            if ( elem % 2 == 0 )
            {
                conn[idx  ] = n1;
                conn[idx+1] = n2;
                conn[idx+2] = n3;
                conn[idx+3] = n4;
                idx+=4;
            }
            else
            {
               conn[idx   ] = n1;
               conn[idx+1 ] = n2;
               conn[idx+2 ] = n4;
               idx+=3;

               conn[idx   ] = n2;
               conn[idx+1 ] = n3;
               conn[idx+2 ] = n4;
               idx+=3;
            }

            ++elem;

        } // END for all i

    } // END for all j


    Node &fields = res["fields"];

    braid_init_example_point_scalar_field(npts_x,
                                          npts_y,
                                          1,
                                          fields["braid"]);

    braid_init_example_point_vector_field(npts_x,
                                          npts_y,
                                          1,
                                          fields["vel"]);
}


//---------------------------------------------------------------------------//
void
braid_lines_2d(index_t npts_x,
               index_t npts_y,
               Node &res)
{
    res.reset();
    
    // require npts_x > 0 && npts_y > 0

    int32 nele_quads_x = (int32)(npts_x-1);
    int32 nele_quads_y = (int32)(npts_y-1);
    int32 nele_quads = nele_quads_x * nele_quads_y;
        
    braid_init_example_state(res);
    braid_init_explicit_coordset(npts_x,
                                 npts_y,
                                 1,
                                 res["coordsets/coords"]);
  
    res["topologies/mesh/type"] = "unstructured";
    res["topologies/mesh/coordset"] = "coords";
    res["topologies/mesh/elements/shape"] = "line";
    res["topologies/mesh/elements/connectivity"].set(DataType::int32(nele_quads*4*2));
    int32 *conn = res["topologies/mesh/elements/connectivity"].value();

    int32 idx = 0;
    for(int32 j = 0; j < nele_quads_y ; j++)
    {
        int32 yoff = j * (nele_quads_x+1);
        
        for(int32 i = 0; i < nele_quads_x; i++)
        {
            // 4 lines per quad.

            // Note: this pattern allows for simple per-quad construction,
            // but it creates spatially overlapping lines

            conn[idx++] = yoff + i;
            conn[idx++] = yoff + i + (nele_quads_x+1);

            conn[idx++] = yoff + i + (nele_quads_x+1);
            conn[idx++] = yoff + i + 1 + (nele_quads_x+1);

            conn[idx++] = yoff + i;
            conn[idx++] = yoff + i + 1;

            conn[idx++] = yoff + i + 1;
            conn[idx++] = yoff + i + 1 + (nele_quads_x+1);
        }
    }


    Node &fields = res["fields"];

    braid_init_example_point_scalar_field(npts_x,
                                          npts_y,
                                          1,
                                          fields["braid"]);

    braid_init_example_element_scalar_field(nele_quads_x,
                                            nele_quads_y,
                                            0,
                                            fields["radial"],4);

    braid_init_example_point_vector_field(npts_x,
                                          npts_y,
                                          1,
                                          fields["vel"]);

}

//---------------------------------------------------------------------------//
void
braid_tris(index_t npts_x,
           index_t npts_y,
           Node &res)
{
    res.reset();
    
    // require npts_x > 0 && npts_y > 0

    int32 nele_quads_x = (int32) npts_x-1;
    int32 nele_quads_y = (int32) npts_y-1;
    int32 nele_quads = nele_quads_x * nele_quads_y;
        
    braid_init_example_state(res);
    braid_init_explicit_coordset(npts_x,
                                 npts_y,
                                 1,
                                 res["coordsets/coords"]);
  
    res["topologies/mesh/type"] = "unstructured";
    res["topologies/mesh/coordset"] = "coords";
    res["topologies/mesh/elements/shape"] = "tri";
    res["topologies/mesh/elements/connectivity"].set(DataType::int32(nele_quads*6));
    int32 *conn = res["topologies/mesh/elements/connectivity"].value();

    int32 idx = 0;
    for(int32 j = 0; j < nele_quads_y ; j++)
    {
        int32 yoff = j * (nele_quads_x+1);
        
        for(int32 i = 0; i < nele_quads_x; i++)
        {
            // two tris per quad. 
            conn[idx+0] = yoff + i;
            conn[idx+1] = yoff + i + (nele_quads_x+1);
            conn[idx+2] = yoff + i + 1 + (nele_quads_x+1);

            conn[idx+3] = yoff + i;
            conn[idx+4] = yoff + i + 1;
            conn[idx+5] = yoff + i + 1 + (nele_quads_x+1);
            
            idx+=6;
        }
    }


    Node &fields = res["fields"];

    braid_init_example_point_scalar_field(npts_x,
                                          npts_y,
                                          1,
                                          fields["braid"]);

    braid_init_example_element_scalar_field(nele_quads_x,
                                            nele_quads_y,
                                            0,
                                            fields["radial"],2);

    braid_init_example_point_vector_field(npts_x,
                                          npts_y,
                                          1,
                                          fields["vel"]);

}


//---------------------------------------------------------------------------//
void
braid_hexs(index_t npts_x,
           index_t npts_y,
           index_t npts_z,
           Node &res)
{
    res.reset();
    
    int32 nele_x = (int32)(npts_x - 1);
    int32 nele_y = (int32)(npts_y - 1);
    int32 nele_z = (int32)(npts_z - 1);
    int32 nele = nele_x * nele_y * nele_z;
    
    braid_init_example_state(res);
    braid_init_explicit_coordset(npts_x,
                                 npts_y,
                                 npts_z,
                                 res["coordsets/coords"]);
  
    res["topologies/mesh/type"] = "unstructured";
    res["topologies/mesh/coordset"] = "coords";
    res["topologies/mesh/elements/shape"] = "hex";
    res["topologies/mesh/elements/connectivity"].set(DataType::int32(nele*8));
    int32 *conn = res["topologies/mesh/elements/connectivity"].value();

    int32 idx = 0;
    for(int32 k = 0; k < nele_z ; k++)
    {
        int32 zoff = k * (nele_x+1)*(nele_y+1);
        int32 zoff_n = (k+1) * (nele_x+1)*(nele_y+1);
        
        for(int32 j = 0; j < nele_y ; j++)
        {
            int32 yoff = j * (nele_x+1);
            int32 yoff_n = (j+1) * (nele_x+1);


            for(int32 i = 0; i < nele_x; i++)
            {
                // ordering is same as VTK_HEXAHEDRON

                conn[idx+0] = zoff + yoff + i;
                conn[idx+1] = zoff + yoff + i + 1;
                conn[idx+2] = zoff + yoff_n + i + 1;
                conn[idx+3] = zoff + yoff_n + i;

                conn[idx+4] = zoff_n + yoff + i;
                conn[idx+5] = zoff_n + yoff + i + 1;
                conn[idx+6] = zoff_n + yoff_n + i + 1;
                conn[idx+7] = zoff_n + yoff_n + i;

                idx+=8;
            }
        }
    }

    Node &fields = res["fields"];

    braid_init_example_point_scalar_field(npts_x,
                                          npts_y,
                                          npts_z,
                                          fields["braid"]);

    braid_init_example_element_scalar_field(nele_x,
                                            nele_y,
                                            nele_z,
                                            fields["radial"]);

    braid_init_example_point_vector_field(npts_x,
                                          npts_y,
                                          npts_z,
                                          fields["vel"]);
}

//---------------------------------------------------------------------------//
void
braid_tets(index_t npts_x,
           index_t npts_y,
           index_t npts_z,
           Node &res)
{
    res.reset();
    
    int32 nele_hexs_x = (int32) (npts_x - 1);
    int32 nele_hexs_y = (int32) (npts_y - 1);
    int32 nele_hexs_z = (int32) (npts_z - 1);
    int32 nele_hexs = nele_hexs_x * nele_hexs_y * nele_hexs_z;
    
    int32 tets_per_hex = 6;
    int32 verts_per_tet = 4;
    int32 n_tets_verts = nele_hexs * tets_per_hex * verts_per_tet;

    braid_init_example_state(res);
    braid_init_explicit_coordset(npts_x,
                                 npts_y,
                                 npts_z,
                                 res["coordsets/coords"]);
  

    res["topologies/mesh/type"] = "unstructured";
    res["topologies/mesh/coordset"] = "coords";
    res["topologies/mesh/elements/shape"] = "tet";
    res["topologies/mesh/elements/connectivity"].set(DataType::int32(n_tets_verts));
    int32 *conn = res["topologies/mesh/elements/connectivity"].value();


    int32 idx = 0;
    for(int32 k = 0; k < nele_hexs_z ; k++)
    {
        int32 zoff = k * (nele_hexs_x+1)*(nele_hexs_y+1);
        int32 zoff_n = (k+1) * (nele_hexs_x+1)*(nele_hexs_y+1);
        
        for(int32 j = 0; j < nele_hexs_y ; j++)
        {
            int32 yoff = j * (nele_hexs_x+1);
            int32 yoff_n = (j+1) * (nele_hexs_x+1);


            for(int32 i = 0; i < nele_hexs_z; i++)
            {
                // Create a local array of the vertex indices
                // ordering is same as VTK_HEXAHEDRON
                int32 vidx[8] =   {zoff + yoff + i
                                  ,zoff + yoff + i + 1
                                  ,zoff + yoff_n + i + 1
                                  ,zoff + yoff_n + i
                                  ,zoff_n + yoff + i
                                  ,zoff_n + yoff + i + 1
                                  ,zoff_n + yoff_n + i + 1
                                  ,zoff_n + yoff_n + i};

                // Create six tets all sharing diagonal from vertex 0 to 6
                // Uses SILO convention for vertex order (normals point in)
                conn[idx++] = vidx[0];
                conn[idx++] = vidx[2];
                conn[idx++] = vidx[1];
                conn[idx++] = vidx[6];

                conn[idx++] = vidx[0];
                conn[idx++] = vidx[3];
                conn[idx++] = vidx[2];
                conn[idx++] = vidx[6];

                conn[idx++] = vidx[0];
                conn[idx++] = vidx[7];
                conn[idx++] = vidx[3];
                conn[idx++] = vidx[6];

                conn[idx++] = vidx[0];
                conn[idx++] = vidx[4];
                conn[idx++] = vidx[7];
                conn[idx++] = vidx[6];

                conn[idx++] = vidx[0];
                conn[idx++] = vidx[5];
                conn[idx++] = vidx[4];
                conn[idx++] = vidx[6];

                conn[idx++] = vidx[0];
                conn[idx++] = vidx[1];
                conn[idx++] = vidx[5];
                conn[idx++] = vidx[6];

            }
        }
    }

    Node &fields = res["fields"];

    braid_init_example_point_scalar_field(npts_x,
                                          npts_y,
                                          npts_z,
                                          fields["braid"]);

    braid_init_example_element_scalar_field(nele_hexs_x,
                                            nele_hexs_y,
                                            nele_hexs_z,
                                            fields["radial"],
                                            tets_per_hex);

    braid_init_example_point_vector_field(npts_x,
                                          npts_y,
                                          npts_z,
                                          fields["vel"]);

}


//---------------------------------------------------------------------------//
void
braid_lines_3d(index_t npts_x,
               index_t npts_y,
               index_t npts_z,
               Node &res)
{
    res.reset();
    
    int32 nele_hexs_x = (int32)(npts_x - 1);
    int32 nele_hexs_y = (int32)(npts_y - 1);
    int32 nele_hexs_z = (int32)(npts_z - 1);
    int32 nele_hexs = nele_hexs_x * nele_hexs_y * nele_hexs_z;
    

    braid_init_example_state(res);
    braid_init_explicit_coordset(npts_x,
                                 npts_y,
                                 npts_z,
                                 res["coordsets/coords"]);

    res["topologies/mesh/type"] = "unstructured";
    res["topologies/mesh/coordset"] = "coords";
    res["topologies/mesh/elements/shape"] = "line";
    res["topologies/mesh/elements/connectivity"].set(DataType::int32(nele_hexs * 12 * 2));
    int32 *conn = res["topologies/mesh/elements/connectivity"].value();

    int32 idx = 0;
    for(int32 k = 0; k < nele_hexs_z ; k++)
    {
        int32 zoff = k * (nele_hexs_x+1)*(nele_hexs_y+1);
        int32 zoff_n = (k+1) * (nele_hexs_x+1)*(nele_hexs_y+1);
        
        for(int32 j = 0; j < nele_hexs_y ; j++)
        {
            int32 yoff = j * (nele_hexs_x+1);
            int32 yoff_n = (j+1) * (nele_hexs_x+1);


            for(int32 i = 0; i < nele_hexs_z; i++)
            {
                // 12 lines per hex 
                // Note: this pattern allows for simple per-hex construction,
                // but it creates spatially overlapping lines

                // front face
                conn[idx++] = zoff + yoff + i;
                conn[idx++] = zoff + yoff + i +1;

                conn[idx++] = zoff + yoff + i + 1;
                conn[idx++] = zoff + yoff_n + i + 1;
                
                conn[idx++] = zoff + yoff_n + i + 1;
                conn[idx++] = zoff + yoff_n + i;
                
                conn[idx++] = zoff + yoff_n + i;
                conn[idx++] = zoff + yoff + i;

                // back face
                conn[idx++] = zoff_n + yoff + i;
                conn[idx++] = zoff_n + yoff + i +1;

                conn[idx++] = zoff_n + yoff + i + 1;
                conn[idx++] = zoff_n + yoff_n + i + 1;
                
                conn[idx++] = zoff_n + yoff_n + i + 1;
                conn[idx++] = zoff_n + yoff_n + i;
                
                conn[idx++] = zoff_n + yoff_n + i;
                conn[idx++] = zoff_n + yoff + i;

                // sides 
                conn[idx++] = zoff   + yoff + i;
                conn[idx++] = zoff_n + yoff + i;

                conn[idx++] = zoff   + yoff + i + 1;
                conn[idx++] = zoff_n + yoff + i + 1;
                
                conn[idx++] = zoff   + yoff_n + i + 1;
                conn[idx++] = zoff_n + yoff_n + i + 1;
                
                conn[idx++] = zoff   + yoff_n + i;
                conn[idx++] = zoff_n + yoff_n + i;

            }
        }
    }

    Node &fields = res["fields"];

    braid_init_example_point_scalar_field(npts_x,
                                          npts_y,
                                          npts_z,
                                          fields["braid"]);

    braid_init_example_element_scalar_field(nele_hexs_x,
                                            nele_hexs_y,
                                            nele_hexs_z,
                                            fields["radial"],
                                            12);

    braid_init_example_point_vector_field(npts_x,
                                          npts_y,
                                          npts_z,
                                          fields["vel"]);

}



//---------------------------------------------------------------------------//
void
braid_hexs_and_tets(index_t npts_x,
                    index_t npts_y,
                    index_t npts_z,
                    Node &res)
{

    // WARNING -- The code below is UNTESTED.
    //            The SILO writer is missing an implementation for
    //            unstructured indexed_stream meshes in 3D.

    res.reset();

    int32 nele_hexs_x = (int32)(npts_x - 1);
    int32 nele_hexs_y = (int32)(npts_y - 1);
    int32 nele_hexs_z = (int32)(npts_z - 1);
    int32 nele_hexs = nele_hexs_x * nele_hexs_y * nele_hexs_z;


    // Set the number of voxels containing hexs and tets
    int32 n_hex_hexs = (nele_hexs > 1)? nele_hexs / 2 : nele_hexs;
    int32 n_hex_tets = nele_hexs - n_hex_hexs;

    // Compute the sizes of the connectivity array for each element type
    int32 hexs_per_hex = 1;
    int32 verts_per_hex = 8;
    int32 n_hexs_verts = n_hex_hexs * hexs_per_hex * verts_per_hex;

    int32 tets_per_hex = 6;
    int32 verts_per_tet = 4;
    int32 n_tets_verts = n_hex_tets * tets_per_hex * verts_per_tet;


    braid_init_example_state(res);
    braid_init_explicit_coordset(npts_x,
                                 npts_y,
                                 npts_z,
                                 res["coordsets/coords"]);

    // Setup mesh as unstructured indexed_stream mesh of hexs and tets
    res["topologies/mesh/type"] = "unstructured";
    res["topologies/mesh/coordset"] = "coords";

    res["topologies/mesh/elements/element_types/hexs/stream_id"] = 0;
    res["topologies/mesh/elements/element_types/hexs/shape"] = "hex";

    res["topologies/mesh/elements/element_types/tets/stream_id"] = 1;
    res["topologies/mesh/elements/element_types/tets/shape"] = "tet";

    res["topologies/mesh/elements/element_index/stream_ids"].set(DataType::int32(4));
    res["topologies/mesh/elements/element_index/element_counts"].set(DataType::int32(4));

    int32* sidx_ids = res["topologies/mesh/elements/element_index/stream_ids"].value();
    int32* sidx_lengths = res["topologies/mesh/elements/element_index/element_counts"].value();

    // There are four groups -- alternating between hexs and tets
    sidx_ids[0] = 0;
    sidx_ids[1] = 1;
    sidx_ids[2] = 0;
    sidx_ids[3] = 1;

    // Set the lengths of the groups
    // The first two groups have at most length 1
    // The last two groups have the balance of the elements
    switch(nele_hexs)
    {
    case 0:
        sidx_lengths[0] = 0;  sidx_lengths[1] = 0;
        sidx_lengths[2] = 0;  sidx_lengths[3] = 0;
        break;
    case 1:
        sidx_lengths[0] = 1;  sidx_lengths[1] = 0;
        sidx_lengths[2] = 0;  sidx_lengths[3] = 0;
        break;
    case 2:
        sidx_lengths[0] = 1;  sidx_lengths[1] = 1;
        sidx_lengths[2] = 0;  sidx_lengths[3] = 0;
        break;
    case 3:
        sidx_lengths[0] = 1;  sidx_lengths[1] = 1;
        sidx_lengths[2] = 1;  sidx_lengths[3] = 0;
        break;
    default:
        sidx_lengths[0] = 1;
        sidx_lengths[1] = 1;
        sidx_lengths[2] = n_hex_hexs-1;
        sidx_lengths[3] = n_hex_tets-1;
        break;
    }

    res["topologies/mesh/elements/stream"].set( DataType::int32(n_hexs_verts + n_tets_verts) );
    int32* conn = res["topologies/mesh/elements/stream"].value();

    int32 idx = 0;
    int32 elem_count = 0;
    for(int32 k = 0; k < nele_hexs_z ; k++)
    {
        int32 zoff = k * (nele_hexs_x+1)*(nele_hexs_y+1);
        int32 zoff_n = (k+1) * (nele_hexs_x+1)*(nele_hexs_y+1);

        for(int32 j = 0; j < nele_hexs_y ; j++)
        {
            int32 yoff = j * (nele_hexs_x+1);
            int32 yoff_n = (j+1) * (nele_hexs_x+1);


            for(int32 i = 0; i < nele_hexs_z; i++)
            {
                // Create a local array of the vertex indices
                // ordering is same as VTK_HEXAHEDRON
                int32 vidx[8] = {zoff + yoff + i
                                  ,zoff + yoff + i + 1
                                  ,zoff + yoff_n + i + 1
                                  ,zoff + yoff_n + i
                                  ,zoff_n + yoff + i
                                  ,zoff_n + yoff + i + 1
                                  ,zoff_n + yoff_n + i + 1
                                  ,zoff_n + yoff_n + i};

                bool isHex = (elem_count == 0)
                          || (elem_count > 1 && elem_count <= n_hex_hexs);


                if(isHex)
                {
                    conn[idx++] = vidx[0];
                    conn[idx++] = vidx[1];
                    conn[idx++] = vidx[2];
                    conn[idx++] = vidx[3];

                    conn[idx++] = vidx[4];
                    conn[idx++] = vidx[5];
                    conn[idx++] = vidx[6];
                    conn[idx++] = vidx[7];
                }
                else // it is a tet
                {
                    // Create six tets all sharing diagonal from vertex 0 to 6
                    // Uses SILO convention for vertex order (normals point in)
                    conn[idx++] = vidx[0];
                    conn[idx++] = vidx[2];
                    conn[idx++] = vidx[1];
                    conn[idx++] = vidx[6];

                    conn[idx++] = vidx[0];
                    conn[idx++] = vidx[3];
                    conn[idx++] = vidx[2];
                    conn[idx++] = vidx[6];

                    conn[idx++] = vidx[0];
                    conn[idx++] = vidx[7];
                    conn[idx++] = vidx[3];
                    conn[idx++] = vidx[6];

                    conn[idx++] = vidx[0];
                    conn[idx++] = vidx[4];
                    conn[idx++] = vidx[7];
                    conn[idx++] = vidx[6];

                    conn[idx++] = vidx[0];
                    conn[idx++] = vidx[5];
                    conn[idx++] = vidx[4];
                    conn[idx++] = vidx[6];

                    conn[idx++] = vidx[0];
                    conn[idx++] = vidx[1];
                    conn[idx++] = vidx[5];
                    conn[idx++] = vidx[6];
                }

                elem_count++;
            }
        }
    }

    Node &fields = res["fields"];

    braid_init_example_point_scalar_field(npts_x,
                                          npts_y,
                                          npts_z,
                                          fields["braid"]);

    braid_init_example_point_vector_field(npts_x,
                                          npts_y,
                                          npts_z,
                                          fields["vel"]);

//    // Omit for now -- the function assumes a uniform element type
//    braid_init_example_element_scalar_field(nele_hexs_x,
//                                            nele_hexs_y,
//                                            nele_hexs_z,
//                                            fields["radial"],
//                                            tets_per_hex);
}



//---------------------------------------------------------------------------//
void
braid(const std::string &mesh_type,
      index_t npts_x, // number of points in x
      index_t npts_y, // number of points in y
      index_t npts_z, // number of points in z
      Node &res)
{

    if(mesh_type == "uniform")
    {
        braid_uniform(npts_x,npts_y,npts_z,res);
    }
    else if(mesh_type == "rectilinear")
    {
        braid_rectilinear(npts_x,npts_y,npts_z,res);
    }
    else if(mesh_type == "structured")
    {
        braid_structured(npts_x,npts_y,npts_z,res);
    }
    else if(mesh_type == "lines")
    {
        if( npts_z <= 1 )
            braid_lines_2d(npts_x,npts_y,res);
        else
            braid_lines_3d(npts_x,npts_y,npts_x,res);
    }
    else if(mesh_type == "tris")
    {
        braid_tris(npts_x,npts_y,res);
    }
    else if(mesh_type == "quads")
    {
        braid_quads(npts_x,npts_y,res);
    }
    else if(mesh_type == "quads_and_tris")
    {
        braid_quads_and_tris(npts_x,npts_y,res);
    }
    else if(mesh_type == "quads_and_tris_offsets")
    {
        braid_quads_and_tris_offsets(npts_x,npts_y,res);
    }
    else if(mesh_type == "tets")
    {
        braid_tets(npts_x,npts_y,npts_z,res);
    }
    else if(mesh_type == "hexs")
    {
        braid_hexs(npts_x,npts_y,npts_z,res);
    }
    else if(mesh_type == "hexs_and_tets")
    {
        braid_hexs_and_tets(npts_x,npts_y,npts_z,res);
    }
    else if(mesh_type == "points")
    {
        braid_points_explicit(npts_x,npts_y,npts_z,res);
    }
    else
    {
        CONDUIT_ERROR("unknown mesh_type = " << mesh_type);
    }
}



//---------------------------------------------------------------------------//
void
misc(const std::string &mesh_type,
     index_t npts_x, // number of points in x
     index_t npts_y, // number of points in y
     index_t /*npts_z*/, // number of points in z
     Node &res)
{
    // TODO(JRC): Improve these examples so that they use different example
    // geometry than is used in the "braid" examples.
    if(mesh_type == "matsets")
    {
        braid_quads(npts_x,npts_y,res);
        braid_init_example_matset(npts_x-1,npts_y-1,0,res["matsets/mesh"]);
    }
    else if(mesh_type == "adjsets")
    {
        for(index_t j = 0; j < 2; j++)
        {
            for(index_t i = 0; i < 2; i++)
            {
                const index_t domain_id = j * 2 + i;

                std::ostringstream oss;
                oss << "domain" << domain_id;
                const std::string domain_name = oss.str();

                Node &domain_node = res[domain_name];
                braid_quads(npts_x,npts_y,domain_node);
                domain_node["state/domain_id"].set(domain_id);

                Node &domain_coords = domain_node["coordsets/coords/values"];
                float64_array domain_coords_x = domain_coords["x"].as_float64_array();
                for(index_t x = 0; x < domain_coords_x.number_of_elements(); x++)
                {
                    domain_coords_x[x] += i * 20.0;
                }
                float64_array domain_coords_y = domain_coords["y"].as_float64_array();
                for(index_t y = 0; y < domain_coords_y.number_of_elements(); y++)
                {
                    domain_coords_y[y] += j * 20.0;
                }
            }
        }

        braid_init_example_adjset(res);
    }
    else if(mesh_type == "nestsets")
    {
        braid_rectilinear(npts_x,npts_y,1,res["domain0"]);
        res["domain0/state/domain_id"].set(0);
        res["domain0/state/level_id"].set(0);

        for(index_t j = 0; j < 2; j++)
        {
            for(index_t i = 0; i < 2; i++)
            {
                const index_t domain_id = j * 2 + i + 1;

                std::ostringstream oss;
                oss << "domain" << domain_id;
                const std::string domain_name = oss.str();

                Node &domain_node = res[domain_name];
                braid_rectilinear(npts_x,npts_y,1,domain_node);
                domain_node["state/domain_id"].set(domain_id);
                domain_node["state/level_id"].set(1);

                Node &domain_coords = domain_node["coordsets/coords/values"];
                float64_array domain_coords_x = domain_coords["x"].as_float64_array();
                for(index_t x = 0; x < domain_coords_x.number_of_elements(); x++)
                {
                    domain_coords_x[x] = ( domain_coords_x[x] / 2.0 ) - 5.0 + i * 10.0;
                }
                float64_array domain_coords_y = domain_coords["y"].as_float64_array();
                for(index_t y = 0; y < domain_coords_y.number_of_elements(); y++)
                {
                    domain_coords_y[y] = ( domain_coords_y[y] / 2.0 ) - 5.0 + j * 10.0;
                }
            }
        }

        braid_init_example_nestset(res);
    }
    else
    {
        CONDUIT_ERROR("unknown mesh_type = " << mesh_type);
    }
}



//-----------------------------------------------------------------------------
}
//-----------------------------------------------------------------------------
// -- end conduit::blueprint::mesh::examples --
//-----------------------------------------------------------------------------


}
//-----------------------------------------------------------------------------
// -- end conduit::blueprint::mesh --
//-----------------------------------------------------------------------------


}
//-----------------------------------------------------------------------------
// -- end conduit::blueprint:: --
//-----------------------------------------------------------------------------


}
//-----------------------------------------------------------------------------
// -- end conduit:: --
//-----------------------------------------------------------------------------
