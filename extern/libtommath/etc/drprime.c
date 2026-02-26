/* Makes safe primes of a DR nature */
#include <tommath.h>

static int sizes[] = { 1+256/MP_DIGIT_BIT, 1+512/MP_DIGIT_BIT, 1+768/MP_DIGIT_BIT, 1+1024/MP_DIGIT_BIT, 1+2048/MP_DIGIT_BIT, 1+4096/MP_DIGIT_BIT };

int main(void)
{
   mp_bool res;
   int x, y;
   char buf[4096];
   FILE *out;
   mp_int a, b;

   mp_init(&a);
   mp_init(&b);

   out = fopen("drprimes.txt", "w");
   if (out != NULL) {
      for (x = 0; x < (int)(sizeof(sizes)/sizeof(sizes[0])); x++) {
top:
         printf("Seeking a %d-bit safe prime\n", sizes[x] * MP_DIGIT_BIT);
         mp_grow(&a, sizes[x]);
         mp_zero(&a);
         for (y = 1; y < sizes[x]; y++) {
            a.dp[y] = MP_MASK;
         }

         /* make a DR modulus */
         a.dp[0] = -1;
         a.used = sizes[x];

         /* now loop */
         res = MP_NO;
         for (;;) {
            a.dp[0] += 4uL;
            if (a.dp[0] >= MP_MASK) break;
            mp_prime_is_prime(&a, 1, &res);
            if (res == MP_NO) continue;
            printf(".");
            fflush(stdout);
            mp_sub_d(&a, 1uL, &b);
            mp_div_2(&b, &b);
            mp_prime_is_prime(&b, 3, &res);
            if (res == MP_NO) continue;
            mp_prime_is_prime(&a, 3, &res);
            if (res == MP_YES) break;
         }

         if (res != MP_YES) {
            printf("Error not DR modulus\n");
            sizes[x] += 1;
            goto top;
         } else {
            mp_to_decimal(&a, buf, sizeof(buf));
            printf("\n\np == %s\n\n", buf);
            fprintf(out, "%d-bit prime:\np == %s\n\n", mp_count_bits(&a), buf);
            fflush(out);
         }
      }
      fclose(out);
   }

   mp_clear(&a);
   mp_clear(&b);

   return 0;
}
