#include <ngx_config.h>
#include <ngx_core.h>
#include <stdbool.h>


#include <mqtt_protocol.h>
#include "ddebug.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define MOSQ_MSB(A) (uint8_t)((A & 0xFF00) >> 8)
#define MOSQ_LSB(A) (uint8_t)(A & 0x00FF)

int lua_utility_indexOf(const u_char *str1,const u_char *str2)
{
	if(str1==NULL || str2==NULL)return -1;
	u_char *p;
	int i=0;
	p=(u_char *)str1;
	p=(u_char *)ngx_strstr(str1,str2);
	if(p==NULL)
		return -1;
	else{
		while(str1!=p)
		{
			str1++;
			i++;
		}
	}
	return i;
}

u_char* lua_utility_longToBytes(long n,u_char *b) {
	b[7] = (u_char) (n & 0xff);
	b[6] = (u_char) (n >> 8  & 0xff);
	b[5] = (u_char) (n >> 16 & 0xff);
	b[4] = (u_char) (n >> 24 & 0xff);
	b[3] = (u_char) (n >> 32 & 0xff);
	b[2] = (u_char) (n >> 40 & 0xff);
	b[1] = (u_char) (n >> 48 & 0xff);
	b[0] = (u_char) (n >> 56 & 0xff);
	return b;
}


int utility_mqtt_write_uint16(u_char *p,size_t len,uint16_t word)
{
	p[0] = MOSQ_MSB(word);
	p[1]=MOSQ_LSB(word);
	return 2;
}


uint16_t utility_mqtt_read_uint16(u_char *p,size_t len)
{
	uint16_t out;
	uint8_t msb, lsb;
	if(len<2) return UINT16_MAX;

	msb = p[0];
	lsb = p[1];
	out=(msb<<8) + lsb;
	return out;
}

int utility_mqtt_write_bytes(u_char *p,size_t len, const void *bytes, uint32_t count)
{
	memcpy(p, bytes, count);
	p += count;
	return count;
}

int utility_mqtt_write_remaining_leng(u_char *p,size_t len,uint32_t remaining_length){
	unsigned   char digit;
	uint8_t remaining_count=0;
	do {
		digit = (unsigned   char) (remaining_length % 128);
		remaining_length = remaining_length / 128;
		// if there are more digits to encode, set the top bit of this digit
		if (remaining_length > 0) {
			digit = (unsigned   char) (digit | 0x80);
		}
		p[remaining_count] = digit;
		remaining_count++;
	} while (remaining_length > 0);
	return remaining_count;
}

int utility_mqtt_read_remaining_leng(u_char *p,size_t len,uint32_t *out){
	uint8_t byte;
	uint8_t remaining_count=0;
	uint32_t remaining_mult=1,remaining_length=0;

	//if(!have_remaining){
	do{

		byte = p[remaining_count];
		remaining_count++;
		if (remaining_count == len) {
	        return NGX_ERROR;
	    }

		/* Max 4 bytes length for remaining length as defined by protocol.
		 * Anything more likely means a broken/malicious client.
		 */
		if(remaining_count > 4) return NGX_ERROR;

		//MQTT协议规定，第八位（最高位）若为1，则表示还有后续字节存在。同时MQTT协议最多允许4个字节表示剩余长度。
		// 那么最大长度为：0xFF,0xFF,0xFF,0x7F，二进制表示为:11111111,11111111,11111111,01111111，
		// 127 0x7F
		remaining_length += (byte & 0x7F) * remaining_mult;
		remaining_mult *= 128;

	}while((byte & 0x80) != 0);
	*out=remaining_length;
	return remaining_count;
}




int utility_mqtt_write_uint32(u_char *b,size_t len,uint32_t n)
{
	b[3] = (u_char) (n & 0xff);
	b[2] = (u_char) (n >> 8  & 0xff);
	b[1] = (u_char) (n >> 16 & 0xff);
	b[0] = (u_char) (n >> 24 & 0xff);
	return 4;
}

uint32_t utility_mqtt_read_uint32(u_char *b,size_t len)
{
	uint32_t l;
	l = ((uint32_t) b[0] << 24);
	l += ((uint32_t) b[1] << 16);
	l += ((uint32_t) b[2] << 8);
	l += (uint32_t) b[3];
	return l;
}

double utility_mqtt_read_double(u_char *b,size_t len)
{
	int64_t l;
	l = ((int64_t) b[0] << 56);
	// 如果不强制转换为int64_t，那么默认会当作int，导致最高64位丢失
	l += ((int64_t) b[1] << 48);
	l += ((int64_t) b[2] << 40);
	l += ((int64_t) b[3] << 32);
	l += ((int64_t) b[4] << 24);
	l += ((int64_t) b[5] << 16);
	l += ((int64_t) b[6] << 8);
	l += (int64_t) b[7];
	return l;
}

