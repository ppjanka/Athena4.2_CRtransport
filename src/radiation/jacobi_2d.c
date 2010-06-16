#include "../copyright.h"
/*==============================================================================
 * FILE: jacobi_2d.c
 *
 * PURPOSE: Solves a single iteration of the formal solution of radiative
 *          transfer on a 2D grid using jacobi's method.  The basic algorithm
 *          is described in Trujillo Bueno and Fabiani Benedicho, ApJ, 455, 646.
 *
 * CONTAINS PUBLIC FUNCTIONS: 
 *   formal_solution_2d.c()
 *   formal_solution_2d_destruct()
 *   formal_solution_2d_init()
 *   jacobi_pass_pointers_to_mg_2d()
 *============================================================================*/

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include "../defs.h"
#include "../athena.h"
#include "../globals.h"
#include "../prototypes.h"
#define INTERP_2D

#ifdef RADIATION
#ifdef JACOBI

#ifdef RAD_MULTIG
static Real *******psi = NULL;
#else
static Real ******psi = NULL;
#endif
static Real *****imuo = NULL;
static Real *****r1imu, *****l1imu;
static Real *****r2imu, *****l2imu;
static Real *mu1 = NULL, *gamma1 = NULL, **am0 = NULL;

/*==============================================================================
 * PRIVATE FUNCTION PROTOTYPES:
 *   sweep_2d()     - computes a single sweep in one direction (right or left)
 *   update_sfunc() - updates source function after compute of mean intensity
 *   set_bvals_imu_y()      - set imu array at vertical boundary
 *   set_bvals_imu_y_j()    - set imu array at horizontal boundary
 *   update_bvals_imu_y()   - update outgoing radiation at vertical boundary
 *   update_bvals_imu_y_j() - update outgoing radiation at horizontal boundary
 *============================================================================*/
static void sweep_2d(RadGridS *pRG, int j, int sx, int sy);
static void update_sfunc(RadS *R);
static void set_bvals_imu_y(RadGridS *pRG, int sy);
static void set_bvals_imu_y_j(RadGridS *pRG, int j, int sy);
static void update_bvals_imu_y(RadGridS *pRG, int sy);
static void update_bvals_imu_y_j(RadGridS *pRG, int j, int sx, int sy);

void formal_solution_2d(RadGridS *pRG)
{
  int i, j, l, m;
  int nmu = pRG->nmu, nmu2 = pRG->nmu / 2;
  int ng = pRG->ng, ng2 = pRG->ng / 2;
  int is = pRG->is, ie = pRG->ie; 
  int js = pRG->js, je = pRG->je; 
  int ks = pRG->ks; 
  int ifr, nf = pRG->nf;

/* initialize mean intensities at all depths to zero */
  for(j=js-1; j<+je+1; j++)
    for(i=is-1; i<=ie+1; i++) 
      for(ifr=0; ifr<nf; ifr++) {
	pRG->R[ks][j][i][ifr].J = 0.0;
      }

/* Initialize right/left arrays */
  for(j=js-1; j<=je+1; j++)
    for(ifr=0; ifr<nf; ifr++)
      for(l=0; l<nmu; l++) 
	for(m=0; m<ng; m++) {
	  r1imu[ks][j][ifr][l][m] = pRG->r1imu[ks][j][ifr][l][m];
	  l1imu[ks][j][ifr][l][m] = pRG->l1imu[ks][j][ifr][l][m];
	}
    
/* Initialize top/bottom arrays */
  for(i=is-1; i<=ie+1; i++)
    for(ifr=0; ifr<nf; ifr++)
      for(l=0; l<nmu; l++) 
	for(m=0; m<ng; m++) {
	  r2imu[ks][i][ifr][l][m] = pRG->r2imu[ks][i][ifr][l][m];
	  l2imu[ks][i][ifr][l][m] = pRG->l2imu[ks][i][ifr][l][m];
	}

/* Compute formal solution for all upward going rays in 
 * each vertical gridzone */

/* set lower boundary condition */
  set_bvals_imu_y(pRG, 1);

  for(j=js; j<=je; j++) {
/* set emission at right and left boundaries */
    set_bvals_imu_y_j(pRG, j, 1);

/* Sweep forward and upward */
    sweep_2d(pRG, j, 1, 1);

/* Update right/left emission */
    update_bvals_imu_y_j(pRG, j, 1, 1);

/* Sweep backward and upward */
    sweep_2d(pRG, j, -1, 1);

/* Update lhs right/left emission */
    update_bvals_imu_y_j(pRG, j, -1, 1);
  }

/* Update intensity at the upper boundary */
  update_bvals_imu_y(pRG, 1);
  
/* Compute formal solution for all downward going rays in 
 * each vertical gridzone */

/* set upper boundary condition */
  set_bvals_imu_y(pRG, -1);

  for(j=je; j>=js; j--) {
/* set emission at right and left boundaries */
    set_bvals_imu_y_j(pRG, j, -1);

/* Sweep forward and downward */
    sweep_2d(pRG, j, 1, -1);
  
/* Update right/left emission */
    update_bvals_imu_y_j(pRG, j, 1, -1);

/* Sweep backward and downward */
    sweep_2d(pRG, j, -1, -1);

/* Update right/left emission */
    update_bvals_imu_y_j(pRG, j, -1, -1);
  }
 
/* Update intensity at the lower boundary */
  update_bvals_imu_y(pRG, -1);

/* Update source function */
  for(j=js; j<=je; j++) 
    for(i=is; i<=ie; i++) 
      for(ifr=0; ifr<nf; ifr++) {
	update_sfunc(&(pRG->R[ks][j][i][ifr]));
      }

  return;
}


