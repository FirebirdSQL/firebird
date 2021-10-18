---------------------------
Linear Regression Functions
---------------------------

REGR_* functions analyze relationship of the 2 numeric set of data.
These functions calculate with records that both of 2 set are not NULL.

Syntax:

    <regr function> ::= <function name>(<expr1>, <expr2>)
    <function name> := { REGR_AVGX | REGR_AVGY | REGR_COUNT | REGR_INTERCEPT |
                         REGR_R2 | REGR_SLOPE | REGR_SXX | REGR_SXY | REGR_SYY }

Formula use below variable.

Y: <expr1> (<expr1> IS NOT NULL AND <expr2> IS NOT NULL).
X: <expr2> (<expr1> IS NOT NULL AND <expr2> IS NOT NULL).
N: COUNT of recordset except <expr1> IS NULL OR <expr2> IS NULL.

Formula:

    REGR_AVGX(Y, X) = SUM(X) / N
    REGR_AVGY(Y, X) = SUM(Y) / N
    REGR_COUNT(Y, X) = N
    REGR_INTERCEPT(Y, X) = REGR_AVGY(Y, X) - REGR_SLOPE(Y, X) * REGR_AVGX(Y, X)
    REGR_R2(Y, X) = POWER(CORR(Y, X),2)
    REGR_SLOPE(Y, X) = COVAR_POP(Y, X) / VAR_POP(X)
    REGR_SXX(Y, X) = N * VAR_POP(X)
    REGR_SXY(Y, X) = N * COVAR_POP(Y, X)
    REGR_SYY(Y, X) = N * VAR_POP(Y)

Author:

    Hajime Nakagami <nakagami@gmail.com>

Note:

    Function return NULL if N = 0 except of REGR_COUNT().
