#include<set>
#include <cstdint>
#include <iostream>
#include <bitset>
using namespace std;

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
//#include <inttypes.h>
#include <limits.h>

static unsigned char BYTE_COUNTS[] = {	  // table of bits/byte
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
  };

typedef struct{
	unsigned char	*bits;
	size_t		size;
	uint64_t		count=-1;
}bit_vector_t;

int bit_vector_init(bit_vector_t *bv,uint64_t n){
	bv->size = (n>>3)+1;
	bv->bits = (unsigned char*)calloc(1,bv->size);
	return 0;
}
void bit_vector_destroy(bit_vector_t *bv){
	free(bv->bits);
}
void bit_vector_set(bit_vector_t *bv,uint64_t bit){
	bv->bits[bit >> 3] |= 1 << (bit & 7);
    bv->count = -1;
}

bool bit_vector_get(bit_vector_t *bv,uint64_t bit){
	return (bv->bits[bit >> 3] & (1 << (bit & 7))) != 0;
}

void bit_vector_clear(bit_vector_t *bv,uint64_t bit){
	bv->bits[bit >> 3] &= ~(1 << (bit & 7));
    bv->count = -1;
}

uint64_t bit_vector_count(bit_vector_t *bv){
	// if the vector has been modified
    if (bv->count == -1) {
      uint64_t c = 0;
      size_t end = bv->size;
      for (int i = 0; i < end; i++)
        c += BYTE_COUNTS[bv->bits[i] & 0xFF];	  // sum bits per byte
      bv->count = c;
    }
    return bv->count;
}

void bugReportStart(void) {
        cout<<"\n\n=== REDIS BUG REPORT START: Cut & paste starting from here ===\n"<<endl;
}

void _serverPanic(const char *msg, const char *file, int line) {
    bugReportStart();
    cout<<"------------------------------------------------"<<endl;
    cout<<"!!! Software Failure. Press left mouse button to continue"<<endl;
    cout<<"Guru Meditation: "<<msg<<file<<line<<endl;
    cout<<"(forcing SIGSEGV in order to print the stack trace)"<<endl;
    cout<<"------------------------------------------------"<<endl;
    *((char*)-1) = 'x';
}

#define serverPanic(_e) _serverPanic(#_e,__FILE__,__LINE__),_exit(1)

typedef struct{
set<uint64_t>			val;
}test_t;


/* -----------------------------------------------------------------------------
 * Helpers and low level bit functions.
 * -------------------------------------------------------------------------- */

/* Count number of bits set in the binary array pointed by 's' and long
 * 'count' bytes. The implementation of this function is required to
 * work with a input string length up to 512 MB. */
size_t redisPopcount(void *s, long count) {
    size_t bits = 0;
    unsigned char *p = (unsigned char*)s;
    uint32_t *p4;
    static const unsigned char bitsinbyte[256] = {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8};

    /* Count initial bytes not aligned to 32 bit. */
    while((unsigned long)p & 3 && count) {
        bits += bitsinbyte[*p++];
        count--;
    }

    /* Count bits 28 bytes at a time */
    p4 = (uint32_t*)p;
    while(count>=28) {
        uint32_t aux1, aux2, aux3, aux4, aux5, aux6, aux7;

        aux1 = *p4++;
        aux2 = *p4++;
        aux3 = *p4++;
        aux4 = *p4++;
        aux5 = *p4++;
        aux6 = *p4++;
        aux7 = *p4++;
        count -= 28;

        aux1 = aux1 - ((aux1 >> 1) & 0x55555555);
        aux1 = (aux1 & 0x33333333) + ((aux1 >> 2) & 0x33333333);
        aux2 = aux2 - ((aux2 >> 1) & 0x55555555);
        aux2 = (aux2 & 0x33333333) + ((aux2 >> 2) & 0x33333333);
        aux3 = aux3 - ((aux3 >> 1) & 0x55555555);
        aux3 = (aux3 & 0x33333333) + ((aux3 >> 2) & 0x33333333);
        aux4 = aux4 - ((aux4 >> 1) & 0x55555555);
        aux4 = (aux4 & 0x33333333) + ((aux4 >> 2) & 0x33333333);
        aux5 = aux5 - ((aux5 >> 1) & 0x55555555);
        aux5 = (aux5 & 0x33333333) + ((aux5 >> 2) & 0x33333333);
        aux6 = aux6 - ((aux6 >> 1) & 0x55555555);
        aux6 = (aux6 & 0x33333333) + ((aux6 >> 2) & 0x33333333);
        aux7 = aux7 - ((aux7 >> 1) & 0x55555555);
        aux7 = (aux7 & 0x33333333) + ((aux7 >> 2) & 0x33333333);
        bits += ((((aux1 + (aux1 >> 4)) & 0x0F0F0F0F) +
                    ((aux2 + (aux2 >> 4)) & 0x0F0F0F0F) +
                    ((aux3 + (aux3 >> 4)) & 0x0F0F0F0F) +
                    ((aux4 + (aux4 >> 4)) & 0x0F0F0F0F) +
                    ((aux5 + (aux5 >> 4)) & 0x0F0F0F0F) +
                    ((aux6 + (aux6 >> 4)) & 0x0F0F0F0F) +
                    ((aux7 + (aux7 >> 4)) & 0x0F0F0F0F))* 0x01010101) >> 24;
    }
    /* Count the remaining bytes. */
    p = (unsigned char*)p4;
    while(count--) bits += bitsinbyte[*p++];
    return bits;
}

