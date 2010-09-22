#include "../../copyright.h"
/*==============================================================================
 * FILE: ludcmp.c
 *
 * PURPOSE: This file is adopted from Numerical Recipes to solve a set of 
 * linear equations with LU decomposition
 *
 * CONTAINS PUBLIC FUNCTIONS: 
 *   ludcmp()
 *   lubksb()
 *============================================================================*/




#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../defs.h"
#include "../../athena.h"
#include "../../globals.h"
#include "../prototypes.h"
#include "../../prototypes.h"


void ludcmp(Real **a, int n, int *indx, Real *d)
{
	int i,imax,j,k;
	Real big,dum,sum,temp;
	Real *vv;

	if ((vv = (Real*)malloc(n*sizeof(Real))) == NULL) goto on_error;
	*d=1.0;
	for (i=0;i<n;i++) {
		big=0.0;
		for (j=0;j<n;j++)
			if ((temp=fabs(a[i][j])) > big) big=temp;
		if (big == 0.0) ath_error("[ludcmp]: Singular matrix in routine ludcmp\n");
		vv[i]=1.0/big;
	}
	
	for (j=0;j<n;j++) {
		for (i=0;i<j;i++) {
			sum=a[i][j];
			for (k=0;k<i;k++) sum -= a[i][k]*a[k][j];
			a[i][j]=sum;
		}
		
		big=0.0;
		for (i=j;i<n;i++) {
			sum=a[i][j];
			for (k=0;k<j;k++)
				sum -= a[i][k]*a[k][j];
			a[i][j]=sum;
			if ( (dum=vv[i]*fabs(sum)) >= big) {
				big=dum;
				imax=i;
			}
		}
		
		if (j != imax) {
			for (k=0;k<n;k++) {
				dum=a[imax][k];
				a[imax][k]=a[j][k];
				a[j][k]=dum;
			}
			*d = -(*d);
			vv[imax]=vv[j];
		}
		
		indx[j]=imax;
		if (a[j][j] == 0.0) a[j][j]=TINY_NUMBER;
		if (j != (n-1)) {
			dum=1.0/(a[j][j]);
			for (i=j+1;i<n;i++) a[i][j] *= dum;
		}
	}
	
	if (vv 	!= NULL) free(vv);

	return;

	on_error:

	ath_error("[ludcmp]: malloc returned a NULL pointer\n");
}




void lubksb(Real **a, int n, int *indx, Real b[])
{
	int i,ii=0,ip,j;
	Real sum;

	for (i=0;i<n;i++) {
		ip=indx[i];
		sum=b[ip];
		b[ip]=b[i];
		if (ii)
			for (j=ii;j<=i-1;j++) sum -= a[i][j]*b[j];
		else if (sum) ii=i;
		b[i]=sum;
	}
	for (i=n-1;i>=0;i--) {
		sum=b[i];
		for (j=i+1;j<=n;j++) sum -= a[i][j]*b[j];
		b[i]=sum/a[i][i];
	}
}
