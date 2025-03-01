/*
  This file is part of p4est.
  p4est is a C library to manage a collection (a forest) of multiple
  connected adaptive quadtrees or octrees in parallel.

  Copyright (C) 2010 The University of Texas System
  Additional copyright (C) 2011 individual authors
  Written by Carsten Burstedde, Lucas C. Wilcox, and Tobin Isaac

  p4est is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  p4est is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with p4est; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

/*
 * Usage: p8est_simple <configuration> <level>
 *        possible configurations:
 *        o unit          The unit cube.
 *        o brick         The brick connectivity.
 *        o periodic      The unit cube with all-periodic boundary conditions.
 *        o rotwrap       The unit cube with various self-periodic b.c.
 *        o twocubes      Two connected cubes.
 *        o twowrap       Two cubes with periodically identified far ends.
 *        o rotcubes      A collection of six connected rotated cubes.
 *        o pillow        A 2-tree  discretization of a hollow sphere.
 *        o shell         A 24-tree discretization of a hollow sphere.
 *        o sphere        A 13-tree discretization of a solid sphere.
 *        o pillow_sphere A 1-tree  discretization of a solid sphere.
 *        o torus         A configurable multi-tree discretization of a torus.
 */

#define VTK_OUTPUT 1

#include <p8est_bits.h>
#include <p8est_extended.h>

#ifdef VTK_OUTPUT
#include <p8est_vtk.h>
#endif

typedef enum
{
  P8EST_CONFIG_NULL,
  P8EST_CONFIG_UNIT,
  P8EST_CONFIG_BRICK,
  P8EST_CONFIG_PERIODIC,
  P8EST_CONFIG_ROTWRAP,
  P8EST_CONFIG_DROP,
  P8EST_CONFIG_TWOCUBES,
  P8EST_CONFIG_TWOWRAP,
  P8EST_CONFIG_ROTCUBES,
  P8EST_CONFIG_PILLOW,
  P8EST_CONFIG_SHELL,
  P8EST_CONFIG_SPHERE,
  P8EST_CONFIG_PILLOW_SPHERE,
  P8EST_CONFIG_TORUS,
  P8EST_CONFIG_LAST
}
simple_config_t;

typedef struct
{
  simple_config_t     config;
  int                 mpisize;
  int                 level;
  unsigned            checksum;
}
simple_regression_t;

typedef struct
{
  p4est_topidx_t      a;
}
user_data_t;

typedef struct
{
  sc_MPI_Comm         mpicomm;
  int                 mpisize;
  int                 mpirank;
}
mpi_context_t;

static int          refine_level = 0;

/* *INDENT-OFF* */
static const simple_regression_t regression[] =
{{ P8EST_CONFIG_UNIT, 1, 7, 0x88fc2229U },
 { P8EST_CONFIG_UNIT, 3, 6, 0xce19fee3U },
 { P8EST_CONFIG_TWOCUBES, 1, 4, 0xd9e96b31U },
 { P8EST_CONFIG_TWOCUBES, 3, 5, 0xe8b16b4aU },
 { P8EST_CONFIG_TWOWRAP, 1, 4, 0xd3e06e2fU },
 { P8EST_CONFIG_TWOWRAP, 5, 5, 0x920ecd43U },
 { P8EST_CONFIG_PERIODIC, 1, 4, 0x28304c83U },
 { P8EST_CONFIG_PERIODIC, 7, 4, 0x28304c83U },
 { P8EST_CONFIG_PERIODIC, 3, 5, 0xe4d123b2U },
 { P8EST_CONFIG_PERIODIC, 6, 6, 0x81c22cc6U },
 { P8EST_CONFIG_ROTWRAP, 1, 5, 0xe4d123b2U },
 { P8EST_CONFIG_ROTWRAP, 3, 5, 0xe4d123b2U },
 { P8EST_CONFIG_ROTWRAP, 5, 6, 0x81c22cc6U },
 { P8EST_CONFIG_DROP, 1, 5, 0x81c22cc6U },
 { P8EST_CONFIG_ROTCUBES, 1, 5, 0x5c497bdaU },
 { P8EST_CONFIG_ROTCUBES, 3, 5, 0x5c497bdaU },
 { P8EST_CONFIG_ROTCUBES, 5, 6, 0x00530556U },
 { P8EST_CONFIG_ROTCUBES, 7, 1, 0x47f00071U },
 { P8EST_CONFIG_ROTCUBES, 7, 6, 0x00530556U },
 { P8EST_CONFIG_ROTCUBES, 7, 7, 0x84730f31U },
 { P8EST_CONFIG_ROTCUBES, 9, 1, 0x00600001U },
 { P8EST_CONFIG_NULL, 0, 0, 0 }};