/* Return the position of the first bit set to one (if 'bit' is 1) or
 * zero (if 'bit' is 0) in the bitmap starting at 's' and long 'count' bytes.
 *
 * The function is guaranteed to return a value >= 0 if 'bit' is 0 since if
 * no zero bit is found, it returns count*8 assuming the string is zero
 * padded on the right. However if 'bit' is 1 it is possible that there is
 * not a single set bit in the bitmap. In this special case -1 is returned. */
long redisBitpos(void *s, unsigned long count, int bit) {
    unsigned long *l;
    unsigned char *c;
    unsigned long skipval, word = 0, one;
    long pos = 0; /* Position of bit, to return to the caller. */
    unsigned long j;

    /* Process whole words first, seeking for first word that is not
     * all ones or all zeros respectively if we are lookig for zeros
     * or ones. This is much faster with large strings having contiguous
     * blocks of 1 or 0 bits compared to the vanilla bit per bit processing.
     *
     * Note that if we start from an address that is not aligned
     * to sizeof(unsigned long) we consume it byte by byte until it is
     * aligned. */

    /* Skip initial bits not aligned to sizeof(unsigned long) byte by byte. */
    skipval = bit ? 0 : UCHAR_MAX;
    c = (unsigned char*) s;
    while((unsigned long)c & (sizeof(*l)-1) && count) {
        if (*c != skipval) break;
        c++;
        count--;
        pos += 8;
    }

    /* Skip bits with full word step. */
    skipval = bit ? 0 : ULONG_MAX;
    l = (unsigned long*) c;
    while (count >= sizeof(*l)) {
        if (*l != skipval) break;
        l++;
        count -= sizeof(*l);
        pos += sizeof(*l)*8;
    }

    /* Load bytes into "word" considering the first byte as the most significant
     * (we basically consider it as written in big endian, since we consider the
     * string as a set of bits from left to right, with the first bit at position
     * zero.
     *
     * Note that the loading is designed to work even when the bytes left
     * (count) are less than a full word. We pad it with zero on the right. */
    c = (unsigned char*)l;
    for (j = 0; j < sizeof(*l); j++) {
        word <<= 8;
        if (count) {
            word |= *c;
            c++;
            count--;
        }
    }

    /* Special case:
     * If bits in the string are all zero and we are looking for one,
     * return -1 to signal that there is not a single "1" in the whole
     * string. This can't happen when we are looking for "0" as we assume
     * that the right of the string is zero padded. */
    if (bit == 1 && word == 0) return -1;

    /* Last word left, scan bit by bit. The first thing we need is to
     * have a single "1" set in the most significant position in an
     * unsigned long. We don't know the size of the long so we use a
     * simple trick. */
    one = ULONG_MAX; /* All bits set to 1.*/
    one >>= 1;       /* All bits set to 1 but the MSB. */
    one = ~one;      /* All bits set to 0 but the MSB. */

    while(one) {
        if (((one & word) != 0) == bit) return pos;
        pos++;
        one >>= 1;
    }

    /* If we reached this point, there is a bug in the algorithm, since
     * the case of no match is handled as a special case before. */
    serverPanic("End of redisBitpos() reached.");
    return 0; /* Just to avoid warnings. */
}