int utility_mqtt_write_int64(u_char *b,size_t len,int64_t n)
{
	b[7] = (u_char) (n & 0xff);
	b[6] = (u_char) (n >> 8  & 0xff);
	b[5] = (u_char) (n >> 16 & 0xff);
	b[4] = (u_char) (n >> 24 & 0xff);
	b[3] = (u_char) (n >> 32 & 0xff);
	b[2] = (u_char) (n >> 40  & 0xff);
	b[1] = (u_char) (n >> 48 & 0xff);
	b[0] = (u_char) (n >> 56 & 0xff);
	return 8;
}

int utility_mqtt_write_uint64(u_char *b,size_t len,uint64_t n)
{
	b[7] = (u_char) (n & 0xff);
	b[6] = (u_char) (n >> 8  & 0xff);
	b[5] = (u_char) (n >> 16 & 0xff);
	b[4] = (u_char) (n >> 24 & 0xff);
	b[3] = (u_char) (n >> 32 & 0xff);
	b[2] = (u_char) (n >> 40  & 0xff);
	b[1] = (u_char) (n >> 48 & 0xff);
	b[0] = (u_char) (n >> 56 & 0xff);
	return 8;
}

int64_t utility_mqtt_read_int64(u_char *b,size_t len)
{
	int64_t l;
	l = ((int64_t) b[0] << 56);
	// 如果不强制转换为int64_t，那么默认会当作int，导致最高64位丢失
	l += ((int64_t) b[1] << 48);
	l += ((int64_t) b[2] << 40);
	l += ((int64_t) b[3] << 32);
	l += ((int64_t) b[4] << 24);
	l += ((int64_t) b[5] << 16);
	l += ((int64_t) b[6] << 8);
	l += (int64_t) b[7];
	return l;
}

uint64_t utility_mqtt_read_uint64(u_char *b,size_t len)
{
	uint64_t l;
	l = ((uint64_t) b[0] << 56);
	// 如果不强制转换为int64_t，那么默认会当作int，导致最高64位丢失
	l += ((uint64_t) b[1] << 48);
	l += ((uint64_t) b[2] << 40);
	l += ((uint64_t) b[3] << 32);
	l += ((uint64_t) b[4] << 24);
	l += ((uint64_t) b[5] << 16);
	l += ((uint64_t) b[6] << 8);
	l += (uint64_t) b[7];
	return l;
}


int utility_mqtt_write_string(u_char *p,size_t plen,ngx_str_t *str)
{
	utility_mqtt_write_uint16(p,plen, str->len);
	utility_mqtt_write_bytes(p+2,plen,(char*)str->data, str->len);
	return str->len+2;
}

int utility_mqtt_read_string(u_char *p,size_t plen,ngx_str_t *out)
{
	uint16_t len;

	len = utility_mqtt_read_uint16(p,plen);
	dd("read_uint16: %d",len);
	if(len==UINT16_MAX) return NGX_ERROR;

	if(plen-2<len) return NGX_ERROR;
	out->len = len;
	out->data = p+2;
	return 2+len;
}

u_char utility_mqtt_encode_flags(u_char qos,bool dup,bool retain) {
	u_char flags = 0;
	if (dup) {
		flags |= 0x08;
	}
	if (retain) {
		flags |= 0x01;
	}
	flags |= ((qos & 0x03) << 1);
	return flags;
}