/* *INDENT-ON* */

static void
init_fn (p8est_t *p8est, p4est_topidx_t which_tree,
         p8est_quadrant_t *quadrant)
{
  user_data_t        *data = (user_data_t *) quadrant->p.user_data;

  data->a = which_tree;
}

static int
refine_sparse_fn (p8est_t *p8est, p4est_topidx_t which_tree,
                  p8est_quadrant_t *quadrant)
{
  if (which_tree != 0) {
    return 0;
  }
  if ((int) quadrant->level >= refine_level) {
    return 0;
  }
  if (quadrant->level == 0) {
    return 1;
  }
  if (quadrant->x < P8EST_QUADRANT_LEN (2) &&
      quadrant->y > 0 && quadrant->z < P8EST_QUADRANT_LEN (2)) {
    return 1;
  }

  return 0;
}

static int
refine_pillow_fn (p8est_t *p8est, p4est_topidx_t which_tree,
                  p8est_quadrant_t *quadrant)
{
  p8est_geometry_t   *geom = (p8est_geometry_t *) p8est->user_pointer;

  /* logical coordinates */
  p4est_qcoord_t      coords[3];

  /* physical coordinates */
  double              XYZ0[3];
  double              XYZ1[3];
  double              R0, R1;

  /* evaluate a criterion in reference coordinates */
  if ((int) quadrant->level >= refine_level) {
    return 0;
  }
  if (quadrant->level < 3) {
    return 1;
  }

  /* one reference point at the bottom z face */
  p8est_quadrant_face_coordinates (quadrant, 4, coords);
  p8est_geometry_transform_coordinates (geom, which_tree, coords, XYZ0);

  /* another reference point at the top z face */
  p8est_quadrant_face_coordinates (quadrant, 5, coords);
  p8est_geometry_transform_coordinates (geom, which_tree, coords, XYZ1);

  /* evaluate a refinement criterion in physical space */
  R0 = sqrt (XYZ0[0] * XYZ0[0] + XYZ0[1] * XYZ0[1] + XYZ0[2] * XYZ0[2]);
  R1 = sqrt (XYZ1[0] * XYZ1[0] + XYZ1[1] * XYZ1[1] + XYZ1[2] * XYZ1[2]);
  if (R0 > 0.7 && R0 < 0.8 && R1 > 0.7 && R1 < 0.8) {
    return 1;
  }

  /* default: don't refine */
  return 0;
}

static int
refine_pillow_sphere_fn (p8est_t *p8est, p4est_topidx_t which_tree,
                         p8est_quadrant_t *quadrant)
{
  p8est_geometry_t   *geom = (p8est_geometry_t *) p8est->user_pointer;

  /* logical coordinates */
  p4est_qcoord_t coords[3];

  /* physical coordinates */
  double              XYZ[3];
  double              R;

  /* evaluate tree- and quadrant-based criterion */
  if (which_tree != 0) {
    return 0;
  }
  if ((int) quadrant->level >= refine_level) {
    return 0;
  }
  if (quadrant->level < 3) {
    return 1;
  }

  /* from logical coordinates to physical coordinates (Cartesian) */
  p8est_quadrant_volume_coordinates (quadrant, coords);
  p8est_geometry_transform_coordinates (geom, which_tree, coords, XYZ);

  /* evaluate coordinate-based refinement criterion */
  R = sqrt (XYZ[0] * XYZ[0] + XYZ[1] * XYZ[1] + XYZ[2] * XYZ[2]);
  if (R > 0.5 && R < 0.7) {
    return 1;
  }

  /* default: don't refine */
  return 0;
}

static int
refine_normal_fn (p8est_t *p8est, p4est_topidx_t which_tree,
                  p8est_quadrant_t *quadrant)
{
  if ((int) quadrant->level >= (refine_level - (int) (which_tree % 3))) {
    return 0;
  }
  if (quadrant->level == 1 && p8est_quadrant_child_id (quadrant) == 3) {
    return 1;
  }
  if (quadrant->x == P8EST_LAST_OFFSET (2) &&
      quadrant->y == P8EST_LAST_OFFSET (2)) {
    return 1;
  }
  if (quadrant->z >= P8EST_QUADRANT_LEN (2)) {
    return 0;
  }

  return 1;
}

