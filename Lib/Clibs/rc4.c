// for gcc-4.2 unsigned char is *very* slow
// otoh for gcc-3.4 unsigned char is faster.
// checked by setup.pe
#ifdef CHAR_STATE
typedef unsigned char tstate;
#else
typedef unsigned int tstate;
#endif

          /*        This code illustrates a sample implementation
           *                 of the Arcfour algorithm
           *         Copyright (c) April 29, 1997 Kalle Kaukonen.
           *                    All Rights Reserved.
           *
           * Redistribution and use in source and binary forms, with or
           * without modification, are permitted provided that this copyright
           * notice and disclaimer are retained.
           *
           * THIS SOFTWARE IS PROVIDED BY KALLE KAUKONEN AND CONTRIBUTORS ``AS
           * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
           * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
           * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL KALLE
           * KAUKONEN OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
           * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
           * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
           * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
           * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
           * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
           * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
           * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
           */
          
          
          typedef struct
          {
            unsigned int x;
            unsigned int y;
            tstate state[256];
          } ArcfourContext;
               
          void arcfour_init(ArcfourContext *ctx, const unsigned char *key, 
                            unsigned int key_len);
          void arcfour_encrypt(ArcfourContext *ctx, unsigned char *dest, 
                               const unsigned char *src, unsigned int len);
          
          void arcfour_init(ArcfourContext *ctx, const unsigned char *key, 
                            unsigned int key_len)
          {
            unsigned int t, u;
            unsigned int keyindex;
            unsigned int stateindex;
            tstate *state;
            unsigned int counter;
               
            state = ctx->state;
            ctx->x = 0;
            ctx->y = 0;
            for (counter = 0; counter < 256; counter++)
                 state[counter] = counter;
            keyindex = 0;
            stateindex = 0;
            for (counter = 0; counter < 256; counter++)
            {
              t = state[counter];
              stateindex = (stateindex + key[keyindex] + t) & 0xff;
              u = state[stateindex];
              state[stateindex] = t;
              state[counter] = u;
              if (++keyindex >= key_len)
                keyindex = 0;
            }
          }
          
           
          static inline tstate arcfour_byte(ArcfourContext *ctx)
          {
            unsigned int x;
            unsigned int y;
            unsigned int sx, sy;
            tstate *state;
               
            state = ctx->state;
            x = (ctx->x + 1) & 0xff;
            sx = state[x];
            y = (sx + ctx->y) & 0xff;
            sy = state[y];
            ctx->x = x;
            ctx->y = y;
            state[y] = sx;
            state[x] = sy;
          
            return state[(sx + sy) & 0xff];
          }

          void arcfour_encrypt(ArcfourContext *ctx, unsigned char *dest, 
                               const unsigned char *src, unsigned int len)
          {
            unsigned int i;
            for (i = 0; i < len; i++)
              dest[i] = src[i] ^ arcfour_byte(ctx);
          }

const int sizeof_rc4 = sizeof (ArcfourContext);

const char ABI [] =
"i sizeof_rc4			\n"
"- arcfour_init		ssi	\n"
"- arcfour_encrypt	sssi	\n"
;