static void sweep_2d(RadGridS *pRG, int j, int sx, int sy)
{
  int i0, i, l, m;
  int nmu = pRG->nmu, nmu2 = pRG->nmu/2;
  int ng = pRG->ng, ng2 = pRG->ng/2;
  int is = pRG->is, ie = pRG->ie;
  int ks = pRG->ks; 
  int ifr = 0, nf = pRG->nf;
  Real imu, imu0;
  Real S0, S2;
  Real am, am1, bm, bm1;
  Real w0, w1, w2;
  Real mus, mue, gs, ge;
  Real maxint, minint;

  if (sy == 1) {
    mus = nmu2;
    mue = nmu-1;
  } else {
    sy = -1;
    mus = 0;
    mue = nmu2-1;
  }

  if (sx == 1) {
    gs = ng2;
    ge = ng-1;
  } else {
    sx = -1;
    gs = 0;
    ge = ng2-1;
  }

  for(i0=is; i0<=ie; i0++) {
     if (sx == 1) 
       i = i0;
     else
       i = ie + is - i0;

     for(ifr=0; ifr<nf; ifr++)
       for(l=mus; l<=mue; l++)
	 for(m=gs; m<=ge; m++) {

/* --------- Interpolate intensity and source functions at endpoints --------- 
 * --------- of characteristics                                      --------- */

	   am = am0[l][m];
	   if (am <= 1.0) {
	     am1 = 1.0 - am;
	     /* Use linear interpolation for source functions */
	     S0 = am  * pRG->R[ks][j-sy][i-sx][ifr].S +
                 am1 * pRG->R[ks][j-sy][i   ][ifr].S;
	     S2 = am  * pRG->R[ks][j+sy][i+sx][ifr].S +
                 am1 * pRG->R[ks][j+sy][i   ][ifr].S;

#ifdef INTERP_2D /* Use parabolic interpolation for intensity */
	     w0 = 0.5 * am * (1.0 + am);
	     w1 = am1 * (1.0 + am);
	     w2 = -0.5 * am * am1;
	     imu0 = w0 * imuo[i-sx][ifr][l][m][1] + w1 * imuo[i][ifr][l][m][0] +
	            w2 * imuo[i+sx][ifr][l][m][0];
	     maxint = MAX(imuo[i-sx][ifr][l][m][1],imuo[i][ifr][l][m][0]);
	     minint = MIN(imuo[i-sx][ifr][l][m][1],imuo[i][ifr][l][m][0]);
	     if(imu0 > maxint) imu0 = maxint;
	     if(imu0 < minint) imu0 = minint;
#else     /* Use linear interpolation for intensity */
	     imu0 = am  * imuo[i-sx][ifr][l][m][1] +
                    am1 * imuo[i   ][ifr][l][m][0];
#endif
	   } else {
	     bm = 1.0 / am;
	     bm1 = 1.0 - bm;

	     /* Use linear interpolation for source functions */
	     S0 = bm  * pRG->R[ks][j-sy][i-sx][ifr].S +
                  bm1 * pRG->R[ks][j   ][i-sx][ifr].S;
	     S2 = bm  * pRG->R[ks][j+sy][i+sx][ifr].S +
                  bm1 * pRG->R[ks][j   ][i+sx][ifr].S;

#ifdef INTERP_2D /* Use parabolic interpolation for intensity */
	     w0 = 0.5 * bm1 * (1.0 + bm1);  // Modify to compute only once
	     w1 = bm * (1.0 + bm1);
	     w2 = -0.5 * bm * bm1;

	     imu0 = w0 * imuo[i-sx][ifr][l][m][0] + w1 * imuo[i-sx][ifr][l][m][1] +
	            w2 * imuo[i-sx][ifr][l][m][2];
	     maxint = MAX(imuo[i-sx][ifr][l][m][0],imuo[i-sx][ifr][l][m][1]);
	     minint = MIN(imuo[i-sx][ifr][l][m][0],imuo[i-sx][ifr][l][m][1]);
	     if(imu0 > maxint) imu0 = maxint;
	     if(imu0 < minint) imu0 =minint;
#else     /* Use linear interpolation for intensity */
	     imu0 = bm  * imuo[i-sx][ifr][l][m][1] +
                    bm1 * imuo[i-sx][ifr][l][m][0];
#endif
	   }
/* ---------  compute intensity at grid center and add to mean intensity ------- */
#ifdef RAD_MULTIG
	   imu = psi[img][j][i][ifr][l][m][0] * imu0 + 
	         psi[img][j][i][ifr][l][m][1] * S0 +
	         psi[img][j][i][ifr][l][m][2] * pRG->R[ks][j][i][ifr].S +
	         psi[img][j][i][ifr][l][m][3] * S2;	
#else
	   imu = psi[j][i][ifr][l][m][0] * imu0 + 
	         psi[j][i][ifr][l][m][1] * S0 +
	         psi[j][i][ifr][l][m][2] * pRG->R[ks][j][i][ifr].S +
	         psi[j][i][ifr][l][m][3] * S2;	
#endif
	   /* Add to mean intensity and save for next iteration */
	   pRG->R[ks][j][i][ifr].J += pRG->w[l][m] * imu;

	   /* Update intensity workspace */
#ifdef INTERP_2D
	   imuo[i][ifr][l][m][2] = imuo[i][ifr][l][m][1];
#endif
	   imuo[i][ifr][l][m][1] = imuo[i][ifr][l][m][0];
	   imuo[i][ifr][l][m][0] = imu;

	 }
  }

  return;
}

