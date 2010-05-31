// Copyright (C) 2007-2010 Anders Logg and Garth N. Wells.
// Licensed under the GNU LGPL Version 2.1.
//
// Modified by Martin Alnes, 2008
// Modified by Kent-Andre Mardal, 2009
// Modified by Ola Skavhaug, 2009
// Modified by Niclas Jansson, 2009
//
// First added:  2007-03-01
// Last changed: 2010-05-20

#include <dolfin/common/Set.h>
#include <dolfin/common/Timer.h>
#include <dolfin/log/LogStream.h>
#include <dolfin/mesh/MeshPartitioning.h>
#include <dolfin/common/NoDeleter.h>
#include <dolfin/common/types.h>
#include <dolfin/mesh/Cell.h>
#include <dolfin/mesh/MeshData.h>
#include "UFC.h"
#include "UFCCell.h"
#include "UFCDofMap.h"

#include <dolfin/mesh/BoundaryMesh.h>

using namespace dolfin;

//-----------------------------------------------------------------------------
UFCDofMap::UFCDofMap(boost::shared_ptr<ufc::dof_map> ufc_dofmap,
                     Mesh& dolfin_mesh)
                   : _ufc_dofmap(ufc_dofmap), _ufc_offset(0),
                     _parallel(MPI::num_processes() > 1)
{
  assert(_ufc_dofmap);

  // Generate and number all mesh entities
  for (uint d = 1; d <= dolfin_mesh.topology().dim(); ++d)
  {
    if (_ufc_dofmap->needs_mesh_entities(d) ||
    	(_parallel && d == (dolfin_mesh.topology().dim() - 1)))
    {
      dolfin_mesh.init(d);
      if (_parallel)
        MeshPartitioning::number_entities(dolfin_mesh, d);
    }
  }

  // Initialize
  init(dolfin_mesh);
}
//-----------------------------------------------------------------------------
UFCDofMap::UFCDofMap(boost::shared_ptr<ufc::dof_map> ufc_dofmap,
                     const Mesh& dolfin_mesh)
                   : _ufc_dofmap(ufc_dofmap), _ufc_offset(0),
                    _parallel(MPI::num_processes() > 1)
{
  assert(_ufc_dofmap);

  // Initialize
  init(dolfin_mesh);
}
//-----------------------------------------------------------------------------
UFCDofMap::~UFCDofMap()
{
  // Do nothing
}
//-----------------------------------------------------------------------------
void UFCDofMap::tabulate_dofs(uint* dofs, const ufc::cell& ufc_cell,
                              uint cell_index) const
{
  assert(_ufc_dofmap);

  // Tabulate UFC dof map
  _ufc_dofmap->tabulate_dofs(dofs, _ufc_mesh, ufc_cell);

  // Add offset if necessary
  if (_ufc_offset > 0)
  {
    const uint local_dim = local_dimension(ufc_cell);
    for (uint i = 0; i < local_dim; i++)
      dofs[i] += _ufc_offset;
  }
}
//-----------------------------------------------------------------------------
void UFCDofMap::tabulate_dofs(uint* dofs, const Cell& cell) const
{
  UFCCell ufc_cell(cell);
  tabulate_dofs(dofs, ufc_cell, cell.index());
}
//-----------------------------------------------------------------------------
void UFCDofMap::tabulate_facet_dofs(uint* dofs, uint local_facet) const
{
  assert(_ufc_dofmap);
  _ufc_dofmap->tabulate_facet_dofs(dofs, local_facet);
}
//-----------------------------------------------------------------------------
void UFCDofMap::tabulate_coordinates(double** coordinates, const Cell& cell) const
{
  UFCCell ufc_cell(cell);
  tabulate_coordinates(coordinates, ufc_cell);
}
//-----------------------------------------------------------------------------
UFCDofMap* UFCDofMap::extract_sub_dofmap(const std::vector<uint>& component,
                                         const Mesh& dolfin_mesh) const
{
  // Reset offset
  uint ufc_offset = 0;

  // Recursively extract UFC sub dofmap
  assert(_ufc_dofmap);
  boost::shared_ptr<ufc::dof_map>
    ufc_sub_dof_map(extract_sub_dofmap(*_ufc_dofmap, ufc_offset, component, _ufc_mesh, dolfin_mesh));
  info(DBG, "Extracted dof map for sub system: %s", ufc_sub_dof_map->signature());
  info(DBG, "Offset for sub system: %d", ufc_offset);

  // Create dofmap
  UFCDofMap* sub_dofmap = new UFCDofMap(ufc_sub_dof_map, dolfin_mesh);

  // Set offset
  sub_dofmap->_ufc_offset = ufc_offset;

  return sub_dofmap;
}
//-----------------------------------------------------------------------------
UFCDofMap* UFCDofMap::collapse(std::map<uint, uint>& collapsed_map,
                               const Mesh& dolfin_mesh) const
{
  // Create a new DofMap
  assert(_ufc_dofmap);
  UFCDofMap* collapsed_dof_map = new UFCDofMap(_ufc_dofmap, dolfin_mesh);
  assert(collapsed_dof_map->global_dimension() == this->global_dimension());

  // Clear map
  collapsed_map.clear();

  // Build map from collapsed to original dofs
  UFCCell ufc_cell(dolfin_mesh);
  std::vector<uint> dofs(this->max_local_dimension());
  std::vector<uint> collapsed_dofs(collapsed_dof_map->max_local_dimension());
  for (CellIterator cell(dolfin_mesh); !cell.end(); ++cell)
  {
    // Update to current cell
    ufc_cell.update(*cell);

   // Tabulate dofs
   this->tabulate_dofs(&dofs[0], ufc_cell, cell->index());
   collapsed_dof_map->tabulate_dofs(&collapsed_dofs[0], ufc_cell, cell->index());

    // Add to map
    for (uint i = 0; i < collapsed_dof_map->local_dimension(ufc_cell); ++i)
      collapsed_map[collapsed_dofs[i]] = dofs[i];
  }
  return collapsed_dof_map;
}
//-----------------------------------------------------------------------------
ufc::dof_map* UFCDofMap::extract_sub_dofmap(const ufc::dof_map& ufc_dofmap,
                                            uint& offset,
                                            const std::vector<uint>& component,
                                            const ufc::mesh ufc_mesh,
                                            const Mesh& dolfin_mesh)
{
  // Check if there are any sub systems
  if (ufc_dofmap.num_sub_dof_maps() == 0)
    error("Unable to extract sub system (there are no sub systems).");

  // Check that a sub system has been specified
  if (component.size() == 0)
    error("Unable to extract sub system (no sub system specified).");

  // Check the number of available sub systems
  if (component[0] >= ufc_dofmap.num_sub_dof_maps())
    error("Unable to extract sub system %d (only %d sub systems defined).",
                  component[0], ufc_dofmap.num_sub_dof_maps());

  // Add to offset if necessary
  for (uint i = 0; i < component[0]; i++)
  {
    // Extract sub dofmap
    boost::shared_ptr<ufc::dof_map> _ufc_dofmap(ufc_dofmap.create_sub_dof_map(i));

    // Initialise
    init_ufc_dofmap(*_ufc_dofmap, ufc_mesh, dolfin_mesh);

    // Get offset
    assert(_ufc_dofmap);
    offset += _ufc_dofmap->global_dimension();
  }

  // Create UFC sub system
  ufc::dof_map* sub_dof_map = ufc_dofmap.create_sub_dof_map(component[0]);

  // Return sub system if sub sub system should not be extracted
  if (component.size() == 1)
    return sub_dof_map;

  // Otherwise, recursively extract the sub sub system
  std::vector<uint> sub_component;
  for (uint i = 1; i < component.size(); i++)
    sub_component.push_back(component[i]);
  ufc::dof_map* sub_sub_dof_map = extract_sub_dofmap(*sub_dof_map, offset,
                                                     sub_component, ufc_mesh,
                                                     dolfin_mesh);
  delete sub_dof_map;

  return sub_sub_dof_map;
}
//-----------------------------------------------------------------------------
void UFCDofMap::init(const Mesh& dolfin_mesh)
{
  // Start timer for dofmap initialization
  Timer t0("Init dofmap");

  // Initialize the UFC mesh
  init_ufc_mesh(_ufc_mesh, dolfin_mesh);

  // Initialize the UFC dofmap
  assert(_ufc_dofmap);
  init_ufc_dofmap(*_ufc_dofmap, _ufc_mesh, dolfin_mesh);
}
//-----------------------------------------------------------------------------
void UFCDofMap::init_ufc_mesh(UFCMesh& ufc_mesh, const Mesh& dolfin_mesh)
{
  // Check that mesh has been ordered
  if (!dolfin_mesh.ordered())
     error("Mesh is not ordered according to the UFC numbering convention, consider calling mesh.order().");

  // Initialize UFC mesh data (must be done after entities are created)
  ufc_mesh.init(dolfin_mesh);
}
//-----------------------------------------------------------------------------
void UFCDofMap::init_ufc_dofmap(ufc::dof_map& dofmap,
                             const ufc::mesh ufc_mesh,
                             const Mesh& dolfin_mesh)
{
  // Check that we have all mesh entities
  for (uint d = 0; d <= dolfin_mesh.topology().dim(); ++d)
  {
    if (dofmap.needs_mesh_entities(d) && dolfin_mesh.num_entities(d) == 0)
      error("Unable to create function space, missing entities of dimension %d. Try calling mesh.init(%d).", d, d);
  }

  // Initialize UFC dof map
  const bool init_cells = dofmap.init_mesh(ufc_mesh);
  if (init_cells)
  {
    UFCCell ufc_cell(dolfin_mesh);
    for (CellIterator cell(dolfin_mesh); !cell.end(); ++cell)
    {
      ufc_cell.update(*cell);
      dofmap.init_cell(ufc_mesh, ufc_cell);
    }
    dofmap.init_cell_finalize();
  }
}
//-----------------------------------------------------------------------------
dolfin::Set<dolfin::uint> UFCDofMap::dofs(const Mesh& mesh, bool sort) const
{
  dolfin::Set<uint> dof_list;

  UFCCell ufc_cell(mesh);
  std::vector<uint> dofs(max_local_dimension());
  for (CellIterator cell(mesh); !cell.end(); ++cell)
  {
    // Update to current cell
    ufc_cell.update(*cell);

    // Tabulate dofs and insert int Set
    tabulate_dofs(&dofs[0], ufc_cell, cell->index());

    for (uint i = 0; i < local_dimension(ufc_cell); ++i)
      dof_list.insert(dofs[i]);
  }

  if(sort)
    dof_list.sort();

  return dof_list;
}
//-----------------------------------------------------------------------------
std::string UFCDofMap::str(bool verbose) const
{
  assert(_ufc_dofmap);

  std::stringstream s;
  if (verbose)
  {
    s << str(false) << std::endl << std::endl;
    s << "  Signature:               " << _ufc_dofmap->signature() << std::endl;
    s << "  Global dimension:        " << _ufc_dofmap->global_dimension() << std::endl;
    s << "  Maximum local dimension: " << _ufc_dofmap->max_local_dimension() << std::endl;
    s << "  Geometric dimension:     " << _ufc_dofmap->geometric_dimension() << std::endl;
    s << "  Number of sub dofmaps:   " << _ufc_dofmap->num_sub_dof_maps() << std::endl;
    s << "  Number of facet dofs:    " << _ufc_dofmap->num_facet_dofs() << std::endl;
    s << std::endl;
    s << "To print the entire dofmap, call FunctionSpace::print_dofmap.";
  }
  else
    s << "<DofMap of global dimension " << global_dimension() << ">";

  return s.str();
}
//-----------------------------------------------------------------------------