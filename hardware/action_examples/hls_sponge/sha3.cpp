/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Markku-Juhani O. Saarinen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

//
// Very small, readable implementation of the FIPS 202 and SHA3 hash function. Public domain
//
// Origin:
//    https://github.com/mjosaarinen/tiny_sha3
//
// License:
//   https://github.com/mjosaarinen/tiny_sha3/blob/master/LICENSE
//
//
//// sha3.c
// 19-Nov-11  Markku-Juhani O. Saarinen <mjos@iki.fi>

// Revised 07-Aug-15 to match with official release of FIPS PUB 202 "SHA3"
// Revised 03-Sep-15 for portability + OpenSSL - style API
// 05-May-17  IBM : adapt to be compiled by Vivado HLS

#include "sha3.H"

//Casting from uint8_t to uint64_t => 94 FF - 118 LUT - II=104 - Latency=103
//void cast_uint8_to_uint64_W25(uint8_t st_in[200], uint64_t st_out[25])
void cast_uint8_to_uint64_W25(uint8_t *st_in, uint64_t *st_out, unsigned int size)
{
    uint64_t mem;
    int i, j;


    i = sizeof(st_out);

    cast_8to64:for( i = 0; i < size; i++ ) {
#pragma HLS UNROLL
          mem = 0;
          for( j = 8; j >= 0; j--) {
                  mem = (mem << 8);
                  //mem(7, 0) = st_in[j+i*8];
                  mem = (mem & 0xFFFFFFFFFFFFFF00 ) | st_in[j+i*8];
          }
          st_out[i] = mem;
    }
}
//Casting from uint64_t to uint8_t => 94 FF - 134 LUT - II=104 - Latency=103
void cast_uint64_to_uint8_W25(uint64_t *st_in, uint8_t *st_out, unsigned int size)
{
    uint64_t tmp = 0;
    int i, j;

    cast_64to8:for( i = 0; i < size; i++ ) {
#pragma HLS UNROLL
          tmp = st_in[i];
          for( j = 0; j < 8; j++ ) {
                  st_out[i*8+j] = (uint8_t)tmp;
                  tmp = (tmp >> 8);
          }
    }
}

// update the state with given number of rounds

