#include "hookapi.h"
#include <stdint.h>
#define HAS_CALLBACK

#define ASSERT(x)\
{\
    if (!(x))\
        rollback(0,0,__LINE__);\
}

#define DONE()\
    accept(0,0,__LINE__)

/**
 * Account Owner can:
 * ttINVOKE:
 *  Blob: (packed data) (up to 32)
 *      ( 1 byte action type + 20 byte account id ) +
 *
 * Action Type: 0 to remove account, 1 to add account 
 */
int64_t hook(uint32_t r)
{
    _g(1,1);

    uint8_t ttbuf[16];
    int64_t br = otxn_field(SBUF(ttbuf), sfTransactionType);
    uint32_t txntype = ((uint32_t)(ttbuf[0]) << 16U) + ((uint32_t)(ttbuf[1]));

    // pass anything that isn't a ttINVOKE
    if (txntype != 99)
        DONE();

    // get the account id
    uint8_t account_field[20];
    ASSERT(otxn_field(SBUF(account_field), sfAccount) == 20);

    uint8_t hook_accid[20];
    hook_account(SBUF(hook_accid));
    
    // pass outgoing txns

    int self_sent_txn = 0;
    // if the account is the sender
    if (BUFFER_EQUAL_20(hook_accid, account_field))
    {
        self_sent_txn = 1;

        // ... and there is a destination set
        uint8_t dest[20];
        if (otxn_field(SBUF(dest), sfDestination) == 20)
        {
            // ... and the destination isnt the account
            if (!BUFFER_EQUAL_20(hook_accid, dest))
            {
                // .. then they are invoking someone else's hook
                // and we need to not interfere with that.
                DONE();
            }
        }
    }

    uint8_t txn_id[32];
    ASSERT(otxn_id(txn_id, 32, 0) == 32);

#define sfBlob ((7U << 16U) + 26U)

    ASSERT(otxn_slot(1) == 1);
    ASSERT(slot_subfield(1, sfBlob, 2) == 2);
    
    uint8_t buffer[676];

    ASSERT(slot(SBUF(buffer), 2) > 0);

    // sfBlob is VL(type=7) field=26
    // therefore its 2 lead-in bytes are: 70 1A
    // it further has length encoding, and payload length can be up to 672 bytes

    uint64_t len = buffer[2];
    uint8_t* ptr = buffer + 3;
    if (len > 192)
    {
        len = 193 + ((len - 193) * 256) + buffer[3];
        ptr++;
    }

    uint8_t* end = ptr + len;

    // execution to here means it's a valid modification instruction
    for (; ptr < end, GUARD(32); ptr += 21)
    {
        ASSERT(state_set(
                    *ptr == 0 ? txn_id : 0, *ptr == 0 ? 32 : 0,
                    ptr+1, 20
                    ) == 32);
    }

    DONE();

}