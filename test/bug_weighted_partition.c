/*
  This file is part of p4est.
  p4est is a C library to manage a parallel collection of quadtrees and/or
  octrees.

  Copyright (C) 2007 Carsten Burstedde, Lucas Wilcox.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <p4est_base.h>
#include <p4est.h>

static int
weight_one (p4est_t * p4est, int32_t which_tree, p4est_quadrant_t * quadrant)
{
  return 1;
}

int
main (int argc, char **argv)
{
  int                 rank = 0;
#ifdef P4EST_MPI
  int                 mpiret;
#endif
  MPI_Comm            mpicomm;
  p4est_t            *p4est;
  p4est_connectivity_t *connectivity;

  mpicomm = MPI_COMM_NULL;
#ifdef P4EST_MPI
  mpiret = MPI_Init (&argc, &argv);
  P4EST_CHECK_MPI (mpiret);
  mpicomm = MPI_COMM_WORLD;
  mpiret = MPI_Comm_rank (mpicomm, &rank);
  P4EST_CHECK_MPI (mpiret);
#endif
  p4est_init (stdout, rank, NULL, NULL);

  /* create connectivity and forest structures */
  connectivity = p4est_connectivity_new_corner ();
  p4est = p4est_new (mpicomm, connectivity, 0, NULL);

  /* do a weighted partition with uniform weights */
  p4est_partition (p4est, weight_one);

#ifdef P4EST_MPI
  mpiret = MPI_Barrier (mpicomm);
  P4EST_CHECK_MPI (mpiret);
#endif

  /* clean up and exit */
  p4est_destroy (p4est);
  p4est_connectivity_destroy (connectivity);
  p4est_memory_check ();

#ifdef P4EST_MPI
  mpiret = MPI_Finalize ();
  P4EST_CHECK_MPI (mpiret);
#endif

  return 0;
}

/* EOF simple.c */
