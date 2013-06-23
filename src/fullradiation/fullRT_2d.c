#include "../copyright.h"
/*==============================================================================
 * FILE: formal_solution.c
 *
 * PURPOSE: integrator for the full radiation transfer in 2D 
 *
 * CONTAINS PUBLIC FUNCTIONS: 
 *   formal_solution()  - interate formal solution until convergence
 *============================================================================*/

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "../defs.h"
#include "../athena.h"
#include "../globals.h"
#include "../prototypes.h"

#ifdef FULL_RADIATION_TRANSFER


static Real *****Divi = NULL;	/* temporary array to store flux for each array */
static Real *flux = NULL;
static Real *fluxsource3 = NULL;
static Real *tempimu = NULL;
static Real *vsource1 = NULL;
static Real *vsource2 = NULL;
static Real *vsource3 = NULL;
static Real *tempV = NULL;
static Real *tempAdv = NULL; /* temporary array for advection flux */
static Real *tempS = NULL;
static Real *tempSV= NULL;
static Real ***RHS = NULL; /* Right hand side of the local matrix for scattering opacity for each cell */
static Real ***Mb = NULL; /* The common elements for each angle */
static Real ***Ma = NULL; /* The special diagonal elements for each angle (each line) */
static Real ***Mc = NULL; /* Matrix elements for the second special matrix */
static Real *Md = NULL;
static Real **lN = NULL;
static Real **UN = NULL; /* Upper triangle matrix in LU decomposition */
/* calculate the upwind and downwind intensity for k,j,i, frequency ifr, octant l and angle n */
/* This function is only used to calculate for one ray */
/* in 2D, the octant is numbered as *
 *           | 
 *	1    |    0 
 * ---------------------
 *	3    |    2
 *	     |
 ********************/