/* The following set.*Bitfield and get.*Bitfield functions implement setting
 * and getting arbitrary size (up to 64 bits) signed and unsigned integers
 * at arbitrary positions into a bitmap.
 *
 * The representation considers the bitmap as having the bit number 0 to be
 * the most significant bit of the first byte, and so forth, so for example
 * setting a 5 bits unsigned integer to value 23 at offset 7 into a bitmap
 * previously set to all zeroes, will produce the following representation:
 *
 * +--------+--------+
 * |00000001|01110000|
 * +--------+--------+
 *
 * When offsets and integer sizes are aligned to bytes boundaries, this is the
 * same as big endian, however when such alignment does not exist, its important
 * to also understand how the bits inside a byte are ordered.
 *
 * Note that this format follows the same convention as SETBIT and related
 * commands.
 */

void setUnsignedBitfield(unsigned char *p, uint64_t offset, uint64_t bits, uint64_t value) {
    uint64_t byte, bit, byteval, bitval, j;

    for (j = 0; j < bits; j++) {
        bitval = (value & ((uint64_t)1<<(bits-1-j))) != 0;
        byte = offset >> 3;
        bit = 7 - (offset & 0x7);
        byteval = p[byte];
        byteval &= ~(1 << bit);
        byteval |= bitval << bit;
        p[byte] = byteval & 0xff;
        offset++;
    }
}

void setSignedBitfield(unsigned char *p, uint64_t offset, uint64_t bits, int64_t value) {
    uint64_t uv = value; /* Casting will add UINT64_MAX + 1 if v is negative. */
    setUnsignedBitfield(p,offset,bits,uv);
}

uint64_t getUnsignedBitfield(unsigned char *p, uint64_t offset, uint64_t bits) {
    uint64_t byte, bit, byteval, bitval, j, value = 0;

    for (j = 0; j < bits; j++) {
        byte = offset >> 3;
        bit = 7 - (offset & 0x7);
        byteval = p[byte];
        bitval = (byteval >> bit) & 1;
        value = (value<<1) | bitval;
        offset++;
    }
    return value;
}

int64_t getSignedBitfield(unsigned char *p, uint64_t offset, uint64_t bits) {
    int64_t value;
    union {uint64_t u; int64_t i;} conv;

    /* Converting from unsigned to signed is undefined when the value does
     * not fit, however here we assume two's complement and the original value
     * was obtained from signed -> unsigned conversion, so we'll find the
     * most significant bit set if the original value was negative.
     *
     * Note that two's complement is mandatory for exact-width types
     * according to the C99 standard. */
    conv.u = getUnsignedBitfield(p,offset,bits);
    value = conv.i;

    /* If the top significant bit is 1, propagate it to all the
     * higher bits for two's complement representation of signed
     * integers. */
    if (value & ((uint64_t)1 << (bits-1)))
        value |= ((uint64_t)-1) << bits;
    return value;
}

/* The following two functions detect overflow of a value in the context
 * of storing it as an unsigned or signed integer with the specified
 * number of bits. The functions both take the value and a possible increment.
 * If no overflow could happen and the value+increment fit inside the limits,
 * then zero is returned, otherwise in case of overflow, 1 is returned,
 * otherwise in case of underflow, -1 is returned.
 *
 * When non-zero is returned (oferflow or underflow), if not NULL, *limit is
 * set to the value the operation should result when an overflow happens,
 * depending on the specified overflow semantics:
 *
 * For BFOVERFLOW_SAT if 1 is returned, *limit it is set maximum value that
 * you can store in that integer. when -1 is returned, *limit is set to the
 * minimum value that an integer of that size can represent.
 *
 * For BFOVERFLOW_WRAP *limit is set by performing the operation in order to
 * "wrap" around towards zero for unsigned integers, or towards the most
 * negative number that is possible to represent for signed integers. */

