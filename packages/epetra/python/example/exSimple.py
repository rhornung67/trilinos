#! /usr/bin/env python

# @HEADER
# ************************************************************************
#
#              PyTrilinos.Epetra: Python Interface to Epetra
#                   Copyright (2005) Sandia Corporation
#
# Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
# license for use of this work by or on behalf of the U.S. Government.
#
# This library is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of the
# License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA
# Questions? Contact Michael A. Heroux (maherou@sandia.gov)
#
# ************************************************************************
# @HEADER

# Imports
import setpath
from   Numeric    import *
from   PyTrilinos import Epetra

print Epetra.Version()

comm  = Epetra.SerialComm()
nElem = 10
map   = Epetra.Map(nElem, 0, comm)
x     = Epetra.Vector(map)
b     = Epetra.Vector(map)
b.Random()
x.Update(2.0, b, 0.0)   # x = 2*b

print "b =", b
print "x =", x

# xNorm = x.Norm2()
# bNorm = b.Norm2()
# print "2 norm of x =", xNorm
# print "2 norm of b =", bNorm
