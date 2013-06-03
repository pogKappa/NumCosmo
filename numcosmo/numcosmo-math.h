/***************************************************************************
 *            numcosmo-math.h
 *
 *  Sun May  6 17:20:29 2007
 *  Copyright  2007  Sandro Dias Pinto Vitenti
 *  <sandro@isoftware.com.br>
 ****************************************************************************/
/*
 * numcosmo
 * Copyright (C) Sandro Dias Pinto Vitenti 2012 <sandro@isoftware.com.br>
 * numcosmo is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * numcosmo is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _NUMCOSMO_MATH_H
#define _NUMCOSMO_MATH_H

/* Supported libraries and build configuration */
#include <numcosmo/build_cfg.h>

/* Macros, Constants and Enums */
#include <numcosmo/ncm_enum_types.h>
#include <numcosmo/math/ncm_c.h>

/* Base types and components */
#include <numcosmo/math/ncm_vector.h>
#include <numcosmo/math/ncm_matrix.h>
#include <numcosmo/math/ncm_lapack.h>
#include <numcosmo/math/ncm_spline.h>
#include <numcosmo/math/ncm_spline_func.h>
#include <numcosmo/math/ncm_spline_gsl.h>
#include <numcosmo/math/ncm_spline_cubic.h>
#include <numcosmo/math/ncm_spline_cubic_notaknot.h>
#include <numcosmo/math/ncm_spline2d.h>
#include <numcosmo/math/ncm_spline2d_spline.h>
#include <numcosmo/math/ncm_spline2d_gsl.h>
#include <numcosmo/math/ncm_spline2d_bicubic.h>
#include <numcosmo/math/ncm_func_eval.h>
#include <numcosmo/math/grid_one.h>
#include <numcosmo/math/ncm_mpsf_trig_int.h>
#include <numcosmo/math/ncm_mpsf_sbessel.h>
#include <numcosmo/math/ncm_mpsf_sbessel_int.h>
#include <numcosmo/math/ncm_sf_sbessel.h>
#include <numcosmo/math/ncm_sf_sbessel_int.h>
#include <numcosmo/math/ncm_mpsf_0F1.h>
#include <numcosmo/math/ncm_fftlog.h>
#include <numcosmo/math/ncm_sparam.h>
#include <numcosmo/math/ncm_vparam.h>
#include <numcosmo/math/ncm_reparam.h>
#include <numcosmo/math/ncm_model.h>
#include <numcosmo/math/ncm_model_ctrl.h>
#include <numcosmo/math/ncm_mset.h>
#include <numcosmo/math/ncm_mset_func.h>
#include <numcosmo/math/ncm_ode_spline.h>
#include <numcosmo/math/ncm_reparam_linear.h>
#include <numcosmo/math/ncm_data.h>
#include <numcosmo/math/ncm_data_dist1d.h>
#include <numcosmo/math/ncm_data_gauss.h>
#include <numcosmo/math/ncm_data_gauss_cov.h>
#include <numcosmo/math/ncm_data_gauss_diag.h>
#include <numcosmo/math/ncm_data_poisson.h>
#include <numcosmo/math/ncm_dataset.h>
#include <numcosmo/math/ncm_likelihood.h>
#include <numcosmo/math/ncm_priors.h>
#include <numcosmo/math/function_cache.h>
#include <numcosmo/math/ncm_cfg.h>
/* Utilities */
#include <numcosmo/math/util.h>

/* Likelihood object */
#include <numcosmo/math/ncm_fit.h>
#ifdef NUMCOSMO_HAVE_NLOPT
#include <numcosmo/math/ncm_fit_nlopt.h>
#include <numcosmo/ncm_fit_nlopt_enum.h>
#endif /* NUMCOSMO_HAVE_NLOPT */
#include <numcosmo/math/ncm_fit_gsl_ls.h>
#include <numcosmo/math/ncm_fit_gsl_mm.h>
#include <numcosmo/math/ncm_fit_gsl_mms.h>
#ifdef NUMCOSMO_HAVE_LEVMAR
#include <numcosmo/math/ncm_fit_levmar.h>
#endif /* NUMCOSMO_HAVE_LEVMAR */
#include <numcosmo/math/ncm_fit_mc.h>
#include <numcosmo/math/ncm_lh_ratio1d.h>
#include <numcosmo/math/ncm_lh_ratio2d.h>

/* Utilities */
#include <numcosmo/math/memory_pool.h>
#include <numcosmo/math/cvode_util.h>
#include <numcosmo/math/mpq_tree.h>
#include <numcosmo/math/quaternion.h>
#include <numcosmo/math/integral.h>
#include <numcosmo/math/poly.h>
#include <numcosmo/math/quadrature.h>
#include <numcosmo/math/matrix_exp.h>
#include <numcosmo/math/magnus_iserles_ode.h>
#include <numcosmo/math/binsplit.h>
#include <numcosmo/math/dividedifference.h>
/* #include <numcosmo/math/cvode_util.h> */

/* Spherical maps, HEALPIX implementation */
#include <numcosmo/sphere/map.h>
#include <numcosmo/sphere/healpix.h>

#endif /* _NUMCOSMO_MATH_H */