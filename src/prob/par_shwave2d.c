#include "copyright.h"
/*==============================================================================
 * FILE: par_shwave2d.c
 *
 * PURPOSE: Problem generator shear wave test with Lagrangian particles.
 *   Must work in 3D, and the wavevector is in x2 direction. Particles can be
 *   initialized either with uniform density or with density profile same as the
 *   gas. SHEARING_BOX must be turned on, FARGO is optional.
 *
 *============================================================================*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "defs.h"
#include "athena.h"
#include "globals.h"
#include "prototypes.h"
#include "particles/particle.h"

#ifndef SHEARING_BOX
#error : The shear wave problem requires shearing-box to be enabled.
#endif /* SHEARING_BOX */

int Nx12;

/*==============================================================================
 * PRIVATE FUNCTION PROTOTYPES:
 * GetPosition()    - get particle status (grid/crossing)
 * ShearingBoxPot() - shearing box tidal potential
 * property_mybin() - particle selection function for output
 *============================================================================*/
static Real ShearingBoxPot(const Real x1, const Real x2, const Real x3);
#ifdef PARTICLES
int GetPosition(Grain *gr);
static int property_mybin(Grain *gr);
extern Real expr_V2par(const Grid *pG, const int i, const int j, const int k);
#endif

/*=========================== PUBLIC FUNCTIONS =================================
 *============================================================================*/
/*----------------------------------------------------------------------------*/
/* problem:   */

void problem(Grid *pGrid, Domain *pDomain)
{
  int i=0,j=0,k=0;
  int is,ie,js,je,ks,ke,n,m,nwave,samp;
  Real x1,x2,x3,x1max,x1min,x2max,x2min;
  Real ky,omg,omg2,amp,v1x,v1y;
#ifdef PARTICLES
  long p;
  int Npar,ip,jp,kp;
  Real x1p,x2p,x3p,x1l,x1u,x2l,x2u,x3l,x3u;
#endif

  if (par_geti("grid","Nx3") == 1) {
    ath_error("[par_shwave2d]: par_shwave2d must work in 3D grid.\n");
  }

  is = pGrid->is; ie = pGrid->ie;
  js = pGrid->js; je = pGrid->je;
  ks = pGrid->ks; ke = pGrid->ke;

/* Read initial conditions  */
  amp = par_getd("problem","amp");
  Omega_0 = par_getd_def("problem","omega",1.0);
  qshear  = par_getd_def("problem","qshear",1.5);
  nwave = par_geti("problem","nwave");
  samp = par_geti("problem","sample");
  x1min = par_getd("grid","x1min");
  x1max = par_getd("grid","x1max");
  x2min = par_getd("grid","x2min");
  x2max = par_getd("grid","x2max");

  Nx12 = pGrid->Nx1*pGrid->Nx2;

  if (nwave <= 0)
    ath_error("[par_shwave2d]: nwave must be positive!\n");

/* calculate dispersion relation and find eigen vectors */
  ky = 2.0*(PI)*nwave/(x2max-x2min);
  omg2 = SQR(Iso_csound*ky)+2.0*(2.0-qshear)*SQR(Omega_0);
  omg = sqrt(omg2);
  v1y = omg*amp/ky;
  v1x = 2.0*Omega_0/omg*v1y;

/* Now set initial conditions to wave solution */ 

  for (k=ks; k<=ke; k++) {
  for (j=js; j<=je; j++) {
  for (i=is; i<=ie; i++) {
    cc_pos(pGrid,i,j,k,&x1,&x2,&x3);
    pGrid->U[k][j][i].d = 1.0+amp*cos(ky*x2);
    pGrid->U[k][j][i].M1 = -pGrid->U[k][j][i].d*v1x*sin(ky*x2);
    pGrid->U[k][j][i].M2 = pGrid->U[k][j][i].d*v1y*cos(ky*x2);
#ifndef FARGO
    pGrid->U[k][j][i].M2 -= pGrid->U[k][j][i].d*qshear*Omega_0*x1;
#endif
    pGrid->U[k][j][i].M3 = 0.0;
#if (NSCALARS > 0)
    if (samp == 1)
      for (n=0; n<NSCALARS; n++)
        pGrid->U[k][j][i].s[n] = pGrid->U[k][j][i].d;
    else
      for (n=0; n<NSCALARS; n++)
        pGrid->U[k][j][i].s[n] = 1.0;
#endif
  }}}

/* Read initial conditions for the particles */
#ifdef PARTICLES

  /* basic parameters */
  if (par_geti("particle","partypes") != 1)
    ath_error("[par_shwave2d]: This test only allows ONE particle species!\n");

  Npar = (int)(sqrt(par_geti("particle","parnumcell")));
  pGrid->nparticle = Npar*Npar*Npar*pGrid->Nx1*pGrid->Nx2*pGrid->Nx3;
  pGrid->grproperty[0].num = pGrid->nparticle;
  if (pGrid->nparticle+2 > pGrid->arrsize)
    particle_realloc(pGrid, pGrid->nparticle+2);

  /* particle stopping time */
  tstop0[0] = par_getd_def("problem","tstop",0.0); /* in code unit */
  if (par_geti("particle","tsmode") != 3)
    ath_error("[par_shwave2d]: This test only allows fixed stopping time!\n");

/* Now set initial conditions for the particles */
  p = 0;

  for (k=ks; k<=ke; k++)
  {
    x3l = pGrid->x3_0 + (k+pGrid->kdisp)*pGrid->dx3;
    x3u = pGrid->x3_0 + ((k+pGrid->kdisp)+1.0)*pGrid->dx3;

   for (j=js; j<=je; j++)
   {
     x2l = pGrid->x2_0 + (j+pGrid->jdisp)*pGrid->dx2;
     x2u = pGrid->x2_0 + ((j+pGrid->jdisp)+1.0)*pGrid->dx2;

    for (i=is; i<=ie; i++)
    {
      x1l = pGrid->x1_0 + (i + pGrid->idisp)*pGrid->dx1;
      x1u = pGrid->x1_0 + ((i + pGrid->idisp) + 1.0)*pGrid->dx1;

        for (ip=0;ip<Npar;ip++)
        {
          x1p = x1l+(x1u-x1l)/Npar*(ip+0.5);

         for (jp=0;jp<Npar;jp++)
         {
           x2p = x2l+(x2u-x2l)/Npar*(jp+0.5);

          for (kp=0;kp<Npar;kp++)
          {
            x3p = x3l+(x3u-x3l)/Npar*(kp+0.5);

            pGrid->particle[p].property = 0;

            pGrid->particle[p].x1 = x1p;
            pGrid->particle[p].x2 = x2p;
            if (samp == 1)
                pGrid->particle[p].x2 += (-amp*sin(ky*x2p));
//                                          + 0.5*SQR(amp)*sin(2.0*ky*x2p))/ky;
            pGrid->particle[p].x3 = x3p;

            pGrid->particle[p].v1 = -v1x*sin(ky*x2p);
            pGrid->particle[p].v2 = v1y*cos(ky*x2p);
#ifndef FARGO
            pGrid->particle[p].v2 -= qshear*Omega_0*x1p;
#endif
            pGrid->particle[p].v3 = 0.0;

            pGrid->particle[p].pos = GetPosition(&pGrid->particle[p]);

            pGrid->particle[p].my_id = p;
#ifdef MPI_PARALLEL
            pGrid->particle[p].init_id = pGrid->my_id;
#endif
            p += 1;
          }
         }
        }
      }
    }
  }

#endif /* PARTICLES */

/* enroll gravitational potential function, shearing sheet BC functions */

  StaticGravPot = ShearingBoxPot;

  return;
}