int
main (int argc, char **argv)
{
  int                 mpiret;
  int                 wrongusage;
  unsigned            crc;
  const char         *usage;
  mpi_context_t       mpi_context, *mpi = &mpi_context;
  p8est_t            *p8est;
  p8est_connectivity_t *connectivity;
  p8est_geometry_t   *geom;
  p8est_refine_t      refine_fn;
  p8est_coarsen_t     coarsen_fn;
  simple_config_t     config;
  const simple_regression_t *r;
  int                 nbrick_x = 1, nbrick_y = 1, nbrick_z = 1;

  /* initialize MPI and p4est internals */
  mpiret = sc_MPI_Init (&argc, &argv);
  SC_CHECK_MPI (mpiret);
  mpi->mpicomm = sc_MPI_COMM_WORLD;
  mpiret = sc_MPI_Comm_size (mpi->mpicomm, &mpi->mpisize);
  SC_CHECK_MPI (mpiret);
  mpiret = sc_MPI_Comm_rank (mpi->mpicomm, &mpi->mpirank);
  SC_CHECK_MPI (mpiret);

  sc_init (mpi->mpicomm, 1, 1, NULL, SC_LP_DEFAULT);
  p4est_init (NULL, SC_LP_DEFAULT);

  /* process command line arguments */
  usage =
    "Arguments: <configuration> <level>\n"
    "   Configuration can be any of\n"
    "      unit|brick|periodic|rotwrap|drop|twocubes|twowrap|rotcubes|pillow|shell|sphere\n"
    "      pillow_sphere|torus\n"
    "   Level controls the maximum depth of refinement\n";
  wrongusage = 0;
  config = P8EST_CONFIG_NULL;
  if (!wrongusage && argc < 3) {
    wrongusage = 1;
  }
  if (!wrongusage) {
    if (!strcmp (argv[1], "unit")) {
      config = P8EST_CONFIG_UNIT;
    }
    else if (!strcmp (argv[1], "brick")) {
      config = P8EST_CONFIG_BRICK;
    }
    else if (!strcmp (argv[1], "periodic")) {
      config = P8EST_CONFIG_PERIODIC;
    }
    else if (!strcmp (argv[1], "rotwrap")) {
      config = P8EST_CONFIG_ROTWRAP;
    }
    else if (!strcmp (argv[1], "drop")) {
      config = P8EST_CONFIG_DROP;
    }
    else if (!strcmp (argv[1], "twocubes")) {
      config = P8EST_CONFIG_TWOCUBES;
    }
    else if (!strcmp (argv[1], "twowrap")) {
      config = P8EST_CONFIG_TWOWRAP;
    }
    else if (!strcmp (argv[1], "rotcubes")) {
      config = P8EST_CONFIG_ROTCUBES;
    }
    else if (!strcmp (argv[1], "pillow")) {
      config = P8EST_CONFIG_PILLOW;
    }
    else if (!strcmp (argv[1], "shell")) {
      config = P8EST_CONFIG_SHELL;
    }
    else if (!strcmp (argv[1], "sphere")) {
      config = P8EST_CONFIG_SPHERE;
    }
    else if (!strcmp (argv[1], "pillow_sphere")) {
      config = P8EST_CONFIG_PILLOW_SPHERE;
    }
    else if (!strcmp (argv[1], "torus")) {
      config = P8EST_CONFIG_TORUS;
    }
    else {
      wrongusage = 1;
    }
  }
  if (wrongusage) {
    P4EST_GLOBAL_LERROR (usage);
    sc_abort_collective ("Usage error");
  }

  /* assign variables based on configuration */
  refine_level = atoi (argv[2]);
  refine_fn = refine_normal_fn;
  coarsen_fn = NULL;

  /* create connectivity and forest structures */
  geom = NULL;
  if (config == P8EST_CONFIG_BRICK) {
    nbrick_x = argc > 3 ? atoi (argv[3]) : 3;
    nbrick_y = argc > 4 ? atoi (argv[4]) : 2;
    nbrick_z = argc > 5 ? atoi (argv[5]) : 1;
    connectivity =
      p8est_connectivity_new_brick (nbrick_x, nbrick_y, nbrick_z, 0, 0, 0);
  }
  else if (config == P8EST_CONFIG_PERIODIC) {
    connectivity = p8est_connectivity_new_periodic ();
  }
  else if (config == P8EST_CONFIG_ROTWRAP) {
    connectivity = p8est_connectivity_new_rotwrap ();
  }
  else if (config == P8EST_CONFIG_DROP) {
    connectivity = p8est_connectivity_new_drop ();
  }
  else if (config == P8EST_CONFIG_TWOCUBES) {
    connectivity = p8est_connectivity_new_twocubes ();
    refine_fn = refine_sparse_fn;
  }
  else if (config == P8EST_CONFIG_TWOWRAP) {
    connectivity = p8est_connectivity_new_twowrap ();
    refine_fn = refine_sparse_fn;
  }
  else if (config == P8EST_CONFIG_ROTCUBES) {
    connectivity = p8est_connectivity_new_rotcubes ();
  }
  else if (config == P8EST_CONFIG_PILLOW) {
    connectivity = p8est_connectivity_new_pillow ();
    geom = p8est_geometry_new_pillow (connectivity, 1., .55);
    refine_fn = refine_pillow_fn;
  }
  else if (config == P8EST_CONFIG_SHELL) {
    connectivity = p8est_connectivity_new_shell ();
    geom = p8est_geometry_new_shell (connectivity, 1., .55);
  }
  else if (config == P8EST_CONFIG_SPHERE) {
    connectivity = p8est_connectivity_new_sphere ();
    geom = p8est_geometry_new_sphere (connectivity, 1., 0.191728, 0.039856);
  }
  else if (config == P8EST_CONFIG_PILLOW_SPHERE) {
    double              R = 1.0;        /* sphere radius default value */
    int                 iconfig;
    pillow_sphere_config_t pconfig = FIG52B;

    if (argc >= 4)
      R = atof (argv[3]);
    if (argc >= 5) {
      iconfig = atoi (argv[4]);
      if (iconfig >= FIG52B && iconfig <= FIG52C) {
        pconfig = (pillow_sphere_config_t) iconfig;
      }
    }

    connectivity = p8est_connectivity_new_unitcube ();
    geom = p8est_geometry_new_pillow_sphere (connectivity, R, pconfig);
    refine_fn = refine_pillow_sphere_fn;
  }
  else if (config == P8EST_CONFIG_TORUS) {
    connectivity = p8est_connectivity_new_torus (8);
    geom = p8est_geometry_new_torus (connectivity, 0.44, 1.0, 3.0);
  }
  else {
    connectivity = p8est_connectivity_new_unitcube ();
  }

  /* create forest data structure */
  P4EST_GLOBAL_PRODUCTIONF ("Size of one quadrant: %d bytes\n",
                            (int) sizeof (p8est_quadrant_t));
  p8est = p8est_new_ext (mpi->mpicomm, connectivity, 1, 0, 0,
                         sizeof (user_data_t), init_fn, geom);

#ifdef VTK_OUTPUT
  p8est_vtk_write_file (p8est, geom, "simple3_new");
#endif

  /* refinement and coarsening */
  p8est_refine (p8est, 1, refine_fn, init_fn);
  if (coarsen_fn != NULL) {
    p8est_coarsen (p8est, 1, coarsen_fn, init_fn);
  }
#ifdef VTK_OUTPUT
  p8est_vtk_write_file (p8est, geom, "simple3_refined");
#endif

  /* balance */
  p8est_balance (p8est, P8EST_CONNECT_FULL, init_fn);
#ifdef VTK_OUTPUT
  p8est_vtk_write_file (p8est, geom, "simple3_balanced");
#endif

  crc = p8est_checksum (p8est);

  /* partition */
  p8est_partition (p8est, 0, NULL);
#ifdef VTK_OUTPUT
  p8est_vtk_write_file (p8est, geom, "simple3_partition");
#endif

#ifdef P4EST_ENABLE_DEBUG
  /* rebalance should not change checksum */
  p8est_balance (p8est, P8EST_CONNECT_FULL, init_fn);
  P4EST_ASSERT (p8est_checksum (p8est) == crc);
#endif

  /* print and verify forest checksum */
  P4EST_GLOBAL_STATISTICSF ("Tree checksum 0x%08x\n", crc);
  if (mpi->mpirank == 0) {
    for (r = regression; r->config != P8EST_CONFIG_NULL; ++r) {
      if (r->config != config || r->mpisize != mpi->mpisize
          || r->level != refine_level)
        continue;
      SC_CHECK_ABORT (crc == r->checksum, "Checksum mismatch");
      P4EST_GLOBAL_INFO ("Checksum regression OK\n");
      break;
    }
  }

  /* destroy the p8est and its connectivity structure */
  p8est_destroy (p8est);
  if (geom != NULL) {
    p8est_geometry_destroy (geom);
  }
  p8est_connectivity_destroy (connectivity);

  /* clean up and exit */
  sc_finalize ();

  mpiret = sc_MPI_Finalize ();
  SC_CHECK_MPI (mpiret);

  return 0;
}