static const uint16_t crc16tab[256]= {
    0x0000,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,
    0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef,
    0x1231,0x0210,0x3273,0x2252,0x52b5,0x4294,0x72f7,0x62d6,
    0x9339,0x8318,0xb37b,0xa35a,0xd3bd,0xc39c,0xf3ff,0xe3de,
    0x2462,0x3443,0x0420,0x1401,0x64e6,0x74c7,0x44a4,0x5485,
    0xa56a,0xb54b,0x8528,0x9509,0xe5ee,0xf5cf,0xc5ac,0xd58d,
    0x3653,0x2672,0x1611,0x0630,0x76d7,0x66f6,0x5695,0x46b4,
    0xb75b,0xa77a,0x9719,0x8738,0xf7df,0xe7fe,0xd79d,0xc7bc,
    0x48c4,0x58e5,0x6886,0x78a7,0x0840,0x1861,0x2802,0x3823,
    0xc9cc,0xd9ed,0xe98e,0xf9af,0x8948,0x9969,0xa90a,0xb92b,
    0x5af5,0x4ad4,0x7ab7,0x6a96,0x1a71,0x0a50,0x3a33,0x2a12,
    0xdbfd,0xcbdc,0xfbbf,0xeb9e,0x9b79,0x8b58,0xbb3b,0xab1a,
    0x6ca6,0x7c87,0x4ce4,0x5cc5,0x2c22,0x3c03,0x0c60,0x1c41,
    0xedae,0xfd8f,0xcdec,0xddcd,0xad2a,0xbd0b,0x8d68,0x9d49,
    0x7e97,0x6eb6,0x5ed5,0x4ef4,0x3e13,0x2e32,0x1e51,0x0e70,
    0xff9f,0xefbe,0xdfdd,0xcffc,0xbf1b,0xaf3a,0x9f59,0x8f78,
    0x9188,0x81a9,0xb1ca,0xa1eb,0xd10c,0xc12d,0xf14e,0xe16f,
    0x1080,0x00a1,0x30c2,0x20e3,0x5004,0x4025,0x7046,0x6067,
    0x83b9,0x9398,0xa3fb,0xb3da,0xc33d,0xd31c,0xe37f,0xf35e,
    0x02b1,0x1290,0x22f3,0x32d2,0x4235,0x5214,0x6277,0x7256,
    0xb5ea,0xa5cb,0x95a8,0x8589,0xf56e,0xe54f,0xd52c,0xc50d,
    0x34e2,0x24c3,0x14a0,0x0481,0x7466,0x6447,0x5424,0x4405,
    0xa7db,0xb7fa,0x8799,0x97b8,0xe75f,0xf77e,0xc71d,0xd73c,
    0x26d3,0x36f2,0x0691,0x16b0,0x6657,0x7676,0x4615,0x5634,
    0xd94c,0xc96d,0xf90e,0xe92f,0x99c8,0x89e9,0xb98a,0xa9ab,
    0x5844,0x4865,0x7806,0x6827,0x18c0,0x08e1,0x3882,0x28a3,
    0xcb7d,0xdb5c,0xeb3f,0xfb1e,0x8bf9,0x9bd8,0xabbb,0xbb9a,
    0x4a75,0x5a54,0x6a37,0x7a16,0x0af1,0x1ad0,0x2ab3,0x3a92,
    0xfd2e,0xed0f,0xdd6c,0xcd4d,0xbdaa,0xad8b,0x9de8,0x8dc9,
    0x7c26,0x6c07,0x5c64,0x4c45,0x3ca2,0x2c83,0x1ce0,0x0cc1,
    0xef1f,0xff3e,0xcf5d,0xdf7c,0xaf9b,0xbfba,0x8fd9,0x9ff8,
    0x6e17,0x7e36,0x4e55,0x5e74,0x2e93,0x3eb2,0x0ed1,0x1ef0
};
//Redis CRC-16-CCITT
uint16_t lua_utility_redisCRC16(const char *buf, int len) {
    int counter;
    uint16_t crc = 0;
    for (counter = 0; counter < len; counter++)
            crc = (crc<<8) ^ crc16tab[((crc>>8) ^ *buf++)&0x00FF];
    return crc;
}



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

static void* _bit_vector_default_alloc(size_t n,size_t size){
	return calloc(n,size);
}
int utility_bit_vector_init(ngx_slab_pool_t *shpool,bit_vector_t *bv,uint64_t n){
	bv->size = n==0?(UINT_MAX>>3)+1:(n>>3)+1;
	if(shpool)
		bv->bits = (unsigned char*)ngx_slab_alloc(shpool, bv->size);
	else
		bv->bits = (unsigned char*)calloc(1,bv->size);
	if(bv->bits==NULL)return -1;
	bv->count=-1;
	return 0;
}
void utility_bit_vector_destroy(ngx_slab_pool_t *shpool,bit_vector_t *bv){
	if(shpool)
		ngx_slab_free(shpool,bv->bits);
	else
		free(bv->bits);
}
void utility_bit_vector_set(bit_vector_t *bv,uint64_t bit){
	size_t pos=bit>>3;
	if(pos>bv->size){
		printf("pos too large[ %lu - %zu ]\n",bit,bv->size);
		return;
	}
	bv->bits[pos] |= 1 << (bit & 7);
    bv->count = -1;
}

bool utility_bit_vector_get(bit_vector_t *bv,uint64_t bit){
	size_t pos=bit>>3;
	if(pos>bv->size){
		printf("pos too large[ %lu - %zu ]\n",bit,bv->size);
		return false;
	}
	return (bv->bits[pos] & (1 << (bit & 7))) != 0;
}

void utility_bit_vector_del(bit_vector_t *bv,uint64_t bit){
	size_t pos=bit>>3;
	if(pos>bv->size){
		printf("pos too large[ %lu - %zu ]\n",bit,bv->size);
		return;
	}
	bv->bits[pos] &= ~(1 << (bit & 7));
    bv->count = -1;
}

void utility_bit_vector_clear(bit_vector_t *bv){
	memset(bv->bits,0,bv->size);
	bv->count = -1;
}

uint64_t utility_bit_vector_count(bit_vector_t *bv){
	// if the vector has been modified
    if (bv->count == -1) {
      uint64_t c = 0;
      size_t end = bv->size;
	  size_t i;
      for (i = 0; i < end; i++)
        c += BYTE_COUNTS[bv->bits[i] & 0xFF];	  // sum bits per byte
      bv->count = c;
    }
    return bv->count;
}

#ifdef __cplusplus
}
#endif

