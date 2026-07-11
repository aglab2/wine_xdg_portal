#include "errno_conv.h"

#include <errno.h>

int errnoConv(int result)
{
    if (-4096 < result && result < 0)
    {
        int err = -result;
        if (err <= 34)
        {
            // base errno - mostly ideal match between Linux and Windows
            if (26 == err)
            {
                return ETXTBSY;
            }
            else
            {
                errno = err;
            }
        }
        else
        {
            switch (err)
            {
                case 36: errno = EDEADLK; break;
                case 38: errno = ENOSYS; break;
                case 39: errno = ENOTEMPTY; break;
                case 40: errno = ELOOP; break;
                case 42: errno = ENOMSG; break;
                case 43: errno = EIDRM; break;
                // case 44: errno = ECHRNG; break;
                // case 45: errno = EL2NSYNC; break;
                // case 46: errno = EL3HLT; break;
                // case 47: errno = EL3RST; break;
                // case 48: errno = ELNRNG; break;
                // case 49: errno = EUNATCH; break;
                // case 50: errno = ENOCSI; break;
                // case 51: errno = EL2HLT; break;
                // case 52: errno = EBADE; break;
                // case 53: errno = EBADR; break;
                // case 54: errno = EXFULL; break;
                // case 55: errno = ENOANO; break;
                // case 56: errno = EBADRQC; break;
                // case 57: errno = EBADSLT; break;
                // case 59: errno = EBFONT; break;
                case 60: errno = ENOSTR; break;
                case 61: errno = ENODATA; break;
                case 62: errno = ETIME; break;
                case 63: errno = ENOSR; break;
                // case 64: errno = ENONET; break;
                // case 65: errno = ENOPKG; break;
                // case 66: errno = EREMOTE; break;
                case 67: errno = ENOLINK; break;
                // case 68: errno = EADV; break;
                // case 69: errno = ESRMNT; break;
                // case 70: errno = ECOMM; break;
                case 71: errno = EPROTO; break;
                // case 72: errno = EMULTIHOP; break;
                // case 73: errno = EDOTDOT; break;
                case 74: errno = EBADMSG; break;
                case 75: errno = EOVERFLOW; break;
                // case 76: errno = ENOTUNIQ; break;
                // case 77: errno = EBADFD; break;
                // case 78: errno = EREMCHG; break;
                // case 79: errno = ELIBACC; break;
                // case 80: errno = ELIBBAD; break;
                // case 81: errno = ELIBSCN; break;
                // case 82: errno = ELIBMAX; break;
                // case 83: errno = ELIBEXEC; break;
                case 84: errno = EILSEQ; break;
                // case 85: errno = ERESTART; break;
                // case 86: errno = ESTRPIPE; break;
                // case 87: errno = EUSERS; break;
                case 88: errno = ENOTSOCK; break;
                case 89: errno = EDESTADDRREQ; break;
                case 90: errno = EMSGSIZE; break;
                case 91: errno = EPROTOTYPE; break;
                case 92: errno = ENOPROTOOPT; break;
                case 93: errno = EPROTONOSUPPORT; break;
                // case 94: errno = ESOCKTNOSUPPORT; break;
                case 95: errno = EOPNOTSUPP; break;
                // case 96: errno = EPFNOSUPPORT; break;
                case 97: errno = EAFNOSUPPORT; break;
                case 98: errno = EADDRINUSE; break;
                case 99: errno = EADDRNOTAVAIL; break;
                case 100: errno = ENETDOWN; break;
                case 101: errno = ENETUNREACH; break;
                case 102: errno = ENETRESET; break;
                case 103: errno = ECONNABORTED; break;
                case 104: errno = ECONNRESET; break;
                case 105: errno = ENOBUFS; break;
                case 106: errno = EISCONN; break;
                case 107: errno = ENOTCONN; break;
                // case 108: errno = ESHUTDOWN; break;
                // case 109: errno = ETOOMANYREFS; break;
                case 110: errno = ETIMEDOUT; break;
                case 111: errno = ECONNREFUSED; break;
                // case 112: errno = EHOSTDOWN; break;
                case 113: errno = EHOSTUNREACH; break;
                case 114: errno = EALREADY; break;
                case 115: errno = EINPROGRESS; break;
                // case 116: errno = ESTALE; break;
                // case 117: errno = EUCLEAN; break;
                // case 118: errno = ENOTNAM; break;
                // case 119: errno = ENAVAIL; break;
                // case 120: errno = EISNAM; break;
                // case 121: errno = EREMOTEIO; break;
                // case 122: errno = EDQUOT; break;
                // case 123: errno = ENOMEDIUM; break;
                // case 124: errno = EMEDIUMTYPE; break;
                case 125: errno = ECANCELED; break;
                // case 126: errno = ENOKEY; break;
                // case 127: errno = EKEYEXPIRED; break;
                // case 128: errno = EKEYREVOKED; break;
                // case 129: errno = EKEYREJECTED; break;
                case 130: errno = EOWNERDEAD; break;
                case 131: errno = ENOTRECOVERABLE; break;
                // case 132: errno = ERFKILL; break;
                // case 133: errno = EHWPOISON; break;

                default:
                    errno = EINVAL;
                    break;
            }
        }

        return -1;
    }

    return result;
}