void fullRT_2d(DomainS *pD)
{

	RadGridS *pRG=(pD->RadGrid);
	GridS *pG = pD->Grid;
	Real dt = pG->dt;
	int i, is, ie;
	int j, js, je;
	int ks;
	is = pRG->is; ie = pRG->ie;
	js = pRG->js; je = pRG->je;
	ks = pRG->ks;

	int l, n, ifr, m, Mi;
	int offset;

	offset = nghost - Radghost;

	Real dx1, dx2, ds, alpha, sigma;
	dx1 = pRG->dx1;
	dx2 = pRG->dx2;

	Real imu[5];
	Real vel, velsource3;
	Real sigmas, sigmaa, AngleV, AngleV2, vx, vy, vz, miux, miuy, miuz;


	
	
	for(ifr=0; ifr<pRG->nf; ifr++){

		/*------------------------------------------------------*/
		for(l=0; l<pRG->noct; l++){
			for(n=0; n<pRG->nang; n++){

				/* Now calculate the x flux */
				ds = dx1;	
				for(j=js; j<=je; j++){	
					for(i=0; i<=ie+Radghost; i++){
						/* First, prepare the array */
						sigmas = pRG->R[ifr][0][j][i].Sigma[2];
                                                /* The absorption opacity in front of I */
                                                sigmaa = pRG->R[ifr][0][j][i].Sigma[1];
                                                miux = pRG->mu[l][n][ks][j][i][0]; 
                                                miuy = pRG->mu[l][n][ks][j][i][1];
                                                miuz = pRG->mu[l][n][ks][j][i][2];
                                                vx = pG->U[0][j+offset][i+offset].M1 / pG->U[0][j+offset][i+offset].d;
                                                vy = pG->U[0][j+offset][i+offset].M2 / pG->U[0][j+offset][i+offset].d;
                                                vz = pG->U[0][j+offset][i+offset].M3 / pG->U[0][j+offset][i+offset].d;
                                                AngleV = miux * vx + miuy * vy + miuz * vz;
						AngleV2 = vx * vx * pRG->R[ifr][ks][j][i].K[0] + 2.0 * vx * vy * pRG->R[ifr][ks][j][i].K[1] 
							+ 2.0 * vx * vz * pRG->R[ifr][ks][j][i].K[3] + vy * vy * pRG->R[ifr][ks][j][i].K[2] 
							+ 2.0 * vy * vz * pRG->R[ifr][ks][j][i].K[4] + vz * vz * pRG->R[ifr][ks][j][i].K[5];

						if((sigmas + sigmaa) > TINY_NUMBER){
							vsource1[i] = AngleV * (pRG->imu[ifr][l][n][ks][j][i]);
							vsource2[i] = AngleV * (3.0 * pRG->R[ifr][ks][j][i].J);
							vsource3[i] = -2.0 * sigmas * (vx * pRG->R[ifr][ks][j][i].H[0] + vy * pRG->R[ifr][ks][j][i].H[1] 
									+ vz * pRG->R[ifr][ks][j][i].H[2])/(sigmaa+sigmas)
							+ (sigmas-sigmaa)* ((vx*vx+vy*vy+vz*vz)*pRG->R[ifr][ks][j][i].J+AngleV2)/(Crat*(sigmas+sigmaa));
							tempS[i] = miux * miux * (3.0 * pRG->R[ifr][ks][j][i].J + pRG->imu[ifr][l][n][ks][j][i]);
							tempSV[i] = vx + miuy * vy/miux + miuz * vz/miux;
						}else{
							vsource1[i] = 0.0;
							vsource2[i] = 0.0;
							vsource3[i] = 0.0;
							tempS[i] = 0.0;
							tempSV[i] = 0.0;
							
						}
						tempimu[i] = pRG->mu[l][n][ks][j][i][0] * (pRG->imu[ifr][l][n][ks][j][i] - (vsource1[i] + vsource2[i] + vsource3[i])/Crat);
						ReduceVelocity(sigmaa+sigmas, dx1, &alpha);	
						tempV[i] = Crat * alpha;		
					}

					/* first, calculate the advection part v(3J+I) */
					flux_AdvJ(tempS, tempSV, is, ie+1, dx1, dt, tempAdv);
					for(i=is; i<=ie; i++)
						 Divi[ifr][l][n][j][i] = (tempAdv[i+1] - tempAdv[i]);
						
					if((l == 0) || (l == 2)){
						for(i=is; i<=ie+1; i++){
							/* This is only true for constant angle */
							/* Need to consider when advection with non-uniform velocity */
							vel = 0.5 * (tempV[i-1]+tempV[i]);
							velsource3 = 0.5 * (pRG->mu[l][n][ks][j][i-1][0] + pRG->mu[l][n][ks][j][i][0]);
							/* From small i to large i */
#ifdef SECOND_ORDER_PRIM
							for(m=0; m<3; m++){
								imu[m] = tempimu[i-2+m];
							}

							

							flux_PLM(dt, ds, vel, imu, &(flux[i]));

							/* Now calculate flux due to vsource3 */
							for(m=0; m<3; m++){
                                                                imu[m] = vsource3[i-2+m];
                                                        }

							flux_PLM(dt, ds, velsource3, imu, &(fluxsource3[i]));
#else							/*------------------------------------------------------------------*/
							for(m=0; m<5; m++){
								imu[m] = tempimu[i-3+m];

							}

							flux_PPM(dt, ds, vel, imu, &(flux[i]));

							/* flux due to vsource3 */
							for(m=0; m<5; m++){
                                                                imu[m] = vsource3[i-3+m];

                                                        }

                                                        flux_PPM(dt, ds, velsource3, imu, &(fluxsource3[i]));

#endif							
													
							/* multiple flux with velocity and dtods */
							fluxsource3[i] *= (velsource3 * dt/dx1);
						}/* end i */		
					}/* end l==0 or l == 2 */
					else{
						for(i=is-1; i<=ie; i++){
							/* From large i to small i */
                                                        vel = 0.5 * (tempV[i]+tempV[i+1]);
							velsource3 = 0.5 * (pRG->mu[l][n][ks][j][i+1][0] + pRG->mu[l][n][ks][j][i][0]);
#ifdef SECOND_ORDER_PRIM
							for(m=2; m>=0; m--){
								imu[m] = tempimu[i+2-m];					
							}

							flux_PLM(dt, ds, vel, imu, &(flux[i+1]));	

							/* flux due to vsource3 */
                                                        for(m=2; m>=0; m--){
                                                                imu[m] = vsource3[i+2-m];
                                                        }
                                                
                                                        flux_PLM(dt, ds, velsource3, imu, &(fluxsource3[i+1]));
	

#else							/*------------------------------------------------------------------*/

							for(m=4; m>=0; m--){
								imu[m] = tempimu[i+3-m];

							}

							flux_PPM(dt, ds, vel, imu, &(flux[i+1]));

							/* flux due to vsource3 */
						
							for(m=4; m>=0; m--){
                                                                imu[m] = vsource3[i+3-m];

                                                        }

                                                        flux_PPM(dt, ds, velsource3, imu, &(fluxsource3[i+1]));
#endif	
							fluxsource3[i+1] *= (velsource3 * dt/dx1);
						} /* end i */
					}/* end l == 1 or l == 3 */



		   			for(i=is; i<=ie; i++){
						Divi[ifr][l][n][j][i] += (Crat * dt * (flux[i+1] - flux[i]) /(dx1)
									+ (fluxsource3[i+1] - fluxsource3[i]));	
		   			}/* end i */
				}/* End j */

				ds = dx2;
	/* Now calculate the flux along j direction */
				for(i=is; i<=ie; i++){
					/* first save the data */

					for(j=0; j<=je+Radghost; j++){
                                                /* First, prepare the array */
                                                sigmas = pRG->R[ifr][0][j][i].Sigma[2];
                                                /* The absorption opacity in front of I */
                                                sigmaa = pRG->R[ifr][0][j][i].Sigma[1];
                                                miux = pRG->mu[l][n][ks][j][i][0];
                                                miuy = pRG->mu[l][n][ks][j][i][1];
                                                miuz = pRG->mu[l][n][ks][j][i][2];
                                                vx = pG->U[0][j+offset][i+offset].M1 / pG->U[0][j+offset][i+offset].d;
                                                vy = pG->U[0][j+offset][i+offset].M2 / pG->U[0][j+offset][i+offset].d;
                                                vz = pG->U[0][j+offset][i+offset].M3 / pG->U[0][j+offset][i+offset].d;
                                                AngleV = miux * vx + miuy * vy + miuz * vz;
                                                AngleV2 = vx * vx * pRG->R[ifr][ks][j][i].K[0] + 2.0 * vx * vy * pRG->R[ifr][ks][j][i].K[1]
                                                        + 2.0 * vx * vz * pRG->R[ifr][ks][j][i].K[3] + vy * vy * pRG->R[ifr][ks][j][i].K[2]
                                                        + 2.0 * vy * vz * pRG->R[ifr][ks][j][i].K[4] + vz * vz * pRG->R[ifr][ks][j][i].K[5];

                                                if((sigmas + sigmaa) > TINY_NUMBER){
                                                        vsource1[j] = AngleV * (pRG->imu[ifr][l][n][ks][j][i]);
                                                        vsource2[j] = AngleV * (3.0 * pRG->R[ifr][ks][j][i].J);
                                                        vsource3[j] = -2.0 * sigmas * (vx * pRG->R[ifr][ks][j][i].H[0] + vy * pRG->R[ifr][ks][j][i].H[1]
                                                                        + vz * pRG->R[ifr][ks][j][i].H[2])/(sigmaa+sigmas)
                                                        + (sigmas-sigmaa)* ((vx*vx+vy*vy+vz*vz)*pRG->R[ifr][ks][j][i].J+AngleV2)/(Crat*(sigmas+sigmaa));
                                                        tempS[j] = miuy * miuy * (3.0 * pRG->R[ifr][ks][j][i].J + pRG->imu[ifr][l][n][ks][j][i]);
                                                        tempSV[j] = miux * vx/miuy + vy + miuz * vz/miuy;
                                                }else{
                                                        vsource1[j] = 0.0;
                                                        vsource2[j] = 0.0;
                                                        vsource3[j] = 0.0;
                                                        tempS[j] = 0.0;
                                                        tempSV[j] = 0.0;

                                                }
                                                tempimu[j] = pRG->mu[l][n][ks][j][i][1] * (pRG->imu[ifr][l][n][ks][j][i] - (vsource1[j] + vsource2[j] + vsource3[j])/Crat);
                                                ReduceVelocity(sigmaa+sigmas, dx2, &alpha);
                                                tempV[j] = Crat * alpha;
                                        }

                                        /* first, calculate the advection part v(3J+I) */
                                        flux_AdvJ(tempS, tempSV, js, je+1, dx2, dt, tempAdv);
                                        for(j=js; j<=je; j++)
                                                 Divi[ifr][l][n][j][i] += (tempAdv[j+1] - tempAdv[j]);


					if((l == 0) || (l == 1)){
						for(j=js; j<=je+1; j++){
							/* Need to consider when advection with non-uniform velocity */
                                                        vel = 0.5 * (tempV[j-1]+tempV[j]);
                                                        velsource3 = 0.5 * (pRG->mu[l][n][ks][j-1][i][1] + pRG->mu[l][n][ks][j][i][1]);
	
#ifdef SECOND_ORDER_PRIM
							for(m=0; m<3; m++)
								imu[m] = tempimu[j-2+m];
							
							flux_PLM(dt, ds, vel, imu, &(flux[j]));
							/*--------------------------------------*/
							/* Now calculate flux due to vsource3 */
                                                        for(m=0; m<3; m++){
                                                                imu[m] = vsource3[j-2+m];
                                                        }

                                                        flux_PLM(dt, ds, velsource3, imu, &(fluxsource3[j]));
	
#else
							for(m=0; m<5; m++)
								imu[m] = tempimu[j-3+m];

							flux_PPM(dt, ds, vel, imu, &(flux[j]));

							/* flux due to vsource3 */
							for(m=0; m<5; m++){
                                                                imu[m] = vsource3[j-3+m];
                                                        }

                                                        flux_PPM(dt, ds, velsource3, imu, &(fluxsource3[j]));
#endif						

							/* multiple flux with velocity and dtods */
							fluxsource3[j] *= (velsource3 * dt/dx2);


						}/* end j */
					}/* end l ==0 or l == 1 */
					else{
						for(j=js-1; j<=je; j++){
							vel = 0.5 * (tempV[j]+tempV[j+1]);
							velsource3 = 0.5 * (pRG->mu[l][n][ks][j+1][i][1] + pRG->mu[l][n][ks][j][i][1]);
#ifdef SECOND_ORDER_PRIM
							for(m=2; m>=0; m--)
								imu[m] = tempimu[j+2-m];
			
							flux_PLM(dt, ds, vel, imu, &(flux[j+1]));	

							/*-----------------------------------*/
							for(m=2; m>=0; m--)
                                                                imu[m] = vsource3[j+2-m];

                                                        flux_PLM(dt, ds, velsource3, imu, &(fluxsource3[j+1]));
#else
							for(m=4; m>=0; m--)
								imu[m] = tempimu[j+3-m];

							flux_PPM(dt, ds, vel, imu, &(flux[j+1]));

							/*----------------------------------------*/
							 for(m=4; m>=0; m--)
                                                                imu[m] = vsource3[j+3-m];

                                                        flux_PPM(dt, ds, velsource3, imu, &(fluxsource3[j+1]));

#endif				
							fluxsource3[j+1] *= (velsource3 * dt/dx2);
						} /* end j */				
					}/* end l == 2 or l == 3 */				
						
		/* Now save the flux difference */
		
		
		   			for(j=js; j<=je; j++){
						Divi[ifr][l][n][j][i] += (Crat * dt * (flux[j+1] - flux[j]) /(dx2)
								+ (fluxsource3[j+1] - fluxsource3[j]));
					} /* end j */
				} /* Finish i */


			}/* end n */
		}/* end l */
	}/* end ifr */

	/* Now we have flux, now add the source terms due to absorption and scattering opacity seperately */ 


	/*--------------------------------------------------------------------*/
	/* The energy change due to T^4/4pi - I and T^4/4pi - J term is already calculated in pRG->heatcool */
	/* All the other terms in the transfer equations are solved together
	 * The independent variable for the matrix solver is actually weight * I
	 * Ma[n] is the diagonal part for each I
	 * Mc[n] is the for the part ndot v (sigma_a+sigma_s)* 3J 
	 * Mb[n] is for the other momentums terms *
	*/
	for(ifr=0; ifr<pRG->nf; ifr++){
		for(l=0; l<pRG->noct; l++){
                        for(n=0; n<pRG->nang; n++){
                                for(j=js; j<=je; j++){
                                        for(i=is; i<=ie; i++){
				/* first construct the Right hand side */
			        /* We need an array for different angles for each cell. The index order of RHS is different */
						Mi = l*(pRG->nang)+n;
                                                RHS[j][i][Mi] = pRG->imu[ifr][l][n][ks][j][i]- Divi[ifr][l][n][j][i] 
								+ pRG->heatcool[ifr][l][n][ks][j][i];
					 	sigmas = pRG->R[ifr][0][j][i].Sigma[2];
						/* The absorption opacity in front of I */
                                                sigmaa = pRG->R[ifr][0][j][i].Sigma[1];
						miux = pRG->mu[l][n][ks][j][i][0];
                                                miuy = pRG->mu[l][n][ks][j][i][1];
                                                miuz = pRG->mu[l][n][ks][j][i][2];
                                                vx = pG->U[0][j+offset][i+offset].M1 / pG->U[0][j+offset][i+offset].d;
                                                vy = pG->U[0][j+offset][i+offset].M2 / pG->U[0][j+offset][i+offset].d;
                                                vz = pG->U[0][j+offset][i+offset].M3 / pG->U[0][j+offset][i+offset].d;
                                                AngleV = miux * vx + miuy * vy + miuz * vz;	
						AngleV2 = vx * vx * miux * miux + vy * vy * miuy * miuy + vz * vz * miuz * miuz
                                                        + 2.0 * vx * vy * miux * miuy + 2.0 * vx * vz * miux * miuz
                                                        + 2.0 * vy * vz * miuy * miuz;
                                                Ma[j][i][Mi] = (1.0 + dt * (sigmas * Crat - (sigmaa + sigmas) * AngleV))/pRG->wmu[n][0][j][i];
                                                Mc[j][i][Mi] = -dt * (sigmas * Crat + 3.0 * AngleV * (sigmas + sigmaa));
						Mb[j][i][Mi] = dt * 2.0 * sigmas * AngleV 
						             + dt * (sigmaa - sigmas) * (vx * vx + vy * vy + vz * vz + AngleV2)/Crat; 
						
					}/* end i */
				}/* end j */
			}/* end n */
		}/* end l */


		/* solve the first matrix */

		for(j=js; j<=je; j++)
			for(i=is; i<=ie; i++){
				SpecialMatrix3(Ma[j][i], Mb[j][i], Mc[j][i], Md, RHS[j][i], lN, UN,  pRG->nang*pRG->noct);
		}

		/* Set the right hand side of the second matrix with the partially updated solution */
		/* The partially updated solution is RHS/wmu */
		for(l=0; l<pRG->noct; l++){
                        for(n=0; n<pRG->nang; n++){
                                for(j=js; j<=je; j++){
                                        for(i=is; i<=ie; i++){
						Mi = l*(pRG->nang)+n;
                                                pRG->imu[ifr][l][n][ks][j][i] = RHS[j][i][Mi]/pRG->wmu[n][0][j][i];

					}
				}
			}
		}

	}/* End ifr */




	/* Moments are updated in the main loop */
	

  return;
}



