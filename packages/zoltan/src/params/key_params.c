#include <stdio.h>
#include "lb_const.h"
#include "params_const.h"

/* 
 * Handle parameter changes for variables stored in LB object.
 * Currently, only example is Tolerance.
 */

int LB_Set_Key_Param(
LB *lb,                         /* load balance object */
char *name,			/* name of variable */
char *val)			/* value of variable */
{
    int status;			/* return code */
    PARAM_UTYPE result;		/* value returned from Check_Param */
    int index;			/* index returned from Check_Param */
    PARAM_VARS key_params[] = {
	{ "IMBALANCE_TOL", NULL, "DOUBLE" },
	{ NULL, NULL, NULL } };

    status = LB_Check_Param(name, val, key_params, &result, &index);

    if (status == 0) {		/* Imbalance_Tol */
	if (result.dval > 1.0) {
	    fprintf(stderr, "WARNING: Invalid Imbalance_Tol value (%g) "
		"being set to 1.0\n", result.dval);
	    result.dval = 1.0;
	}
	if (result.dval < 0.0) {
	    fprintf(stderr, "WARNING: Invalid Imbalance_Tol value (%g) "
		"being set to 0.0\n", result.dval);
	    result.dval = 0.0;
	}
	lb->Tolerance = result.dval;
	status = 3;		/* Don't add to Params field of LB */
    }

    return(status);
}
