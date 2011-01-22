// Copyright (C) 2008 Kent-Andre Mardal.
// Licensed under the GNU LGPL Version 2.1.
//
// Modified by Anders Logg, 2008.
// Modified by Garth N. Wells, 2011.
//
// First added:  2008-08-25
// Last changed: 2011-01-13

#include <iostream>
#include <boost/scoped_ptr.hpp>

#include <dolfin/common/NoDeleter.h>
#include "dolfin/common/utils.h"
#include "BlockVector.h"
#include "DefaultFactory.h"
#include "GenericVector.h"
#include "Matrix.h"
#include "BlockMatrix.h"

using namespace dolfin;

//-----------------------------------------------------------------------------
BlockMatrix::BlockMatrix(uint m, uint n)
    : matrices(m, std::vector<boost::shared_ptr<GenericMatrix> >(n))
{
  for (uint i = 0; i < m; i++)
    for (uint j = 0; j < n; j++)
      matrices[i][j].reset(new Matrix());
}
//-----------------------------------------------------------------------------
BlockMatrix::~BlockMatrix()
{
  // Do nothing
}
//-----------------------------------------------------------------------------
void BlockMatrix::set_block(uint i, uint j, boost::shared_ptr<GenericMatrix> m)
{
  assert(i < matrices.size());
  assert(j < matrices[i].size());
  matrices[i][j] = m;
}
//-----------------------------------------------------------------------------
const boost::shared_ptr<GenericMatrix> BlockMatrix::get_block(uint i, uint j) const
{
  assert(i < matrices.size());
  assert(j < matrices[i].size());

  return matrices[i][j];
}
//-----------------------------------------------------------------------------
boost::shared_ptr<GenericMatrix> BlockMatrix::get_block(uint i, uint j)
{
  assert(i < matrices.size());
  assert(j < matrices[i].size());

  return matrices[i][j];
}
//-----------------------------------------------------------------------------
dolfin::uint BlockMatrix::size(uint dim) const
{
  if (dim == 0)
    return matrices.size();
  else if (dim == 1)
    return matrices[0].size();
  else
    error("BlockMatrix has rank 2!");

  return 0;
}
//-----------------------------------------------------------------------------
void BlockMatrix::zero()
{
  for(uint i = 0; i < matrices.size(); i++)
    for(uint j = 0; j < matrices[i].size(); j++)
      matrices[i][j]->zero();
}
//-----------------------------------------------------------------------------
void BlockMatrix::apply(std::string mode)
{
  for(uint i = 0; i < matrices.size(); i++)
    for(uint j = 0; j < matrices[i].size(); j++)
      matrices[i][j]->apply(mode);
}
//-----------------------------------------------------------------------------
std::string BlockMatrix::str(bool verbose) const
{
  std::stringstream s;

  if (verbose)
  {
    s << str(false) << std::endl << std::endl;
    for (uint i = 0; i < matrices.size(); i++)
    {
      for (uint j = 0; i < matrices[0].size(); j++)
      {
        s << "  BlockMatrix (" << i << ", " << j << ")" << std::endl << std::endl;
        s << indent(indent(matrices[i][j]->str(true))) << std::endl;
      }
    }
  }
  else
    s << "<BlockMatrix containing " << matrices.size() << " x " << matrices[0].size() << " blocks>";

  return s.str();
}
//-----------------------------------------------------------------------------
void BlockMatrix::mult(const BlockVector& x, BlockVector& y,
                       bool transposed) const
{
  if (transposed)
    error("BlockMatrix::mult: transposed not implemented.");

  // Create tempory vector
  assert(matrices[0][0]);
  boost::scoped_ptr<GenericVector> z_tmp(matrices[0][0]->factory().create_vector());

  // Loop of block rows
  for(uint row = 0; row < matrices.size(); row++)
  {
    assert(matrices[row][0]);

    // RHS sub-vector
    GenericVector& _y = *(y.get_block(row));

    // Resize y and zero
    _y.resize(matrices[row][0]->size(0));
    _y.zero();

    // Resize z_tmp and zero
    z_tmp->resize(_y.size());
    z_tmp->zero();

    // Loop over block columns
    for(uint j = 0; j < matrices[row].size(); ++j)
    {
      const GenericVector& _x = *(x.get_block(j));
      assert(matrices[row][j]);
      matrices[row][j]->mult(_x, *z_tmp);
      _y += *z_tmp;
    }
  }
}
//-----------------------------------------------------------------------------