void fullRT_2d_destruct(void)
{

  if(Divi != NULL) free_5d_array(Divi);	
  if(flux != NULL) free_1d_array(flux);
  if(fluxsource3 != NULL) free_1d_array(fluxsource3);
  if(tempimu != NULL) free_1d_array(tempimu);
  if(tempS != NULL) free_1d_array(tempS);
  if(tempSV != NULL) free_1d_array(tempSV);
  if(vsource1 != NULL) free_1d_array(vsource1);
  if(vsource2 != NULL) free_1d_array(vsource2);
  if(vsource3 != NULL) free_1d_array(vsource3);
  if(tempV != NULL) free_1d_array(tempV);
  if(tempAdv != NULL) free_1d_array(tempAdv);
  if(RHS != NULL) free_3d_array(RHS);
  if(Ma != NULL) free_3d_array(Ma);
  if(Mb != NULL) free_3d_array(Mb);
  if(Mc != NULL) free_3d_array(Mc);
  if(Md != NULL) free_1d_array(Md);	
  if(lN != NULL) free_2d_array(lN);
  if(UN != NULL) free_2d_array(UN);

  return;
}


void fullRT_2d_init(RadGridS *pRG)
{

	
	int nx1 = pRG->Nx[0], nx2 = pRG->Nx[1];
	int nfr = pRG->nf, noct = pRG->noct, nang = pRG->nang;
	int nmax;


	nmax = MAX(nx1,nx2);

	if ((flux = (Real *)calloc_1d_array( nmax+2*Radghost, sizeof(Real))) == NULL)
    		goto on_error;

        if ((fluxsource3 = (Real *)calloc_1d_array( nmax+2*Radghost, sizeof(Real))) == NULL)
                goto on_error;

	if ((Divi = (Real *****)calloc_5d_array(nfr, noct, nang, nx2+2*Radghost, nx1+2*Radghost, sizeof(Real))) == NULL)
    		goto on_error;
	
	if ((tempimu = (Real *)calloc_1d_array(nmax+2*Radghost, sizeof(Real))) == NULL)
		goto on_error;

        if ((tempS = (Real *)calloc_1d_array(nmax+2*Radghost, sizeof(Real))) == NULL)
                goto on_error;
	
	if ((tempSV = (Real *)calloc_1d_array(nmax+2*Radghost, sizeof(Real))) == NULL)
                goto on_error;

	if ((vsource1 = (Real *)calloc_1d_array(nmax+2*Radghost, sizeof(Real))) == NULL)
                goto on_error;

	if ((vsource2 = (Real *)calloc_1d_array(nmax+2*Radghost, sizeof(Real))) == NULL)
                goto on_error;

	if ((vsource3 = (Real *)calloc_1d_array(nmax+2*Radghost, sizeof(Real))) == NULL)
                goto on_error;

        if ((tempV = (Real *)calloc_1d_array(nmax+2*Radghost, sizeof(Real))) == NULL)
                goto on_error;

	if ((tempAdv = (Real *)calloc_1d_array(nmax+2*Radghost, sizeof(Real))) == NULL)
                goto on_error;

        if ((RHS = (Real ***)calloc_3d_array(nx2+2*Radghost,nx1+2*Radghost,noct*nang, sizeof(Real))) == NULL)
                goto on_error;

	if ((Ma = (Real ***)calloc_3d_array(nx2+2*Radghost,nx1+2*Radghost,noct*nang, sizeof(Real))) == NULL)
                goto on_error;

	if ((Mb = (Real ***)calloc_3d_array(nx2+2*Radghost,nx1+2*Radghost,noct*nang, sizeof(Real))) == NULL)
                goto on_error;
	
	if ((Mc = (Real ***)calloc_3d_array(nx2+2*Radghost,nx1+2*Radghost,noct*nang, sizeof(Real))) == NULL)
                goto on_error;

        if ((Md = (Real *)calloc_1d_array(noct*nang, sizeof(Real))) == NULL)
                goto on_error;
	
	
	if ((lN = (Real **)calloc_2d_array(noct*nang, noct*nang, sizeof(Real))) == NULL)
                goto on_error;

	if ((UN = (Real **)calloc_2d_array(noct*nang, noct*nang, sizeof(Real))) == NULL)
                goto on_error;

	return;

	on_error:
  	fullRT_2d_destruct();
  	ath_error("[fullRT_2d_init]: Error allocating memory\n");
  	return;




}
#endif /* FULL_RADIATION_TRANSFER */