static void update_sfunc(RadS *R)
{
  Real snew, deltas;
  
  snew = (1.0 - R->eps) * R->J + R->eps;
  deltas = (snew - R->S) / (1.0 - (1.0 - R->eps) * R->lamstr);
  R->S += deltas;
 
  return;
}

void formal_solution_2d_destruct(void)
{
  int i;

#ifdef RAD_MULTIG
  if (psi != NULL) {
    for(i=0; i<nmgrid; i++)
      if (psi[i] != NULL) free_6d_array(psi[i]);
    free(psi);
  }
#else
  if (psi != NULL) free_6d_array(psi);
#endif
  if (imuo != NULL) free_5d_array(imuo);
  if (l1imu != NULL) free_5d_array(l1imu);
  if (r1imu != NULL) free_5d_array(r1imu);
  if (r2imu != NULL) free_5d_array(r2imu);
  if (l2imu != NULL) free_5d_array(l2imu);
  if (mu1 != NULL) free(mu1);
  if (gamma1 != NULL) free(gamma1);
  if (am0 != NULL) free_2d_array(am0);

  return;
}

void formal_solution_2d_init(RadGridS *pRG)
{
  int nx1 = pRG->Nx[0], nx2 = pRG->Nx[1], nx3 = pRG->Nx[2];
  int nmu = pRG->nmu, ng = pRG->ng;
  int nmu2 = pRG->nmu/2, ng2 = pRG->ng/2;
  int ifr, nf = pRG->nf;
  int is = pRG->is, ie = pRG->ie;
  int js = pRG->js, je = pRG->je;
  int ks = pRG->ks; 
  Real dx = pRG->dx1, dy = pRG->dx2;
  int i, j, l, m;
  int sy, sx;
  Real chi0, chi1, chi2, dtaum, dtaup;
  Real edtau, a0, a1, a2;
  Real am, am1, bm, bm1;

  if ((imuo = (Real *****)calloc_5d_array(nx1+2,nf,nmu,ng,3,sizeof(Real))) == NULL)
    goto on_error;

  if ((r1imu = (Real *****)calloc_5d_array(nx3+2,nx2+2,nf,nmu,ng,sizeof(Real))) == NULL)
    goto on_error;

  if ((l1imu = (Real *****)calloc_5d_array(nx3+2,nx2+2,nf,nmu,ng,sizeof(Real))) == NULL)
    goto on_error;

  if ((l2imu = (Real *****)calloc_5d_array(nx2+2,nx1+2,nf,nmu,ng,sizeof(Real))) == NULL)
    goto on_error;

  if ((r2imu = (Real *****)calloc_5d_array(nx2+2,nx1+2,nf,nmu,ng,sizeof(Real))) == NULL)
    goto on_error;

  if ((mu1 = (Real *)calloc(nmu,sizeof(Real))) == NULL)
    goto on_error;

  if ((gamma1 = (Real *)calloc(ng,sizeof(Real))) == NULL)
    goto on_error;

  if ((am0 = (Real **)calloc_2d_array(nmu,ng,sizeof(Real))) == NULL)
    goto on_error;

  for(i=0; i<nmu; i++) 
    mu1[i] = fabs(1.0 / pRG->mu[i]);

  for(i=0; i<ng; i++) 
    gamma1[i] = fabs(1.0 / pRG->gamma[i]);

  for(i=0; i<nmu; i++) 
    for(j=0; j<ng; j++) 
      am0[i][j] = fabs(dy * mu1[i] / (dx * gamma1[j]));
    
#ifdef RAD_MULTIG
  if ((psi = (Real *******)calloc(nmgrid,sizeof(Real******))) == NULL) 
    goto on_error;

  for(i=0; i<nmgrid; i++)
    psi[i] = NULL;

  if ((psi[img] = (Real ******)calloc_6d_array(nx2+2,nx1+2,nf,nmu,ng,4,sizeof(Real))) == NULL)
    goto on_error;
/* ---------  Compute weights and exponentials ------------------------- */
  for(j=js; j<=je; j++)
    for(i=is; i<=ie; i++) 
      for(ifr=0; ifr<nf; ifr++) {
	chi1 = pRG->R[ks][j][i][ifr].chi;
	for(l=0; l<nmu; l++) {
	  if(l < nmu2) sy = -1; else sy = 1;
	  for(m=0; m<ng; m++) {
	    if(m < ng2) sx = -1; else sx = 1;
	    am = am0[l][m];
	    if (am <= 1.0) {
	      am1 = 1.0 - am;
	      chi0 = am  * pRG->R[ks][j-sy][i-sx][ifr].chi + 
                     am1 * pRG->R[ks][j-sy][i   ][ifr].chi;
	      chi2 = am  * pRG->R[ks][j+sy][i+sx][ifr].chi + 
                     am1 * pRG->R[ks][j+sy][i   ][ifr].chi;
	      dtaum = 0.5 * (chi0 + chi1) * dy * mu1[l]; 
	      dtaup = 0.5 * (chi2 + chi1) * dy * mu1[l]; 
	    } else {
	      bm = 1.0 / am;
	      bm1 = 1.0 - bm;
	      chi0 = bm  * pRG->R[ks][j-sy][i-sx][ifr].chi + 
	             bm1 * pRG->R[ks][j   ][i-sx][ifr].chi;
	      chi2 = bm  * pRG->R[ks][j+sy][i+sx][ifr].chi +
                     bm1 * pRG->R[ks][j   ][i+sx][ifr].chi;
	      dtaum = 0.5 * (chi0 + chi1) * dx * gamma1[m]; 
	      dtaup = 0.5 * (chi2 + chi1) * dx * gamma1[m]; 
	    }
	    get_weights_parabolic(dtaum, dtaup, &edtau, &a0, &a1, &a2);
	    psi[0][j][i][ifr][l][m][0] = edtau;
	    psi[0][j][i][ifr][l][m][1] = a0;
	    psi[0][j][i][ifr][l][m][2] = a1;
	    psi[0][j][i][ifr][l][m][3] = a2;
	    pRG->R[ks][j][i][ifr].lamstr += pRG->w[l][m] * a1;
	  }
	}
      }
#else

  if ((psi = (Real ******)calloc_6d_array(nx2+2,nx1+2,nf,nmu,ng,4,sizeof(Real))) == NULL) 
    goto on_error;

/* ---------  Compute weights and exponentials ------------------------- */
  for(j=js; j<=je; j++)
    for(i=is; i<=ie; i++) 
      for(ifr=0; ifr<nf; ifr++) {
	chi1 = pRG->R[ks][j][i][ifr].chi;
	for(l=0; l<nmu; l++) {
	  if(l < nmu2) sy = -1; else sy = 1;
	  for(m=0; m<ng; m++) {
	    if(m < ng2) sx = -1; else sx = 1;
	    am = am0[l][m];
	    if (am <= 1.0) {
	      am1 = 1.0 - am;
	      chi0 = am  * pRG->R[ks][j-sy][i-sx][ifr].chi + 
                     am1 * pRG->R[ks][j-sy][i   ][ifr].chi;
	      chi2 = am  * pRG->R[ks][j+sy][i+sx][ifr].chi + 
                     am1 * pRG->R[ks][j+sy][i   ][ifr].chi;
	      dtaum = 0.5 * (chi0 + chi1) * dy * mu1[l]; 
	      dtaup = 0.5 * (chi2 + chi1) * dy * mu1[l]; 
	    } else {
	      bm = 1.0 / am;
	      bm1 = 1.0 - bm;
	      chi0 = bm  * pRG->R[ks][j-sy][i-sx][ifr].chi + 
	             bm1 * pRG->R[ks][j   ][i-sx][ifr].chi;
	      chi2 = bm  * pRG->R[ks][j+sy][i+sx][ifr].chi +
                     bm1 * pRG->R[ks][j   ][i+sx][ifr].chi;
	      dtaum = 0.5 * (chi0 + chi1) * dx * gamma1[m]; 
	      dtaup = 0.5 * (chi2 + chi1) * dx * gamma1[m]; 
	    }

	    get_weights_parabolic(dtaum, dtaup, &edtau, &a0, &a1, &a2);
	    psi[j][i][ifr][l][m][0] = edtau;
	    psi[j][i][ifr][l][m][1] = a0;
	    psi[j][i][ifr][l][m][2] = a1;
	    psi[j][i][ifr][l][m][3] = a2;
	    pRG->R[ks][j][i][ifr].lamstr += pRG->w[l][m] * a1;
	  }
	}
      }

#endif /* RAD_MULTIG */

  return;

  on_error:
  formal_solution_2d_destruct();
  ath_error("[formal_solution__2d_init]: Error allocating memory\n");
  return;

}