#define BFOVERFLOW_WRAP 0
#define BFOVERFLOW_SAT 1
#define BFOVERFLOW_FAIL 2 /* Used by the BITFIELD command implementation. */

int checkUnsignedBitfieldOverflow(uint64_t value, int64_t incr, uint64_t bits, int owtype, uint64_t *limit) {
    uint64_t max = (bits == 64) ? UINT64_MAX : (((uint64_t)1<<bits)-1);
    int64_t maxincr = max-value;
    int64_t minincr = -value;

    if (value > max || (incr > 0 && incr > maxincr)) {
        if (limit) {
            if (owtype == BFOVERFLOW_WRAP) {
                goto handle_wrap;
            } else if (owtype == BFOVERFLOW_SAT) {
                *limit = max;
            }
        }
        return 1;
    } else if (incr < 0 && incr < minincr) {
        if (limit) {
            if (owtype == BFOVERFLOW_WRAP) {
                goto handle_wrap;
            } else if (owtype == BFOVERFLOW_SAT) {
                *limit = 0;
            }
        }
        return -1;
    }
    return 0;

handle_wrap:
    {
        uint64_t mask = ((uint64_t)-1) << bits;
        uint64_t res = value+incr;

        res &= ~mask;
        *limit = res;
    }
    return 1;
}

int checkSignedBitfieldOverflow(int64_t value, int64_t incr, uint64_t bits, int owtype, int64_t *limit) {
    int64_t max = (bits == 64) ? INT64_MAX : (((int64_t)1<<(bits-1))-1);
    int64_t min = (-max)-1;

    /* Note that maxincr and minincr could overflow, but we use the values
     * only after checking 'value' range, so when we use it no overflow
     * happens. */
    int64_t maxincr = max-value;
    int64_t minincr = min-value;

    if (value > max || (bits != 64 && incr > maxincr) || (value >= 0 && incr > 0 && incr > maxincr))
    {
        if (limit) {
            if (owtype == BFOVERFLOW_WRAP) {
                goto handle_wrap;
            } else if (owtype == BFOVERFLOW_SAT) {
                *limit = max;
            }
        }
        return 1;
    } else if (value < min || (bits != 64 && incr < minincr) || (value < 0 && incr < 0 && incr < minincr)) {
        if (limit) {
            if (owtype == BFOVERFLOW_WRAP) {
                goto handle_wrap;
            } else if (owtype == BFOVERFLOW_SAT) {
                *limit = min;
            }
        }
        return -1;
    }
    return 0;

handle_wrap:
    {
        uint64_t mask = ((uint64_t)-1) << bits;
        uint64_t msb = (uint64_t)1 << (bits-1);
        uint64_t a = value, b = incr, c;
        c = a+b; /* Perform addition as unsigned so that's defined. */

        /* If the sign bit is set, propagate to all the higher order
         * bits, to cap the negative value. If it's clear, mask to
         * the positive integer limit. */
        if (c & msb) {
            c |= mask;
        } else {
            c &= ~mask;
        }
        *limit = c;
    }
    return 1;
}

/* Debugging function. Just show bits in the specified bitmap. Not used
 * but here for not having to rewrite it when debugging is needed. */
void printBits(unsigned char *p, unsigned long count) {
    unsigned long j, i, byte;

    for (j = 0; j < count; j++) {
        byte = p[j];
        for (i = 0x80; i > 0; i /= 2)
            printf("%c", (byte & i) ? '1' : '0');
        printf("|");
    }
    printf("\n");
}

int test()
{
int i;
uint64_t num;

test_t t;

set<uint64_t> *val;
val=new set<uint64_t>;
t.val=*val;
for(i=0;i<10;i++){
	num=i;
	t.val.insert(num);
}
set<uint64_t>::iterator it;
for(it=t.val.begin();it!=t.val.end();it++){
num=*it;
cout<<num<<endl;

}

}



