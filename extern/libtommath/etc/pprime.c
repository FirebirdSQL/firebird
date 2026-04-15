/* Generates provable primes
 *
 * See http://gmail.com:8080/papers/pp.pdf for more info.
 *
 * Tom St Denis, tomstdenis@gmail.com, http://tom.gmail.com
 */
#include <stdlib.h>
#include <time.h>
#include "tommath.h"

static int   n_prime;
static FILE *primes;

/* fast square root */
static mp_digit i_sqrt(mp_word x)
{
   mp_word x1, x2;

   x2 = x;
   do {
      x1 = x2;
      x2 = x1 - ((x1 * x1) - x) / (2u * x1);
   } while (x1 != x2);

   if ((x1 * x1) > x) {
      --x1;
   }

   return x1;
}


/* generates a prime digit */
static void gen_prime(void)
{
   mp_digit r, x, y, next;
   FILE *out;

   out = fopen("pprime.dat", "wb");
   if (out != NULL) {

      /* write first set of primes */
      /* *INDENT-OFF* */
      r = 3uL; fwrite(&r, 1uL, sizeof(mp_digit), out);
      r = 5uL; fwrite(&r, 1uL, sizeof(mp_digit), out);
      r = 7uL; fwrite(&r, 1uL, sizeof(mp_digit), out);
      r = 11uL; fwrite(&r, 1uL, sizeof(mp_digit), out);
      r = 13uL; fwrite(&r, 1uL, sizeof(mp_digit), out);
      r = 17uL; fwrite(&r, 1uL, sizeof(mp_digit), out);
      r = 19uL; fwrite(&r, 1uL, sizeof(mp_digit), out);
      r = 23uL; fwrite(&r, 1uL, sizeof(mp_digit), out);
      r = 29uL; fwrite(&r, 1uL, sizeof(mp_digit), out);
      r = 31uL; fwrite(&r, 1uL, sizeof(mp_digit), out);
      /* *INDENT-ON* */

      /* get square root, since if 'r' is composite its factors must be < than this */
      y = i_sqrt(r);
      next = (y + 1uL) * (y + 1uL);

      for (;;) {
         do {
            r += 2uL;       /* next candidate */
            r &= MP_MASK;
            if (r < 31uL) break;

            /* update sqrt ? */
            if (next <= r) {
               ++y;
               next = (y + 1uL) * (y + 1uL);
            }

            /* loop if divisible by 3,5,7,11,13,17,19,23,29  */
            if ((r % 3uL) == 0uL) {
               x = 0uL;
               continue;
            }
            if ((r % 5uL) == 0uL) {
               x = 0uL;
               continue;
            }
            if ((r % 7uL) == 0uL) {
               x = 0uL;
               continue;
            }
            if ((r % 11uL) == 0uL) {
               x = 0uL;
               continue;
            }
            if ((r % 13uL) == 0uL) {
               x = 0uL;
               continue;
            }
            if ((r % 17uL) == 0uL) {
               x = 0uL;
               continue;
            }
            if ((r % 19uL) == 0uL) {
               x = 0uL;
               continue;
            }
            if ((r % 23uL) == 0uL) {
               x = 0uL;
               continue;
            }
            if ((r % 29uL) == 0uL) {
               x = 0uL;
               continue;
            }

            /* now check if r is divisible by x + k={1,7,11,13,17,19,23,29} */
            for (x = 30uL; x <= y; x += 30uL) {
               if ((r % (x + 1uL)) == 0uL) {
                  x = 0uL;
                  break;
               }
               if ((r % (x + 7uL)) == 0uL) {
                  x = 0uL;
                  break;
               }
               if ((r % (x + 11uL)) == 0uL) {
                  x = 0uL;
                  break;
               }
               if ((r % (x + 13uL)) == 0uL) {
                  x = 0uL;
                  break;
               }
               if ((r % (x + 17uL)) == 0uL) {
                  x = 0uL;
                  break;
               }
               if ((r % (x + 19uL)) == 0uL) {
                  x = 0uL;
                  break;
               }
               if ((r % (x + 23uL)) == 0uL) {
                  x = 0uL;
                  break;
               }
               if ((r % (x + 29uL)) == 0uL) {
                  x = 0uL;
                  break;
               }
            }
         } while (x == 0uL);
         if (r > 31uL) {
            fwrite(&r, 1uL, sizeof(mp_digit), out);
            printf("%9lu\r", r);
            fflush(stdout);
         }
         if (r < 31uL) break;
      }

      fclose(out);
   }
}

