/* Host-only test harness; core remains freestanding. */
#include "raf_gate.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

typedef struct fixture {
 int pre,apply,verify,rollback,failback,recovery,deny_persist_at,uncertain_at;
 unsigned calls,applied,verified,rollbacks,failbacks,recovery_checks,persisted;
 raf_receipt journal[3];
} fixture;
static int pre(void *v){return ((fixture*)v)->pre;}
static int app(void *v){fixture*f=v;f->applied++;return f->apply;}
static int ver(void *v){fixture*f=v;f->verified++;return f->verify;}
static int roll(void *v){fixture*f=v;f->rollbacks++;return f->rollback;}
static int back(void *v){fixture*f=v;f->failbacks++;return f->failback;}
static int recover_check(void *v){fixture*f=v;f->recovery_checks++;return f->recovery;}
static int store(void *v,const raf_receipt*r){
 fixture*f=v;f->calls++;
 if((int)f->calls==f->uncertain_at)return -1;
 if((int)f->calls==f->deny_persist_at)return 0;
 assert(f->persisted<3u);f->journal[f->persisted++]=*r;return 1;
}
static void setup(fixture*f,raf_ports*p,raf_request*request){
 static const uint8_t ev[4]={'T','E','S','T'};
 memset(f,0,sizeof(*f));f->pre=f->apply=f->verify=f->rollback=f->failback=f->recovery=1;
 p->context=f;p->preflight=pre;p->apply=app;p->verify=ver;p->rollback=roll;
 p->failback=back;p->verify_recovery=recover_check;p->persist=store;
 memset(request,0,sizeof(*request));request->next_seq=42u;
 request->evidence=ev;request->evidence_bytes=4u;
}
static void verify_links(fixture*f,raf_request*r){
 assert(f->persisted==2u);
 assert(raf_receipt_valid(&f->journal[0],42u,r->previous_sha256));
 assert(raf_receipt_valid(&f->journal[1],43u,&f->journal[0].bytes[82]));
}
static void test_sha(void){
 const uint8_t abc[3]={'a','b','c'};uint8_t h[32];char b[65];size_t i;
 static const char *gold="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
 static const char *empty="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
 static const char hexd[]="0123456789abcdef";
 raf_sha256(abc,3u,h);for(i=0;i<32u;i++){b[2u*i]=hexd[h[i]>>4];b[2u*i+1u]=hexd[h[i]&15u];}b[64]=0;
 assert(strcmp(b,gold)==0);
 raf_sha256(NULL,0,h);for(i=0;i<32u;i++){b[2u*i]=hexd[h[i]>>4];b[2u*i+1u]=hexd[h[i]&15u];}assert(strcmp(b,empty)==0);
}
int main(void){
 fixture f;raf_ports p;raf_request q;raf_result r;size_t length=0;char sys[180];int tests=0;
 test_sha();tests++;
 setup(&f,&p,&q);raf_gate_run(&r,&q,&p);
 assert(raf_receipt_evidence_match(&r.terminal,q.evidence,q.evidence_bytes));
 {const uint8_t bad[4]={'B','A','D','!'};
  assert(!raf_receipt_evidence_match(&r.terminal,bad,sizeof(bad)));}tests++;
 setup(&f,&p,&q);raf_gate_run(&r,&q,&p);assert(r.state==RAF_PASS&&r.terminal_durable&&f.applied==1u);verify_links(&f,&q);tests++;
 assert(raf_syslog5424_format(sys,sizeof(sys),&length,&r.terminal)&&length==strlen(sys));
 assert(strncmp(sys,"<134>1 - - RAFAELIA - AUDIT - seq=43",strlen("<134>1 - - RAFAELIA - AUDIT - seq=43"))==0);tests++;
 sys[0]='X';assert(!raf_syslog5424_format(sys,10u,&length,&r.terminal)&&sys[0]==0);tests++;
 r.terminal.bytes[56]^=1u;assert(!raf_receipt_valid(&r.terminal,43u,&f.journal[0].bytes[82]));tests++;
 setup(&f,&p,&q);f.pre=0;raf_gate_run(&r,&q,&p);assert(r.state==RAF_BLOCKED&&f.applied==0u&&f.persisted==1u);tests++;
 setup(&f,&p,&q);f.deny_persist_at=1;raf_gate_run(&r,&q,&p);assert(r.state==RAF_BLOCKED&&f.applied==0u&&f.persisted==0u);tests++;
 setup(&f,&p,&q);f.uncertain_at=1;raf_gate_run(&r,&q,&p);assert(r.state==RAF_BLOCKED&&f.applied==0u);tests++;
 setup(&f,&p,&q);f.apply=0;raf_gate_run(&r,&q,&p);assert(r.state==RAF_ROLLED_BACK&&f.rollbacks==1u&&f.recovery_checks==1u&&f.verified==0u);verify_links(&f,&q);tests++;
 setup(&f,&p,&q);f.verify=0;raf_gate_run(&r,&q,&p);assert(r.state==RAF_ROLLED_BACK&&f.rollbacks==1u&&f.verified==1u);tests++;
 setup(&f,&p,&q);f.verify=0;f.rollback=0;raf_gate_run(&r,&q,&p);assert(r.state==RAF_FAILBACK&&f.failbacks==1u&&r.terminal_durable);tests++;
 setup(&f,&p,&q);f.verify=0;f.rollback=0;f.failback=0;raf_gate_run(&r,&q,&p);assert(r.state==RAF_UNRECOVERABLE&&r.terminal_durable);tests++;
 setup(&f,&p,&q);f.verify=0;f.recovery=0;raf_gate_run(&r,&q,&p);
 assert(r.state==RAF_UNRECOVERABLE&&f.rollbacks==1u&&f.failbacks==1u&&f.recovery_checks==2u);tests++;
 setup(&f,&p,&q);f.deny_persist_at=2;raf_gate_run(&r,&q,&p);assert(r.state==RAF_ROLLED_BACK&&f.rollbacks==1u&&f.persisted==2u);verify_links(&f,&q);tests++;
 setup(&f,&p,&q);f.uncertain_at=2;raf_gate_run(&r,&q,&p);assert(r.state==RAF_UNRECOVERABLE&&f.rollbacks==0u&&!r.terminal_durable);tests++;
 setup(&f,&p,&q);q.next_seq=UINT64_MAX;raf_gate_run(&r,&q,&p);assert(r.state==RAF_BLOCKED&&f.calls==0u);tests++;
 setup(&f,&p,&q);p.verify_recovery=NULL;raf_gate_run(&r,&q,&p);
 assert(r.state==RAF_BLOCKED&&f.calls==0u);tests++;
 setup(&f,&p,&q);p.rollback=NULL;raf_gate_run(&r,&q,&p);assert(r.state==RAF_BLOCKED&&f.calls==0u);tests++;
 setup(&f,&p,&q);q.evidence=NULL;raf_gate_run(&r,&q,&p);assert(r.state==RAF_BLOCKED&&f.calls==0u);tests++;
 setup(&f,&p,&q);q.evidence_bytes=0u;raf_gate_run(&r,&q,&p);
 assert(r.state==RAF_BLOCKED&&f.calls==0u);tests++;
 printf("PASS_SCOPED_HOST: %d tests\n",tests);return 0;
}
