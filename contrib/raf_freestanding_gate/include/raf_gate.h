/*
 * RAFAELIA freestanding evidence gate — independent optional experiment.
 * Original contribution offered under the InterBase Public License 1.0.
 * Provided AS IS, without warranty; not part of Firebird's public ABI.
 *
 * Warnings are diagnostics, not successes. Do not suppress them to fake a gate.
 * Strict types, explicit bounds, and replayable receipts aid inspection.
 */
#ifndef RAF_FREESTANDING_GATE_H
#define RAF_FREESTANDING_GATE_H
#include <stddef.h>
#include <stdint.h>

#define RAF_SHA256_BYTES 32u
#define RAF_RECEIPT_BYTES 114u
#define RAF_RECEIPT_PREFIX_BYTES 82u

typedef enum raf_state {
    RAF_BLOCKED = 0,
    RAF_PASS = 1,
    RAF_ROLLED_BACK = 2,
    RAF_FAILBACK = 3,
    RAF_UNRECOVERABLE = 4,
    RAF_PREPARED = 16
} raf_state;

typedef struct raf_receipt { uint8_t bytes[RAF_RECEIPT_BYTES]; } raf_receipt;
typedef int (*raf_step_fn)(void *context);
/* Return 1 only for atomically durable append, 0 for definite rejection,
 * -1 for indeterminate persistence. Never report an unverified success. */
typedef int (*raf_persist_fn)(void *context, const raf_receipt *receipt);

typedef struct raf_ports {
    void *context;
    raf_step_fn preflight; /* Mandatory, read-only */
    raf_step_fn apply;     /* Mandatory, reversible and idempotent */
    raf_step_fn verify;    /* Mandatory, checks postconditions */
    raf_step_fn rollback;  /* Mandatory, checked compensation */
    raf_step_fn failback;  /* Optional, switch to independently known-good */
    raf_step_fn verify_recovery; /* Mandatory independent recovery postcheck */
    raf_persist_fn persist;/* Mandatory, CAS chain/sequence at durable store */
} raf_ports;

typedef struct raf_request {
    uint64_t next_seq;
    uint8_t previous_sha256[RAF_SHA256_BYTES];
    const uint8_t *evidence;
    size_t evidence_bytes;
} raf_request;

typedef struct raf_result {
    raf_state state;
    uint8_t terminal_durable;
    raf_receipt prepared;
    raf_receipt terminal;
} raf_result;

/* No libc, allocation, I/O, clock, threads, or platform runtime calls. */
void raf_sha256(const uint8_t *data, size_t size, uint8_t digest[RAF_SHA256_BYTES]);
int raf_receipt_create(raf_receipt *out, uint64_t seq, raf_state state,
                       const uint8_t prev_sha256[RAF_SHA256_BYTES],
                       const uint8_t *evidence, size_t evidence_bytes);
int raf_receipt_valid(const raf_receipt *r, uint64_t expected_seq,
                      const uint8_t expected_prev_sha256[RAF_SHA256_BYTES]);
int raf_receipt_evidence_match(const raf_receipt *r, const uint8_t *evidence,
                               size_t evidence_bytes);
void raf_gate_run(raf_result *out, const raf_request *request, const raf_ports *ports);
/* RFC 5424 header/message text profile only; TLS transport is NOT included. */
int raf_syslog5424_format(char *out, size_t capacity, size_t *written,
                          const raf_receipt *receipt);
#endif