//void sha3_keccakf(uint64_t st[25])
void sha3_keccakf(uint64_t st_in[25], uint64_t st_out[25])
{
    // constants
    const uint64_t keccakf_rndc[24] = {
        0x0000000000000001, 0x0000000000008082, 0x800000000000808a,
        0x8000000080008000, 0x000000000000808b, 0x0000000080000001,
        0x8000000080008081, 0x8000000000008009, 0x000000000000008a,
        0x0000000000000088, 0x0000000080008009, 0x000000008000000a,
        0x000000008000808b, 0x800000000000008b, 0x8000000000008089,
        0x8000000000008003, 0x8000000000008002, 0x8000000000000080,
        0x000000000000800a, 0x800000008000000a, 0x8000000080008081,
        0x8000000000008080, 0x0000000080000001, 0x8000000080008008
    };
    const int keccakf_rotc[24] = {
        1,  3,  6,  10, 15, 21, 28, 36, 45, 55, 2,  14,
        27, 41, 56, 8,  25, 43, 62, 18, 39, 61, 20, 44
    };
    const int keccakf_piln[24] = {
        10, 7,  11, 17, 18, 3, 5,  16, 8,  21, 24, 4,
        15, 23, 19, 13, 12, 2, 20, 14, 22, 9,  6,  1
    };

    // variables
    int i, j, r;
    uint64_t t, bc[5];
    uint64_t st[25];
#pragma HLS ARRAY_RESHAPE variable=st block factor=4 dim=1


    //separate entry port from logic
    for (i = 0; i < 25; i++)
#pragma HLS UNROLL
        st[i] = st_in[i];

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
    uint8_t *v;

    // endianess conversion. this is redundant on little-endian targets
    for (i = 0; i < 25; i++) {
        v = (uint8_t *) &st[i];
        st[i] = ((uint64_t) v[0])     | (((uint64_t) v[1]) << 8) |
            (((uint64_t) v[2]) << 16) | (((uint64_t) v[3]) << 24) |
            (((uint64_t) v[4]) << 32) | (((uint64_t) v[5]) << 40) |
            (((uint64_t) v[6]) << 48) | (((uint64_t) v[7]) << 56);
    }
#endif


    // actual iteration
    for (r = 0; r < KECCAKF_ROUNDS; r++) {
#pragma HLS PIPELINE
        // Theta
        for (i = 0; i < 5; i++)
            bc[i] = st[i] ^ st[i + 5] ^ st[i + 10] ^ st[i + 15] ^ st[i + 20];

        for (i = 0; i < 5; i++) {
            t = bc[(i + 4) % 5] ^ ROTL64(bc[(i + 1) % 5], 1);
            for (j = 0; j < 25; j += 5)
                st[j + i] ^= t;
        }

        // Rho Pi
        t = st[1];
        for (i = 0; i < 24; i++) {
            j = keccakf_piln[i];
            bc[0] = st[j];
            st[j] = ROTL64(t, keccakf_rotc[i]);
            t = bc[0];
        }

        //  Chi
        for (j = 0; j < 25; j += 5) {
            for (i = 0; i < 5; i++)
                bc[i] = st[j + i];
            for (i = 0; i < 5; i++)
                st[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
        }

        //  Iota
        st[0] ^= keccakf_rndc[r];
    }

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
    // endianess conversion. this is redundant on little-endian targets
    for (i = 0; i < 25; i++) {
        v = (uint8_t *) &st[i];
        t = st[i];
        v[0] = t & 0xFF;
        v[1] = (t >> 8) & 0xFF;
        v[2] = (t >> 16) & 0xFF;
        v[3] = (t >> 24) & 0xFF;
        v[4] = (t >> 32) & 0xFF;
        v[5] = (t >> 40) & 0xFF;
        v[6] = (t >> 48) & 0xFF;
        v[7] = (t >> 56) & 0xFF;
    }
#endif

    //separate entry port from logic
    for (i = 0; i < 25; i++)
#pragma HLS UNROLL
        st_out[i] = st[i];

}

// Initialize the context for SHA3

int sha3_init(sha3_ctx_t *c, int mdlen)
{
    int i;

//    for (i = 0; i < 25; i++)
//        c->st.q[i] = 0; 
    for (i = 0; i < 200; i++)
#pragma HLS UNROLL
    	c->st.b[i] = 0;
    c->mdlen = mdlen;
    c->rsiz = 200 - 2 * mdlen;
    c->pt = 0;

    return 1;
}

// update state with more data

//int sha3_update(sha3_ctx_t *c, const void *data, size_t len)
int sha3_update(sha3_ctx_t *c, const uint8_t *data, size_t len)
{
    size_t i;
    int j;
    uint64_t st[25];

    j = c->pt;
    for (i = 0; i < len; i++) {
#pragma HLS UNROLL
			c->st.b[j++] ^= ((const uint8_t *) data)[i];
			if (j >= c->rsiz) {
				//sha3_keccakf(c->st.q);

			    cast_uint8_to_uint64_W25(c->st.b, st, 25);
				sha3_keccakf(st, st);
			    cast_uint64_to_uint8_W25(st, c->st.b, 25);
				j = 0;
			}
    }
    c->pt = j;

    return 1;
}

// finalize and output a hash

//int sha3_final(void *md, sha3_ctx_t *c)
int sha3_final(uint8_t *md, sha3_ctx_t *c)
{
    int i;
    uint64_t st[25];

    c->st.b[c->pt] ^= 0x06;
    c->st.b[c->rsiz - 1] ^= 0x80;

    //sha3_keccakf(c->st.q);

    //Casting from uint8_t to uint64_t
    cast_uint8_to_uint64_W25(c->st.b, st, 25);
	sha3_keccakf(st, st);
	//sha3_keccakf(c->st.b, c->st.b);
    //Casting from uint64_t to uint8_t
    cast_uint64_to_uint8_W25(st, c->st.b, 25);


    for (i = 0; i < c->mdlen; i++) {
#pragma HLS UNROLL
    		((uint8_t *) md)[i] = c->st.b[i];
    }

    return 1;
}

// compute a SHA-3 hash (md) of given byte length from "in"

//void *sha3(const void *in, size_t inlen, void *md, int mdlen)
void sha3(const uint8_t *in, size_t inlen, uint8_t *md, int mdlen)
{
    sha3_ctx_t sha3;

    sha3_init(&sha3, mdlen);
    sha3_update(&sha3, in, inlen);
    sha3_final(md, &sha3);

    //return md;
}

// SHAKE128 and SHAKE256 extensible-output functionality

void shake_xof(sha3_ctx_t *c)
{
    uint64_t st[25];

    c->st.b[c->pt] ^= 0x1F;
    c->st.b[c->rsiz - 1] ^= 0x80;
    //sha3_keccakf(c->st.q);

    //Casting from uint8_t to uint64_t
    cast_uint8_to_uint64_W25(c->st.b, st, 25);
	sha3_keccakf(st, st);
	//sha3_keccakf(c->st.b, c->st.b);
    //Casting from uint64_t to uint8_t
    cast_uint64_to_uint8_W25(st, c->st.b, 25);

    c->pt = 0;
}

void shake_out(sha3_ctx_t *c, void *out, size_t len)
{
    size_t i;
    int j;
    uint64_t st[25];

    j = c->pt;
    for (i = 0; i < len; i++) {
        if (j >= c->rsiz) {
            //sha3_keccakf(c->st.q);
            //Casting from uint8_t to uint64_t
            cast_uint8_to_uint64_W25(c->st.b, st,25);
        	sha3_keccakf(st, st);
        	//sha3_keccakf(c->st.b, c->st.b);
            //Casting from uint64_t to uint8_t
            cast_uint64_to_uint8_W25(st, c->st.b, 25);
            j = 0;
        }
        ((uint8_t *) out)[i] = c->st.b[j++];
    }
    c->pt = j;
}

