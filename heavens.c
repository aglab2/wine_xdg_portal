#include "heavens.h"

#include "log.h"

#include <stdbool.h>
#include <windows.h>

/*
                     syscall:
000000000011af80         endbr64                                                ; Begin of unwind block (FDE at 0x1deb68)
000000000011af84         mov        rax, rdi
000000000011af87         mov        rdi, rsi
000000000011af8a         mov        rsi, rdx
000000000011af8d         mov        rdx, rcx
000000000011af90         mov        r10, r8
000000000011af93         mov        r8, r9
000000000011af96         mov        r9, qword [rsp+8]
000000000011af9b         syscall
000000000011af9d         cmp        rax, 0xfffffffffffff001
000000000011afa3         jae        loc_11afa6

000000000011afa5         ret
                        ; endp

                     loc_11afa6:
000000000011afa6         mov        rcx, qword [qword_212cf0]                   ; qword_212cf0, CODE XREF=syscall+35
000000000011afad         neg        eax
000000000011afaf         mov        dword [fs:rcx], eax
000000000011afb2         or         rax, 0xffffffffffffffff
000000000011afb6         ret
                        ; endp
*/

/* Wine heaven's gate caller part - we want to remove it
4a: 48 89 c1                mov    rcx,rax
4d: 49 8d 56 08             lea    rdx,[r14+0x8]
51: e8 7a 01 00 00          call   0x1d0
 */

 // as compiled from syscall.S
static const BYTE syscall_tmpl[] = { 0x55, 0x48, 0x89, 0xE5, 0x48, 0x81, 0xEC, 0x20, 0x03, 0x00, 0x00, 0x48, 0x89, 0xCF, 0x48, 0x89, 0xD3, 0x8B, 0x33, 0x8B, 0x53, 0x04, 0x8B, 0x4B, 0x08, 0x44, 0x8B, 0x43, 0x0C, 0x44, 0x8B, 0x4B, 0x10, 0xF3, 0x48, 0x0F, 0xAE, 0xC0, 0x48, 0x89, 0x44, 0x24, 0x08, 0x48, 0x8D, 0x84, 0x24, 0x18, 0x02, 0x00, 0x00, 0x48, 0x89, 0x00, 0xF3, 0x48, 0x0F, 0xAE, 0xD0, 0xFF, 0x15, 0xAF, 0xFF, 0xFF, 0xFF, 0x48, 0x83, 0xF8, 0xFF, 0x75, 0x14, 0x48, 0x8D, 0x84, 0x24, 0x18, 0x02, 0x00, 0x00, 0x48, 0x03, 0x05, 0xA2, 0xFF, 0xFF, 0xFF, 0x8B, 0x00, 0x48, 0xF7, 0xD8, 0x48, 0x8B, 0x5C, 0x24, 0x08, 0xF3, 0x48, 0x0F, 0xAE, 0xD3, 0x48, 0x81, 0xC4, 0x20, 0x03, 0x00, 0x00, 0x48, 0x89, 0xEC, 0x5D, 0xC3 };

#pragma pack(push,1)
struct thunk_32to64
{
    BYTE  ljmp;   /* jump far, absolute indirect */
    BYTE  modrm;  /* address=disp32, opcode=5 */
    DWORD op;
    DWORD addr;
    WORD  cs;
};
#pragma pack(pop)

#define OFFSET_LJMP 0
#define OFFSET_TRAMPOLINE 0x200
#define OFFSET_SYSCALL_PTRS 0x400
#define OFFSET_SYSCALL (OFFSET_SYSCALL_PTRS + 0x10)

static BYTE DECLSPEC_ALIGN(4096) code_buffer[0x1000];

enum Instruction
{
    I_UNKNOWN,
    I_MOV,
    I_CALL,
    I_LEA,
};