/*==============================================================================
 * PROBLEM USER FUNCTIONS:
 * problem_write_restart() - writes problem-specific user data to restart files
 * problem_read_restart()  - reads problem-specific user data from restart files
 * get_usr_expr()          - sets pointer to expression for special output data
 * get_usr_out_fun()       - returns a user defined output function pointer
 * get_usr_par_prop()      - returns a user defined particle selection function
 * Userwork_in_loop        - problem specific work IN     main loop
 * Userwork_after_loop     - problem specific work AFTER  main loop
 *----------------------------------------------------------------------------*/

void problem_write_restart(Grid *pG, Domain *pD, FILE *fp)
{
  return;
}

void problem_read_restart(Grid *pG, Domain *pD, FILE *fp)
{
  Omega_0 = par_getd_def("problem","omega",1.0e-3);
  qshear  = par_getd_def("problem","qshear",1.5);

  StaticGravPot = ShearingBoxPot;
  return;
}

#if (NSCALARS > 0)
static Real ScalarDen(const Grid *pG, const int i, const int j, const int k)
{
  return pG->U[k][j][i].s[0]-1.0;
}
#endif

static Real diffd(const Grid *pG, const int i, const int j, const int k)
{
  return pG->U[k][j][i].d-1.0;
}

static Real expr_dV2(const Grid *pG, const int i, const int j, const int k)
{
  Real x1,x2,x3;
  cc_pos(pG,i,j,k,&x1,&x2,&x3);
#ifdef FARGO
  return pG->U[k][j][i].M2/pG->U[k][j][i].d;
#else
  return (pG->U[k][j][i].M2/pG->U[k][j][i].d + qshear*Omega_0*x1);
#endif
}

#ifdef PARTICLES
static Real diffdpar(const Grid *pG, const int i, const int j, const int k)
{
  return pG->Coup[k][j][i].grid_d-1.0;
}

