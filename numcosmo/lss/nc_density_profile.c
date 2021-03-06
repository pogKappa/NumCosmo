/***************************************************************************
 *            nc_density_profile.c
 *
 *  Sat June 07 19:46:31 2014
 *  Copyright  2014  
 *  <pennalima@gmail.com>
 ****************************************************************************/
/*
 * nc_density_profile.c
 * Copyright (C) 2014 Mariana Penna Lima <pennalima@gmail.com>
 *
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

/**
 * SECTION:nc_density_profile
 * @title: NcDensityProfile
 * @short_description: Abstract class for density profile functions. 
 * 
 * This module comprises the set of functions to compute the matter density profile in both real 
 * and Fourier spaces. 
 * 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include "build_cfg.h"

#include "lss/nc_density_profile.h"
#include "math/ncm_serialize.h"
#include "math/ncm_cfg.h"
#include "math/ncm_util.h"

G_DEFINE_TYPE (NcDensityProfile, nc_density_profile, NCM_TYPE_MODEL);

static void
nc_density_profile_init (NcDensityProfile *nc_density_profile)
{
  NCM_UNUSED (nc_density_profile);  
}

static void
_nc_density_profile_dispose (GObject *object)
{
  
  /* Chain up : end */
  G_OBJECT_CLASS (nc_density_profile_parent_class)->dispose (object);
}

static void
_nc_density_profile_finalize (GObject *object)
{
  /* Chain up : end */
  G_OBJECT_CLASS (nc_density_profile_parent_class)->finalize (object);
}

NCM_MSET_MODEL_REGISTER_ID (nc_density_profile, NC_TYPE_DENSITY_PROFILE);

static void
nc_density_profile_class_init (NcDensityProfileClass *klass)
{

  GObjectClass* object_class = G_OBJECT_CLASS (klass);
  NcmModelClass* model_class = NCM_MODEL_CLASS (klass);

  //model_class->set_property = &_nc_density_profile_set_property;
  //model_class->get_property = &_nc_density_profile_get_property;
  object_class->dispose     = &_nc_density_profile_dispose;
  object_class->finalize    = &_nc_density_profile_finalize;

  ncm_mset_model_register_id (model_class,
                              "NcDensityProfile",
                              "NcDensityProfile.",
                              NULL,
                              FALSE,
                              NCM_MSET_MODEL_MAIN);

  ncm_model_class_set_name_nick (model_class, "Matter Density Profile", "DensityProfile");
  //ncm_model_class_add_params (model_class, NC_DENSITY_PROFILE_SPARAM_LEN, 0, PROP_SIZE);
  
  klass->eval_fourier = NULL;
}

/**
 * nc_density_profile_new_from_name:
 * @density_profile_name: Density profile's name
 * 
 * This function returns a new #NcDensityProfile whose type is defined by @density_profile_name string.
 * 
 * Returns: A new #NcDensityProfile.
 */
NcDensityProfile *
nc_density_profile_new_from_name (gchar *density_profile_name)
{
  GObject *obj = ncm_serialize_global_from_string (density_profile_name);
  GType density_profile_type = G_OBJECT_TYPE (obj);
  if (!g_type_is_a (density_profile_type, NC_TYPE_DENSITY_PROFILE))
    g_error ("nc_density_profile_new_from_name: NcDensityProfile %s do not descend from %s.", density_profile_name, 
             g_type_name (NC_TYPE_DENSITY_PROFILE));

	return NC_DENSITY_PROFILE (obj);
}

/**
 * nc_density_profile_ref:
 * @dp: a #NcDensityProfile
 *
 * Increases the reference count of @dp by one.
 *
 * Returns: (transfer full): @dp
 */
NcDensityProfile *
nc_density_profile_ref (NcDensityProfile *dp)
{
  return g_object_ref (dp);
}

/**
 * nc_density_profile_free: 
 * @dp: a #NcDensityProfile.
 *
 * Atomically decrements the reference count of @dp by one. If the reference count drops to 0, 
 * all memory allocated by @dp is released.
 *
 */
void 
nc_density_profile_free (NcDensityProfile *dp)
{
  g_object_unref (dp);
}

/**
 * nc_density_profile_clear: 
 * @dp: a #NcDensityProfile.
 *
 * Atomically decrements the reference count of @dp by one. If the reference count drops to 0, 
 * all memory allocated by @dp is released. Set the pointer to NULL;
 *
 */
void 
nc_density_profile_clear (NcDensityProfile **dp)
{
  g_clear_object (dp);
}

/**
 * nc_density_profile_eval_density:
 * @dp: a #NcDensityProfile
 * @cosmo: a #NcHICosmo
 * @r: radius 
 * @z: redshift
 *  
 * This function computes the density profile in real space. 
 *
 * Returns: The value of the density profile in real space.
 */
gdouble 
nc_density_profile_eval_density (NcDensityProfile *dp, NcHICosmo *cosmo, gdouble r, gdouble z) 
{ 
  return NC_DENSITY_PROFILE_GET_CLASS (dp)->eval_density (dp, cosmo, r, z);
}

/**
 * nc_density_profile_integral_density_los:
 * @dp: a #NcDensityProfile
 * @cosmo: a #NcHICosmo
 * @R: radius 
 * @z: redshift
 *  
 * This function computes the integral of density profile along the line of sight. 
 *
 * Returns: The value of the integral of the density profile along the line of sight.
 */
gdouble 
nc_density_profile_integral_density_los (NcDensityProfile *dp, NcHICosmo *cosmo, const gdouble R, const gdouble z) 
{ 
  return NC_DENSITY_PROFILE_GET_CLASS (dp)->integral_density_los (dp, cosmo, R, z);
}

/**
 * nc_density_profile_integral_density_2d:
 * @dp: a #NcDensityProfile
 * @cosmo: a #NcHICosmo
 * @R: radius 
 * @z: redshift
 *  
 * This function computes the 2D integral of density profile along the line of sight and the cluster radius ($r < R$). 
 *
 * Returns: The value of the 2D integral of the density profile along the line of sight and the cluster radius ($r < R$).
 */
gdouble 
nc_density_profile_integral_density_2d (NcDensityProfile *dp, NcHICosmo *cosmo, const gdouble R, const gdouble z) 
{ 
  return NC_DENSITY_PROFILE_GET_CLASS (dp)->integral_density_2d (dp, cosmo, R, z);
}

/**
 * nc_density_profile_eval_fourier:
 * @dp: a #NcDensityProfile
 * @cosmo: a #NcHICosmo
 * @k: mode 
 * @M: mass
 * @z: redshift
 *  
 * This function computes the density profile in the Fourier space. 
 *
 * Returns: The value of the density profile in the Fourier space.
 */
gdouble 
nc_density_profile_eval_fourier (NcDensityProfile *dp, NcHICosmo *cosmo, const gdouble k, const gdouble M, const gdouble z) 
{ 
  return NC_DENSITY_PROFILE_GET_CLASS (dp)->eval_fourier (dp, cosmo, k, M, z);
}

/**
 * nc_density_profile_scale_radius:
 * @dp: a #NcDensityProfile
 * @cosmo: a #NcHICosmo
 * @z: redshift
 *  
 * This function computes the scale radius $r_s$. 
 *
 * Returns: The value of the scale radius $r_s$.
 */
gdouble
nc_density_profile_scale_radius (NcDensityProfile *dp, NcHICosmo *cosmo, const gdouble z) 
{ 
  return NC_DENSITY_PROFILE_GET_CLASS (dp)->scale_radius (dp, cosmo, z);
}