#ifdef RAD_MULTIG
void jacobi_pass_pointers_to_mg_2d(Real ********psi0, Real **mu10,
				   Real **gamma10, Real ***am00)
{
  *psi0 = psi;
  *mu10 = mu1;
  *gamma10 = gamma1;
  *am00 = am0;
  return;
}
#endif /* RAD_MULTIG */

static void set_bvals_imu_y(RadGridS *pRG, int sy)
{
  int is = pRG->is, ie = pRG->ie;
  int ks = pRG->ks;
  int nmu = pRG->nmu, nmu2 = pRG->nmu / 2;
  int ng = pRG->ng;
  int ifr, nf = pRG->nf;
  int i, l, m;

  if(sy == 1) {
/* Account for lower boundary condition */
    for(i=is-1; i<=ie+1; i++)
      for(ifr=0; ifr<nf; ifr++)
	for(l=nmu2; l<nmu; l++) 
	  for(m=0; m<ng; m++) {
	    imuo[i][ifr][l][m][0] = l2imu[ks][i][ifr][l][m];
	    imuo[i][ifr][l][m][1] = l2imu[ks][i][ifr][l][m];
#ifdef INTERP_2D
	    imuo[i][ifr][l][m][2] = l2imu[ks][i][ifr][l][m];
#endif
	  }

  } else {

/* Account for upper boundary condition */
    for(i=is-1; i<=ie+1; i++) 
      for(ifr=0; ifr<nf; ifr++)
	for(l=0; l<nmu2; l++) 
	  for(m=0; m<ng; m++) {
	    imuo[i][ifr][l][m][0] = r2imu[ks][i][ifr][l][m];
	    imuo[i][ifr][l][m][1] = r2imu[ks][i][ifr][l][m];
#ifdef INTERP_2D
	    imuo[i][ifr][l][m][2] = r2imu[ks][i][ifr][l][m];
#endif
	  }
  }


  return;
}