// Takes the trampoline routine and decompiles it, copying it to the output buffer, and patching call to the alt_callback function.
static int decompile_copy(const void* in, size_t limit, void* out, const void* alt_callback)
{
    const BYTE* p = (const BYTE*) in;
    const BYTE* end = p + limit;

    BYTE* out_p = (BYTE*) out;

    unsigned distance = p - out_p;
    log("decompile_copy: %p -> %p, distance: %x\n", p, out_p, distance);

#define consume(n) do { memcpy(out_p, p, n); out_p += n; p += n; } while(0)

    while (p < end)
    {
        void* start = (void*) out_p;
        BYTE maybeRex = *p;
        if (maybeRex >= 0x40 && maybeRex <= 0x4F)
        {
            log("Rex: %02x\n", maybeRex);
            consume(1);
        }

        bool want_modrm = false;
        bool want_imm8 = false;

        enum Instruction instruction = I_UNKNOWN;

        BYTE opcode = *p;
        consume(1);
        if (0x0f == opcode)
        {
            log("0f\n");
            opcode = *p;
            consume(1);
            if (0xba == opcode)
            {
                log("btr\n");
                want_modrm = true;
                want_imm8 = true;
            }
            else
            {
                log("unknown opcode: 0f %02x???\n", opcode);
                return -1;
            }
        }
        else if (0x50 <= opcode && opcode <= 0x57)
        {
            log("push r%d\n", opcode - 0x50);
        }
        else if (0x58 <= opcode && opcode <= 0x5F)
        {
            log("pop r%d\n", opcode - 0x58);
        }
        else if (0x72 == opcode)
        {
            log("jb\n");
            want_imm8 = true;
        }
        else if (0x87 == opcode)
        {
            log("xchg\n");
            want_modrm = true;
        }
        else if (0x89 == opcode)
        {
            log("mov\n");
            instruction = I_MOV;
            want_modrm = true;
        }
        else if (0x8b == opcode)
        {
            log("mov\n");
            instruction = I_MOV;
            want_modrm = true;
        }
        else if (0x8D == opcode)
        {
            log("lea\n");
            instruction = I_LEA;
            want_modrm = true;
        }
        else if (0x8e == opcode)
        {
            log("movseg\n");
            want_modrm = true;
        }
        else if (0x9C == opcode)
        {
            log("pushf\n");
        }
        else if (0xcf == opcode)
        {
            log("iret\n");
            break;
        }
        else if (0xe8 == opcode)
        {
            log("call\n");
            instruction = I_CALL;
        }
        else if (0xff == opcode)
        {
            log("group\n");
            want_modrm = true;
        }
        else
        {
            log("unknown opcode: %02x???\n", opcode);
            return -1;
        }

        if (want_modrm)
        {
            BYTE modrm = *p;
            log("\tmodrm: %02x\n", modrm);
            consume(1);

            BYTE mod = (modrm & 0xC0) >> 6;
            BYTE reg = (modrm & 0x38) >> 3;
            BYTE rm  = (modrm & 0x07);

            log("\tmod: %02x, reg: %02x, rm: %02x\n", mod, reg, rm);

            if (mod != 3 && rm == 4)
            {
                log("\tsib: %02x\n", *p);
                consume(1);
            }

            switch (mod)
            {
                case 0:
                    if (rm == 5)
                    {
                        log("\trip rel: %08x\n", *(unsigned*)p);
                        consume(4);
                        *(unsigned*)(out_p - 4) += distance;
                    }

                    break;
                case 1:
                    log("\tdisp8: %02x\n", *p);
                    consume(1);
                    break;
                case 2:
                    log("\tdisp32: %08x\n", *(unsigned*)p);
                    consume(4);
                    break;
                case 3:
                    log("\treg: %02x\n", rm);
                    break;
            }
        }

        if (I_CALL == instruction)
        {
            log("\t%08x\n", *(unsigned*)p);
            p += 4;
            *(unsigned*)(out_p) = (const BYTE*) alt_callback - (const BYTE*) out_p - 4;
            log("\tpatched call to %p, offset: %08x\n", alt_callback, *(unsigned*)(out_p));
            out_p += 4;
        }

        if (want_imm8)
        {
            log("\timm8: %02x\n", *p);
            consume(1);
        }
    }

    return 0;
}

void* hg_setup(struct LibcFunctions libc)
{
    memset(code_buffer, 0x90, sizeof(code_buffer));

    struct thunk_32to64 *linsys = (struct thunk_32to64 *) (code_buffer + OFFSET_LJMP);

    void* teb = NtCurrentTeb();
    if (!teb)
    {
        log("Failed to get TEB\n");
        return NULL;
    }

    struct thunk_32to64* winsys = *(void**)((BYTE*)teb + 0xC0); // TEB->WOW32Reserved

    // TODO: make sure that winsys matches the expected layout

    log("WOW32Reserved: %p\n", winsys);

    log("winsys: %p\n", winsys);
    log("winsys.op: %x\n", winsys->op);
    log("winsys.addr: %x\n", winsys->addr);

    void* ori_trampoline = (void*) winsys->addr;
    void* new_trampoline = (void*) (code_buffer + OFFSET_TRAMPOLINE);
    void* new_syscall_core = (void*) (code_buffer + OFFSET_SYSCALL);

    *( int64_t*) (code_buffer + OFFSET_SYSCALL - 0x8 ) = libc.errno_fs_offset;
    *(uint64_t*) (code_buffer + OFFSET_SYSCALL - 0x10) = libc.syscall;
    memcpy(new_syscall_core, syscall_tmpl, sizeof(syscall_tmpl));

    // Decompile the original trampoline and copy it to the new trampoline, patching calls to the syscall core.
    int res = decompile_copy(ori_trampoline, 0x400, new_trampoline, new_syscall_core);

    if (res)
    {
        log("Failed to decompile trampoline\n");
        log_buff("trampoline", ori_trampoline, 0x400);
        return NULL;
    }

    log("trampoline: %p\n", new_trampoline);
    log("syscall core: %p\n", new_syscall_core);
    log("linsys: %p\n", linsys);

    *linsys = *winsys;
    linsys->op = (DWORD) &linsys->addr;
    linsys->addr = (DWORD) new_trampoline;

    SIZE_T size;
    ULONG old_prot;

    VirtualProtect(code_buffer, sizeof(code_buffer), PAGE_EXECUTE_READ, &old_prot);
    return linsys;
}