int DectoHex(long dec, unsigned char *hex, long length)  
{  
    long i;  
    for (i = length - 1; i >= 0; i--)  
    {  
        hex[i] = (dec % 256) & 0xFF;  
        dec /= 256;  
    }  
    return 0;  
}

const int LONG_MAX_buff_len=256*1024*1024;

unsigned char LONG_MAX_buff[LONG_MAX_buff_len];


typedef struct {
int x;
int y;
}XX_T;

typedef struct {
int x;
XX_T     *t;
int y;
}Point_t;

int main()
{
    Point_t* pt,*x;
    XX_T     *t,*y;
    pt = (Point_t*)malloc(10 * sizeof(Point_t));
    int i,j;
    for(i=0;i<10;i++){
        x=&pt[i];
        x->x=i;
        x->y=i+100;
        t=(XX_T*)malloc(10 * sizeof(XX_T));
        x->t=t;
        for(j=0;j<10;j++){
            t[j].x=i+j;
            t[j].y=i+100;
        }
        
    }
    for(i=0;i<10;i++){
        x=&pt[i];
        cout<<x->x<<", "<<x->y<<endl;
        for(j=0;j<10;j++){
            y=&x->t[j];
            cout<<"\t"<<y->x<<", "<<y->y<<endl;
        }
    }
    
    return 0;
}
int test111(){
	unsigned char *strbit;
	cout<<"ULONG_MAX: "<<ULONG_MAX<<", LONG_MAX: "<<LONG_MAX<<", UINT_MAX: "<<UINT_MAX<<", INT_MAX: "<<INT_MAX<<", USHRT_MAX: "<<USHRT_MAX<<", SHRT_MAX: "<<SHRT_MAX<< ", UCHAR_MAX: "<<UCHAR_MAX<< ", SCHAR_MAX: "<<SCHAR_MAX<<endl;
	printf("ULONG_MAX: %x, LONG_MAX: %x, UINT_MAX: %x, INT_MAX: %x, USHRT_MAX: %x, SHRT_MAX: %x, UCHAR_MAX: %x, SCHAR_MAX: %x\n",ULONG_MAX,LONG_MAX,UINT_MAX,INT_MAX,USHRT_MAX,SHRT_MAX,UCHAR_MAX,SCHAR_MAX); 
	//sprintf(LONG_MAX_buff,"%x",LONG_MAX);
	//cout << LONG_MAX_buff << endl;
	//DectoHex(LONG_MAX,LONG_MAX_buff,1152921504606846975);
	 

//so for example * setting a 5 bits unsigned integer to value 23 at offset 7 into a bitmap
uint64_t offset=7;
uint64_t bits=5;
uint64_t value=23;
/*
printBits(LONG_MAX_buff,LONG_MAX_buff_len);
setUnsignedBitfield(LONG_MAX_buff, offset, bits, value);
printBits(LONG_MAX_buff,LONG_MAX_buff_len);
cout << getUnsignedBitfield(LONG_MAX_buff, offset, bits)<<endl;
*/
memset(LONG_MAX_buff,0,LONG_MAX_buff_len);
unsigned long count=54;
int bit=1;
//printBits(LONG_MAX_buff,LONG_MAX_buff_len);
cout << redisBitpos(LONG_MAX_buff, count, bit)<<endl;
//printBits(LONG_MAX_buff,LONG_MAX_buff_len);
cout << redisPopcount(LONG_MAX_buff, count)<<endl;


bit_vector_t bv;
uint64_t pos = UINT_MAX;
//pos *= 1000;
cout << "pos: " << pos <<endl;
//bit_vector_init(NULL,&bv,pos);
cout << "init completed"<<endl;
//for(int i=0;i<100;i++)
//bit_vector_set(&bv,pos-i);
//cout << "get: " << bit_vector_get(&bv,pos) << ", get: " << bit_vector_get(&bv,pos-1) << ", count: " << bit_vector_count(&bv)<<endl;
//bit_vector_destroy(NULL,&bv);


}

#ifdef __cplusplus
}
#endif
