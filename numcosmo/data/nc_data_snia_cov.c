/***************************************************************************
 *            nc_data_snia_cov.c
 *
 *  Sat December 08 15:58:15 2012
 *  Copyright  2012  Sandro Dias Pinto Vitenti
 *  <sandro@isoftware.com.br>
 ****************************************************************************/
/*
 * numcosmo
 * Copyright (C) 2012 Sandro Dias Pinto Vitenti <sandro@isoftware.com.br>
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
 * SECTION:nc_data_snia_cov
 * @title: NcDataSNIACov
 * @short_description: Supernovae Ia Data with covariance error matrix.
 * 
 * See #NcSNIADistCov.
 * 
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */
#include "build_cfg.h"

#include "data/nc_data_snia_cov.h"
#include "nc_snia_dist_cov.h"
#include "math/ncm_model_ctrl.h"
#include "math/ncm_lapack.h"
#include "math/ncm_cfg.h"

#include <glib/gstdio.h>
#ifdef NUMCOSMO_HAVE_CFITSIO
#include <fitsio.h>
#endif /* NUMCOSMO_HAVE_CFITSIO */

#include <gsl/gsl_blas.h>
#include <gsl/gsl_multifit.h>

enum
{
  PROP_0,
  PROP_ZCMB,
  PROP_ZHE,
  PROP_SIGMA_Z,
  PROP_MAG,
  PROP_WIDTH,
  PROP_COLOUR,
  PROP_THIRDPAR,
  PROP_SIGMA_THIRDPAR,
  PROP_ABSMAG_SET, 
  PROP_COV_PACKED,
  PROP_SIZE,
};

G_DEFINE_TYPE (NcDataSNIACov, nc_data_snia_cov, NCM_TYPE_DATA_GAUSS_COV);

enum {
  NC_DATA_SNIA_COV_PREP_TO_NOTHING = 0,
  NC_DATA_SNIA_COV_PREP_TO_RESAMPLE,
  NC_DATA_SNIA_COV_PREP_TO_ESTIMATE,
};

static void
nc_data_snia_cov_init (NcDataSNIACov *snia_cov)
{
  snia_cov->mu_len            = 0;
  snia_cov->uppertri_len      = 0;
  
  snia_cov->z_cmb             = NULL;
  snia_cov->z_he              = NULL;

  snia_cov->mag               = NULL;
  snia_cov->width             = NULL;
  snia_cov->colour            = NULL;
  snia_cov->thirdpar          = NULL;

  snia_cov->width_true        = NULL;
  snia_cov->colour_true       = NULL;  
  snia_cov->mag_width_colour  = NULL;

  snia_cov->sigma_z           = NULL;
  snia_cov->sigma_thirdpar    = NULL;

  snia_cov->cov_full          = NULL;
  snia_cov->cov_full_diag     = NULL;
  snia_cov->cov_packed        = NULL;

  snia_cov->has_complete_cov  = FALSE;
  snia_cov->cov_full_state    = NC_DATA_SNIA_COV_PREP_TO_NOTHING;
  snia_cov->has_true_wc       = FALSE;
  snia_cov->saved_cov_full    = FALSE;

  snia_cov->dataset           = NULL;
  snia_cov->dataset_len       = 0;

  snia_cov->cosmo_resample_ctrl = ncm_model_ctrl_new (NULL);
  snia_cov->dcov_resample_ctrl  = ncm_model_ctrl_new (NULL);
  snia_cov->dcov_cov_full_ctrl  = ncm_model_ctrl_new (NULL);
}