static void set_bvals_imu_y_j(RadGridS *pRG, int j, int sy)
{
  int is = pRG->is, ie = pRG->ie;
  int ks = pRG->ks;
  int ng = pRG->ng;
  int ifr, nf = pRG->nf;
  int mus, mue;
  int l, m;

  if (sy == 1) {
    mus = pRG->nmu / 2;
    mue = pRG->nmu - 1;
  } else {
    mus = 0;
    mue = pRG->nmu / 2 - 1;
  }

  for(ifr=0; ifr<nf; ifr++)
    for(l=mus; l<=mue; l++) 
      for(m=0; m<ng; m++) {
/* Account for emission on right and left boundaries */
	imuo[is-1][ifr][l][m][0] = r1imu[ks][j   ][ifr][l][m];
	imuo[is-1][ifr][l][m][1] = r1imu[ks][j-sy][ifr][l][m];
	imuo[ie+1][ifr][l][m][0] = l1imu[ks][j   ][ifr][l][m];
	imuo[ie+1][ifr][l][m][1] = l1imu[ks][j-sy][ifr][l][m];
#ifdef INTERP_2D
	if ((j >= pRG->js+1) && (j <= pRG->je-1)) {
	  imuo[is-1][ifr][l][m][2] = r1imu[ks][j-2*sy][ifr][l][m];
	  imuo[ie+1][ifr][l][m][2] = l1imu[ks][j-2*sy][ifr][l][m];
	}
#endif
      }

  return;
}

