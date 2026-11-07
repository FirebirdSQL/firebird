/*
 * RAFAELIA freestanding evidence gate — independent optional experiment.
 * Original contribution offered under the InterBase Public License 1.0.
 * Provided AS IS, without warranty; not part of Firebird's public ABI.
 *
 * Compiler warnings must be investigated: an unsafe conversion or omitted
 * branch may invalidate a failure gate. No warning is silenced here.
 */
#include "raf_gate.h"

/* SHA-256 following FIPS 180-4/RFC 6234 algorithm, original implementation. */
static const uint32_t k256[64] = {
 0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
 0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
 0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
 0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
 0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
 0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
 0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
 0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};
static uint32_t rotate(uint32_t x, unsigned count) { return (x >> count) | (x << (32u-count)); }
static uint32_t get_be32(const uint8_t *p) {
 return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|(uint32_t)p[3];
}
static void put_be32(uint8_t *p, uint32_t x) {
 p[0]=(uint8_t)(x>>24);p[1]=(uint8_t)(x>>16);p[2]=(uint8_t)(x>>8);p[3]=(uint8_t)x;
}
static void put_be64(uint8_t *p, uint64_t x) {
 unsigned i;for(i=0;i<8u;i++)p[i]=(uint8_t)(x>>(56u-8u*i));
}
static uint64_t get_be64(const uint8_t *p) {
 uint64_t x=0; unsigned i;for(i=0;i<8u;i++) x=(x<<8)|(uint64_t)p[i];return x;
}
static int same(const uint8_t *a,const uint8_t *b,size_t n) {
 uint8_t d=0;size_t i;for(i=0;i<n;i++)d=(uint8_t)(d|(a[i]^b[i]));return d==0u;
}
typedef struct sha256ctx {
 uint32_t h[8];uint8_t block[64];size_t used;uint64_t count;
} sha256ctx;
static void sha_block(sha256ctx *c) {
 uint32_t w[64],a,b,d,e,f,g,h,t1,t2,cc;unsigned i;
 for(i=0;i<16u;i++)w[i]=get_be32(&c->block[i*4u]);
 for(i=16;i<64u;i++){
  uint32_t x=w[i-15u],y=w[i-2u];
  uint32_t s0=rotate(x,7u)^rotate(x,18u)^(x>>3);
  uint32_t s1=rotate(y,17u)^rotate(y,19u)^(y>>10);
  w[i]=w[i-16u]+s0+w[i-7u]+s1;
 }
 a=c->h[0];b=c->h[1];cc=c->h[2];d=c->h[3];e=c->h[4];f=c->h[5];g=c->h[6];h=c->h[7];
 for(i=0;i<64u;i++){
  uint32_t s1=rotate(e,6u)^rotate(e,11u)^rotate(e,25u);
  uint32_t ch=(e&f)^(~e&g);
  uint32_t s0=rotate(a,2u)^rotate(a,13u)^rotate(a,22u);
  uint32_t maj=(a&b)^(a&cc)^(b&cc);
  t1=h+s1+ch+k256[i]+w[i];t2=s0+maj;
  h=g;g=f;f=e;e=d+t1;d=cc;cc=b;b=a;a=t1+t2;
 }
 c->h[0]+=a;c->h[1]+=b;c->h[2]+=cc;c->h[3]+=d;
 c->h[4]+=e;c->h[5]+=f;c->h[6]+=g;c->h[7]+=h;
}
static void sha_init(sha256ctx *c) {
 static const uint32_t iv[8]={0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
  0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
 unsigned i;for(i=0;i<8u;i++)c->h[i]=iv[i];c->used=0;c->count=0;
}
static void sha_byte(sha256ctx *c,uint8_t v) {
 c->block[c->used++]=v;c->count++;
 if(c->used==64u){sha_block(c);c->used=0;}
}
void raf_sha256(const uint8_t *data,size_t size,uint8_t digest[RAF_SHA256_BYTES]){
 sha256ctx c;size_t i;unsigned j;uint64_t bits;
 if(!digest || (!data && size!=0u) || size>UINT64_MAX/8u) return;
 sha_init(&c);for(i=0;i<size;i++)sha_byte(&c,data[i]);bits=c.count*8u;
 sha_byte(&c,0x80u);
 while(c.used!=56u)sha_byte(&c,0u);
 for(j=0;j<8u;j++)sha_byte(&c,(uint8_t)(bits>>(56u-8u*j)));
 for(j=0;j<8u;j++)put_be32(&digest[j*4u],c.h[j]);
}
static void copy(uint8_t *d,const uint8_t *s,size_t n) {size_t i;for(i=0;i<n;i++)d[i]=s[i];}
static int state_valid(raf_state s) {
 return s==RAF_BLOCKED || s==RAF_PASS || s==RAF_ROLLED_BACK ||
        s==RAF_FAILBACK || s==RAF_UNRECOVERABLE || s==RAF_PREPARED;
}
int raf_receipt_create(raf_receipt *out,uint64_t seq,raf_state state,
                       const uint8_t prev[RAF_SHA256_BYTES],const uint8_t *evidence,size_t n){
 uint8_t *b;uint8_t evhash[32];
 if(!out||!prev||(!evidence&&n!=0u)||n>UINT32_MAX||!state_valid(state))return 0;
 b=out->bytes;b[0]='R';b[1]='F';b[2]='G';b[3]='1';b[4]=1u;
 put_be64(&b[5],seq);b[13]=(uint8_t)state;copy(&b[14],prev,32u);
 put_be32(&b[46],(uint32_t)n);raf_sha256(evidence,n,evhash);
 copy(&b[50],evhash,32u);raf_sha256(b,RAF_RECEIPT_PREFIX_BYTES,&b[82]);return 1;
}
int raf_receipt_valid(const raf_receipt *r,uint64_t seq,const uint8_t prev[32]){
 uint8_t check[32];const uint8_t *b;
 if(!r||!prev)return 0;
 b=r->bytes;
 if(b[0]!='R'||b[1]!='F'||b[2]!='G'||b[3]!='1'||b[4]!=1u||
    get_be64(&b[5])!=seq||!state_valid((raf_state)b[13])||!same(&b[14],prev,32u))return 0;
 raf_sha256(b,RAF_RECEIPT_PREFIX_BYTES,check);return same(check,&b[82],32u);
}
int raf_receipt_evidence_match(const raf_receipt *r,const uint8_t *ev,size_t n){
 uint8_t h[32];
 if(!r||!ev||n==0u||n>UINT32_MAX||get_be32(&r->bytes[46])!=(uint32_t)n)return 0;
 raf_sha256(ev,n,h);return same(h,&r->bytes[50],32u);
}
static void initialize(raf_result *out){
 size_t i;out->state=RAF_BLOCKED;out->terminal_durable=0u;
 for(i=0;i<RAF_RECEIPT_BYTES;i++){out->prepared.bytes[i]=0u;out->terminal.bytes[i]=0u;}
}
static raf_state recover(raf_ports const *ports){
 if(ports->rollback(ports->context)>0 && ports->verify_recovery(ports->context)>0)
  return RAF_ROLLED_BACK;
 if(ports->failback && ports->failback(ports->context)>0 &&
    ports->verify_recovery(ports->context)>0)return RAF_FAILBACK;
 return RAF_UNRECOVERABLE;
}
void raf_gate_run(raf_result *out,const raf_request *req,const raf_ports *ports){
 uint8_t prev2[32];int persist_status;raf_state s;
 if(!out)return;
 initialize(out);
 if(!req||!ports||!ports->preflight||!ports->apply||!ports->verify||
    !ports->rollback||!ports->verify_recovery||!ports->persist||
    req->next_seq==UINT64_MAX||!req->evidence||req->evidence_bytes==0u||
    req->evidence_bytes>UINT32_MAX)return;
 if(ports->preflight(ports->context)<=0){
  if(raf_receipt_create(&out->terminal,req->next_seq,RAF_BLOCKED,req->previous_sha256,
                       req->evidence,req->evidence_bytes))
   out->terminal_durable=(uint8_t)(ports->persist(ports->context,&out->terminal)>0);
  return;
 }
 if(!raf_receipt_create(&out->prepared,req->next_seq,RAF_PREPARED,req->previous_sha256,
                        req->evidence,req->evidence_bytes))return;
 persist_status=ports->persist(ports->context,&out->prepared);
 if(persist_status!=1)return; /* no execution after missing/uncertain prepare receipt */
 copy(prev2,&out->prepared.bytes[82],32u);
 s=(ports->apply(ports->context)>0 && ports->verify(ports->context)>0)?RAF_PASS:recover(ports);
 if(!raf_receipt_create(&out->terminal,req->next_seq+1u,s,prev2,
                        req->evidence,req->evidence_bytes)){out->state=RAF_UNRECOVERABLE;return;}
 persist_status=ports->persist(ports->context,&out->terminal);
 if(persist_status==1){out->state=s;out->terminal_durable=1u;return;}
 if(persist_status<0){out->state=RAF_UNRECOVERABLE;return;} /* unknown outcome: reconcile! */
 if(s==RAF_PASS){
  s=recover(ports);
  if(!raf_receipt_create(&out->terminal,req->next_seq+1u,s,prev2,
                         req->evidence,req->evidence_bytes)){out->state=RAF_UNRECOVERABLE;return;}
  if(ports->persist(ports->context,&out->terminal)==1){
   out->state=s;out->terminal_durable=1u;return;
  }
 }
 out->state=RAF_UNRECOVERABLE; /* never claim safe recovery without receipt */
}
static int append(char *out,size_t cap,size_t *at,const char *s){
 size_t n=0u,i;while(s[n])n++;
 if(*at>cap||n>=cap-*at)return 0;
 for(i=0;i<n;i++)out[*at+i]=s[i];
 *at+=n;out[*at]='\0';return 1;
}
static int dec(char *out,size_t cap,size_t *at,uint64_t n){
 char temp[20];size_t i=0u;
 do{temp[i++]=(char)('0'+(n%10u));n/=10u;}while(n&&i<20u);
 if(*at>cap||i>=cap-*at)return 0;
 while(i)out[(*at)++]=temp[--i];
 out[*at]='\0';return 1;
}
int raf_syslog5424_format(char *out,size_t capacity,size_t *written,const raf_receipt *receipt){
 static const char hex[]="0123456789abcdef";size_t at=0,i;const uint8_t *b;
 if(written)*written=0;
 if(!out||capacity==0u)return 0;
 out[0]='\0';
 if(!receipt||!raf_receipt_valid(receipt,get_be64(&receipt->bytes[5]),&receipt->bytes[14]))return 0;
 b=receipt->bytes;
 if(!append(out,capacity,&at,"<134>1 - - RAFAELIA - AUDIT - seq=")||
    !dec(out,capacity,&at,get_be64(&b[5]))||
    !append(out,capacity,&at," state=")||
    !dec(out,capacity,&at,(uint64_t)b[13])||
    !append(out,capacity,&at," sha256="))goto fail;
 if(at>capacity||64u>=capacity-at)goto fail;
 for(i=0;i<32u;i++){out[at++]=hex[b[82u+i]>>4];out[at++]=hex[b[82u+i]&0x0fu];}
 out[at]='\0';if(written)*written=at;return 1;
fail:out[0]='\0';return 0;
}