static void load_tab(void)
{
   primes = fopen("pprime.dat", "rb");
   if (primes == NULL) {
      gen_prime();
      primes = fopen("pprime.dat", "rb");
   }
   fseek(primes, 0L, SEEK_END);
   n_prime = ftell(primes) / sizeof(mp_digit);
}

static mp_digit prime_digit(void)
{
   int n;
   mp_digit d;

   n = abs(rand()) % n_prime;
   fseek(primes, n * sizeof(mp_digit), SEEK_SET);
   fread(&d, 1uL, sizeof(mp_digit), primes);
   return d;
}


/* makes a prime of at least k bits */
static mp_err pprime(int k, int li, mp_int *p, mp_int *q)
{
   mp_int  a, b, c, n, x, y, z, v;
   mp_err  res;
   int     ii;
   static const mp_digit bases[] = { 2, 3, 5, 7, 11, 13, 17, 19 };

   /* single digit ? */
   if (k <= (int) MP_DIGIT_BIT) {
      mp_set(p, prime_digit());
      return MP_OKAY;
   }

   if ((res = mp_init(&c)) != MP_OKAY) {
      return res;
   }

   if ((res = mp_init(&v)) != MP_OKAY) {
      goto LBL_C;
   }

   /* product of first 50 primes */
   if ((res =
           mp_read_radix(&v,
                         "19078266889580195013601891820992757757219839668357012055907516904309700014933909014729740190",
                         10)) != MP_OKAY) {
      goto LBL_V;
   }

   if ((res = mp_init(&a)) != MP_OKAY) {
      goto LBL_V;
   }

   /* set the prime */
   mp_set(&a, prime_digit());

   if ((res = mp_init(&b)) != MP_OKAY) {
      goto LBL_A;
   }

   if ((res = mp_init(&n)) != MP_OKAY) {
      goto LBL_B;
   }

   if ((res = mp_init(&x)) != MP_OKAY) {
      goto LBL_N;
   }

   if ((res = mp_init(&y)) != MP_OKAY) {
      goto LBL_X;
   }

   if ((res = mp_init(&z)) != MP_OKAY) {
      goto LBL_Y;
   }

   /* now loop making the single digit */
   while (mp_count_bits(&a) < k) {
      fprintf(stderr, "prime has %4d bits left\r", k - mp_count_bits(&a));
      fflush(stderr);
top:
      mp_set(&b, prime_digit());

      /* now compute z = a * b * 2 */
      if ((res = mp_mul(&a, &b, &z)) != MP_OKAY) {   /* z = a * b */
         goto LBL_Z;
      }

      if ((res = mp_copy(&z, &c)) != MP_OKAY) {   /* c = a * b */
         goto LBL_Z;
      }

      if ((res = mp_mul_2(&z, &z)) != MP_OKAY) {  /* z = 2 * a * b */
         goto LBL_Z;
      }

      /* n = z + 1 */
      if ((res = mp_add_d(&z, 1uL, &n)) != MP_OKAY) {  /* n = z + 1 */
         goto LBL_Z;
      }

      /* check (n, v) == 1 */
      if ((res = mp_gcd(&n, &v, &y)) != MP_OKAY) {   /* y = (n, v) */
         goto LBL_Z;
      }

      if (mp_cmp_d(&y, 1uL) != MP_EQ)
         goto top;

      /* now try base x=bases[ii]  */
      for (ii = 0; ii < li; ii++) {
         mp_set(&x, bases[ii]);

         /* compute x^a mod n */
         if ((res = mp_exptmod(&x, &a, &n, &y)) != MP_OKAY) {  /* y = x^a mod n */
            goto LBL_Z;
         }

         /* if y == 1 loop */
         if (mp_cmp_d(&y, 1uL) == MP_EQ)
            continue;

         /* now x^2a mod n */
         if ((res = mp_sqrmod(&y, &n, &y)) != MP_OKAY) {    /* y = x^2a mod n */
            goto LBL_Z;
         }

         if (mp_cmp_d(&y, 1uL) == MP_EQ)
            continue;

         /* compute x^b mod n */
         if ((res = mp_exptmod(&x, &b, &n, &y)) != MP_OKAY) {  /* y = x^b mod n */
            goto LBL_Z;
         }

         /* if y == 1 loop */
         if (mp_cmp_d(&y, 1uL) == MP_EQ)
            continue;

         /* now x^2b mod n */
         if ((res = mp_sqrmod(&y, &n, &y)) != MP_OKAY) {    /* y = x^2b mod n */
            goto LBL_Z;
         }

         if (mp_cmp_d(&y, 1uL) == MP_EQ)
            continue;

         /* compute x^c mod n == x^ab mod n */
         if ((res = mp_exptmod(&x, &c, &n, &y)) != MP_OKAY) {  /* y = x^ab mod n */
            goto LBL_Z;
         }

         /* if y == 1 loop */
         if (mp_cmp_d(&y, 1uL) == MP_EQ)
            continue;

         /* now compute (x^c mod n)^2 */
         if ((res = mp_sqrmod(&y, &n, &y)) != MP_OKAY) {    /* y = x^2ab mod n */
            goto LBL_Z;
         }

         /* y should be 1 */
         if (mp_cmp_d(&y, 1uL) != MP_EQ)
            continue;
         break;
      }

      /* no bases worked? */
      if (ii == li)
         goto top;

      {
         char buf[4096];

         mp_to_decimal(&n, buf, sizeof(buf));
         printf("Certificate of primality for:\n%s\n\n", buf);
         mp_to_decimal(&a, buf, sizeof(buf));
         printf("A == \n%s\n\n", buf);
         mp_to_decimal(&b, buf, sizeof(buf));
         printf("B == \n%s\n\nG == %lu\n", buf, bases[ii]);
         printf("----------------------------------------------------------------\n");
      }

      /* a = n */
      mp_copy(&n, &a);
   }

   /* get q to be the order of the large prime subgroup */
   mp_sub_d(&n, 1uL, q);
   mp_div_2(q, q);
   mp_div(q, &b, q, NULL);

   mp_exch(&n, p);

   res = MP_OKAY;
LBL_Z:
   mp_clear(&z);
LBL_Y:
   mp_clear(&y);
LBL_X:
   mp_clear(&x);
LBL_N:
   mp_clear(&n);
LBL_B:
   mp_clear(&b);
LBL_A:
   mp_clear(&a);
LBL_V:
   mp_clear(&v);
LBL_C:
   mp_clear(&c);
   return res;
}


int main(void)
{
   mp_int  p, q;
   char    buf[4096];
   int     k, li;
   clock_t t1;

   srand(time(NULL));
   load_tab();

   printf("Enter # of bits: \n");
   fgets(buf, sizeof(buf), stdin);
   sscanf(buf, "%d", &k);

   printf("Enter number of bases to try (1 to 8):\n");
   fgets(buf, sizeof(buf), stdin);
   sscanf(buf, "%d", &li);


   mp_init(&p);
   mp_init(&q);

   t1 = clock();
   pprime(k, li, &p, &q);
   t1 = clock() - t1;

   printf("\n\nTook %d ticks, %d bits\n", t1, mp_count_bits(&p));

   mp_to_decimal(&p, buf, sizeof(buf));
   printf("P == %s\n", buf);
   mp_to_decimal(&q, buf, sizeof(buf));
   printf("Q == %s\n", buf);

   return 0;
}
