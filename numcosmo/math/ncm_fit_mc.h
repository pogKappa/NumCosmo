/***************************************************************************
 *            ncm_fit_mc.h
 *
 *  Sat December 01 17:19:03 2012
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

#ifndef _NCM_FIT_MC_H_
#define _NCM_FIT_MC_H_

#include <glib.h>
#include <glib-object.h>
#include <numcosmo/math/ncm_fit.h>
#include <gsl/gsl_histogram.h>

G_BEGIN_DECLS

#define NCM_TYPE_FIT_MC             (ncm_fit_mc_get_type ())
#define NCM_FIT_MC(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), NCM_TYPE_FIT_MC, NcmFitMC))
#define NCM_FIT_MC_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), NCM_TYPE_FIT_MC, NcmFitMCClass))
#define NCM_IS_FIT_MC(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NCM_TYPE_FIT_MC))
#define NCM_IS_FIT_MC_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), NCM_TYPE_FIT_MC))
#define NCM_FIT_MC_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), NCM_TYPE_FIT_MC, NcmFitMCClass))

typedef struct _NcmFitMCClass NcmFitMCClass;
typedef struct _NcmFitMC NcmFitMC;

struct _NcmFitMCClass
{
  /*< private >*/
  GObjectClass parent_class;
};

struct _NcmFitMC
{
  /*< private >*/
  GObject parent_instance;
  NcmFit *fit;
  NcmMatrix *fparam;
  NcmVector *m2lnL;
  gdouble m2lnL_min;
  gdouble m2lnL_max;
  guint n;
  gsl_histogram *h;
  gsl_histogram_pdf *h_pdf;
};

GType ncm_fit_mc_get_type (void) G_GNUC_CONST;

NcmFitMC *ncm_fit_mc_new (NcmFit *fit);
void ncm_fit_mc_free (NcmFitMC *mc);
void ncm_fit_mc_clear (NcmFitMC **mc);

void ncm_fit_mc_run (NcmFitMC *mc, NcmMSet *fiduc, guint ni, guint nf, NcmFitRunMsgs mtype);
void ncm_fit_mc_print (NcmFitMC *mc);
void ncm_fit_mc_mean_covar (NcmFitMC *mc);

void ncm_fit_mc_gof_pdf (NcmFitMC *mc);
void ncm_fit_mc_gof_pdf_print (NcmFitMC *mc);

gdouble ncm_fit_mc_gof_pdf_pvalue (NcmFitMC *mc, gdouble m2lnL, gboolean both);

G_END_DECLS

#endif /* _NCM_FIT_MC_H_ */