// Copyright (c) Lawrence Livermore National Security, LLC and other Conduit
// Project developers. See top-level LICENSE AND COPYRIGHT files for dates and
// other details. No copyright assignment is required to contribute to Conduit.

//-----------------------------------------------------------------------------
///
/// file: conduit_blueprint_mpi_mesh.cpp
///
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// conduit includes
//-----------------------------------------------------------------------------
#include "conduit_blueprint_mesh.hpp"
#include "conduit_blueprint_mesh_utils.hpp"
#include "conduit_blueprint_mpi_mesh.hpp"
#include "conduit_relay_mpi.hpp"
#include <assert.h>
#include <cmath>
#include <limits>
// access conduit blueprint mesh utilities
namespace bputils = conduit::blueprint::mesh::utils;

//-----------------------------------------------------------------------------
// -- begin conduit --
//-----------------------------------------------------------------------------
namespace conduit
{

//-----------------------------------------------------------------------------
// -- begin conduit::blueprint --
//-----------------------------------------------------------------------------
namespace blueprint
{

//-----------------------------------------------------------------------------
// -- begin conduit::blueprint::mpi --
//-----------------------------------------------------------------------------
namespace mpi
{

//-----------------------------------------------------------------------------
// -- begin conduit::blueprint::mesh --
//-----------------------------------------------------------------------------

namespace mesh 
{
//-----------------------------------------------------------------------------

// TODO(JRC): Consider moving these structures somewhere else.

//-------------------------------------------------------------------------
struct SharedFace
{
    index_t m_face_id;
    index_t m_crse_pt = -1;
    index_t m_fine_pt = -1;
};


struct PolyBndry
{
    index_t side; //which 3D side 0-5
    index_t m_nbr_rank;
    index_t m_nbr_id;
    std::vector<index_t> m_elems; //elems of nbr domain that touch side
    std::map<index_t, index_t> m_bface; //map from nbr elem to face of nbr elem
    std::map<index_t, std::vector<index_t> > m_nbr_elems; //map from local
                                                          //elem to all
                                                          //nbr elems that
                                                          //touch it 
    //outer map: local elem, inner map: nbr elem to face
    std::map<index_t, std::map<index_t, SharedFace> > m_nbr_faces;
};

//-----------------------------------------------------------------------------
// blueprint::mesh::index protocol interface
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
bool
verify(const conduit::Node &n,
       conduit::Node &info,
       MPI_Comm comm)
{
    int par_size = relay::mpi::size(comm);

    // NOTE(JRC): MPI tasks without any domains should use a multi-domain
    // format with empty contents (i.e. an empty object or list node).
    int local_verify_ok = conduit::blueprint::mesh::verify(n, info) ? 1 : 0;
    int global_verify_ok = 0;

    Node n_snd, n_reduce;
    // make sure some MPI task actually had bp data
    n_snd.set_external(&local_verify_ok,1);
    n_reduce.set_external(&global_verify_ok,1);

    relay::mpi::sum_all_reduce(n_snd, n_reduce, comm);
    return global_verify_ok == par_size;
}

//-------------------------------------------------------------------------
void
generate_index(const conduit::Node &mesh,
               const std::string &ref_path,
               Node &index_out,
               MPI_Comm comm)
{
    int par_rank = relay::mpi::rank(comm);
    int par_size = relay::mpi::size(comm);
    // we need to know the mesh structure and the number of domains
    // we can't assume rank zero has any domains (could be empty)
    // so we look for the lowest rank with 1 or more domains

    index_t local_num_domains = ::conduit::blueprint::mesh::number_of_domains(mesh);
    index_t global_num_domains = number_of_domains(mesh,comm);

    index_t rank_send = par_size;
    index_t selected_rank = par_size;
    if(local_num_domains > 0)
        rank_send = par_rank;

    Node n_snd, n_reduce;
    // make sure some MPI task actually had bp data
    n_snd.set_external(&rank_send,1);
    n_reduce.set_external(&selected_rank,1);

    relay::mpi::min_all_reduce(n_snd, n_reduce, comm);

    Node do_par_map;
    do_par_map.set_uint8(0);
    if(par_rank == selected_rank )
    {
        if(::conduit::blueprint::mesh::is_multi_domain(mesh))
        {
            if(mesh.child(0).has_child("state") &&
               mesh.child(0)["state"].has_child("domain_id"))
            {
                do_par_map.set_uint8(1);
            }
        }
    }

    relay::mpi::broadcast(do_par_map, selected_rank, comm); 

    Node partition;
    if(do_par_map.as_uint8())
    {
        generate_partition(mesh, partition, comm);
    }

    if(par_rank == selected_rank )
    {
        if(::conduit::blueprint::mesh::is_multi_domain(mesh))
        {
            ::conduit::blueprint::mesh::generate_index(mesh.child(0),
                                                       ref_path,
                                                       global_num_domains,
                                                       index_out);
        }
        else
        {
            ::conduit::blueprint::mesh::generate_index(mesh,
                                                       ref_path,
                                                       global_num_domains,
                                                       index_out);
        }

        if (!partition.dtype().is_empty())
        {
            index_out["state/partition_size"].set(par_size);
            index_out["state/domain_to_partition_map"].set(partition);
        }
    }

    // broadcast the resulting index to all other ranks
    relay::mpi::broadcast_using_schema(index_out,
                                       selected_rank,
                                       comm);
}


//-----------------------------------------------------------------------------
void generate_domain_to_rank_map(const conduit::Node &mesh,
                                 Node &domain_to_rank_map,
                                 MPI_Comm comm)
{
    int64 par_rank = relay::mpi::rank(comm);
    int64 max_local_id = -1;

    std::vector<const Node *> domains = ::conduit::blueprint::mesh::domains(mesh);
    std::vector<int64> local_domains;
    for(index_t di = 0; di < (index_t)domains.size(); di++)
    {
        const conduit::Node &domain = *domains[di];

        int64 domain_id = par_rank;
        if(domain.has_child("state") && domain["state"].has_child("domain_id"))
        {
            domain_id = domain["state/domain_id"].as_int64();
        }
        local_domains.push_back(domain_id);

        max_local_id = (domain_id > max_local_id) ? domain_id : max_local_id;
    }

    Node max_local, max_global;
    max_local.set_int64(max_local_id);
    max_global.set_int64(-1);
    relay::mpi::max_all_reduce(max_local, max_global, comm);

    std::vector<int64> local_map(max_global.as_int64() + 1, -1);
    for(auto m_itr = local_domains.begin(); m_itr != local_domains.end(); ++m_itr)
    {
        local_map[*m_itr] = par_rank;
    }

    Node local_par;
    local_par.set_external(&local_map[0], local_map.size());

    relay::mpi::max_all_reduce(local_par, domain_to_rank_map, comm);
}

//-----------------------------------------------------------------------------
index_t
number_of_domains(const conduit::Node &n,
                  MPI_Comm comm)
{
    // called only when mesh bp very is true, simplifies logic here
    index_t local_num_domains = 0;
    if(!n.dtype().is_empty())
    {
        local_num_domains = ::conduit::blueprint::mesh::number_of_domains(n);
    }

    index_t global_num_domains = 0;

    Node n_snd, n_reduce;
    // count all domains with mpi
    n_snd.set_external(&local_num_domains,1);
    n_reduce.set_external(&global_num_domains,1);

    relay::mpi::all_reduce(n_snd, n_reduce, MPI_SUM, comm);
    return global_num_domains;
}

//-------------------------------------------------------------------------
void generate_partition(const conduit::Node &mesh,
                        Node &partition,
                        MPI_Comm comm)
{
    int par_rank = relay::mpi::rank(comm);

    std::vector<conduit::int64> local_domains;

    conduit::NodeConstIterator itr = mesh.children();
    while (itr.has_next())
    {
        const conduit::Node &chld = itr.next();
        if (chld.has_child("state"))
        {
            const conduit::Node &state = chld["state"];
            if (state.has_child("domain_id"))
            {
                 conduit::int64 dom_id = state["domain_id"].as_int64();
                 local_domains.push_back(dom_id);
            }
        }
    }

    Node num_local, num_global;
    num_local.set_int64(local_domains.size());
    num_global.set_int64(0);
    relay::mpi::sum_all_reduce(num_local, num_global, comm);

    std::vector<int64> local_partition(num_global.as_int64(), 0);
    for (auto m_itr = local_domains.begin(); m_itr != local_domains.end();
         ++m_itr)
    {
         local_partition[*m_itr] = par_rank;
    }

    Node local_par;
    local_par.set_external(&local_partition[0], local_partition.size());

    relay::mpi::max_all_reduce(local_par, partition, comm);
}


//-------------------------------------------------------------------------
void to_polygonal(const Node &n,
                  Node &dest,
                  const std::string& name)
{
    // Helper Functions //

    const static auto gen_default_name = [] (const std::string &prefix, const index_t id)
    {
        std::ostringstream oss;
        oss << prefix << "_" << std::setw(6) << std::setfill('0') << id;
        return oss.str();
    };

    const static auto is_domain_local = [] (const conduit::Node &root, const std::string &name)
    {
        return ::conduit::blueprint::mesh::is_multi_domain(root) && root.has_child(name);
    };

    // Implementation //

    // TODO(JRC): Use widest data type available in given Node for both ints and doubles
    // TODO(JRC): Communicate this across the network?

    dest.reset();

    Node temp;

    std::map<index_t, std::map<index_t, std::vector<index_t> > > poly_elems_map;
    std::map<index_t, std::map<index_t, std::vector<double> > > dom_to_nbr_to_xbuffer;
    std::map<index_t, std::map<index_t, std::vector<double> > > dom_to_nbr_to_ybuffer;

    const std::vector<const conduit::Node *> doms = ::conduit::blueprint::mesh::domains(n);
    for (index_t si = 0; si < 3; si++)
    {
        // TODO(JRC): Figure out better names for all of these stages.
        // TODO(JRC): Consider using a local enumeration to track this
        // instead, to have compile-time consistency.
        const std::string stage_name = (
            (si == 0) ? "send_missing_windows" : (
            (si == 1) ? "fill_all_windows" : (
            (si == 2) ? "generate_polygons" : (""))));

        for (const auto& dom_ptr : doms)
        {
            const Node &dom = *dom_ptr;
            Node &dest_dom = ::conduit::blueprint::mesh::is_multi_domain(n) ?
                dest[dom.name()] : dest;

            const Node &in_topo = dom["topologies"][name];
            Node in_cset;
            bputils::find_reference_node(in_topo, "coordset", in_cset);
            Node &out_cset = dest_dom["coordsets"][in_topo["coordset"].as_string()];

            const index_t domain_id = dom["state/domain_id"].to_index_t();
            const index_t level_id = dom["state/level_id"].to_index_t();
            const std::string ref_name = gen_default_name("window", domain_id);

            const index_t i_lo = in_topo["elements/origin/i0"].to_index_t();
            const index_t j_lo = in_topo["elements/origin/j0"].to_index_t();
            const index_t iwidth = in_topo["elements/dims/i"].to_index_t();
            const index_t jwidth = in_topo["elements/dims/j"].to_index_t();
            const index_t niwidth = in_topo["elements/dims/i"].to_index_t() + 1;

            // TODO(JRC): There's an implicit assumption here about each
            // domain having a 'state' path, which is optional in the general
            // case. This needs to be checked and warned about before any
            // processing occurs.
            if (si == 0)
            {
                if (level_id == 0)
                {
                    continue;
                }
            }
            else if (si == 2)
            {
                if (blueprint::mesh::coordset::uniform::verify(in_cset, temp))
                {
                    blueprint::mesh::coordset::uniform::to_explicit(in_cset, out_cset);
                }
                else if (blueprint::mesh::coordset::rectilinear::verify(in_cset, temp))
                {
                    blueprint::mesh::coordset::rectilinear::to_explicit(in_cset, out_cset);
                }
                else
                {
                    out_cset.set(in_cset);
                }
            }

            auto &poly_elems = poly_elems_map[domain_id];
            auto &nbr_to_xbuffer = dom_to_nbr_to_xbuffer[domain_id];
            auto &nbr_to_ybuffer = dom_to_nbr_to_ybuffer[domain_id];

            if (dom.has_path("adjsets/adjset/groups"))
            {
                NodeConstIterator grp_itr = dom["adjsets/adjset/groups"].children();
                while(grp_itr.has_next())
                {
                    const Node& group = grp_itr.next();
                    if (group.has_child("neighbors") && group.has_child("windows"))
                    {
                        temp.reset();
                        temp.set_external(DataType(group["neighbors"].dtype().id(), 1),
                                          (void*)group["neighbors"].element_ptr(1));
                        const index_t nbr_id = temp.to_index_t();
                        const std::string nbr_name = gen_default_name("domain", nbr_id);

                        const Node &in_windows = group["windows"];
                        const std::string nbr_win_name = gen_default_name("window", nbr_id);

                        const Node &ref_win = in_windows[ref_name];
                        const Node &nbr_win = in_windows[nbr_win_name];

                        const index_t nbr_level = nbr_win["level_id"].to_index_t();
                        const index_t ref_level = ref_win["level_id"].to_index_t();

                        if ((si == 0 && nbr_level < ref_level) ||
                            (si != 0 && nbr_level > ref_level))
                        {
                            const index_t ref_size_i = ref_win["dims/i"].to_index_t();
                            const index_t ref_size_j = ref_win["dims/j"].to_index_t();
                            const index_t ref_size = ref_size_i * ref_size_j;

                            const index_t nbr_size_i = nbr_win["dims/i"].to_index_t();
                            const index_t nbr_size_j = nbr_win["dims/j"].to_index_t();
                            const index_t nbr_size = nbr_size_i * nbr_size_j;

                            const index_t ratio_i = nbr_win["ratio/i"].to_index_t();
                            const index_t ratio_j = nbr_win["ratio/j"].to_index_t();

                            if (si == 0 && nbr_size < ref_size && !is_domain_local(n, nbr_name))
                            {
                                std::vector<double> xbuffer, ybuffer;
                                const Node &fcoords = in_cset["values"];

                                const index_t origin_i = ref_win["origin/i"].to_index_t();
                                const index_t origin_j = ref_win["origin/j"].to_index_t();

                                temp.reset();

                                if (ref_size_i == 1)
                                {
                                    const index_t icnst = origin_i - i_lo;
                                    const index_t jstart = origin_j - j_lo;
                                    const index_t jend = jstart + ref_size_j;
                                    for (index_t jidx = jstart; jidx < jend; ++jidx)
                                    {
                                        const index_t offset = jidx * niwidth + icnst;
                                        temp.set_external(DataType(fcoords["x"].dtype().id(), 1),
                                                          (void*)fcoords["x"].element_ptr(offset));
                                        xbuffer.push_back(temp.to_double());
                                        temp.set_external(DataType(fcoords["y"].dtype().id(), 1),
                                                          (void*)fcoords["y"].element_ptr(offset));
                                        ybuffer.push_back(temp.to_double());
                                    }
                                }
                                else if (ref_size_j == 1)
                                {
                                    const index_t jcnst = origin_j - j_lo;
                                    const index_t istart = origin_i - i_lo;
                                    const index_t iend = istart + ref_size_i;
                                    for (index_t iidx = istart; iidx < iend; ++iidx)
                                    {
                                        const index_t offset = jcnst * niwidth + iidx;
                                        temp.set_external(DataType(fcoords["x"].dtype().id(), 1),
                                                          (void*)fcoords["x"].element_ptr(offset));
                                        xbuffer.push_back(temp.to_double());
                                        temp.set_external(DataType(fcoords["y"].dtype().id(), 1),
                                                          (void*)fcoords["y"].element_ptr(offset));
                                        ybuffer.push_back(temp.to_double());
                                    }
                                }
                                const index_t nbr_rank = group["rank"].to_index_t();
                                MPI_Send(&xbuffer[0],
                                         xbuffer.size(),
                                         MPI_DOUBLE,
                                         nbr_rank,
                                         domain_id,
                                         MPI_COMM_WORLD);
                                MPI_Send(&ybuffer[0],
                                         ybuffer.size(),
                                         MPI_DOUBLE,
                                         nbr_rank,
                                         domain_id,
                                         MPI_COMM_WORLD);
                            }
                            else if (si == 1 && nbr_size > ref_size)
                            {
                                auto& xbuffer = nbr_to_xbuffer[nbr_id];
                                auto& ybuffer = nbr_to_ybuffer[nbr_id];

                                if (!is_domain_local(n, nbr_name))
                                {
                                    if (nbr_size_i == 1)
                                    {
                                        xbuffer.resize(nbr_size_j);
                                        ybuffer.resize(nbr_size_j);
                                    }
                                    else if (nbr_size_j == 1)
                                    {
                                        xbuffer.resize(nbr_size_i);
                                        ybuffer.resize(nbr_size_i);
                                    }

                                    index_t nbr_rank = group["rank"].to_index_t();
                                    MPI_Recv(&xbuffer[0],
                                             xbuffer.size(),
                                             MPI_DOUBLE,
                                             nbr_rank,
                                             nbr_id,
                                             MPI_COMM_WORLD,
                                             MPI_STATUS_IGNORE);
                                    MPI_Recv(&ybuffer[0],
                                             ybuffer.size(),
                                             MPI_DOUBLE, nbr_rank,
                                             nbr_id, MPI_COMM_WORLD,
                                             MPI_STATUS_IGNORE);

                                }
                                else
                                {
                                    const Node& nbr_dom = n[nbr_name];
                                    const Node& nbr_coords = nbr_dom["coordsets/coords"];

                                    const Node& ntopo = nbr_dom["topologies"][name];
                                    index_t ni_lo = ntopo["elements/origin/i0"].to_index_t();
                                    index_t nj_lo = ntopo["elements/origin/j0"].to_index_t();
                                    index_t nbr_iwidth = ntopo["elements/dims/i"].to_index_t() + 1;

                                    // FIXME(JRC): These arrays aren't guaranteed to be double arrays;
                                    // this code must be changed to accomodate arbitrary types.
                                    const Node& fcoords = nbr_coords["values"];
                                    const double_array& xarray = fcoords["x"].as_double_array();
                                    const double_array& yarray = fcoords["y"].as_double_array();

                                    index_t origin_i = nbr_win["origin/i"].to_index_t();
                                    index_t origin_j = nbr_win["origin/j"].to_index_t();

                                    index_t istart = origin_i - ni_lo;
                                    index_t jstart = origin_j - nj_lo;
                                    index_t iend = istart + nbr_size_i;
                                    index_t jend = jstart + nbr_size_j;
                                    for (index_t jidx = jstart; jidx < jend; ++jidx)
                                    {
                                        index_t joffset = jidx*nbr_iwidth;
                                        for (index_t iidx = istart; iidx < iend; ++iidx)
                                        {
                                            index_t offset = joffset+iidx;
                                            xbuffer.push_back(xarray[offset]);
                                            ybuffer.push_back(yarray[offset]);
                                        }
                                    }
                                }
                            }
                            else if (si == 2 && ref_size < nbr_size)
                            {
                                bputils::connectivity::create_elements_2d(ref_win,
                                                                          i_lo,
                                                                          j_lo,
                                                                          iwidth,
                                                                          poly_elems);

                                const index_t use_ratio = (
                                    (nbr_size_j == 1) ? ratio_i : (
                                    (nbr_size_i == 1) ? ratio_j : (0)));

                                auto& xbuffer = nbr_to_xbuffer[nbr_id];
                                auto& ybuffer = nbr_to_ybuffer[nbr_id];

                                const index_t added = (
                                    (nbr_size_j == 1) ? (xbuffer.size() - ref_size_i) : (
                                    (nbr_size_i == 1) ? (ybuffer.size() - ref_size_j) : (0)));

                                // FIXME(JRC): These arrays aren't guaranteed to be double arrays;
                                // this code must be changed to accomodate arbitrary types.
                                const auto& out_x = out_cset["values"]["x"].as_double_array();
                                const auto& out_y = out_cset["values"]["y"].as_double_array();
                                index_t new_vertex = out_x.number_of_elements();

                                const index_t out_x_size = out_x.number_of_elements();
                                const index_t out_y_size = out_y.number_of_elements();

                                std::vector<double> new_x;
                                std::vector<double> new_y;
                                new_x.reserve(out_x_size + added);
                                new_y.reserve(out_y_size + added);
                                const double* out_x_ptr = static_cast<const double*>(out_x.element_ptr(0));
                                const double* out_y_ptr = static_cast<const double*>(out_y.element_ptr(0));

                                new_x.insert(new_x.end(), out_x_ptr, out_x_ptr + out_x_size);
                                new_y.insert(new_y.end(), out_y_ptr, out_y_ptr + out_y_size);

                                if ((xbuffer.size()-1)%use_ratio)
                                {
                                    new_x.reserve(out_x_size + added*2);
                                }
                                for (index_t ni = 0; ni < (index_t)xbuffer.size(); ++ni)
                                {
                                    if (ni % use_ratio)
                                    {
                                        new_x.push_back(xbuffer[ni]);
                                        new_y.push_back(ybuffer[ni]);
                                    }
                                }

                                out_cset["values"]["x"].set(new_x);
                                out_cset["values"]["y"].set(new_y);

                                bputils::connectivity::connect_elements_2d(ref_win,
                                                                           i_lo,
                                                                           j_lo,
                                                                           iwidth,
                                                                           use_ratio,
                                                                           new_vertex,
                                                                           poly_elems);
                            }
                        }
                    }
                }
            }

            if (si == 2)
            {
                dest_dom["state"].set(dom["state"]);
                dest_dom["topologies"][name]["coordset"].set(dom["topologies"][name]["coordset"]);

                Node& out_topo = dest_dom["topologies"][name];
                out_topo["type"].set("unstructured");
                out_topo["elements/shape"].set("polygonal");

                const index_t elemsize = iwidth*jwidth;

                std::vector<index_t> connect;
                std::vector<index_t> num_vertices;
                std::vector<index_t> elem_offsets;
                index_t offset_sum = 0;
                for (index_t elem = 0; elem < elemsize; ++elem)
                {
                    auto elem_itr = poly_elems.find(elem);
                    if (elem_itr == poly_elems.end())
                    {
                        bputils::connectivity::make_element_2d(connect, elem, iwidth);
                        num_vertices.push_back(4);
                    }
                    else
                    {
                        std::vector<index_t>& poly_elem = elem_itr->second;
                        connect.insert(connect.end(), poly_elem.begin(), poly_elem.end());
                        num_vertices.push_back(poly_elem.size());
                    }
                    elem_offsets.push_back(offset_sum);
                    offset_sum += num_vertices.back();
                }

                // TODO(JRC): Remove extra copy here.
                out_topo["elements/connectivity"].set(connect);
                out_topo["elements/sizes"].set(num_vertices);
                out_topo["elements/offsets"].set(elem_offsets);

                //TODO:  zero copy
                if (dom.has_child("fields"))
                {
                    dest_dom["fields"].set(dom["fields"]);
                }
            }
        }
    }
}


//-----------------------------------------------------------------------------
void match_nbr_elems(PolyBndry& pbnd,
                     std::map<index_t, bputils::connectivity::ElemType> nbr_elems,
                     const Node& ref_topo,
                     const Node& ref_win,
                     const Node& nbr_win,
                     index_t nbr_iwidth, index_t nbr_jwidth,
                     index_t nbr_kwidth,
                     index_t ni_lo, index_t nj_lo, index_t nk_lo,
                     index_t ratio_i, index_t ratio_j, index_t ratio_k,
                     bool local)
{
    index_t nbr_size_i = nbr_win["dims/i"].to_index_t();
    index_t nbr_size_j = nbr_win["dims/j"].to_index_t();
    index_t nbr_size_k = nbr_win["dims/k"].to_index_t();

    index_t ri_lo = ref_topo["elements/origin/i0"].to_index_t();
    index_t rj_lo = ref_topo["elements/origin/j0"].to_index_t();
    index_t rk_lo = ref_topo["elements/origin/k0"].to_index_t();

    index_t ref_iwidth = ref_topo["elements/dims/i"].to_index_t();
    index_t ref_jwidth = ref_topo["elements/dims/j"].to_index_t();

    index_t ref_origin_i = ref_win["origin/i"].to_index_t();
    index_t ref_origin_j = ref_win["origin/j"].to_index_t();
    index_t ref_origin_k = ref_win["origin/k"].to_index_t();

    index_t origin_i = nbr_win["origin/i"].to_index_t();
    index_t origin_j = nbr_win["origin/j"].to_index_t();
    index_t origin_k = nbr_win["origin/k"].to_index_t();

    index_t side = pbnd.side;

    if (side == 0 || side == 1)
    {
        index_t nside = (side+1)%2;     //nbr side counterpart to  ref side
        index_t shift = -(nside%2); //0 on low side, -1 on high side
        index_t icnst = origin_i - ni_lo + shift;
        index_t jstart = origin_j;
        index_t jend = jstart + nbr_size_j - 1;
        index_t kstart = origin_k;
        index_t kend = kstart + nbr_size_k - 1;

        index_t rshift = -(side%2); 
        index_t ricnst = ref_origin_i - ri_lo + rshift;

        for (index_t kidx = kstart; kidx < kend; ++kidx)
        {
            index_t rkidx = kidx/ratio_k;
            index_t nk = kidx - nk_lo;
            index_t rk = rkidx - rk_lo;
            index_t nkoffset = icnst + nk*nbr_iwidth*nbr_jwidth;
            index_t rkoffset = ricnst + rk*ref_iwidth*ref_jwidth;
            for (index_t jidx = jstart; jidx < jend; ++jidx)
            {
                index_t rjidx = jidx/ratio_j;
                index_t nj = jidx - nj_lo;
                index_t rj = rjidx - rj_lo;
                index_t noffset = nkoffset + nj*nbr_iwidth;
                index_t roffset = rkoffset + rj*ref_iwidth;

                pbnd.m_nbr_elems[roffset].push_back(noffset);

                if (local)
                {
                    auto& nbr_elem = nbr_elems[noffset];
                    pbnd.m_nbr_faces[roffset][noffset].m_face_id =
                        nside == 0 ?
                        nbr_elem[0] :
                        nbr_elem[1];
                }
                else
                {
                    index_t jfoffset = nj*(nbr_iwidth+1);
                    index_t kfoffset = nk*nbr_jwidth*(nbr_iwidth+1);
                    if (nside == 0)
                    {
                        pbnd.m_nbr_faces[roffset][noffset].m_face_id =
                            icnst + jfoffset + kfoffset;
                    }
                    else
                    {
                        pbnd.m_nbr_faces[roffset][noffset].m_face_id =
                            icnst + jfoffset + kfoffset + 1;
                    }
                }
            }
        }
    }
    else if (side == 2 || side == 3)
    {
        index_t jface_start = (nbr_iwidth+1)*nbr_jwidth*nbr_kwidth;
        index_t nside = 2 + (side+1)%2;     //nbr side counterpart to  ref side
        index_t shift = -(nside%2); //0 on low side, -1 on high side
        index_t jcnst = origin_j - nj_lo + shift;
        index_t istart = origin_i;
        index_t iend = istart + nbr_size_i - 1;
        index_t kstart = origin_k;
        index_t kend = kstart + nbr_size_k - 1;

        index_t rshift = -(side%2); 
        index_t rjcnst = ref_origin_j - rj_lo + rshift;

        for (index_t kidx = kstart; kidx < kend; ++kidx)
        {
            index_t rkidx = kidx/ratio_k;
            index_t nk = kidx - nk_lo;
            index_t rk = rkidx - rk_lo;
            index_t nkoffset = nk*nbr_iwidth*nbr_jwidth;
            index_t rkoffset = rk*ref_iwidth*ref_jwidth;
            for (index_t iidx = istart; iidx < iend; ++iidx)
            {
                index_t riidx = iidx/ratio_i;
                index_t ni = iidx - ni_lo;
                index_t ri = riidx - ri_lo;
                index_t noffset = nkoffset + jcnst*nbr_iwidth + ni;
                index_t roffset = rkoffset + rjcnst*ref_iwidth + ri;

                pbnd.m_nbr_elems[roffset].push_back(noffset);

                if (local)
                {
                    auto& nbr_elem = nbr_elems[noffset];
                    pbnd.m_nbr_faces[roffset][noffset].m_face_id =
                        nside == 2 ?
                        nbr_elem[2] :
                        nbr_elem[3];
                }
                else
                {
                    index_t ifoffset = ni;
                    index_t kfoffset = nk*(nbr_jwidth+1)*nbr_iwidth;
                    if (nside == 2)
                    {
                        pbnd.m_nbr_faces[roffset][noffset].m_face_id =
                            jface_start +
                            nbr_iwidth*jcnst + ifoffset + kfoffset;
                    }
                    else
                    {
                        pbnd.m_nbr_faces[roffset][noffset].m_face_id =
                            jface_start +
                            nbr_iwidth*(jcnst+1) + ifoffset + kfoffset;
                    }
                }
            }
        }
    }
    else if (side == 4 || side == 5)
    {
        index_t jface_start = (nbr_iwidth+1)*nbr_jwidth*nbr_kwidth;
        index_t kface_start = jface_start +
                              nbr_iwidth*(nbr_jwidth+1)*nbr_kwidth;
        index_t nside = 4 + (side+1)%2;     //nbr side counterpart to  ref side
        index_t shift = -(nside%2); //0 on low side, -1 on high side
        index_t kcnst = origin_k - nk_lo + shift;
        index_t jstart = origin_j;
        index_t jend = jstart + nbr_size_j - 1;
        index_t istart = origin_i;
        index_t iend = istart + nbr_size_i - 1;

        index_t rshift = -(side%2); 
        index_t rkcnst = ref_origin_k - rk_lo + rshift;

        for (index_t jidx = jstart; jidx < jend; ++jidx)
        {
            index_t rjidx = jidx/ratio_j;
            index_t nj = jidx - nj_lo;
            index_t rj = rjidx - rj_lo;
            index_t njoffset = kcnst*nbr_iwidth*nbr_jwidth + nj*nbr_iwidth;
            index_t rjoffset = rkcnst*ref_iwidth*ref_jwidth + rj*ref_iwidth;
            for (index_t iidx = istart; iidx < iend; ++iidx)
            {
                index_t riidx = iidx/ratio_i;
                index_t ni = iidx - ni_lo;
                index_t ri = riidx - ri_lo;
                index_t noffset = njoffset + ni;
                index_t roffset = rjoffset + ri;

                pbnd.m_nbr_elems[roffset].push_back(noffset);

                if (local)
                { 
                    auto& nbr_elem = nbr_elems[noffset];
                    pbnd.m_nbr_faces[roffset][noffset].m_face_id =
                        nside == 4 ?
                        nbr_elem[4] :
                        nbr_elem[5];
                }
                else
                {
                    index_t ifoffset = ni;
                    index_t jfoffset = nj*nbr_iwidth;
                    if (nside == 4)
                    {
                        pbnd.m_nbr_faces[roffset][noffset].m_face_id =
                            kface_start +
                            nbr_iwidth*nbr_jwidth*kcnst + ifoffset + jfoffset;
                    }
                    else
                    {
                        pbnd.m_nbr_faces[roffset][noffset].m_face_id =
                            kface_start +
                            nbr_iwidth*nbr_jwidth*(kcnst+1) + ifoffset + jfoffset;
                    }
                } 
            }
        }
    }
    else
    {
        //not a side
    }

}


//-------------------------------------------------------------------------
void to_polyhedral(const Node &n,
                   Node &dest,
                   const std::string& name)
{
    dest.reset();

    index_t par_rank = relay::mpi::rank(MPI_COMM_WORLD);

    NodeConstIterator itr = n.children();

    std::map<index_t, std::map<index_t, bputils::connectivity::ElemType> > poly_elems_map;
    std::map<index_t, bputils::connectivity::SubelemMap> allfaces_map;

    std::map<index_t, std::vector<index_t> > elem_connect;
    std::map<index_t, std::vector<index_t> > elem_sizes;
    std::map<index_t, std::vector<index_t> > elem_offsets;
    std::map<index_t, std::vector<index_t> > subelem_connect;
    std::map<index_t, std::vector<index_t> > subelem_sizes;
    std::map<index_t, std::vector<index_t> > subelem_offsets;

    while(itr.has_next())
    {
        const Node& chld = itr.next();
        std::string domain_name = itr.name();
        dest[domain_name]["state"] = chld["state"];
        const Node& in_coords = chld["coordsets/coords"];
        const Node& in_topo = chld["topologies"][name];

        index_t iwidth = in_topo["elements/dims/i"].to_index_t();
        index_t jwidth = in_topo["elements/dims/j"].to_index_t();
        index_t kwidth = in_topo["elements/dims/k"].to_index_t();

        Node& out_coords = dest[domain_name]["coordsets/coords"];

        index_t domain_id = chld["state/domain_id"].to_index_t();
        Node& out_values = out_coords["values"];
        if (in_coords["type"].as_string() == "uniform")
        {
            blueprint::mesh::coordset::uniform::to_explicit(in_coords, out_coords);
        }
        else
        {
            out_coords["type"] = in_coords["type"];
            const Node& in_values = in_coords["values"];
            out_values = in_values;
        }

        auto& poly_elems = poly_elems_map[domain_id];
        auto& allfaces = allfaces_map[domain_id];

        index_t elemsize = iwidth*jwidth*kwidth;

        for (index_t elem = 0; elem < elemsize; ++elem)
        {
            bputils::connectivity::make_element_3d(poly_elems[elem],
                                                   elem,
                                                   iwidth,
                                                   jwidth,
                                                   kwidth,
                                                   allfaces);
        }
    }

    itr = n.children();
    while(itr.has_next())
    {
        const Node& chld = itr.next();
        std::string domain_name = itr.name();

        index_t domain_id = chld["state/domain_id"].to_index_t();

        const Node& in_topo = chld["topologies"][name];

        Node in_cset;
        bputils::find_reference_node(in_topo, "coordset", in_cset);

        const index_t i_lo = in_topo["elements/origin/i0"].to_index_t();
        const index_t j_lo = in_topo["elements/origin/j0"].to_index_t();
        const index_t k_lo = in_topo["elements/origin/j0"].to_index_t();
        const index_t niwidth = in_topo["elements/dims/i"].to_index_t() + 1;
        const index_t njwidth = in_topo["elements/dims/j"].to_index_t() + 1;

        const Node* in_parent = chld.parent();

        std::ostringstream win_oss;
        win_oss << "window_" << std::setw(6) << std::setfill('0') << domain_id;
        std::string win_name = win_oss.str();

        if (chld.has_path("adjsets/adjset/groups"))
        {
            const Node& in_groups = chld["adjsets/adjset/groups"];
            NodeConstIterator grp_itr = in_groups.children();
            while(grp_itr.has_next())
            {
                const Node& group = grp_itr.next();
                if (group.has_child("neighbors") && group.has_child("windows"))
                {
                    int64_array neighbors = group["neighbors"].as_int64_array();

                    index_t nbr_id = neighbors[1];
                    const Node& in_windows = group["windows"];
                    std::ostringstream nw_oss;
                    nw_oss << "window_" << std::setw(6)
                            << std::setfill('0') << nbr_id;
                    std::string nbr_win_name = nw_oss.str();

                    const Node& ref_win = in_windows[win_name];
                    const Node& nbr_win = in_windows[nbr_win_name];

                    if (nbr_win["level_id"].to_index_t() < ref_win["level_id"].to_index_t())
                    {
                        index_t ref_size_i = ref_win["dims/i"].to_index_t();
                        index_t ref_size_j = ref_win["dims/j"].to_index_t();
                        index_t ref_size_k = ref_win["dims/k"].to_index_t();
                        index_t ref_size = ref_size_i*ref_size_j*ref_size_k;

                        index_t nbr_size_i = nbr_win["dims/i"].to_index_t();
                        index_t nbr_size_j = nbr_win["dims/j"].to_index_t();
                        index_t nbr_size_k = nbr_win["dims/k"].to_index_t();
                        index_t nbr_size = nbr_size_i*nbr_size_j*nbr_size_k;

                        if (nbr_size < ref_size)
                        {
                            std::ostringstream nbr_oss;
                            nbr_oss << "domain_" << std::setw(6)
                                    << std::setfill('0') << nbr_id;
                            std::string nbr_name = nbr_oss.str();

                            bool is_side = true;
                            if (nbr_size_i * nbr_size_j == 1 ||
                                nbr_size_i * nbr_size_k == 1 ||
                                nbr_size_j * nbr_size_k == 1)
                            {
                                is_side = false;
                            }

                            if (is_side && !in_parent->has_child(nbr_name))
                            {
                                index_t buffer[6];
                                buffer[0] = in_topo["elements/dims/i"].to_index_t();
                                buffer[1] = in_topo["elements/dims/j"].to_index_t();
                                buffer[2] = in_topo["elements/dims/k"].to_index_t();
                                buffer[3] = in_topo["elements/origin/i0"].to_index_t();
                                buffer[4] = in_topo["elements/origin/j0"].to_index_t();
                                buffer[5] = in_topo["elements/origin/k0"].to_index_t();

                                index_t nbr_rank = group["rank"].to_index_t();
                                MPI_Send(buffer,
                                         6,
                                         MPI_INT64_T,
                                         nbr_rank,
                                         domain_id,
                                         MPI_COMM_WORLD);

                                std::vector<index_t> vertices;
                                std::vector<double> xbuffer, ybuffer, zbuffer;
                                const Node &fcoords = in_cset["values"];

                                const index_t origin_i = ref_win["origin/i"].to_index_t();
                                const index_t origin_j = ref_win["origin/j"].to_index_t();
                                const index_t origin_k = ref_win["origin/k"].to_index_t();

                                Node temp;

                                if (ref_size_i == 1)
                                {
                                    const index_t icnst = origin_i - i_lo;
                                    const index_t jstart = origin_j - j_lo;
                                    const index_t jend = jstart + ref_size_j;
                                    const index_t kstart = origin_k - k_lo;
                                    const index_t kend = kstart + ref_size_k;
                                    for (index_t kidx = kstart; kidx < kend; ++kidx)
                                    {
                                        const index_t koffset = kidx * niwidth * njwidth;
                                        for (index_t jidx = jstart; jidx < jend; ++jidx)
                                        {
                                            const index_t offset = koffset + jidx * niwidth + icnst;
                                            vertices.push_back(offset);
                                            temp.set_external(DataType(fcoords["x"].dtype().id(), 1),
                                                              (void*)fcoords["x"].element_ptr(offset));
                                            xbuffer.push_back(temp.to_double());
                                            temp.set_external(DataType(fcoords["y"].dtype().id(), 1),
                                                              (void*)fcoords["y"].element_ptr(offset));
                                            ybuffer.push_back(temp.to_double());
                                            temp.set_external(DataType(fcoords["z"].dtype().id(), 1),
                                                              (void*)fcoords["z"].element_ptr(offset));
                                            zbuffer.push_back(temp.to_double());
                                        }
                                    }
                                }
                                else if (ref_size_j == 1)
                                {
                                    const index_t istart = origin_i - i_lo;
                                    const index_t iend = istart + ref_size_i;
                                    const index_t jcnst = origin_j - j_lo;
                                    const index_t kstart = origin_k - k_lo;
                                    const index_t kend = kstart + ref_size_k;
                                    for (index_t kidx = kstart; kidx < kend; ++kidx)
                                    {
                                        const index_t koffset = kidx * niwidth * njwidth;
                                        for (index_t iidx = istart; iidx < iend; ++iidx)
                                        {
                                            const index_t offset = koffset + jcnst * niwidth + iidx;
                                            vertices.push_back(offset);
                                            temp.set_external(DataType(fcoords["x"].dtype().id(), 1),
                                                              (void*)fcoords["x"].element_ptr(offset));
                                            xbuffer.push_back(temp.to_double());
                                            temp.set_external(DataType(fcoords["y"].dtype().id(), 1),
                                                              (void*)fcoords["y"].element_ptr(offset));
                                            ybuffer.push_back(temp.to_double());
                                            temp.set_external(DataType(fcoords["z"].dtype().id(), 1),
                                                              (void*)fcoords["z"].element_ptr(offset));
                                            zbuffer.push_back(temp.to_double());
                                        }
                                    }
                                }
                                else
                                {
                                    const index_t istart = origin_i - i_lo;
                                    const index_t iend = istart + ref_size_i;
                                    const index_t jstart = origin_j - j_lo;
                                    const index_t jend = jstart + ref_size_j;
                                    const index_t kcnst = origin_k - k_lo;
                                    const index_t koffset = kcnst * niwidth * njwidth;
                                    for (index_t jidx = jstart; jidx < jend; ++jidx)
                                    {
                                        const index_t joffset = jidx * niwidth;
                                        for (index_t iidx = istart; iidx < iend; ++iidx)
                                        {
                                            const index_t offset = koffset + joffset + iidx;
                                            vertices.push_back(offset);
                                            temp.set_external(DataType(fcoords["x"].dtype().id(), 1),
                                                              (void*)fcoords["x"].element_ptr(offset));
                                            xbuffer.push_back(temp.to_double());
                                            temp.set_external(DataType(fcoords["y"].dtype().id(), 1),
                                                              (void*)fcoords["y"].element_ptr(offset));
                                            ybuffer.push_back(temp.to_double());
                                            temp.set_external(DataType(fcoords["z"].dtype().id(), 1),
                                                              (void*)fcoords["z"].element_ptr(offset));
                                            zbuffer.push_back(temp.to_double());
                                        }
                                    }
                                }


                                MPI_Send(&vertices[0],
                                         vertices.size(),
                                         MPI_INT64_T,
                                         nbr_rank,
                                         domain_id,
                                         MPI_COMM_WORLD);
                                MPI_Send(&xbuffer[0],
                                         xbuffer.size(),
                                         MPI_DOUBLE,
                                         nbr_rank,
                                         domain_id,
                                         MPI_COMM_WORLD);
                                MPI_Send(&ybuffer[0],
                                         ybuffer.size(),
                                         MPI_DOUBLE,
                                         nbr_rank,
                                         domain_id,
                                         MPI_COMM_WORLD);
                                MPI_Send(&zbuffer[0],
                                         zbuffer.size(),
                                         MPI_DOUBLE,
                                         nbr_rank,
                                         domain_id,
                                         MPI_COMM_WORLD);


                            }
                        }
                    }
                }
            }
        }
    }

    //Outer map: domain_id, inner map: nbr_id to PolyBndry
    std::map<index_t, std::map<index_t, PolyBndry> > poly_bndry_map;

    itr = n.children();
    while(itr.has_next())
    {
        const Node& chld = itr.next();
        std::string domain_name = itr.name();

        index_t domain_id = chld["state/domain_id"].to_index_t();

        auto& poly_elems = poly_elems_map[domain_id];

        const Node& in_topo = chld["topologies"][name];

        index_t iwidth = in_topo["elements/dims/i"].to_index_t();
        index_t jwidth = in_topo["elements/dims/j"].to_index_t();

        index_t i_lo = in_topo["elements/origin/i0"].to_index_t();
        index_t j_lo = in_topo["elements/origin/j0"].to_index_t();
        index_t k_lo = in_topo["elements/origin/k0"].to_index_t();

        const Node* in_parent = chld.parent();

        std::ostringstream win_oss;
        win_oss << "window_" << std::setw(6) << std::setfill('0') << domain_id;
        std::string win_name = win_oss.str();

        if (chld.has_path("adjsets/adjset/groups"))
        {
            const Node& in_groups = chld["adjsets/adjset/groups"];
            NodeConstIterator grp_itr = in_groups.children();
            while(grp_itr.has_next())
            {
                const Node& group = grp_itr.next();

                if (group.has_child("neighbors") && group.has_child("windows"))
                {
                    int64_array neighbors = group["neighbors"].as_int64_array();

                    index_t nbr_id = neighbors[1];
                    const Node& in_windows = group["windows"];
                    std::ostringstream nw_oss;
                    nw_oss << "window_" << std::setw(6)
                            << std::setfill('0') << nbr_id;
                    std::string nbr_win_name = nw_oss.str();

                    const Node& ref_win = in_windows[win_name];
                    const Node& nbr_win = in_windows[nbr_win_name];
                    if (nbr_win["level_id"].to_index_t() > ref_win["level_id"].to_index_t())
                    {
                        index_t ratio_i = nbr_win["ratio/i"].to_index_t();
                        index_t ratio_j = nbr_win["ratio/j"].to_index_t();
                        index_t ratio_k = nbr_win["ratio/k"].to_index_t();

                        index_t ref_size_i = ref_win["dims/i"].to_index_t();
                        index_t ref_size_j = ref_win["dims/j"].to_index_t();
                        index_t ref_size_k = ref_win["dims/k"].to_index_t();
                        index_t ref_size = ref_size_i*ref_size_j*ref_size_k;

                        index_t nbr_size_i = nbr_win["dims/i"].to_index_t();
                        index_t nbr_size_j = nbr_win["dims/j"].to_index_t();
                        index_t nbr_size_k = nbr_win["dims/k"].to_index_t();
                        index_t nbr_size = nbr_size_i*nbr_size_j*nbr_size_k;

                        if (nbr_size > ref_size)
                        {
                            index_t origin_i = ref_win["origin/i"].to_index_t();
                            index_t origin_j = ref_win["origin/j"].to_index_t();
                            index_t origin_k = ref_win["origin/k"].to_index_t();

                            PolyBndry& pbnd = poly_bndry_map[domain_id][nbr_id];

                            if (ref_size_i == 1 && ref_size_j != 1 &&
                                ref_size_k != 1)
                            {
                                index_t shift = 0;
                                if (origin_i == i_lo)
                                {
                                    pbnd.side = 0;
                                }
                                else
                                {
                                    pbnd.side = 1;
                                    shift = -1;
                                }
                                index_t icnst = origin_i - i_lo + shift;
                                index_t jstart = origin_j - j_lo;
                                index_t jend = jstart + ref_size_j - 1;
                                index_t kstart = origin_k - k_lo;
                                index_t kend = kstart + ref_size_k - 1;
                                for (index_t kidx = kstart; kidx < kend; ++kidx)
                                    {
                                    index_t koffset = icnst + kidx*iwidth*jwidth;
                                    for (index_t jidx = jstart; jidx < jend; ++jidx)
                                    {
                                        index_t offset = koffset + jidx*iwidth;
                                        pbnd.m_elems.push_back(offset);
                                        pbnd.m_bface[offset] =
                                            pbnd.side == 0 ?
                                            poly_elems[offset][0] :
                                            poly_elems[offset][1];
                                    }
                                }
                            }
                            else if (ref_size_j == 1 && ref_size_i != 1 &&
                                     ref_size_k != 1)
                            {
                                index_t shift = 0;
                                if (origin_j == j_lo)
                                {
                                    pbnd.side = 2;
                                }
                                else
                                {
                                    pbnd.side = 3;
                                    shift = -1;
                                }
                                index_t jcnst = origin_j - j_lo + shift;
                                index_t istart = origin_i - i_lo;
                                index_t iend = istart + ref_size_i - 1;
                                index_t kstart = origin_k - k_lo;
                                index_t kend = kstart + ref_size_k - 1;
                                for (index_t kidx = kstart; kidx < kend; ++kidx)
                                {
                                    index_t koffset = jcnst*iwidth + kidx*iwidth*jwidth;
                                    for (index_t iidx = istart; iidx < iend; ++iidx)
                                    {
                                        index_t offset = koffset + iidx;
                                        pbnd.m_elems.push_back(offset);
                                        pbnd.m_bface[offset] =
                                            pbnd.side == 2 ?
                                            poly_elems[offset][2] :
                                            poly_elems[offset][3];
                                    }
                                }
                            }
                            else if (ref_size_k == 1 && ref_size_i != 1 &&
                                     ref_size_j != 1)
                            {
                                index_t shift = 0; 
                                if (origin_k == k_lo)
                                {
                                    pbnd.side = 4;
                                }
                                else
                                {
                                    pbnd.side = 5;
                                    shift = -1;
                                }
                                index_t kcnst = origin_k - k_lo + shift;
                                index_t istart = origin_i - i_lo;
                                index_t iend = istart + ref_size_i - 1;
                                index_t jstart = origin_j - j_lo;
                                index_t jend = jstart + ref_size_j - 1;
                                for (index_t jidx = jstart; jidx < jend; ++jidx)
                                {
                                    index_t joffset = jidx*iwidth + kcnst*iwidth*jwidth;
                                    for (index_t iidx = istart; iidx < iend; ++iidx)
                                    {
                                        index_t offset = joffset + iidx;
                                        pbnd.m_elems.push_back(offset);
                                        pbnd.m_bface[offset] =
                                            pbnd.side == 4 ?
                                            poly_elems[offset][4] :
                                            poly_elems[offset][5];
                                    }
                                }
                            }
                            else
                            {
                                pbnd.side = -1;
                            }


                            std::ostringstream nbr_oss;
                            nbr_oss << "domain_" << std::setw(6)
                                    << std::setfill('0') << nbr_id;
                            std::string nbr_name = nbr_oss.str();

                            if (in_parent->has_child(nbr_name))
                            {
                                const Node& nbr_dom =
                                   (*in_parent)[nbr_name];

                                const Node& ntopo =
                                   nbr_dom["topologies"][name];

                                index_t ni_lo = ntopo["elements/origin/i0"].to_index_t();
                                index_t nj_lo = ntopo["elements/origin/j0"].to_index_t();
                                index_t nk_lo = ntopo["elements/origin/k0"].to_index_t();

                                index_t nbr_iwidth =
                                   ntopo["elements/dims/i"].to_index_t();
                                index_t nbr_jwidth =
                                   ntopo["elements/dims/j"].to_index_t();
                                index_t nbr_kwidth =
                                   ntopo["elements/dims/k"].to_index_t();

                                auto& nbr_elems = poly_elems_map[nbr_id];

                                if (pbnd.side >= 0)
                                {
                                    pbnd.m_nbr_rank = par_rank;
                                    pbnd.m_nbr_id = nbr_id;
                                    match_nbr_elems(pbnd, nbr_elems, in_topo,
                                                    ref_win, nbr_win,
                                                    nbr_iwidth, nbr_jwidth,
                                                    nbr_kwidth,
                                                    ni_lo, nj_lo, nk_lo,
                                                    ratio_i,ratio_j,ratio_k,
                                                    true);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    std::map<index_t, std::map<index_t, std::map<index_t,index_t> > > dom_to_nbr_to_buffidx;
    std::map<index_t, std::map<index_t, std::vector<double> > > dom_to_nbr_to_xbuffer;
    std::map<index_t, std::map<index_t, std::vector<double> > > dom_to_nbr_to_ybuffer;
    std::map<index_t, std::map<index_t, std::vector<double> > > dom_to_nbr_to_zbuffer;

    itr = n.children();
    while(itr.has_next())
    {
        const Node& chld = itr.next();
        std::string domain_name = itr.name();

        index_t domain_id = chld["state/domain_id"].to_index_t();
        const Node& in_topo = chld["topologies"][name];

        std::ostringstream win_oss;
        win_oss << "window_" << std::setw(6) << std::setfill('0') << domain_id;
        std::string win_name = win_oss.str();

        const Node* in_parent = chld.parent();

        auto& nbr_to_buffidx = dom_to_nbr_to_buffidx[domain_id];
        auto& nbr_to_xbuffer = dom_to_nbr_to_xbuffer[domain_id];
        auto& nbr_to_ybuffer = dom_to_nbr_to_ybuffer[domain_id];
        auto& nbr_to_zbuffer = dom_to_nbr_to_zbuffer[domain_id];

        if (chld.has_path("adjsets/adjset/groups"))
        {
            const Node& in_groups = chld["adjsets/adjset/groups"];
            NodeConstIterator grp_itr = in_groups.children();
            while(grp_itr.has_next())
            {
                const Node& group = grp_itr.next();

                if (group.has_child("neighbors") && group.has_child("windows"))
                {
                    int64_array neighbors = group["neighbors"].as_int64_array();

                    index_t nbr_id = neighbors[1];
                    const Node& in_windows = group["windows"];
                    std::ostringstream nw_oss;
                    nw_oss << "window_" << std::setw(6)
                            << std::setfill('0') << nbr_id;
                    std::string nbr_win_name = nw_oss.str();

                    const Node& ref_win = in_windows[win_name];
                    const Node& nbr_win = in_windows[nbr_win_name];
                    if (nbr_win["level_id"].to_index_t() > ref_win["level_id"].to_index_t())
                    {
                        index_t ratio_i = nbr_win["ratio/i"].to_index_t();
                        index_t ratio_j = nbr_win["ratio/j"].to_index_t();
                        index_t ratio_k = nbr_win["ratio/k"].to_index_t();

                        index_t ref_size_i = ref_win["dims/i"].to_index_t();
                        index_t ref_size_j = ref_win["dims/j"].to_index_t();
                        index_t ref_size_k = ref_win["dims/k"].to_index_t();
                        index_t ref_size = ref_size_i*ref_size_j*ref_size_k;

                        index_t nbr_size_i = nbr_win["dims/i"].to_index_t();
                        index_t nbr_size_j = nbr_win["dims/j"].to_index_t();
                        index_t nbr_size_k = nbr_win["dims/k"].to_index_t();
                        index_t nbr_size = nbr_size_i*nbr_size_j*nbr_size_k;

                        if (nbr_size > ref_size)
                        {
                            std::vector<index_t> vertices;
                            auto& buffidx = nbr_to_buffidx[nbr_id];
                            auto& xbuffer = nbr_to_xbuffer[nbr_id];
                            auto& ybuffer = nbr_to_ybuffer[nbr_id];
                            auto& zbuffer = nbr_to_zbuffer[nbr_id];

                            std::ostringstream nbr_oss;
                            nbr_oss << "domain_" << std::setw(6)
                                    << std::setfill('0') << nbr_id;
                            std::string nbr_name = nbr_oss.str();

                            if (!in_parent->has_child(nbr_name))
                            {
                                PolyBndry& pbnd = poly_bndry_map[domain_id][nbr_id];

                                if (pbnd.side >= 0)
                                {
                                    index_t nbr_rank = group["rank"].to_index_t();
                                    index_t buffer[6];

                                    MPI_Recv(buffer,
                                             6,
                                             MPI_INT64_T,
                                             nbr_rank,
                                             nbr_id,
                                             MPI_COMM_WORLD,
                                             MPI_STATUS_IGNORE);

                                    index_t nbr_iwidth = buffer[0];
                                    index_t nbr_jwidth = buffer[1];
                                    index_t nbr_kwidth = buffer[2];
                                    index_t ni_lo = buffer[3];
                                    index_t nj_lo = buffer[4];
                                    index_t nk_lo = buffer[5];

                                    auto& nbr_elems = poly_elems_map[nbr_id];

                                    pbnd.m_nbr_rank = nbr_rank;
                                    pbnd.m_nbr_id = nbr_id;
                                    match_nbr_elems(pbnd, nbr_elems, in_topo,
                                                    ref_win, nbr_win,
                                                    nbr_iwidth, nbr_jwidth,
                                                    nbr_kwidth,
                                                    ni_lo, nj_lo, nk_lo,
                                                    ratio_i,ratio_j,ratio_k,
                                                    false);
                                    if (nbr_size_i == 1)
                                    {
                                        vertices.resize(nbr_size_j*nbr_size_k);
                                        xbuffer.resize(nbr_size_j*nbr_size_k);
                                        ybuffer.resize(nbr_size_j*nbr_size_k);
                                        zbuffer.resize(nbr_size_j*nbr_size_k);
                                    }
                                    else if (nbr_size_j == 1)
                                    {
                                        vertices.resize(nbr_size_i*nbr_size_k);
                                        xbuffer.resize(nbr_size_i*nbr_size_k);
                                        ybuffer.resize(nbr_size_i*nbr_size_k);
                                        zbuffer.resize(nbr_size_i*nbr_size_k);
                                    }
                                    else
                                    {
                                        vertices.resize(nbr_size_i*nbr_size_j);
                                        xbuffer.resize(nbr_size_i*nbr_size_j);
                                        ybuffer.resize(nbr_size_i*nbr_size_j);
                                        zbuffer.resize(nbr_size_i*nbr_size_j);
                                    }
    
                                    MPI_Recv(&vertices[0],
                                             vertices.size(),
                                             MPI_INT64_T,
                                             nbr_rank,
                                             nbr_id,
                                             MPI_COMM_WORLD,
                                             MPI_STATUS_IGNORE);
                                    MPI_Recv(&xbuffer[0],
                                             xbuffer.size(),
                                             MPI_DOUBLE,
                                             nbr_rank,
                                             nbr_id,
                                             MPI_COMM_WORLD,
                                             MPI_STATUS_IGNORE);
                                    MPI_Recv(&ybuffer[0],
                                             ybuffer.size(),
                                             MPI_DOUBLE,
                                             nbr_rank,
                                             nbr_id,
                                             MPI_COMM_WORLD,
                                             MPI_STATUS_IGNORE);
                                    MPI_Recv(&zbuffer[0],
                                             zbuffer.size(),
                                             MPI_DOUBLE,
                                             nbr_rank,
                                             nbr_id,
                                             MPI_COMM_WORLD,
                                             MPI_STATUS_IGNORE);
                                    index_t v = 0;
                                    for (auto vitr = vertices.begin();
                                         vitr != vertices.end(); ++vitr)
                                    {
                                        buffidx[*vitr] = v;
                                        ++v;
                                    }
                                }
                            }
                            else
                            {
                                const Node& nbr_dom =
                                   (*in_parent)[nbr_name];
                                const Node& nbr_coords =
                                   nbr_dom["coordsets/coords"];

                                const Node& ntopo =
                                   nbr_dom["topologies"][name];
                                index_t ni_lo =
                                   ntopo["elements/origin/i0"].to_index_t();
                                index_t nj_lo =
                                   ntopo["elements/origin/j0"].to_index_t();
                                index_t nk_lo =
                                   ntopo["elements/origin/k0"].to_index_t();
                                index_t nbr_iwidth =
                                   ntopo["elements/dims/i"].to_index_t() + 1;
                                index_t nbr_jwidth =
                                   ntopo["elements/dims/j"].to_index_t() + 1;

                                const Node& fcoords =
                                   nbr_coords["values"];
                                const double_array& xarray =
                                   fcoords["x"].as_double_array();
                                const double_array& yarray =
                                   fcoords["y"].as_double_array();
                                const double_array& zarray =
                                   fcoords["z"].as_double_array();

                                index_t origin_i = nbr_win["origin/i"].to_index_t();
                                index_t origin_j = nbr_win["origin/j"].to_index_t();
                                index_t origin_k = nbr_win["origin/k"].to_index_t();

                                index_t istart = origin_i - ni_lo;
                                index_t jstart = origin_j - nj_lo;
                                index_t kstart = origin_k - nk_lo;
                                index_t iend = istart + nbr_size_i;
                                index_t jend = jstart + nbr_size_j;
                                index_t kend = kstart + nbr_size_k;
                                for (index_t kidx = kstart; kidx < kend; ++kidx)
                                {
                                    index_t koffset = kidx*nbr_iwidth*nbr_jwidth;
                                    for (index_t jidx = jstart; jidx < jend; ++jidx)
                                    {
                                        index_t joffset = jidx*nbr_iwidth;
                                        for (index_t iidx = istart; iidx < iend; ++iidx)
                                        {
                                            index_t offset = koffset+joffset+iidx;
                                            buffidx[offset] = xbuffer.size();
                                            xbuffer.push_back(xarray[offset]);
                                            ybuffer.push_back(yarray[offset]);
                                            zbuffer.push_back(zarray[offset]);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    std::map<index_t, std::map<index_t, std::map<index_t,index_t> > > dom_to_nbr_to_sharedmap;


    itr = n.children();
    while(itr.has_next())
    {
        const Node& chld = itr.next();
        std::string domain_name = itr.name();

        index_t domain_id = chld["state/domain_id"].to_index_t();

        const Node& dom_coords = chld["coordsets/coords"];

        const double_array& ref_xarray =
           dom_coords["values/x"].as_double_array();
        const double_array& ref_yarray =
           dom_coords["values/y"].as_double_array();
        const double_array& ref_zarray =
           dom_coords["values/z"].as_double_array();

        Node& out_coords = dest[domain_name]["coordsets/coords"];

        std::vector<double> out_xvec;
        std::vector<double> out_yvec;
        std::vector<double> out_zvec;
        if (dom_coords["type"].as_string() == "uniform")
        {
            Node tmp_coords;
            blueprint::mesh::coordset::uniform::to_explicit(dom_coords, tmp_coords);
            const Node& tmp_x = tmp_coords["values/x"];
            const Node& tmp_y = tmp_coords["values/y"];
            const Node& tmp_z = tmp_coords["values/z"];
            const double* tmp_x_ptr = static_cast< const double*>(tmp_x.data_ptr());
            const double* tmp_y_ptr = static_cast< const double*>(tmp_y.data_ptr());
            const double* tmp_z_ptr = static_cast< const double*>(tmp_z.data_ptr());
            index_t num_coords = tmp_x.dtype().number_of_elements();
            out_xvec.insert(out_xvec.end(), tmp_x_ptr, tmp_x_ptr + num_coords);
            out_yvec.insert(out_yvec.end(), tmp_y_ptr, tmp_y_ptr + num_coords);
            out_zvec.insert(out_zvec.end(), tmp_z_ptr, tmp_z_ptr + num_coords);
            out_coords["type"] = "explicit"; 
        }
        else
        {
            out_coords["type"] = dom_coords["type"];
            const double* tmp_x_ptr = static_cast< const double*>(ref_xarray.data_ptr());
            const double* tmp_y_ptr = static_cast< const double*>(ref_yarray.data_ptr());
            const double* tmp_z_ptr = static_cast< const double*>(ref_zarray.data_ptr());

            index_t num_coords = ref_xarray.dtype().number_of_elements();
            out_xvec.insert(out_xvec.end(), tmp_x_ptr, tmp_x_ptr + num_coords);
            out_yvec.insert(out_yvec.end(), tmp_y_ptr, tmp_y_ptr + num_coords);
            out_zvec.insert(out_zvec.end(), tmp_z_ptr, tmp_z_ptr + num_coords);
        }


        auto& poly_elems = poly_elems_map[domain_id];
        auto& allfaces = allfaces_map[domain_id];

        auto& nbr_to_sharedmap = dom_to_nbr_to_sharedmap[domain_id];
        auto& nbr_to_buffidx = dom_to_nbr_to_buffidx[domain_id];
        auto& nbr_to_xbuffer = dom_to_nbr_to_xbuffer[domain_id];
        auto& nbr_to_ybuffer = dom_to_nbr_to_ybuffer[domain_id];
        auto& nbr_to_zbuffer = dom_to_nbr_to_zbuffer[domain_id];

        if (poly_bndry_map.find(domain_id) != poly_bndry_map.end())
        {
            //std::map<index_t, PolyBndry>
            auto& bndries = poly_bndry_map[domain_id];
            for (auto bitr = bndries.begin(); bitr != bndries.end(); ++bitr)
            {
                //One PolyBndry for each fine neighbor
                PolyBndry& pbnd = bitr->second;
                index_t nbr_id = pbnd.m_nbr_id; //domain id of nbr.
                auto& nbr_faces = allfaces_map[nbr_id];

                auto& sharedmap = nbr_to_sharedmap[nbr_id];
                auto& buffidx = nbr_to_buffidx[nbr_id];
                auto& xbuffer = nbr_to_xbuffer[nbr_id];
                auto& ybuffer = nbr_to_ybuffer[nbr_id];
                auto& zbuffer = nbr_to_zbuffer[nbr_id];

                std::map<index_t, std::map<index_t, std::vector<index_t> > >fv_map;

                auto& nbr_elems = pbnd.m_nbr_elems;
                for (auto eitr = nbr_elems.begin(); eitr != nbr_elems.end(); ++eitr) 
                {
                    //offset for a single domain elem
                    index_t ref_offset = eitr->first;

                    //Holds neighbor elem offsets
                    std::vector<index_t>& nbrs = eitr->second;
                    size_t num_nbrs = nbrs.size();
                    for (size_t n = 0; n < num_nbrs; ++n)
                    {
                        index_t nbr_elem = nbrs[n];
                        //nbr_face is subelem offset for the face of
                        //nbr_elem touching the boundary
                        index_t nbr_face = pbnd.m_nbr_faces[ref_offset][nbr_elem].m_face_id;
                        //face_subelem holds the vertices for the nbr_face
                        auto& face_subelem = nbr_faces[nbr_face];
                        //subelem at ref/nbr interface
                        fv_map[ref_offset][nbr_face] = face_subelem;
                    }
                }

                //Above:: fv_map maps a coarse subelem to a vector of its
                //fine subelems;
                //
                for (auto fitr = fv_map.begin(); fitr != fv_map.end(); ++fitr)
                {
                    index_t ref_offset = fitr->first;
                    bputils::connectivity::ElemType& ref_elem = poly_elems[ref_offset];
                    index_t ref_face = ref_elem[pbnd.side];
                    std::vector<index_t>& ref_subelem = allfaces[ref_face];

                    auto nbrs = fitr->second; 
                    std::set<index_t> sh_verts;
                    std::set<index_t> others;

                    // This loop causes sh_verts to be filled with the
                    // vertices that only exist once in the neighbor
                    // subelems.  Those vertices are the once shared by
                    // the reference subelem.
                    for (auto nitr = nbrs.begin(); nitr != nbrs.end(); ++nitr)
                    {
                        index_t nbr_face = nitr->first;
                        std::vector<index_t>& nbr_subelem = nbr_faces[nbr_face];
                        for (auto nv = nbr_subelem.begin(); nv != nbr_subelem.end(); ++nv)
                        {
                            index_t nvert = *nv;
                            if (sh_verts.find(nvert) == sh_verts.end())
                            {
                                if (others.find(nvert) == others.end())
                                {
                                    sh_verts.insert(nvert);
                                }
                            }
                            else
                            {
                                sh_verts.erase(nvert);
                                others.insert(nvert);
                            }
                        }
                    }

                    // Determine which reference vertex is shared with
                    // each neighbor vertex.
                    for (auto rv = ref_subelem.begin(); rv != ref_subelem.end(); ++rv)
                    {
                        double min_dist_sqr = std::numeric_limits<double>::max();
                        index_t shared_vert = -1;
                        for (auto sv = sh_verts.begin(); sv != sh_verts.end();
                             ++sv)
                        {
                            index_t nvert = *sv; 

                            index_t buf_offset = buffidx[nvert];
                            double nbr_x = xbuffer[buf_offset];
                            double nbr_y = ybuffer[buf_offset];
                            double nbr_z = zbuffer[buf_offset];

                            double xdiff = nbr_x-ref_xarray[*rv];
                            double ydiff = nbr_y-ref_yarray[*rv];
                            double zdiff = nbr_z-ref_zarray[*rv];

                            double dist_sqr = xdiff*xdiff + ydiff*ydiff + 
                                              zdiff*zdiff;

                            if (dist_sqr < min_dist_sqr)
                            {
                                min_dist_sqr = dist_sqr;
                                shared_vert = nvert;
                            }
                        }

                        sharedmap[shared_vert] = *rv;
                    }
                }

                index_t num_vertices = out_xvec.size();

                //For a 2x refinement ratio, nbr_subelems provide 5 new
                //vertices in addition to the 4 vertices that are coincident
                //with the coarse element.
                for (auto eitr = nbr_elems.begin(); eitr != nbr_elems.end(); ++eitr)
                {
                    index_t ref_offset = eitr->first;
                    bputils::connectivity::ElemType& ref_elem = poly_elems[ref_offset];
                    index_t ref_face = ref_elem[pbnd.side];

                    auto& nbr_subelems = fv_map[ref_offset];

                    index_t last_added_face = allfaces.rbegin()->first;
                    for (auto nb = nbr_subelems.begin();
                         nb != nbr_subelems.end(); ++nb)
                    {
                        //Here should be code to create each new face
                        std::vector<index_t> new_face;
                        auto& n_subelem = nb->second;

                        for (unsigned int v = 0; v < n_subelem.size(); ++v)
                        {
                            index_t n_vtx = n_subelem[v];
                            if (sharedmap.find(n_vtx) == sharedmap.end())
                            {
                                new_face.push_back(num_vertices);
                                sharedmap[n_vtx] = num_vertices;
                                ++num_vertices;

                                index_t buf_offset = buffidx[n_vtx];
                                out_xvec.push_back(xbuffer[buf_offset]); 
                                out_yvec.push_back(ybuffer[buf_offset]); 
                                out_zvec.push_back(zbuffer[buf_offset]); 
                            }
                            else
                            {
                                new_face.push_back(sharedmap[n_vtx]);
                            }
                        }
                        ++last_added_face;
                        allfaces[last_added_face] = new_face;
                        ref_elem.push_back(last_added_face);
                    }

                    allfaces[ref_face] = allfaces[last_added_face];
                    allfaces.erase(last_added_face);
                    ref_elem.pop_back();
                }
            }
        }

// above--go through other faces on ref_elem to add new edge nodes

        out_coords["values/x"].set(out_xvec);
        out_coords["values/y"].set(out_yvec);
        out_coords["values/z"].set(out_zvec);
    }

    itr = n.children();
    while(itr.has_next())
    {
        const Node& chld = itr.next();
        std::string domain_name = itr.name();

        index_t domain_id = chld["state/domain_id"].to_index_t();

        const Node& in_topo = chld["topologies"][name];

        index_t iwidth = in_topo["elements/dims/i"].to_index_t();
        index_t jwidth = in_topo["elements/dims/j"].to_index_t();
        index_t kwidth = in_topo["elements/dims/k"].to_index_t();

        auto& poly_elems = poly_elems_map[domain_id];
        auto& allfaces = allfaces_map[domain_id];

        index_t elemsize = iwidth*jwidth*kwidth;

        std::vector<index_t>& e_connect = elem_connect[domain_id];
        std::vector<index_t>& e_sizes = elem_sizes[domain_id];
        std::vector<index_t>& e_offsets = elem_offsets[domain_id];
        std::vector<index_t>& sub_connect = subelem_connect[domain_id];
        std::vector<index_t>& sub_sizes = subelem_sizes[domain_id];
        std::vector<index_t>& sub_offsets = subelem_offsets[domain_id];
        index_t elem_offset_sum = 0;
        index_t subelem_offset_sum = 0;
        for (index_t elem = 0; elem < elemsize; ++elem)
        {
            auto& poly_elem = poly_elems[elem];
            e_connect.insert(e_connect.end(), poly_elem.begin(), poly_elem.end());
            e_sizes.push_back(poly_elem.size());
            e_offsets.push_back(elem_offset_sum);
            elem_offset_sum += e_sizes.back();
        }
        for (auto if_itr = allfaces.begin(); if_itr != allfaces.end(); ++if_itr)
        {
            auto& if_elem = if_itr->second;
            sub_connect.insert(sub_connect.end(), if_elem.begin(), if_elem.end());
            sub_sizes.push_back(if_elem.size());
            sub_offsets.push_back(subelem_offset_sum);
            subelem_offset_sum += sub_sizes.back();
        }
    }

    itr = n.children();
    while(itr.has_next())
    {
        const Node& chld = itr.next();
        std::string domain_name = itr.name();

        index_t domain_id = chld["state/domain_id"].to_index_t();

        std::vector<index_t>& e_connect = elem_connect[domain_id];
        std::vector<index_t>& e_sizes = elem_sizes[domain_id];
        std::vector<index_t>& e_offsets = elem_offsets[domain_id];
        std::vector<index_t>& sub_connect = subelem_connect[domain_id];
        std::vector<index_t>& sub_sizes = subelem_sizes[domain_id];
        std::vector<index_t>& sub_offsets = subelem_offsets[domain_id];

        Node& topo = dest[domain_name]["topologies"][name];
        const Node& in_topo = chld["topologies"][name];

        topo["coordset"] = in_topo["coordset"];

        topo["type"] = "unstructured";
        topo["elements/shape"] = "polyhedral";
        topo["elements/shape"].set_string("polyhedral");
        topo["elements/connectivity"].set(e_connect);
        topo["elements/sizes"].set(e_sizes);
        topo["elements/offsets"].set(e_offsets);
        topo["subelements/shape"].set_string("polygonal");
        topo["subelements/connectivity"].set(sub_connect);
        topo["subelements/sizes"].set(sub_sizes);
        topo["subelements/offsets"].set(sub_offsets);

    }
}

//-----------------------------------------------------------------------------
}
//-----------------------------------------------------------------------------
// -- end conduit::blueprint::mpi::mesh --
//-----------------------------------------------------------------------------


}
//-----------------------------------------------------------------------------
// -- end conduit::blueprint::mpi --
//-----------------------------------------------------------------------------


}
//-----------------------------------------------------------------------------
// -- end conduit::blueprint --
//-----------------------------------------------------------------------------

}
//-----------------------------------------------------------------------------
// -- end conduit:: --
//-----------------------------------------------------------------------------