static void update_bvals_imu_y_j(RadGridS *pRG, int j, int sx, int sy)
{
  int is = pRG->is, ie = pRG->ie;
  int ks = pRG->ks;
  int ng = pRG->ng;
  int ifr, nf = pRG->nf;
  int mus, mue;
  int gs, ge;
  int l, m;
  
  if (sy == 1) {
    mus = pRG->nmu / 2;
    mue = pRG->nmu - 1;
  } else {
    mus = 0;
    mue = pRG->nmu / 2 - 1;
  }

  if (sx == 1) {
    gs = pRG->ng / 2;
    ge = pRG->ng -1 ;
  } else {
    gs = 0;
    ge = pRG->ng / 2 - 1;
  }

/* Update right/left emission */
  for(ifr=0; ifr<nf; ifr++)
    for(l=mus; l<=mue; l++) 
      for(m=gs; m<=ge; m++) {
	pRG->r1imu[ks][j][ifr][l][m] = imuo[pRG->ie][ifr][l][m][0];
	pRG->l1imu[ks][j][ifr][l][m] = imuo[pRG->is][ifr][l][m][0];
      }
  return;
}

static void update_bvals_imu_y(RadGridS *pRG, int sy)
{
  int is = pRG->is, ie = pRG->ie;
  int ks = pRG->ks;
  int ifr, nf = pRG->nf;
  int i, l, m;;


  if(sy == 1)
/* Update intensity at the upper boundary */
    for(i=is; i<=ie; i++) 
      for(ifr=0; ifr<nf; ifr++)
	for(l=pRG->nmu / 2; l<pRG->nmu; l++) 
	  for(m=0; m<pRG->ng; m++) 
	    pRG->r2imu[ks][i][ifr][l][m] = imuo[i][ifr][l][m][0];
  else
/* Update intensity at the lower boundary */
    for(i=is; i<=ie; i++) 
      for(ifr=0; ifr<nf; ifr++)
	for(l=0; l<pRG->nmu / 2; l++) 
	  for(m=0; m<pRG->ng; m++) 
	    pRG->l2imu[ks][i][ifr][l][m] = imuo[i][ifr][l][m][0];

  return;
}

#endif /* JACOBI */
#endif /* RADIATION */
