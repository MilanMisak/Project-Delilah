#ifndef THREADS_FIXED-POINT_H
#define THREADS_FIXED-POINT_H

/*
Implementation for fixed-point real arithmetic.
The lowest 14 bits of a signed integer are fractional bits.
*/

/* f = 2 ** 14 */
#define FP_F 16384


/* Converts n to fixed point. */
#define FP_TO_FIXED_POINT(N)    ((N) * FP_F)

/* Convertx x to integer (rounding toward zero). */
#define FP_TO_INT_TRUNCATE(X)   ((X) / FP_F)

/* Converts x to integer (rounding to nearest). */
#define FP_TO_INT_ROUND(X)  (((X) >= 0) ? ((X) + FP_F / 2) : ((X) - FP_F / 2))


/* Adds x and y. */
#define FP_ADD(X,Y)             ((X) + (Y))

/* Adds x and n where n is an ordinary int, not a fixed-point number. */
#define FP_ADD_INT(X,N)         ((X) + (N) * FP_F)

/* Subtracts y from x. */
#define FP_SUBTRACT(X,Y)        ((X) - (Y))

/* Subtracts n from x where n is an ordinary int, not a fixed-point number. */
#define FP_SUBTRACT_INT(X,N)    ((X) - (N) * FP_F)

/* Multiplies x by y. */
#define FP_MULTIPLY(X,Y)        (((int64_t) (X)) * (Y) / FP_F)

/* Multiplies x by y where n is an ordinary int, not a fixed-point number. */
#define FP_MULTIPLY_INT(X,N)    ((X) * (N))

/* Divides x by y. */
#define FP_DIVIDE(X,Y)          (((int64_t) (X)) * FP_F / (Y))

/* Divides x by y where n is an ordinary int, not a fixed-point number. */
#define FP_DIVIDE_INT(X,N)    ((X) / (N))

#endif /* threads/fixed-point.h */