static void
nc_data_snia_cov_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  NcDataSNIACov *snia_cov = NC_DATA_SNIA_COV (object);
  g_return_if_fail (NC_IS_DATA_SNIA_COV (object));

  switch (prop_id)
  {
    case PROP_ZCMB:
      ncm_vector_set_from_variant (snia_cov->z_cmb, g_value_get_variant (value));
      break;
    case PROP_ZHE:
      ncm_vector_set_from_variant (snia_cov->z_he, g_value_get_variant (value));
      break;
    case PROP_SIGMA_Z:
      ncm_vector_set_from_variant (snia_cov->sigma_z, g_value_get_variant (value));
      break;
    case PROP_MAG:
      ncm_vector_set_from_variant (snia_cov->mag, g_value_get_variant (value));
      break;
    case PROP_WIDTH:
      ncm_vector_set_from_variant (snia_cov->width, g_value_get_variant (value));
      break;
    case PROP_COLOUR:
      ncm_vector_set_from_variant (snia_cov->colour, g_value_get_variant (value));
      break;
    case PROP_THIRDPAR:
      ncm_vector_set_from_variant (snia_cov->thirdpar, g_value_get_variant (value));
      break;
    case PROP_SIGMA_THIRDPAR:
      ncm_vector_set_from_variant (snia_cov->sigma_thirdpar, g_value_get_variant (value));
      break;
    case PROP_ABSMAG_SET:
    {
      guint lv = 0, i;
      ncm_cfg_array_set_variant (snia_cov->dataset, g_value_get_variant (value));
      for (i = 0; i < snia_cov->dataset->len; i++)
      {
        guint v = g_array_index (snia_cov->dataset, guint, i);
        lv = GSL_MAX (lv, v);
      }
      snia_cov->dataset_len = lv + 1;
      break;
    }
    case PROP_COV_PACKED:
      ncm_vector_set_from_variant (snia_cov->cov_packed, g_value_get_variant (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nc_data_snia_cov_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  NcDataSNIACov *snia_cov = NC_DATA_SNIA_COV (object);
  g_return_if_fail (NC_IS_DATA_SNIA_COV (object));

  switch (prop_id)
  {
    case PROP_ZCMB:
      g_value_take_variant (value, ncm_vector_peek_variant (snia_cov->z_cmb));
      break;
    case PROP_ZHE:
      g_value_take_variant (value, ncm_vector_peek_variant (snia_cov->z_he));
      break;
    case PROP_SIGMA_Z:
      g_value_take_variant (value, ncm_vector_peek_variant (snia_cov->sigma_z));
      break;
    case PROP_MAG:
      g_value_take_variant (value, ncm_vector_peek_variant (snia_cov->mag));
      break;
    case PROP_WIDTH:
      g_value_take_variant (value, ncm_vector_peek_variant (snia_cov->width));
      break;
    case PROP_COLOUR:
      g_value_take_variant (value, ncm_vector_peek_variant (snia_cov->colour));
      break;
    case PROP_THIRDPAR:
      g_value_take_variant (value, ncm_vector_peek_variant (snia_cov->thirdpar));
      break;
    case PROP_SIGMA_THIRDPAR:
      g_value_take_variant (value, ncm_vector_peek_variant (snia_cov->sigma_thirdpar));
      break;
    case PROP_ABSMAG_SET:
      g_value_take_variant (value, ncm_cfg_array_to_variant (snia_cov->dataset, G_VARIANT_TYPE ("u")));
      break;
    case PROP_COV_PACKED:
      g_value_take_variant (value, ncm_vector_peek_variant (snia_cov->cov_packed));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void _nc_data_snia_cov_prep_to_resample (NcDataSNIACov *snia_cov, NcSNIADistCov *dcov);
static void _nc_data_snia_cov_prep_to_estimate (NcDataSNIACov *snia_cov, NcSNIADistCov *dcov);
static void _nc_data_snia_cov_restore_full_cov (NcDataSNIACov *snia_cov);

static void
nc_data_snia_cov_constructed (GObject *object)
{
  /* Chain up : start */
  G_OBJECT_CLASS (nc_data_snia_cov_parent_class)->constructed (object);
  {
    /*NcDataSNIACov *snia_cov = NC_DATA_SNIA_COV (object);*/

    ncm_data_set_init (NCM_DATA (object), TRUE);
  }
}

static void
nc_data_snia_cov_dispose (GObject *object)
{
  NcDataSNIACov *snia_cov = NC_DATA_SNIA_COV (object);
  NcmDataGaussCov *gauss = NCM_DATA_GAUSS_COV (object);
  ncm_data_gauss_cov_set_size (gauss, 0);
  
  ncm_model_ctrl_clear (&snia_cov->cosmo_resample_ctrl);
  ncm_model_ctrl_clear (&snia_cov->dcov_resample_ctrl);
  ncm_model_ctrl_clear (&snia_cov->dcov_cov_full_ctrl);
    
  /* Chain up : end */
  G_OBJECT_CLASS (nc_data_snia_cov_parent_class)->dispose (object);
}

static void
nc_data_snia_cov_finalize (GObject *object)
{
  /* NcDataSNIACov *snia_cov = NC_DATA_SNIA_COV (object); */
  
  /* Chain up : end */
  G_OBJECT_CLASS (nc_data_snia_cov_parent_class)->finalize (object);
}

static void _nc_data_snia_cov_prepare (NcmData *data, NcmMSet *mset);
static void _nc_data_snia_cov_resample (NcmData *data, NcmMSet *mset, NcmRNG *rng);
static void _nc_data_snia_cov_mean_func (NcmDataGaussCov *gauss, NcmMSet *mset, NcmVector *vp);
static gboolean _nc_data_snia_cov_func (NcmDataGaussCov *gauss, NcmMSet *mset, NcmMatrix *cov);
static void _nc_data_snia_cov_set_size (NcmDataGaussCov *gauss, guint np);
static gdouble _nc_data_snia_cov_lnNorma2 (NcmDataGaussCov *gauss, NcmMSet *mset);

static void
nc_data_snia_cov_class_init (NcDataSNIACovClass *klass)
{
  GObjectClass* object_class = G_OBJECT_CLASS (klass);
  NcmDataClass *data_class   = NCM_DATA_CLASS (klass);
  NcmDataGaussCovClass* gauss_class = NCM_DATA_GAUSS_COV_CLASS (klass);

  object_class->set_property = &nc_data_snia_cov_set_property;
  object_class->get_property = &nc_data_snia_cov_get_property;
  object_class->constructed  = &nc_data_snia_cov_constructed;
  object_class->dispose      = &nc_data_snia_cov_dispose;
  object_class->finalize     = &nc_data_snia_cov_finalize;

  g_object_class_install_property (object_class,
                                   PROP_ZCMB,
                                   g_param_spec_variant ("z-cmb",
                                                         NULL,
                                                         "Data cmb redshifts",
                                                         G_VARIANT_TYPE ("ad"), NULL,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (object_class,
                                   PROP_ZHE,
                                   g_param_spec_variant ("z-He",
                                                         NULL,
                                                         "Data He redshifts",
                                                         G_VARIANT_TYPE ("ad"), NULL,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (object_class,
                                   PROP_SIGMA_Z,
                                   g_param_spec_variant ("sigma-z",
                                                         NULL,
                                                         "Redshifts standard deviation",
                                                         G_VARIANT_TYPE ("ad"), NULL,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (object_class,
                                   PROP_MAG,
                                   g_param_spec_variant ("magnitudes",
                                                         NULL,
                                                         "Magnitudes",
                                                         G_VARIANT_TYPE ("ad"), NULL,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (object_class,
                                   PROP_WIDTH,
                                   g_param_spec_variant ("width",
                                                         NULL,
                                                         "Width",
                                                         G_VARIANT_TYPE ("ad"), NULL,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (object_class,
                                   PROP_COLOUR,
                                   g_param_spec_variant ("colour",
                                                         NULL,
                                                         "Colour",
                                                         G_VARIANT_TYPE ("ad"), NULL,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (object_class,
                                   PROP_THIRDPAR,
                                   g_param_spec_variant ("thirdpar",
                                                         NULL,
                                                         "Thirdpar",
                                                         G_VARIANT_TYPE ("ad"), NULL,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (object_class,
                                   PROP_SIGMA_THIRDPAR,
                                   g_param_spec_variant ("sigma-thirdpar",
                                                         NULL,
                                                         "Thirdpar standard deviation",
                                                         G_VARIANT_TYPE ("ad"), NULL,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (object_class,
                                   PROP_ABSMAG_SET,
                                   g_param_spec_variant ("absmag-set",
                                                         NULL,
                                                         "Absolute magnitude set",
                                                         G_VARIANT_TYPE ("au"), NULL,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (object_class,
                                   PROP_COV_PACKED,
                                   g_param_spec_variant ("cov-packed",
                                                         NULL,
                                                         "Covariance in packed form",
                                                         G_VARIANT_TYPE ("ad"), NULL,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB));

  data_class->resample   = &_nc_data_snia_cov_resample;
  data_class->prepare    = &_nc_data_snia_cov_prepare;

  gauss_class->mean_func = &_nc_data_snia_cov_mean_func;
  gauss_class->cov_func  = &_nc_data_snia_cov_func;
  gauss_class->set_size  = &_nc_data_snia_cov_set_size;  
  gauss_class->lnNorma2  = &_nc_data_snia_cov_lnNorma2;
}

static void
_nc_data_snia_cov_prepare (NcmData *data, NcmMSet *mset)
{
  NcSNIADistCov *dcov = NC_SNIA_DIST_COV (ncm_mset_peek (mset, nc_snia_dist_cov_id ()));
  NCM_UNUSED (data);
  nc_snia_dist_cov_prepare_if_needed (dcov, mset);
}

static void 
_nc_data_snia_cov_mean_func (NcmDataGaussCov *gauss, NcmMSet *mset, NcmVector *vp)
{
  NcDataSNIACov *snia_cov = NC_DATA_SNIA_COV (gauss);
  NcHICosmo *cosmo = NC_HICOSMO (ncm_mset_peek (mset, nc_hicosmo_id ()));
  NcSNIADistCov *dcov = NC_SNIA_DIST_COV (ncm_mset_peek (mset, nc_snia_dist_cov_id ()));
  nc_snia_dist_cov_mean (dcov, cosmo, snia_cov, vp);
}

static gboolean 
_nc_data_snia_cov_func (NcmDataGaussCov *gauss, NcmMSet *mset, NcmMatrix *cov)
{
  NcDataSNIACov *snia_cov = NC_DATA_SNIA_COV (gauss);
  NcSNIADistCov *dcov = NC_SNIA_DIST_COV (ncm_mset_peek (mset, nc_snia_dist_cov_id ()));
  nc_snia_dist_cov_calc (dcov, snia_cov, cov);
  
  return TRUE;
}

static gdouble 
_nc_data_snia_cov_lnNorma2 (NcmDataGaussCov *gauss, NcmMSet *mset)
{
  NcDataSNIACov *snia_cov = NC_DATA_SNIA_COV (gauss);
  NcSNIADistCov *dcov = NC_SNIA_DIST_COV (ncm_mset_peek (mset, nc_snia_dist_cov_id ()));
  const guint tmu_len = 3 * snia_cov->mu_len;
  guint i;
  gint exponent = 0;
  gdouble detL = 1.0;
  gboolean dcov_cov_full_up = ncm_model_ctrl_update (snia_cov->dcov_cov_full_ctrl, NCM_MODEL (dcov));

  if (dcov_cov_full_up || (snia_cov->cov_full_state != NC_DATA_SNIA_COV_PREP_TO_ESTIMATE))
    _nc_data_snia_cov_prep_to_estimate (snia_cov, dcov);

  for (i = 0; i < tmu_len; i++)
  {
    const gdouble Lii = ncm_matrix_get (snia_cov->cov_full, i, i);
    const gdouble ndetL = detL * Lii;
    if (ndetL < 1.0e-200 || ndetL > 1.0e200)
    {
      gint exponent_i = 0;
      detL = frexp (ndetL, &exponent_i);
      exponent += exponent_i;
    }
    else
      detL = ndetL;
  }

  return tmu_len * ncm_c_ln2pi () + 2.0 * (log (detL) + exponent * M_LN2);
}

/**
 * nc_data_snia_cov_new:
 * @use_norma: Whether to use the correct Likelihood normalzation
 * 
 * Creates a new empty #NcDataSNIACov object. If @use_norma is 
 * true the object will use the correct Likelihood normalzation 
 * when calculating $-2\ln(L)$
 *
 * Returns: (transfer full): the newly created instance of #NcDataSNIACov.
 */
NcmData *
nc_data_snia_cov_new (gboolean use_norma)
{
  return g_object_new (NC_TYPE_DATA_SNIA_COV,
                       "use-norma", use_norma,
                       NULL);
}

/**
 * nc_data_snia_cov_new_full:
 * @filename: catalog file name
 * @use_norma: Whether to use the correct Likelihood normalzation
 * 
 * Creates a new #NcDataSNIACov object and load with the catalog
 * in @filename. If @use_norma is true the object will use the 
 * correct Likelihood normalzation when calculating $-2\ln(L)$
 *
 * Returns: (transfer full): the newly created instance of #NcDataSNIACov.
 */
NcmData *
nc_data_snia_cov_new_full (gchar *filename, gboolean use_det)
{
  NcmData *data = g_object_new (NC_TYPE_DATA_SNIA_COV,
                                "use-det", use_det,
                                NULL);
#ifdef NUMCOSMO_HAVE_CFITSIO
  nc_data_snia_cov_load (NC_DATA_SNIA_COV (data), filename);
#else
  g_error ("nc_data_snia_cov_new_full: cannot load data file, no cfitsio support.");
#endif /* NUMCOSMO_HAVE_CFITSIO */
  return data;
}

/**
 * nc_data_snia_cov_sigma_int_len:
 * @snia_cov: a #NcDataSNIACov.
 * 
 * Gets the number of different intrinsic sigma parameters in the
 * catalog.
 * 
 * Returns: The number of different sigma_int.
 */
guint
nc_data_snia_cov_sigma_int_len (NcDataSNIACov *snia_cov)
{
  return snia_cov->dataset_len;
}

static void 
_nc_data_snia_cov_set_size (NcmDataGaussCov *gauss, guint mu_len)
{
  /* Chain up : start */
  NCM_DATA_GAUSS_COV_CLASS (nc_data_snia_cov_parent_class)->set_size (gauss, mu_len);  
  {
    NcDataSNIACov *snia_cov = NC_DATA_SNIA_COV (gauss);

    if (mu_len == 0 || mu_len != snia_cov->mu_len)
    {
      ncm_vector_clear (&snia_cov->z_cmb);
      ncm_vector_clear (&snia_cov->z_he);

      ncm_vector_clear (&snia_cov->mag);
      ncm_vector_clear (&snia_cov->width);
      ncm_vector_clear (&snia_cov->colour);
      ncm_vector_clear (&snia_cov->thirdpar);

      ncm_vector_clear (&snia_cov->width_true);
      ncm_vector_clear (&snia_cov->colour_true);
      ncm_vector_clear (&snia_cov->mag_width_colour);

      ncm_vector_clear (&snia_cov->sigma_z);
      ncm_vector_clear (&snia_cov->sigma_thirdpar);

      ncm_vector_clear (&snia_cov->cov_packed);
      ncm_vector_clear (&snia_cov->cov_full_diag);
      ncm_matrix_clear (&snia_cov->cov_full);

      if (snia_cov->dataset != NULL)
      {
        g_array_unref (snia_cov->dataset);
        snia_cov->dataset     = NULL;
        snia_cov->dataset_len = 0;
      }
      NCM_DATA (snia_cov)->init = FALSE;
    }

    if (mu_len > 0 && mu_len != snia_cov->mu_len)
    {
      snia_cov->mu_len           = mu_len;
      snia_cov->uppertri_len     = (mu_len * (mu_len + 1)) / 2;

      snia_cov->z_cmb            = ncm_vector_new (mu_len);
      snia_cov->z_he             = ncm_vector_new (mu_len);

      g_assert_cmpuint (ncm_vector_len (gauss->y), ==, mu_len);

      snia_cov->mag              = ncm_vector_ref (gauss->y);
      snia_cov->width            = ncm_vector_new (mu_len);
      snia_cov->colour           = ncm_vector_new (mu_len);
      snia_cov->thirdpar         = ncm_vector_new (mu_len);
      
      snia_cov->width_true       = ncm_vector_new (mu_len);
      snia_cov->colour_true      = ncm_vector_new (mu_len);
      snia_cov->mag_width_colour = ncm_vector_new (mu_len * 3);

      snia_cov->sigma_z          = ncm_vector_new (mu_len);
      snia_cov->sigma_thirdpar   = ncm_vector_new (mu_len);

      snia_cov->cov_full_diag    = ncm_vector_new (3 * mu_len);
      snia_cov->cov_full         = ncm_matrix_new (3 * mu_len, 3 * mu_len);
      snia_cov->cov_packed       = ncm_vector_new (snia_cov->uppertri_len * NC_DATA_SNIA_COV_ORDER_LENGTH);

      snia_cov->dataset          = g_array_sized_new (FALSE, FALSE, sizeof (guint32), mu_len);
      snia_cov->dataset_len      = 0;

      g_array_set_size (snia_cov->dataset, mu_len);

      NCM_DATA (snia_cov)->init = FALSE;
    }
  }
}

static void _nc_data_snia_cov_load_snia_data (NcDataSNIACov *snia_cov, const gchar *filename);
static void _nc_data_snia_cov_load_matrix (const gchar *filename, NcmMatrix *data);
static void _nc_data_snia_cov_matrix_to_packed (NcDataSNIACov *snia_cov, NcmMatrix *cov, guint index);
static void _nc_data_snia_cov_diag_to_packed (NcDataSNIACov *snia_cov, 
                                              NcmVector *sigma_mag, 
                                              NcmVector *sigma_width, 
                                              NcmVector *sigma_colour, 
                                              NcmVector *diag_mag_width, 
                                              NcmVector *diag_mag_colour,
                                              NcmVector *diag_width_colour);
static void _nc_data_snia_cov_save_cov_lowertri (NcDataSNIACov *snia_cov);

/**
 * nc_data_snia_cov_load_txt:
 * @snia_cov: a #NcDataSNIACov
 * @filename: FIXME
 * 
 * FIXME
 * 
 */
void 
nc_data_snia_cov_load_txt (NcDataSNIACov *snia_cov, const gchar *filename)
{
  GKeyFile *snia_keyfile = g_key_file_new ();
  GError *error  = NULL;
  NcmMatrix *cov = NULL;

  if (!g_key_file_load_from_file (snia_keyfile, filename, G_KEY_FILE_NONE, &error))
    g_error ("nc_data_snia_cov_load: invalid configuration: %s %s", 
             filename, 
             error->message);

  if (!g_key_file_has_key (snia_keyfile, 
                           NC_DATA_SNIA_COV_DATA_GROUP,
                           NC_DATA_SNIA_COV_DATA_LEN_KEY,
                           &error))
  {
    g_error ("nc_data_snia_cov_load: invalid %s key file, it must define at least the data length file key ["NC_DATA_SNIA_COV_DATA_LEN_KEY"]", filename);
  }
  else
  {
    guint64 mu_len = g_key_file_get_uint64 (snia_keyfile, 
                                            NC_DATA_SNIA_COV_DATA_GROUP,
                                            NC_DATA_SNIA_COV_DATA_LEN_KEY,
                                            &error);
    ncm_data_gauss_cov_set_size (NCM_DATA_GAUSS_COV (snia_cov), mu_len);
    cov = ncm_matrix_new (mu_len, mu_len);
  }

  /* Setting everything to zero */
  ncm_matrix_set_zero (snia_cov->cov_full);
  ncm_vector_set_zero (snia_cov->cov_packed);
  
  if (!g_key_file_has_key (snia_keyfile, 
                           NC_DATA_SNIA_COV_DATA_GROUP,
                           NC_DATA_SNIA_COV_DATA_HAS_COMPLETE_COV_KEY,
                           &error))
  {
    snia_cov->has_complete_cov = FALSE;
  }
  else
  {
    snia_cov->has_complete_cov = g_key_file_get_boolean (snia_keyfile, 
                                                         NC_DATA_SNIA_COV_DATA_GROUP,
                                                         NC_DATA_SNIA_COV_DATA_HAS_COMPLETE_COV_KEY,
                                                         &error);
  }
    
  if (!g_key_file_has_key (snia_keyfile, 
                           NC_DATA_SNIA_COV_DATA_GROUP,
                           NC_DATA_SNIA_COV_DATA_KEY,
                           &error))
  {
    g_error ("nc_data_snia_cov_load: invalid %s key file, it must define at least the data file key ["NC_DATA_SNIA_COV_DATA_KEY"]", filename);
  }
  else
  {
    gchar *datafile = g_key_file_get_string (snia_keyfile, 
                                             NC_DATA_SNIA_COV_DATA_GROUP,
                                             NC_DATA_SNIA_COV_DATA_KEY,
                                             &error);
    _nc_data_snia_cov_load_snia_data (snia_cov, datafile);
    g_free (datafile);
  }

  if (g_key_file_has_key (snia_keyfile, 
                          NC_DATA_SNIA_COV_DATA_GROUP,
                          NC_DATA_SNIA_COV_DATA_DESC,
                          &error))
  {
    gchar *desc = g_key_file_get_string (snia_keyfile, 
                                         NC_DATA_SNIA_COV_DATA_GROUP,
                                         NC_DATA_SNIA_COV_DATA_DESC,
                                         &error);
    ncm_data_set_desc (NCM_DATA (snia_cov), desc);    
    g_free (desc);
  }
  
  /* Get magnitude cov matrix */
  
  if (g_key_file_has_key (snia_keyfile, 
                          NC_DATA_SNIA_COV_DATA_GROUP,
                          NC_DATA_SNIA_COV_MAG_KEY,
                          &error))
  {
    gchar *datafile = g_key_file_get_string (snia_keyfile, 
                                             NC_DATA_SNIA_COV_DATA_GROUP,
                                             NC_DATA_SNIA_COV_MAG_KEY,
                                             &error);

    _nc_data_snia_cov_load_matrix (datafile, cov);    
    _nc_data_snia_cov_matrix_to_packed (snia_cov, cov, NC_DATA_SNIA_COV_ORDER_MAG_MAG);
    g_free (datafile);
  }

  /* Get width cov matrix */

  if (g_key_file_has_key (snia_keyfile, 
                          NC_DATA_SNIA_COV_DATA_GROUP,
                          NC_DATA_SNIA_COV_WIDTH_KEY,
                          &error))
  {
    gchar *datafile = g_key_file_get_string (snia_keyfile, 
                                             NC_DATA_SNIA_COV_DATA_GROUP,
                                             NC_DATA_SNIA_COV_WIDTH_KEY,
                                             &error);

    _nc_data_snia_cov_load_matrix (datafile, cov);
    _nc_data_snia_cov_matrix_to_packed (snia_cov, cov, NC_DATA_SNIA_COV_ORDER_WIDTH_WIDTH);
    g_free (datafile);
  }

  /* Get colour cov matrix */

  if (g_key_file_has_key (snia_keyfile, 
                          NC_DATA_SNIA_COV_DATA_GROUP,
                          NC_DATA_SNIA_COV_COLOUR_KEY,
                          &error))
  {
    gchar *datafile = g_key_file_get_string (snia_keyfile, 
                                             NC_DATA_SNIA_COV_DATA_GROUP,
                                             NC_DATA_SNIA_COV_COLOUR_KEY,
                                             &error);

    _nc_data_snia_cov_load_matrix (datafile, cov);
    _nc_data_snia_cov_matrix_to_packed (snia_cov, cov, NC_DATA_SNIA_COV_ORDER_COLOUR_COLOUR);

    g_free (datafile);
  }

  /* Get magnitude-width cov matrix */

  if (g_key_file_has_key (snia_keyfile, 
                          NC_DATA_SNIA_COV_DATA_GROUP,
                          NC_DATA_SNIA_COV_MAG_WIDTH_KEY,
                          &error))
  {
    gchar *datafile = g_key_file_get_string (snia_keyfile, 
                                             NC_DATA_SNIA_COV_DATA_GROUP,
                                             NC_DATA_SNIA_COV_MAG_WIDTH_KEY,
                                             &error);

    _nc_data_snia_cov_load_matrix (datafile, cov);
    _nc_data_snia_cov_matrix_to_packed (snia_cov, cov, NC_DATA_SNIA_COV_ORDER_MAG_WIDTH);

    g_free (datafile);
  }

  /* Get magnitude-colour cov matrix */

  if (g_key_file_has_key (snia_keyfile, 
                          NC_DATA_SNIA_COV_DATA_GROUP,
                          NC_DATA_SNIA_COV_MAG_COLOUR_KEY,
                          &error))
  {
    gchar *datafile = g_key_file_get_string (snia_keyfile, 
                                             NC_DATA_SNIA_COV_DATA_GROUP,
                                             NC_DATA_SNIA_COV_MAG_COLOUR_KEY,
                                             &error);

    _nc_data_snia_cov_load_matrix (datafile, cov);
    _nc_data_snia_cov_matrix_to_packed (snia_cov, cov, NC_DATA_SNIA_COV_ORDER_MAG_COLOUR);
    g_free (datafile);
  }

  /* Get width-colour cov matrix */

  if (g_key_file_has_key (snia_keyfile, 
                          NC_DATA_SNIA_COV_DATA_GROUP,
                          NC_DATA_SNIA_COV_WIDTH_COLOUR_KEY,
                          &error))
  {
    gchar *datafile = g_key_file_get_string (snia_keyfile, 
                                             NC_DATA_SNIA_COV_DATA_GROUP,
                                             NC_DATA_SNIA_COV_WIDTH_COLOUR_KEY,
                                             &error);

    _nc_data_snia_cov_load_matrix (datafile, cov);
    _nc_data_snia_cov_matrix_to_packed (snia_cov, cov, NC_DATA_SNIA_COV_ORDER_WIDTH_COLOUR);
    g_free (datafile);
  }

  {
    const guint mu_len = snia_cov->mu_len;
    const guint tmu_len = 3 * mu_len;
    glong i, j;
    for (i = 0; i < tmu_len; i++)
    {
      for (j = i + 1; j < tmu_len; j++)
      {
        const gdouble cov_ij = ncm_matrix_get (snia_cov->cov_full, i, j);
        const gdouble cov_ji = ncm_matrix_get (snia_cov->cov_full, j, i);
        if (((i / mu_len) == (j / mu_len)) && ncm_cmp (cov_ij, cov_ji, NC_DATA_SNIA_COV_SYMM_TOL) != 0)
        {
          g_error ("nc_data_snia_cov_load_txt: full covariance matrix is not symmetric in %ld %ld [% 20.15g != % 20.15g]\n", 
                   i, j, cov_ij, cov_ji);
        }
        ncm_matrix_set (snia_cov->cov_full, j, i, cov_ij);
      }
    }     
  }
  
  
  ncm_data_set_init (NCM_DATA (snia_cov), TRUE);
  ncm_matrix_clear (&cov);
  g_key_file_free (snia_keyfile);

  _nc_data_snia_cov_save_cov_lowertri (snia_cov);
}

static void 
_nc_data_snia_cov_load_snia_data (NcDataSNIACov *snia_cov, const gchar *filename)
{
  GArray *dataset = snia_cov->dataset;
  gchar *line = NULL;
  gsize len = 0, tpos = 0;
  GError *error = NULL;
  GIOChannel *file = g_io_channel_new_file (filename, "r", &error);
  NcmVector *sigma_mag         = ncm_vector_new (snia_cov->mu_len);
  NcmVector *sigma_width       = ncm_vector_new (snia_cov->mu_len);
  NcmVector *sigma_colour      = ncm_vector_new (snia_cov->mu_len);
  NcmVector *diag_mag_width    = ncm_vector_new (snia_cov->mu_len);
  NcmVector *diag_mag_colour   = ncm_vector_new (snia_cov->mu_len);
  NcmVector *diag_width_colour = ncm_vector_new (snia_cov->mu_len);
  guint i;
  
  if (file == NULL)
    g_error ("_nc_data_snia_cov_load_snia_data: cannot open file %s: %s", 
             filename, error->message);

  {
    GRegex *comment_line = g_regex_new ("^\\s*#", 0, 0, &error);
    guint n = 0;
    guint nrow = 0;
    guint max_dset_id = 0;
    guint min_dset_id = 1000;
    gboolean has_dataset = FALSE;
    
    while (g_io_channel_read_line (file, &line, &len, &tpos, &error) != G_IO_STATUS_EOF)
    {
      if (g_regex_match (comment_line, line, 0, NULL))
      {
        g_free (line);
        continue;
      }
      else
      {
        gchar **itens = g_regex_split_simple ("\\s+", line, 0, 0);
        if (n == 0)
        {
          guint pad_end = 0;
          n = g_strv_length (itens);
          if (*itens[n-1] == '\0')
            pad_end++;
          if (n - 1 - pad_end == NC_DATA_SNIA_COV_LENGTH)
            has_dataset = FALSE;
          else if (n - 2 - pad_end == NC_DATA_SNIA_COV_LENGTH)
            has_dataset = TRUE;
          else
            g_error ("_nc_data_snia_cov_load_snia_data: data file [%s] must have %d or %d columns, it has %u", 
                     filename, NC_DATA_SNIA_COV_LENGTH + 1, NC_DATA_SNIA_COV_LENGTH + 2, n - pad_end);
        }
        else if (n != g_strv_length (itens))
          g_error ("_nc_data_snia_cov_load_snia_data: data file [%s] has different number of columns in different rows [%u]", filename, nrow);
        
        if (nrow >= snia_cov->mu_len)
          g_error ("_nc_data_snia_cov_load_snia_data: cannot load data file [%s] expected nrows %u obtained >%u", 
                   filename, snia_cov->mu_len, nrow);

        {
          NcmVector *data[NC_DATA_SNIA_COV_LENGTH];
          data[NC_DATA_SNIA_COV_ZCMB]              = snia_cov->z_cmb;
          data[NC_DATA_SNIA_COV_ZHE]               = snia_cov->z_he;
          data[NC_DATA_SNIA_COV_SIGMA_Z]           = snia_cov->sigma_z;
          data[NC_DATA_SNIA_COV_MAG]               = snia_cov->mag;
          data[NC_DATA_SNIA_COV_SIGMA_MAG]         = sigma_mag;
          data[NC_DATA_SNIA_COV_WIDTH]             = snia_cov->width;
          data[NC_DATA_SNIA_COV_SIGMA_WIDTH]       = sigma_width;
          data[NC_DATA_SNIA_COV_COLOUR]            = snia_cov->colour;
          data[NC_DATA_SNIA_COV_SIGMA_COLOUR]      = sigma_colour;
          data[NC_DATA_SNIA_COV_THIRDPAR]          = snia_cov->thirdpar;
          data[NC_DATA_SNIA_COV_SIGMA_THIRDPAR]    = snia_cov->sigma_thirdpar;
          data[NC_DATA_SNIA_COV_DIAG_MAG_WIDTH]    = diag_mag_width;
          data[NC_DATA_SNIA_COV_DIAG_MAG_COLOUR]   = diag_mag_colour;
          data[NC_DATA_SNIA_COV_DIAG_WIDTH_COLOUR] = diag_width_colour;

          for (i = 0; i < NC_DATA_SNIA_COV_LENGTH; i++)
          {
            const gdouble val = g_ascii_strtod (itens[i + 1], NULL);
            ncm_vector_set (data[i], nrow, val);
          }
        }
        
        if (has_dataset)
        {
          gint64 dset_id = g_ascii_strtoll (itens[i + 1], NULL, 10);
          g_array_index (dataset, guint32, nrow) = dset_id;
          max_dset_id = GSL_MAX (max_dset_id, dset_id);
          min_dset_id = GSL_MIN (min_dset_id, dset_id);
        }
        else
          g_array_index (dataset, guint32, nrow) = 0;

        nrow++;
        g_strfreev (itens);
        g_free (line);
      }
    }

    if (nrow != snia_cov->mu_len)
    {
      g_error ("_nc_data_snia_cov_load_snia_data: cannot load data file [%s] expected nrows %u obtained %u",
               filename, snia_cov->mu_len, nrow);
    }
    if (min_dset_id > 1)
    {
      g_error ("_nc_data_snia_cov_load_snia_data: wrongly aligned dataset ids. First id = %u\n", min_dset_id);
    }
    else if (min_dset_id == 1)
    {
      for (i = 0; i < dataset->len; i++)
      {
        g_array_index (dataset, guint32, i)--;
      }
      max_dset_id--;
    }
    snia_cov->dataset_len = max_dset_id + 1;

    g_regex_unref (comment_line);
  }

  _nc_data_snia_cov_diag_to_packed (snia_cov, sigma_mag, sigma_width, sigma_colour, diag_mag_width, diag_mag_colour, diag_width_colour);
  
  ncm_vector_free (sigma_mag);
  ncm_vector_free (sigma_width);
  ncm_vector_free (sigma_colour);
  ncm_vector_free (diag_mag_width);
  ncm_vector_free (diag_mag_colour);
  ncm_vector_free (diag_width_colour);
  
  g_io_channel_unref (file);
}

static void 
_nc_data_snia_cov_load_matrix (const gchar *filename, NcmMatrix *data)
{
  GError *error = NULL;
  gchar *file = NULL;
  gsize length = 0;
  gchar **itens;
  guint itens_len = 0;
  guint pad_start = 0;
  guint pad_end = 0;
  guint64 matrix_len = 0;
  guint i, j;
  if (!g_file_get_contents (filename, &file, &length, &error))
    g_error ("_nc_data_snia_cov_load_matrix: cannot open file %s: %s", 
             filename, error->message);

  itens = g_regex_split_simple ("\\s+", file, 0, 0);
  itens_len = g_strv_length (itens);

  if (*itens[0] == '\0')
    pad_start = 1;
  if (*itens[itens_len - 1] == '\0')
    pad_end = 1;
  
  if (pad_start) itens_len--;
  if (pad_end) itens_len--;

  matrix_len = g_ascii_strtoll (itens[pad_start], NULL, 10);
  pad_start++;
  itens_len--;
  
  if (matrix_len != ncm_matrix_nrows (data))
    g_error ("_nc_data_snia_cov_load_matrix: expected a %ux%u matrix but got %"G_GINT64_MODIFIER"dx%"G_GINT64_MODIFIER"d", 
             ncm_matrix_nrows (data), ncm_matrix_nrows (data), matrix_len, matrix_len);
  
  if (matrix_len * matrix_len != itens_len)
    g_error ("_nc_data_snia_cov_load_matrix: matrix header say %"G_GINT64_MODIFIER"d length but got %u", 
             matrix_len * matrix_len , itens_len);

  for (i = 0; i < matrix_len; i++)
  {
    for (j = 0; j < matrix_len; j++)
    {
      const gdouble val = g_ascii_strtod (itens[pad_start + i * matrix_len + j], NULL);
      ncm_matrix_set (data, i, j, val);
    }
  }

  g_strfreev (itens);
  g_free (file);
}

static void _nc_data_snia_cov_diag_to_packed (NcDataSNIACov *snia_cov, 
                                              NcmVector *sigma_mag, 
                                              NcmVector *sigma_width, 
                                              NcmVector *sigma_colour, 
                                              NcmVector *diag_mag_width, 
                                              NcmVector *diag_mag_colour,
                                              NcmVector *diag_width_colour)
{
  const guint mu_len = snia_cov->mu_len;
  guint row_len = mu_len;
  guint ij = 0, i;

  for (i = 0; i < mu_len; i++)
  {
    const gdouble sigma_mag_i         = ncm_vector_get (sigma_mag, i);
    const gdouble sigma_width_i       = ncm_vector_get (sigma_width, i);
    const gdouble sigma_colour_i      = ncm_vector_get (sigma_colour, i);

    const gdouble mag_mag_i       = sigma_mag_i * sigma_mag_i;
    const gdouble mag_width_i     = ncm_vector_get (diag_mag_width, i);
    const gdouble mag_colour_i    = ncm_vector_get (diag_mag_colour, i);
    const gdouble width_width_i   = sigma_width_i * sigma_width_i;
    const gdouble width_colour_i  = ncm_vector_get (diag_width_colour, i);
    const gdouble colour_colour_i = sigma_colour_i * sigma_colour_i;

    ncm_vector_set (snia_cov->cov_packed, NC_DATA_SNIA_COV_ORDER_LENGTH * ij + NC_DATA_SNIA_COV_ORDER_MAG_MAG,       mag_mag_i);
    ncm_vector_set (snia_cov->cov_packed, NC_DATA_SNIA_COV_ORDER_LENGTH * ij + NC_DATA_SNIA_COV_ORDER_MAG_WIDTH,     mag_width_i);
    ncm_vector_set (snia_cov->cov_packed, NC_DATA_SNIA_COV_ORDER_LENGTH * ij + NC_DATA_SNIA_COV_ORDER_MAG_COLOUR,    mag_colour_i);
    ncm_vector_set (snia_cov->cov_packed, NC_DATA_SNIA_COV_ORDER_LENGTH * ij + NC_DATA_SNIA_COV_ORDER_WIDTH_WIDTH,   width_width_i);
    ncm_vector_set (snia_cov->cov_packed, NC_DATA_SNIA_COV_ORDER_LENGTH * ij + NC_DATA_SNIA_COV_ORDER_WIDTH_COLOUR,  width_colour_i);
    ncm_vector_set (snia_cov->cov_packed, NC_DATA_SNIA_COV_ORDER_LENGTH * ij + NC_DATA_SNIA_COV_ORDER_COLOUR_COLOUR, colour_colour_i);

    ncm_matrix_set (snia_cov->cov_full, 0 * mu_len + i, 0 * mu_len + i, mag_mag_i);
    ncm_matrix_set (snia_cov->cov_full, 0 * mu_len + i, 1 * mu_len + i, mag_width_i);
    ncm_matrix_set (snia_cov->cov_full, 0 * mu_len + i, 2 * mu_len + i, mag_colour_i);
    ncm_matrix_set (snia_cov->cov_full, 1 * mu_len + i, 1 * mu_len + i, width_width_i);
    ncm_matrix_set (snia_cov->cov_full, 1 * mu_len + i, 2 * mu_len + i, width_colour_i);
    ncm_matrix_set (snia_cov->cov_full, 2 * mu_len + i, 2 * mu_len + i, colour_colour_i);

    ij += row_len--;
  }
}

static void 
_nc_data_snia_cov_matrix_to_packed (NcDataSNIACov *snia_cov, NcmMatrix *cov, guint index)
{
  guint i, j, ij;

  ij = 0;
  for (i = 0; i < snia_cov->mu_len; i++)
  {
    for (j = i; j < snia_cov->mu_len; j++)
    {
      const gdouble symm_cov = 0.5 * (ncm_matrix_get (cov, i, j) + ncm_matrix_get (cov, j, i));
      ncm_vector_addto (snia_cov->cov_packed, NC_DATA_SNIA_COV_ORDER_LENGTH * ij + index, symm_cov);
      ij++;
    }
  }

  if (index == 5)
  {
    i = 2;
    j = 2;
  }
  else if (index > 2)
  {
    i = 1;
    j = index - 2;
  }
  else if (index <= 2)
  {
    i = 0;
    j = index;
  }
  else
  {
    g_assert_not_reached ();
  }

  {
    const guint mu_len = snia_cov->mu_len;
    NcmMatrix *subcov = ncm_matrix_get_submatrix (snia_cov->cov_full, i * mu_len, j * mu_len, mu_len, mu_len);
    ncm_matrix_add_mul (subcov, 1.0, cov);
    ncm_matrix_clear (&subcov);
  }
}

#ifdef NUMCOSMO_HAVE_CFITSIO

void 
nc_data_snia_cov_load_V0 (NcDataSNIACov *snia_cov, fitsfile *fptr)
{
  NcmVector *sigma_mag         = ncm_vector_new (snia_cov->mu_len);
  NcmVector *sigma_width       = ncm_vector_new (snia_cov->mu_len);
  NcmVector *sigma_colour      = ncm_vector_new (snia_cov->mu_len);
  NcmVector *diag_mag_width    = ncm_vector_new (snia_cov->mu_len);
  NcmVector *diag_mag_colour   = ncm_vector_new (snia_cov->mu_len);
  NcmVector *diag_width_colour = ncm_vector_new (snia_cov->mu_len);
  NcmVector *data[NC_DATA_SNIA_COV_LENGTH];
  gint status = 0;
  gint i;

  data[NC_DATA_SNIA_COV_ZCMB]              = snia_cov->z_cmb;
  data[NC_DATA_SNIA_COV_ZHE]               = snia_cov->z_he;
  data[NC_DATA_SNIA_COV_SIGMA_Z]           = snia_cov->sigma_z;
  data[NC_DATA_SNIA_COV_MAG]               = snia_cov->mag;
  data[NC_DATA_SNIA_COV_SIGMA_MAG]         = sigma_mag;
  data[NC_DATA_SNIA_COV_WIDTH]             = snia_cov->width;
  data[NC_DATA_SNIA_COV_SIGMA_WIDTH]       = sigma_width;
  data[NC_DATA_SNIA_COV_COLOUR]            = snia_cov->colour;
  data[NC_DATA_SNIA_COV_SIGMA_COLOUR]      = sigma_colour;
  data[NC_DATA_SNIA_COV_THIRDPAR]          = snia_cov->thirdpar;
  data[NC_DATA_SNIA_COV_SIGMA_THIRDPAR]    = snia_cov->sigma_thirdpar;
  data[NC_DATA_SNIA_COV_DIAG_MAG_WIDTH]    = diag_mag_width;
  data[NC_DATA_SNIA_COV_DIAG_MAG_COLOUR]   = diag_mag_colour;
  data[NC_DATA_SNIA_COV_DIAG_WIDTH_COLOUR] = diag_width_colour;

  for (i = 0; i < NC_DATA_SNIA_COV_LENGTH; i++)
  {
    fits_read_col_dbl (fptr, i + 1, 1, 1, snia_cov->mu_len, GSL_NAN,
                       ncm_vector_ptr (data[i], 0), NULL, 
                       &status);
    NCM_FITS_ERROR(status);
  }
  
  fits_read_col_uint (fptr, NC_DATA_SNIA_COV_ABSMAG_SET + 1, 1, 1, 
                          snia_cov->mu_len, 
                          0, &g_array_index (snia_cov->dataset, guint32, 0), 
                          NULL, &status);
  NCM_FITS_ERROR (status);

  for (i = 0; i < snia_cov->mu_len; i++)
  {
    snia_cov->dataset_len = GSL_MAX (g_array_index (snia_cov->dataset, guint32, i) + 1, snia_cov->dataset_len);
  }

  _nc_data_snia_cov_diag_to_packed (snia_cov, sigma_mag, sigma_width, sigma_colour, diag_mag_width, diag_mag_colour, diag_width_colour);
  
  {
    NcmMatrix *cov = ncm_matrix_new (snia_cov->mu_len, snia_cov->mu_len);
    
    for (i = NC_DATA_SNIA_COV_VAR_MAG; i < NC_DATA_SNIA_COV_TOTAL_LENGTH; i++)
    {
      fits_read_col_dbl (fptr, i + 1, 1, 1, snia_cov->mu_len * snia_cov->mu_len, 
                         GSL_NAN, ncm_matrix_ptr (cov, 0, 0), 
                         NULL, &status);
      NCM_FITS_ERROR (status);
      
      switch (i)
      {
        case NC_DATA_SNIA_COV_VAR_MAG:
          _nc_data_snia_cov_matrix_to_packed (snia_cov, cov, NC_DATA_SNIA_COV_ORDER_MAG_MAG);
          break;
        case NC_DATA_SNIA_COV_VAR_MAG_WIDTH:
          _nc_data_snia_cov_matrix_to_packed (snia_cov, cov, NC_DATA_SNIA_COV_ORDER_MAG_WIDTH);
          break;
        case NC_DATA_SNIA_COV_VAR_MAG_COLOUR:
          _nc_data_snia_cov_matrix_to_packed (snia_cov, cov, NC_DATA_SNIA_COV_ORDER_MAG_COLOUR);
          break;
        case NC_DATA_SNIA_COV_VAR_WIDTH:
          _nc_data_snia_cov_matrix_to_packed (snia_cov, cov, NC_DATA_SNIA_COV_ORDER_WIDTH_WIDTH);
          break;
        case NC_DATA_SNIA_COV_VAR_WIDTH_COLOUR:
          _nc_data_snia_cov_matrix_to_packed (snia_cov, cov, NC_DATA_SNIA_COV_ORDER_WIDTH_COLOUR);
          break;
        case NC_DATA_SNIA_COV_VAR_COLOUR:
          _nc_data_snia_cov_matrix_to_packed (snia_cov, cov, NC_DATA_SNIA_COV_ORDER_COLOUR_COLOUR);
          break;
        default:
          g_assert_not_reached ();
          break;
      }
    }
  }

  ncm_vector_free (sigma_mag);
  ncm_vector_free (sigma_width);
  ncm_vector_free (sigma_colour);
  ncm_vector_free (diag_mag_width);
  ncm_vector_free (diag_mag_colour);
  ncm_vector_free (diag_width_colour);  
}

/**
 * nc_data_snia_cov_load:
 * @snia_cov: a #NcDataSNIACov
 * @filename: file name of the catalog
 * 
 * Loads the catalog from @filename.
 * 
 */
void 
nc_data_snia_cov_load (NcDataSNIACov *snia_cov, const gchar *filename)
{
  fitsfile *fptr;
  gchar *desc = NULL;
  gchar comment[FLEN_COMMENT];
  glong nrows, cat_version;
  gint hdutype;
  gint status = 0;
  
  if (filename == NULL)
    g_error ("nc_data_snia_cov_load: null filename");

  fits_open_file (&fptr, filename, READONLY, &status);
  NCM_FITS_ERROR (status);

  fits_movabs_hdu (fptr, 2, &hdutype, &status);
  NCM_FITS_ERROR (status);

  if (hdutype != BINARY_TBL)
  {
    g_error ("nc_data_snia_cov_load: NcSNIADistCov catalog is not binary.");
  }

  fits_read_key_lng (fptr, NC_DATA_SNIA_COV_CAT_VERSION, &cat_version, NULL, &status);
  if (status)
  {
    cat_version = 0;
    status = 0;
  }

  fits_read_key_log (fptr, NC_DATA_SNIA_COV_CAT_VERSION, &snia_cov->has_complete_cov, NULL, &status);
  if (status)
  {
    snia_cov->has_complete_cov = FALSE;
    status = 0;
  }

  fits_read_key_longstr (fptr, NC_DATA_SNIA_COV_CAT_DESC, &desc, comment, &status);
  if (!status)
  {
    NCM_FITS_ERROR (status);

    ncm_data_set_desc (NCM_DATA (snia_cov), desc);
#ifdef HAVE_CFITSIO_3_27
    fits_free_memory (desc, &status);
    NCM_FITS_ERROR (status);
#else
    g_free (desc);
#endif /* HAVE_CFITSIO_3_27 */
  }
  else
    status = 0;

  fits_get_num_rows (fptr, &nrows, &status);
  NCM_FITS_ERROR (status);
  ncm_data_gauss_cov_set_size (NCM_DATA_GAUSS_COV (snia_cov), nrows);

  /* Setting everything to zero */
  ncm_matrix_set_zero (snia_cov->cov_full);
  ncm_vector_set_zero (snia_cov->cov_full_diag);
  ncm_vector_set_zero (snia_cov->cov_packed);
  
  if (cat_version == 0)
  {
    nc_data_snia_cov_load_V0 (snia_cov, fptr);
  }
  else
  {
    NcmMatrix *cov = ncm_matrix_new (snia_cov->mu_len, snia_cov->mu_len);
    NcmVector *data[NC_DATA_SNIA_COV_LENGTH];
    guint i;

    data[NC_DATA_SNIA_COV_V1_ZCMB]           = snia_cov->z_cmb;
    data[NC_DATA_SNIA_COV_V1_ZHE]            = snia_cov->z_he;
    data[NC_DATA_SNIA_COV_V1_SIGMA_Z]        = snia_cov->sigma_z;
    data[NC_DATA_SNIA_COV_V1_MAG]            = snia_cov->mag;
    data[NC_DATA_SNIA_COV_V1_WIDTH]          = snia_cov->width;
    data[NC_DATA_SNIA_COV_V1_COLOUR]         = snia_cov->colour;
    data[NC_DATA_SNIA_COV_V1_THIRDPAR]       = snia_cov->thirdpar;
    data[NC_DATA_SNIA_COV_V1_SIGMA_THIRDPAR] = snia_cov->sigma_thirdpar;
    
    for (i = 0; i < NC_DATA_SNIA_COV_V1_LENGTH; i++)
    {
      fits_read_col_dbl (fptr, i + 1, 1, 1, snia_cov->mu_len, GSL_NAN,
                         ncm_vector_ptr (data[i], 0), NULL, 
                         &status);
      NCM_FITS_ERROR(status);
    }

    fits_read_col_uint (fptr, NC_DATA_SNIA_COV_V1_ABSMAG_SET + 1, 1, 1, 
                        snia_cov->mu_len, 
                        0, &g_array_index (snia_cov->dataset, guint32, 0), 
                        NULL, &status);
    NCM_FITS_ERROR (status);

    for (i = 0; i < snia_cov->mu_len; i++)
    {
      snia_cov->dataset_len = GSL_MAX (g_array_index (snia_cov->dataset, guint32, i) + 1, snia_cov->dataset_len);
    }

    for (i = NC_DATA_SNIA_COV_V1_MAG_MAG; i < NC_DATA_SNIA_COV_V1_TOTAL_LENGTH; i++)
    {
      fits_read_col_dbl (fptr, i + 1, 1, 1, snia_cov->mu_len * snia_cov->mu_len, 
                         GSL_NAN, ncm_matrix_ptr (cov, 0, 0), 
                         NULL, &status);
      NCM_FITS_ERROR (status);
      switch (i)
      {
        case NC_DATA_SNIA_COV_V1_MAG_MAG:
          _nc_data_snia_cov_matrix_to_packed (snia_cov, cov, NC_DATA_SNIA_COV_ORDER_MAG_MAG);
          break;
        case NC_DATA_SNIA_COV_V1_MAG_WIDTH:
          _nc_data_snia_cov_matrix_to_packed (snia_cov, cov, NC_DATA_SNIA_COV_ORDER_MAG_WIDTH);
          break;
        case NC_DATA_SNIA_COV_V1_MAG_COLOUR:
          _nc_data_snia_cov_matrix_to_packed (snia_cov, cov, NC_DATA_SNIA_COV_ORDER_MAG_COLOUR);
          break;
        case NC_DATA_SNIA_COV_V1_WIDTH_WIDTH:
          _nc_data_snia_cov_matrix_to_packed (snia_cov, cov, NC_DATA_SNIA_COV_ORDER_WIDTH_WIDTH);
          break;
        case NC_DATA_SNIA_COV_V1_WIDTH_COLOUR:
          _nc_data_snia_cov_matrix_to_packed (snia_cov, cov, NC_DATA_SNIA_COV_ORDER_WIDTH_COLOUR);
          break;
        case NC_DATA_SNIA_COV_V1_COLOUR_COLOUR:
          _nc_data_snia_cov_matrix_to_packed (snia_cov, cov, NC_DATA_SNIA_COV_ORDER_COLOUR_COLOUR);
          break;
        default:
          g_assert_not_reached ();
          break;
      }
    }
  }
  
  fits_close_file (fptr, &status);
  NCM_FITS_ERROR (status);
  
  _nc_data_snia_cov_save_cov_lowertri (snia_cov);
  ncm_data_set_init (NCM_DATA (snia_cov), TRUE);
}

/**
 * nc_data_snia_cov_save:
 * @snia_cov: a #NcDataSNIACov
 * @filename: file name of the catalog
 * @overwrite: whether to overwrite an already existing catalog
 * 
 * Saves the catalog in fits (cfitsio) format using @filename.
 * 
 */
void 
nc_data_snia_cov_save (NcDataSNIACov *snia_cov, const gchar *filename, gboolean overwrite)
{
  /*******************************************************************/
  /* Create a binary table extension                                 */
  /*******************************************************************/
  fitsfile *fptr;       /* pointer to the FITS file, defined in fitsio.h */
  gint status;
  gint tfields;

  gchar extname[] = "NcDataSNIACov";   /* extension name */
  GPtrArray *ttype_array = g_ptr_array_sized_new (NC_DATA_SNIA_COV_TOTAL_LENGTH);
  GPtrArray *tform_array = g_ptr_array_sized_new (NC_DATA_SNIA_COV_TOTAL_LENGTH);
  GPtrArray *tunit_array = g_ptr_array_sized_new (NC_DATA_SNIA_COV_TOTAL_LENGTH);

  if (ncm_data_gauss_cov_get_size (NCM_DATA_GAUSS_COV (snia_cov)) == 0)
    g_error ("nc_data_snia_cov_save: cannot save an empy catalog.");
  
  g_ptr_array_set_size (ttype_array, NC_DATA_SNIA_COV_V1_TOTAL_LENGTH);
  g_ptr_array_set_size (tform_array, NC_DATA_SNIA_COV_V1_TOTAL_LENGTH);
  g_ptr_array_set_size (tunit_array, NC_DATA_SNIA_COV_V1_TOTAL_LENGTH);
  
  g_ptr_array_set_free_func (tform_array, g_free);

  g_ptr_array_index (ttype_array, NC_DATA_SNIA_COV_V1_ZCMB)        = "Z_CMB";
  g_ptr_array_index (tform_array, NC_DATA_SNIA_COV_V1_ZCMB)        = g_strdup ("1D");
  g_ptr_array_index (tunit_array, NC_DATA_SNIA_COV_V1_ZCMB)        = "CMB FRAME REDSHIFT";

  g_ptr_array_index (ttype_array, NC_DATA_SNIA_COV_V1_ZHE)         = "Z_HE";
  g_ptr_array_index (tform_array, NC_DATA_SNIA_COV_V1_ZHE)         = g_strdup ("1D");
  g_ptr_array_index (tunit_array, NC_DATA_SNIA_COV_V1_ZHE)         = "SUN FRAME REDSHIFT";

  g_ptr_array_index (ttype_array, NC_DATA_SNIA_COV_V1_SIGMA_Z)     = "SIGMA_Z";
  g_ptr_array_index (tform_array, NC_DATA_SNIA_COV_V1_SIGMA_Z)     = g_strdup ("1D");
  g_ptr_array_index (tunit_array, NC_DATA_SNIA_COV_V1_SIGMA_Z)     = "Z STANDARD DEVIATION";

  g_ptr_array_index (ttype_array, NC_DATA_SNIA_COV_V1_MAG)         = "MAG";
  g_ptr_array_index (tform_array, NC_DATA_SNIA_COV_V1_MAG)         = g_strdup ("1D");
  g_ptr_array_index (tunit_array, NC_DATA_SNIA_COV_V1_MAG)         = "MAGNITUDE";

  g_ptr_array_index (ttype_array, NC_DATA_SNIA_COV_V1_WIDTH)       = "WIDTH";
  g_ptr_array_index (tform_array, NC_DATA_SNIA_COV_V1_WIDTH)       = g_strdup ("1D");
  g_ptr_array_index (tunit_array, NC_DATA_SNIA_COV_V1_WIDTH)       = "WIDTH";

  g_ptr_array_index (ttype_array, NC_DATA_SNIA_COV_V1_COLOUR)      = "COLOUR";
  g_ptr_array_index (tform_array, NC_DATA_SNIA_COV_V1_COLOUR)      = g_strdup ("1D");
  g_ptr_array_index (tunit_array, NC_DATA_SNIA_COV_V1_COLOUR)      = "COLOUR";
  
  g_ptr_array_index (ttype_array, NC_DATA_SNIA_COV_V1_THIRDPAR)    = "THIRDPAR";
  g_ptr_array_index (tform_array, NC_DATA_SNIA_COV_V1_THIRDPAR)    = g_strdup ("1D");
  g_ptr_array_index (tunit_array, NC_DATA_SNIA_COV_V1_THIRDPAR)    = "THIRDPAR";
  
  g_ptr_array_index (ttype_array, NC_DATA_SNIA_COV_V1_SIGMA_THIRDPAR) = "SIGMA_THIRDPAR";
  g_ptr_array_index (tform_array, NC_DATA_SNIA_COV_V1_SIGMA_THIRDPAR) = g_strdup ("1D");
  g_ptr_array_index (tunit_array, NC_DATA_SNIA_COV_V1_SIGMA_THIRDPAR) = "THIRDPAR SIGMA";

  g_ptr_array_index (ttype_array, NC_DATA_SNIA_COV_V1_ABSMAG_SET)     = "ABSMAG_SET";
  g_ptr_array_index (tform_array, NC_DATA_SNIA_COV_V1_ABSMAG_SET)     = g_strdup ("1J");
  g_ptr_array_index (tunit_array, NC_DATA_SNIA_COV_V1_ABSMAG_SET)     = "ABSOLUTE MAG SET";

  g_ptr_array_index (ttype_array, NC_DATA_SNIA_COV_V1_MAG_MAG)        = "COV_MAG_MAG";
  g_ptr_array_index (tform_array, NC_DATA_SNIA_COV_V1_MAG_MAG)        = g_strdup_printf ("%uD", snia_cov->mu_len);
  g_ptr_array_index (tunit_array, NC_DATA_SNIA_COV_V1_MAG_MAG)        = "MAG-MAG";

  g_ptr_array_index (ttype_array, NC_DATA_SNIA_COV_V1_MAG_WIDTH)      = "COV_MAG_WIDTH";
  g_ptr_array_index (tform_array, NC_DATA_SNIA_COV_V1_MAG_WIDTH)      = g_strdup_printf ("%uD", snia_cov->mu_len);
  g_ptr_array_index (tunit_array, NC_DATA_SNIA_COV_V1_MAG_WIDTH)      = "MAG-WIDTH";

  g_ptr_array_index (ttype_array, NC_DATA_SNIA_COV_V1_MAG_COLOUR)     = "COV_MAG_COLOUR";
  g_ptr_array_index (tform_array, NC_DATA_SNIA_COV_V1_MAG_COLOUR)     = g_strdup_printf ("%uD", snia_cov->mu_len);
  g_ptr_array_index (tunit_array, NC_DATA_SNIA_COV_V1_MAG_COLOUR)     = "MAG-COLOUR";

  g_ptr_array_index (ttype_array, NC_DATA_SNIA_COV_V1_WIDTH_WIDTH)    = "COV_WIDTH_WIDTH";
  g_ptr_array_index (tform_array, NC_DATA_SNIA_COV_V1_WIDTH_WIDTH)    = g_strdup_printf ("%uD", snia_cov->mu_len);
  g_ptr_array_index (tunit_array, NC_DATA_SNIA_COV_V1_WIDTH_WIDTH)    = "WIDTH-WIDTH";

  g_ptr_array_index (ttype_array, NC_DATA_SNIA_COV_V1_WIDTH_COLOUR)   = "COV_WIDTH_COLOUR";
  g_ptr_array_index (tform_array, NC_DATA_SNIA_COV_V1_WIDTH_COLOUR)   = g_strdup_printf ("%uD", snia_cov->mu_len);
  g_ptr_array_index (tunit_array, NC_DATA_SNIA_COV_V1_WIDTH_COLOUR)   = "WIDTH-COLOUR";

  g_ptr_array_index (ttype_array, NC_DATA_SNIA_COV_V1_COLOUR_COLOUR)  = "COV_COLOUR_COLOUR";
  g_ptr_array_index (tform_array, NC_DATA_SNIA_COV_V1_COLOUR_COLOUR)  = g_strdup_printf ("%uD", snia_cov->mu_len);
  g_ptr_array_index (tunit_array, NC_DATA_SNIA_COV_V1_COLOUR_COLOUR)  = "COLOUR-COLOUR";

  tfields = ttype_array->len;

  /* initialize status before calling fitsio routines */
  status = 0;

  if (overwrite && g_file_test (filename, G_FILE_TEST_EXISTS))
    g_unlink (filename);

  /* create new FITS file */
  fits_create_file (&fptr, filename, &status);
  NCM_FITS_ERROR (status);

  /* append a new empty binary table onto the FITS file */
  fits_create_tbl (fptr, BINARY_TBL, snia_cov->mu_len, tfields, (gchar **)ttype_array->pdata, (gchar **)tform_array->pdata,
                   (gchar **)tunit_array->pdata, extname, &status);
  NCM_FITS_ERROR (status);

  {
    const gchar *desc = ncm_data_peek_desc (NCM_DATA (snia_cov));
    fits_write_key_longstr (fptr, NC_DATA_SNIA_COV_CAT_DESC, desc, NC_DATA_SNIA_COV_CAT_DESC_COMMENT, &status);
    NCM_FITS_ERROR (status);
  }

  {
    /* Always create catalogs of latest version */
    fits_write_key_lng (fptr, NC_DATA_SNIA_COV_CAT_VERSION, NC_DATA_SNIA_COV_CAT_LAST_VERSION, NC_DATA_SNIA_COV_CAT_VERSION_COMMENT, &status);
    NCM_FITS_ERROR (status);
  }

  {
    /* Set to true if there is a complete covariance matrix */
    fits_write_key_log (fptr, NC_DATA_SNIA_COV_CAT_HAS_COMPLETE_COV, snia_cov->has_complete_cov, NC_DATA_SNIA_COV_CAT_HAS_COMPLETE_COV_COMMENT, &status);
    NCM_FITS_ERROR (status);
  }

  {
    NcmVector *data[NC_DATA_SNIA_COV_V1_LENGTH];
    gint i;
    
    data[NC_DATA_SNIA_COV_V1_ZCMB]              = snia_cov->z_cmb;
    data[NC_DATA_SNIA_COV_V1_ZHE]               = snia_cov->z_he;
    data[NC_DATA_SNIA_COV_V1_SIGMA_Z]           = snia_cov->sigma_z;
    data[NC_DATA_SNIA_COV_V1_MAG]               = snia_cov->mag;
    data[NC_DATA_SNIA_COV_V1_WIDTH]             = snia_cov->width;
    data[NC_DATA_SNIA_COV_V1_COLOUR]            = snia_cov->colour;
    data[NC_DATA_SNIA_COV_V1_THIRDPAR]          = snia_cov->thirdpar;
    data[NC_DATA_SNIA_COV_V1_SIGMA_THIRDPAR]    = snia_cov->sigma_thirdpar;

    for (i = 0; i < NC_DATA_SNIA_COV_V1_LENGTH; i++)
    {
      fits_write_col_dbl (fptr, i + 1, 1, 1, snia_cov->mu_len, ncm_vector_ptr (data[i], 0), &status);
      NCM_FITS_ERROR(status);
    }
  }

  fits_write_col_uint (fptr, NC_DATA_SNIA_COV_V1_ABSMAG_SET + 1, 1, 1, 
                       snia_cov->mu_len, &g_array_index (snia_cov->dataset, guint32, 0), &status);
  NCM_FITS_ERROR (status);

  {
    const guint mu_len = snia_cov->mu_len;
    NcmMatrix *data[NC_DATA_SNIA_COV_V1_TOTAL_LENGTH];
    gint i;

    /* Restoring covariance matrix from Cholesky decomposition. */
    _nc_data_snia_cov_restore_full_cov (snia_cov);

    data[NC_DATA_SNIA_COV_V1_MAG_MAG]       = ncm_matrix_get_submatrix (snia_cov->cov_full, 0 * mu_len, 0 * mu_len, mu_len, mu_len);
    data[NC_DATA_SNIA_COV_V1_MAG_WIDTH]     = ncm_matrix_get_submatrix (snia_cov->cov_full, 0 * mu_len, 1 * mu_len, mu_len, mu_len);
    data[NC_DATA_SNIA_COV_V1_MAG_COLOUR]    = ncm_matrix_get_submatrix (snia_cov->cov_full, 0 * mu_len, 2 * mu_len, mu_len, mu_len);
    data[NC_DATA_SNIA_COV_V1_WIDTH_WIDTH]   = ncm_matrix_get_submatrix (snia_cov->cov_full, 1 * mu_len, 1 * mu_len, mu_len, mu_len);
    data[NC_DATA_SNIA_COV_V1_WIDTH_COLOUR]  = ncm_matrix_get_submatrix (snia_cov->cov_full, 1 * mu_len, 2 * mu_len, mu_len, mu_len);
    data[NC_DATA_SNIA_COV_V1_COLOUR_COLOUR] = ncm_matrix_get_submatrix (snia_cov->cov_full, 2 * mu_len, 2 * mu_len, mu_len, mu_len);

    for (i = NC_DATA_SNIA_COV_V1_MAG_MAG; i < NC_DATA_SNIA_COV_V1_TOTAL_LENGTH; i++)
    {
      NcmMatrix *cov = ncm_matrix_dup (data[i]);

      fits_write_col_dbl (fptr, i + 1, 1, 1, snia_cov->mu_len * snia_cov->mu_len, 
                          ncm_matrix_ptr (cov, 0, 0), &status);
      NCM_FITS_ERROR(status);

      ncm_matrix_clear (&data[i]);
      ncm_matrix_clear (&cov);
    }
  }

  fits_close_file (fptr, &status);
  NCM_FITS_ERROR(status);

  g_ptr_array_unref (ttype_array);
  g_ptr_array_unref (tform_array);
  g_ptr_array_unref (tunit_array);

  return; 
}

#endif /* NUMCOSMO_HAVE_CFITSIO */

static void
_nc_data_snia_cov_restore_full_cov (NcDataSNIACov *snia_cov)
{
  const guint mu_len = snia_cov->mu_len;
  const guint tmu_len = 3 * snia_cov->mu_len;
  guint i, j;

  if (mu_len == 0 || !snia_cov->has_complete_cov)
    return;

  for (i = 0; i < tmu_len; i++)
  {
    const gdouble diag = ncm_vector_get (snia_cov->cov_full_diag, i);
    ncm_matrix_set (snia_cov->cov_full, i, i, diag);
    for (j = i + 1; j < tmu_len; j++)
    {
      ncm_matrix_set (snia_cov->cov_full, i, j, ncm_matrix_get (snia_cov->cov_full, j, i));
    }
  }
}

static void
_nc_data_snia_cov_prep_to_resample (NcDataSNIACov *snia_cov, NcSNIADistCov *dcov)
{
  const guint mu_len = snia_cov->mu_len;
  const guint tmu_len = 3 * snia_cov->mu_len;
  guint i, j;

  if (mu_len == 0 || !snia_cov->has_complete_cov)
    return;

  if (!snia_cov->saved_cov_full)
    _nc_data_snia_cov_save_cov_lowertri (snia_cov);
  
  for (i = 0; i < tmu_len; i++)
  {
    const gdouble diag = ncm_vector_get (snia_cov->cov_full_diag, i);
    ncm_matrix_set (snia_cov->cov_full, i, i, diag);
    for (j = i + 1; j < tmu_len; j++)
    {
      ncm_matrix_set (snia_cov->cov_full, i, j, ncm_matrix_get (snia_cov->cov_full, j, i));
    }
  }

  ncm_matrix_cholesky_decomp (snia_cov->cov_full, 'U');
  snia_cov->cov_full_state = NC_DATA_SNIA_COV_PREP_TO_RESAMPLE;
}

static void
_nc_data_snia_cov_prep_to_estimate (NcDataSNIACov *snia_cov, NcSNIADistCov *dcov)
{
  const guint mu_len = snia_cov->mu_len;
  const guint tmu_len = 3 * snia_cov->mu_len;
  guint i, j;

  if (mu_len == 0 || !snia_cov->has_complete_cov)
    return;

  if (!snia_cov->saved_cov_full)
    _nc_data_snia_cov_save_cov_lowertri (snia_cov);

  for (i = 0; i < mu_len; i++)
  {
    const gdouble diag = ncm_vector_get (snia_cov->cov_full_diag, i) + 
      nc_snia_dist_cov_extra_var (dcov, snia_cov, i);
    ncm_matrix_set (snia_cov->cov_full, i, i, diag);

    for (j = i + 1; j < tmu_len; j++)
    {
      ncm_matrix_set (snia_cov->cov_full, i, j, ncm_matrix_get (snia_cov->cov_full, j, i));
    }
  }

  /* Continue for the last 2 * mu_len. */
  for (i = mu_len; i < tmu_len; i++)
  {
    const gdouble diag = ncm_vector_get (snia_cov->cov_full_diag, i);
    ncm_matrix_set (snia_cov->cov_full, i, i, diag);
    for (j = i + 1; j < tmu_len; j++)
    {
      ncm_matrix_set (snia_cov->cov_full, i, j, ncm_matrix_get (snia_cov->cov_full, j, i));
    }
  }

  /* Make the Cholesky decomposition substituting the upper triagle of cov_full. */
  ncm_matrix_cholesky_decomp (snia_cov->cov_full, 'U');
  snia_cov->cov_full_state = NC_DATA_SNIA_COV_PREP_TO_ESTIMATE;
}

static void
_nc_data_snia_cov_save_cov_lowertri (NcDataSNIACov *snia_cov)
{
  const guint mu_len = snia_cov->mu_len;
  const guint tmu_len = 3 * snia_cov->mu_len;
  guint i, j;

  if (mu_len == 0 || !snia_cov->has_complete_cov)
    return;

  if (snia_cov->cov_full_state != NC_DATA_SNIA_COV_PREP_TO_NOTHING)
    g_error ("_nc_data_snia_cov_save_cov_lowertri: cannot save since Cholesky decomposition was already done.");

  for (i = 0; i < mu_len; i++)
  {
    /* Save the original diagonal terms. */
    ncm_vector_set (snia_cov->cov_full_diag, i, ncm_matrix_get (snia_cov->cov_full, i, i));

    /* Save the original triangular superior terms in the lower triangle. */
    for (j = i + 1; j < tmu_len; j++)
    {
      ncm_matrix_set (snia_cov->cov_full, j, i, ncm_matrix_get (snia_cov->cov_full, i, j));
    }
  }

  /* Continue for the last 2 * mu_len. */
  for (i = mu_len; i < tmu_len; i++)
  {
    /* Save the original diagonal terms. */
    ncm_vector_set (snia_cov->cov_full_diag, i, ncm_matrix_get (snia_cov->cov_full, i, i));

    /* Save the original triangular superior terms in the lower triangle. */
    for (j = i + 1; j < tmu_len; j++)
    {
      ncm_matrix_set (snia_cov->cov_full, j, i, ncm_matrix_get (snia_cov->cov_full, i, j));
    }
  }

  snia_cov->saved_cov_full = TRUE;
}

static void 
_nc_data_snia_cov_resample (NcmData *data, NcmMSet *mset, NcmRNG *rng)
{
  NcDataSNIACov *snia_cov = NC_DATA_SNIA_COV (data);
  if (snia_cov->has_complete_cov)
  {
    NcHICosmo *cosmo = NC_HICOSMO (ncm_mset_peek (mset, nc_hicosmo_id ()));
    NcSNIADistCov *dcov = NC_SNIA_DIST_COV (ncm_mset_peek (mset, nc_snia_dist_cov_id ()));
    const guint total_len = 3 * snia_cov->mu_len;
    gboolean dcov_resample_up  = ncm_model_ctrl_update (snia_cov->dcov_resample_ctrl, NCM_MODEL (dcov));
    gboolean cosmo_resample_up = ncm_model_ctrl_update (snia_cov->cosmo_resample_ctrl, NCM_MODEL (cosmo));
    gboolean dcov_cov_full_up  = ncm_model_ctrl_update (snia_cov->dcov_cov_full_ctrl, NCM_MODEL (dcov));
    gint ret;
    guint i;

    if (dcov_resample_up || cosmo_resample_up || !snia_cov->has_true_wc)
      nc_data_snia_cov_estimate_width_colour (snia_cov, mset);

    if (dcov_cov_full_up || (snia_cov->cov_full_state != NC_DATA_SNIA_COV_PREP_TO_RESAMPLE))
      _nc_data_snia_cov_prep_to_resample (snia_cov, dcov);
    
    ncm_rng_lock (rng);
    for (i = 0; i < total_len; i++)
    {
      const gdouble u_i = gsl_ran_ugaussian (rng->r);
      ncm_vector_set (snia_cov->mag_width_colour, i, u_i);
    }

    ret = gsl_blas_dtrmv (CblasUpper, CblasTrans, CblasNonUnit, 
                          ncm_matrix_gsl (snia_cov->cov_full), ncm_vector_gsl (snia_cov->mag_width_colour));
    NCM_TEST_GSL_RESULT ("_nc_data_snia_cov_resample", ret);

    for (i = 0; i < snia_cov->mu_len; i++)
    {
      const gdouble width_th  = ncm_vector_get (snia_cov->width_true, i);
      const gdouble colour_th = ncm_vector_get (snia_cov->colour_true, i);
      const gdouble mag_th    = nc_snia_dist_cov_mag (dcov, cosmo, snia_cov, i, width_th, colour_th);
      const gdouble var_tot   = nc_snia_dist_cov_extra_var (dcov, snia_cov, i);

      const gdouble delta_mag    = ncm_vector_get (snia_cov->mag_width_colour, i + 0 * snia_cov->mu_len) + gsl_ran_ugaussian (rng->r) * sqrt (var_tot);
      const gdouble delta_width  = ncm_vector_get (snia_cov->mag_width_colour, i + 1 * snia_cov->mu_len);
      const gdouble delta_colour = ncm_vector_get (snia_cov->mag_width_colour, i + 2 * snia_cov->mu_len);
/*
      printf ("MAG:[%u]    % 20.15g % 20.15g % 20.15g\n", i, ncm_vector_get (snia_cov->mag, i),    mag_th    - delta_mag, mag_th);
      printf ("WIDTH:[%u]  % 20.15g % 20.15g % 20.15g\n", i, ncm_vector_get (snia_cov->width, i),  width_th  - delta_width, width_th);
      printf ("COLOUR:[%u] % 20.15g % 20.15g % 20.15g\n", i, ncm_vector_get (snia_cov->colour, i), colour_th - delta_colour, colour_th);
*/
      ncm_vector_set (snia_cov->mag,    i, mag_th    - delta_mag);
      ncm_vector_set (snia_cov->width,  i, width_th  - delta_width);
      ncm_vector_set (snia_cov->colour, i, colour_th - delta_colour);
    }  

    ncm_rng_unlock (rng);
  }
  else
  {
    /* Fallback to the default method. */
    NCM_DATA_CLASS (nc_data_snia_cov_parent_class)->resample (data, mset, rng);
  }
}

/**
 * nc_data_snia_cov_estimate_width_colour:
 * @snia_cov: a #NcDataSNIACov
 * @mset: a #NcmMSet
 * 
 * Estimate the values of width and colour from the catalog using the models in @mset
 * and fitting the width and colour as free parameters.
 * 
 * Returns: the value of the chisq for the fit.
 */
gdouble 
nc_data_snia_cov_estimate_width_colour (NcDataSNIACov *snia_cov, NcmMSet *mset)
{
  const guint mu_len = snia_cov->mu_len;
  if (mu_len == 0)
  {
    g_error ("nc_data_snia_estimate_width_colour: empty catalog.");
    return 0.0;
  }
  else
  {
    NcHICosmo *cosmo = NC_HICOSMO (ncm_mset_peek (mset, nc_hicosmo_id ()));
    NcSNIADistCov *dcov = NC_SNIA_DIST_COV (ncm_mset_peek (mset, nc_snia_dist_cov_id ()));
    guint nobs    = mu_len * 3;
    guint nparams = mu_len * 2;
    gsl_multifit_linear_workspace *mfit_ws = gsl_multifit_linear_alloc (nobs, nparams);
    NcmMatrix *cov    = ncm_matrix_new (nparams, nparams);
    NcmMatrix *X      = ncm_matrix_new (nobs, nparams);
    NcmVector *obs    = ncm_vector_new (nobs);
    NcmVector *y      = ncm_vector_new (nobs);
    NcmVector *params = ncm_vector_new (nparams);
    gdouble chisq = 0.0;

    _nc_data_snia_cov_prep_to_estimate (snia_cov, dcov);

#ifdef HAVE_LAPACK
    {
      NcmMatrix *L = ncm_matrix_new0 (nobs, nobs);
      GArray *ws   = ncm_lapack_dggglm_alloc (L, X, params, obs, y);
      nc_snia_dist_cov_mag_to_width_colour (dcov, cosmo, snia_cov, obs, X, TRUE);
      gint i, j, info;

      for (i = 0; i < nobs; i++)
      {
        for (j = i; j < nobs; j++)
        {
          ncm_matrix_set_colmajor (L, j, i, ncm_matrix_get (snia_cov->cov_full, i, j));
        }
      }
      info = ncm_lapack_dggglm_run (ws, L, X, params, obs, y);
      NCM_LAPACK_CHECK_INFO ("nc_data_snia_cov_estimate_width_colour", info);
      ncm_matrix_clear (&L);
      g_array_unref (ws);
    }
    
#else /* Fallback to GSL (much slower) */
    {
      gint ret;
      nc_snia_dist_cov_mag_to_width_colour (dcov, cosmo, snia_cov, obs, X, FALSE);
      ret = gsl_blas_dtrsv (CblasUpper, CblasTrans, CblasNonUnit, 
                            ncm_matrix_gsl (snia_cov->cov_full), ncm_vector_gsl (obs));
      NCM_TEST_GSL_RESULT ("nc_data_snia_estimate_width_colour", ret);

      ret = gsl_blas_dtrsm (CblasLeft, CblasUpper, CblasTrans, CblasNonUnit, 1.0, ncm_matrix_gsl (snia_cov->cov_full), ncm_matrix_gsl (X));
      NCM_TEST_GSL_RESULT ("nc_data_snia_estimate_width_colour", ret);

      ret = gsl_multifit_linear (ncm_matrix_gsl (X), 
                                 ncm_vector_gsl (obs), 
                                 ncm_vector_gsl (params),
                                 ncm_matrix_gsl (cov), 
                                 &chisq, 
                                 mfit_ws);
      NCM_TEST_GSL_RESULT ("nc_data_snia_estimate_width_colour", ret);
    }
#endif

    {
      gint i;
      for (i = 0; i < mu_len; i++)
      {
        const gdouble width_true = ncm_vector_get (params, i);
        const gdouble colour_true = ncm_vector_get (params, i + mu_len);
        ncm_vector_set (snia_cov->width_true, i, width_true);
        ncm_vector_set (snia_cov->colour_true, i, colour_true);
      }
    }
    snia_cov->has_true_wc = TRUE;

    _nc_data_snia_cov_restore_full_cov (snia_cov);

    ncm_vector_clear (&obs);
    ncm_vector_clear (&y);
    ncm_vector_clear (&params);
    ncm_matrix_clear (&X);
    ncm_matrix_clear (&cov);
    gsl_multifit_linear_free (mfit_ws);

    return chisq;
  }
}


