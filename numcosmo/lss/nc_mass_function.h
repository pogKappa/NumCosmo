/***************************************************************************
 *            nc_mass_function.h
 *
 *  Mon Jun 28 15:09:13 2010
 *  Copyright  2010  Mariana Penna Lima
 *  <pennalima@gmail.com>
 ****************************************************************************/
/*
 * numcosmo
 * Copyright (C) Mariana Penna Lima 2012 <pennalima@gmail.com>
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

#ifndef _NC_MASS_FUNCTION_H_
#define _NC_MASS_FUNCTION_H_

#include <glib.h>
#include <glib-object.h>
#include <numcosmo/build_cfg.h>
#include <numcosmo/nc_distance.h>
#include <numcosmo/lss/nc_matter_var.h>
#include <numcosmo/lss/nc_growth_func.h>
#include <numcosmo/lss/nc_multiplicity_func.h>
#include <numcosmo/math/ncm_spline2d.h>

G_BEGIN_DECLS

#define NC_TYPE_MASS_FUNCTION             (nc_mass_function_get_type ())
#define NC_MASS_FUNCTION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), NC_TYPE_MASS_FUNCTION, NcMassFunction))
#define NC_MASS_FUNCTION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), NC_TYPE_MASS_FUNCTION, NcMassFunctionClass))
#define NC_IS_MASS_FUNCTION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NC_TYPE_MASS_FUNCTION))
#define NC_IS_MASS_FUNCTION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), NC_TYPE_MASS_FUNCTION))
#define NC_MASS_FUNCTION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), NC_TYPE_MASS_FUNCTION, NcMassFunctionClass))
typedef struct _NcMassFunctionClass NcMassFunctionClass;
typedef struct _NcMassFunction NcMassFunction;

struct _NcMassFunction
{
  /*< private > */
  GObject parent_instance;
  NcDistance *dist;
  NcMatterVar *vp;
  NcGrowthFunc *gf;
  NcMultiplicityFunc *mulf;
  gdouble area_survey;
  gdouble N_sigma;
  gdouble growth;		/* Internal use only */
  NcmSpline2d *d2NdzdlnM;
  gdouble lnMi;
  gdouble lnMf;
  gdouble zi;
  gdouble zf;
  gdouble prec;
  NcmModelCtrl *ctrl_cosmo;
  NcmModelCtrl *ctrl_reion;
};

struct _NcMassFunctionClass
{
  /*< private > */
  GObjectClass parent_class;
};

/**
 * NcMassFunctionSplineOptimize:
 * @NC_MASS_FUNCTION_SPLINE_NONE: FIXME
 * @NC_MASS_FUNCTION_SPLINE_LNM: FIXME
 * @NC_MASS_FUNCTION_SPLINE_Z: FIXME
 *
 * FIXME
 *
 */
typedef enum _NcMassFunctionSplineOptimize
{
  NC_MASS_FUNCTION_SPLINE_NONE = 0,
  NC_MASS_FUNCTION_SPLINE_LNM,
  NC_MASS_FUNCTION_SPLINE_Z,
} NcMassFunctionSplineOptimize;

GType nc_mass_function_get_type (void) G_GNUC_CONST;

NcMassFunction *nc_mass_function_new (NcDistance *dist, NcMatterVar *vp, NcGrowthFunc *gf, NcMultiplicityFunc *mulf);
NcMassFunction *nc_mass_function_copy (NcMassFunction *mfp);
void nc_mass_function_free (NcMassFunction *mfp);
void nc_mass_function_clear (NcMassFunction **mfp);

void nc_mass_function_set_area (NcMassFunction *mfp, gdouble area);
void nc_mass_function_set_prec (NcMassFunction *mfp, gdouble prec);
void nc_mass_function_set_area_sd (NcMassFunction *mfp, gdouble area_sd);
void nc_mass_function_set_eval_limits (NcMassFunction *mfp, NcHICosmo *cosmo, gdouble lnMi, gdouble lnMf, gdouble zi, gdouble zf);
void nc_mass_function_prepare (NcMassFunction *mfp, NcHICosmo *cosmo);
G_INLINE_FUNC void nc_mass_function_prepare_if_needed (NcMassFunction *mfp, NcHICosmo *cosmo);

gdouble nc_mass_function_dn_dlnm (NcMassFunction *mfp, NcHICosmo *cosmo, gdouble lnM, gdouble z);
gdouble nc_mass_function_dv_dzdomega (NcMassFunction *mfp, NcHICosmo *cosmo, gdouble z);
G_INLINE_FUNC gdouble nc_mass_function_d2n_dzdlnm (NcMassFunction *mfp, NcHICosmo *cosmo, gdouble lnM, gdouble z);
gdouble nc_mass_function_dn_dz (NcMassFunction *mfp, NcHICosmo *cosmo, gdouble lnMl, gdouble lnMu, gdouble z, gboolean spline);
gdouble nc_mass_function_n (NcMassFunction *mfp, NcHICosmo *cosmo, gdouble lnMl, gdouble lnMu, gdouble zl, gdouble zu, NcMassFunctionSplineOptimize spline);
gdouble nc_mass_function_dn_M_to_inf_dv (NcMassFunction *mfp, NcHICosmo *cosmo, gdouble M, gdouble z);
gdouble nc_mass_function_dn_M1_to_M2_dv (NcMassFunction *mfp, NcHICosmo *cosmo, gdouble M1, gdouble M2, gdouble z);
void nc_mass_function_sigma (NcMassFunction *mfp, NcHICosmo *cosmo, gdouble lnM, gdouble z, gdouble *dn_dlnM_ptr, gdouble *sigma_ptr);
void nc_mass_function_alpha_eff (NcMatterVar *vp, NcHICosmo *cosmo, gdouble lnM, gdouble *a_eff_ptr);

G_END_DECLS

#endif /* _NC_MASS_FUNCTION_H_ */

#ifndef _NC_MASS_FUNCTION_INLINE_H_
#define _NC_MASS_FUNCTION_INLINE_H_
#ifdef NUMCOSMO_HAVE_INLINE

#include <glib-object.h>

G_BEGIN_DECLS

G_INLINE_FUNC void
nc_mass_function_prepare_if_needed (NcMassFunction *mfp, NcHICosmo *cosmo)
{
  gboolean cosmo_up = ncm_model_ctrl_update (mfp->ctrl_cosmo, NCM_MODEL (cosmo));
  
  if (cosmo_up)
	  nc_mass_function_prepare (mfp, cosmo);
}

G_INLINE_FUNC gdouble
nc_mass_function_d2n_dzdlnm (NcMassFunction *mfp, NcHICosmo *cosmo, gdouble lnM, gdouble z)
{
  return ncm_spline2d_eval (mfp->d2NdzdlnM, lnM, z);
}

G_END_DECLS

#endif /* NUMCOSMO_HAVE_INLINE */
#endif /* _NC_MASS_FUNCTION_INLINE_H_ */