static Real expr_dV2par(const Grid *pG, const int i, const int j, const int k)
{
  Real x1,x2,x3,v2par;
  cc_pos(pG,i,j,k,&x1,&x2,&x3);
#ifdef FARGO
  v2par = expr_V2par(pG, i, j, k);
#else
  v2par = expr_V2par(pG, i, j, k) + qshear*Omega_0*x1;
#endif
  return v2par;
}
#endif /* PARTICLES */

Gasfun_t get_usr_expr(const char *expr)
{
#if (NSCALARS > 0)
  if(strcmp(expr,"scalar")==0) return ScalarDen;
#endif
  if(strcmp(expr,"difd")==0) return diffd;
  if(strcmp(expr,"dVy")==0) return expr_dV2;
#ifdef PARTICLES
  if(strcmp(expr,"dVypar")==0) return expr_dV2par;
  if(strcmp(expr,"difdpar")==0) return diffdpar;
#endif
  return NULL;
}

VGFunout_t get_usr_out_fun(const char *name){
  return NULL;
}

#ifdef PARTICLES
PropFun_t get_usr_par_prop(const char *name)
{
  if (strcmp(name,"plane")==0) return property_mybin;
  return NULL;
}

void gasvshift(const Real x1, const Real x2, const Real x3,
                                    Real *u1, Real *u2, Real *u3)
{
  return;
}

void Userforce_particle(Vector *ft, const Real x1, const Real x2, const Real x3,
                                    const Real v1, const Real v2, const Real v3)
{
  return;
}
#endif

void Userwork_in_loop(Grid *pGrid, Domain *pDomain)
{
  return;
}

/*---------------------------------------------------------------------------
 * Userwork_after_loop: computes L1-error in linear waves,
 * ASSUMING WAVE HAS PROPAGATED AN INTEGER NUMBER OF PERIODS
 * Must set parameters in input file appropriately so that this is true
 */

void Userwork_after_loop(Grid *pGrid, Domain *pDomain)
{
  int i=0,j=0,k=0;
  int is,js,je,ks,n;
  Real x1,x2,x3,vshear,time;
  char *fname;

  is = (pGrid->is+pGrid->ie)/2;
  js = pGrid->js; je = pGrid->je;
  ks = pGrid->ks;

  time = pGrid->time;

  /* Bin particles to grid */
  particle_to_grid(pGrid, pDomain, property_all);

  /* Print error to file "Par_LinWave-errors.#.dat", where #=wavedir  */
  fname = ath_fname(NULL,"Par_Shwave2d-errors",0,0,NULL,"dat");
  /* Open output file in write mode */
  FILE *fid = fopen(fname,"w");

  /* Calculate the error and output */
  fprintf(fid, "%f	%d\n", time, je-js+1);
  for (j=js; j<=je; j++) {
    cc_pos(pGrid,is,j,ks,&x1,&x2,&x3);
#ifdef FARGO
    vshear = 0.0;
#else
    vshear = -qshear*Omega_0*x1;
#endif
    fprintf(fid,"%f	",x2);
    fprintf(fid,"%e	",pGrid->U[ks][j][is].d-1.0);
#ifdef PARTICLES
    fprintf(fid,"%e	%e	",pGrid->Coup[ks][j][is].grid_d-1.0,
                        pGrid->Coup[ks][j][is].grid_d-pGrid->U[ks][j][is].d);
#if (NSCALARS > 0)
    for (n=0; n<NSCALARS; n++)
      fprintf(fid,"%e	%e	",pGrid->U[ks][j][is].s[n]-1.0,
                        pGrid->Coup[ks][j][is].grid_d-pGrid->U[ks][j][is].s[n]);
#endif
#endif
    fprintf(fid,"\n");
  }

  fclose(fid);

  return;
}


/*=========================== PRIVATE FUNCTIONS ==============================*/
/*--------------------------------------------------------------------------- */
/* ShearingBoxPot:
 */

static Real ShearingBoxPot(const Real x1, const Real x2, const Real x3)
{
  Real phi=0.0;
#ifndef FARGO
  phi -= qshear*SQR(Omega_0*x1);
#endif
  return phi;
}

#ifdef PARTICLES
int GetPosition(Grain *gr)
{
  if ((gr->x1>=x1upar) || (gr->x1<x1lpar) || (gr->x2>=x2upar) || (gr->x2<x2lpar)                       || (gr->x3>=x3upar) || (gr->x3<x3lpar))
    return 10; /* crossing particle */
  else
    return 1;  /* grid particle */
}

/* user defined particle selection function (1: true; 0: false) */
static int property_mybin(Grain *gr)
{
  if ((gr->my_id<Nx12) && (gr->pos == 1))
    return 1;
  else
    return 0;
}
#endif /* PARTICLES */
