#ifndef PARTICLES_PROTOTYPES_H
#define PARTICLES_PROTOTYPES_H 
#include "../copyright.h"
/*==============================================================================
 * FILE: prototypes.h
 *
 * PURPOSE: Prototypes for all public functions in the src/particles
 *   directory.
 *============================================================================*/

#include <stdio.h>
#include <stdarg.h>
#include "../athena.h"
#include "../defs.h"

#include "../config.h"

/*----------------------------------------------------------------------------*/
#ifdef PARTICLES

/*----------------------------------------------------------------------------*/
/* feedback.c */
#ifdef FEEDBACK
void exchange_feedback(Grid *pG, Domain *pD);
void exchange_feedback_init(Grid *pG, Domain *pD);
void exchange_feedback_fun(enum Direction dir, VBCFun_t prob_bc);
void exchange_feedback_destruct(Grid *pG, Domain *pD);
#endif

/*----------------------------------------------------------------------------*/
/* init_particle.c */
void init_particle(Grid *pG, Domain *pD);
void particle_destruct(Grid *pG);
void particle_realloc(Grid *pG, long n);

/*----------------------------------------------------------------------------*/
/* integrators_particle.c */
void Integrate_Particles(Grid *pG, Domain *pD);
void int_par_exp   (Grid *pG, Grain *curG, Vector cell1, Real *dv1, Real *dv2, Real *dv3);
void int_par_semimp(Grid *pG, Grain *curG, Vector cell1, Real *dv1, Real *dv2, Real *dv3);
void int_par_fulimp(Grid *pG, Grain *curG, Vector cell1, Real *dv1, Real *dv2, Real *dv3);
#ifdef FEEDBACK
void feedback_predictor(Grid* pG);
void feedback_corrector(Grid *pG, Grain *gri, Grain *grf, Vector cell1, Real dv1, Real dv2, Real dv3);
#endif

/*----------------------------------------------------------------------------*/
/* output_particle.c */
void particle_to_grid(Grid *pG, Domain *pD, PropFun_t par_prop);
void dump_particle_binary(Grid *pG, Domain *pD, Output *pOut);
int  property_all(Grain *gr);

/*----------------------------------------------------------------------------*/
/* set_bvals_particle.c */
void set_bvals_particle(Grid *pG, Domain *pD);
#ifdef FARGO
void advect_particles(Grid *pG, Domain *pD);
#endif
void set_bvals_particle_init(Grid *pG, Domain *pD);
void set_bvals_particle_fun(enum Direction dir, VBCFun_t prob_bc);
void set_bvals_final_particle(Grid *pG, Domain *pD);

/*----------------------------------------------------------------------------*/
/* utils_particle.c */
void get_gasinfo(Grid *pG);

void getwei_linear(Grid *pG, Real x1, Real x2, Real x3, Vector cell1, Real weight[3][3][3], int *is, int *js, int *ks);
void getwei_TSC   (Grid *pG, Real x1, Real x2, Real x3, Vector cell1, Real weight[3][3][3], int *is, int *js, int *ks);
void getwei_QP    (Grid *pG, Real x1, Real x2, Real x3, Vector cell1, Real weight[3][3][3], int *is, int *js, int *ks);
int  getvalues(Grid *pG, Real weight[3][3][3], int is, int js, int ks, Real *rho, Real *u1, Real *u2, Real *u3, Real *cs);

Real get_ts_epstein(Grid *pG, int type, Real rho, Real cs, Real vd);
Real get_ts_general(Grid *pG, int type, Real rho, Real cs, Real vd);
Real get_ts_fixed  (Grid *pG, int type, Real rho, Real cs, Real vd);

#ifdef FEEDBACK
void feedback_clear(Grid *pG);
void distrFB      (Grid *pG, Real weight[3][3][3], int is, int js, int ks, Vector fb);
#endif

void shuffle(Grid* pG);

void gasvshift_zero(Real x1, Real x2, Real x3, Real *u1, Real *u2, Real *u3);
void User_ParticleForce_Zero(Vector *ft, Real x1, Real x2, Real x3, Real *w1, Real *w2, Real *w3);

#endif /* PARTICLES */

#endif /* PARTICLES_PROTOTYPES_H */
