#!/bin/bash

CMD=$1          # specs or bugs
INPUT=$2        # artifact or camera
TARGET=$3       # project or "all" or "small"
DOCKER=$4       # docker or local

if [ -z "$CMD" ] || [ -z "$INPUT" ] || [ -z "$TARGET" ] || [ -z "$DOCKER" ]; then
    echo "Usage: tabledata.sh specs-or-bugs artifact-or-camera project docker-or-local"
    exit 1
fi

RUN_PIDGIN_OTRNG=0
RUN_OPENSSL=0
RUN_MBEDTLS=0
RUN_NETDATA=0
RUN_LINUX_FS=0
RUN_LINUX_NFC=0
RUN_LINUX_FULLKERNEL=0
RUN_LITTLEFS=0
RUN_ZLIB=0

if [ "$TARGET" = "all" ]; then
    RUN_PIDGIN_OTRNG=1
    RUN_OPENSSL=1
    RUN_MBEDTLS=1
    RUN_NETDATA=1
    RUN_LINUX_FS=1
    RUN_LINUX_NFC=1
    RUN_LINUX_FULLKERNEL=1
    RUN_LITTLEFS=1
    RUN_ZLIB=1
elif [ "$TARGET" = "small" ]; then
    RUN_PIDGIN_OTRNG=1
    RUN_OPENSSL=1
    RUN_MBEDTLS=1
    RUN_NETDATA=1
    RUN_LINUX_NFC=1
    RUN_LITTLEFS=1
    RUN_ZLIB=1
elif [ "$TARGET" = "pidgin-otrng" ]; then
    RUN_PIDGIN_OTRNG=1
elif [ "$TARGET" = "openssl" ]; then
    RUN_OPENSSL=1
elif [ "$TARGET" = "mbedtls" ]; then
    RUN_MBEDTLS=1
elif [ "$TARGET" = "netdata" ]; then
    RUN_NETDATA=1
elif [ "$TARGET" = "linux-fs" ]; then
    RUN_LINUX_FS=1
elif [ "$TARGET" = "linux-nfc" ]; then
    RUN_LINUX_NFC=1
elif [ "$TARGET" = "linux-fullkernel" ]; then
    RUN_LINUX_FULLKERNEL=1
elif [ "$TARGET" = "littlefs" ]; then
    RUN_LITTLEFS=1
elif [ "$TARGET" = "zlib" ]; then
    RUN_ZLIB=1
fi

# Get source directory of script
# https://stackoverflow.com/questions/59895/get-the-source-directory-of-a-bash-script-from-within-the-script-itself
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

cd "$DIR/../.."

# Download data directory if necessary
if [ ! -d "data" ]; then
    curl https://eesi-data.s3-us-west-2.amazonaws.com/data.tgz -o data.tgz
    tar xf data.tgz
fi

mkdir -p results/$INPUT

if [ "$DOCKER" = "docker" ]; then
    EESI_CMD="docker run --rm -v $PWD:/d defreez/eesi:artifact"
elif [ "$DOCKER" = "local" ]; then
    EESI_CMD="src/build/eesi"
fi

if [ "$RUN_PIDGIN_OTRNG" = "1" ]; then
    BC="data/bitcode/pidgin-otrng-reg2mem.bc"
    INPUTSPECS="config/input-specs-pidgin-otrng.txt"
    OUTPUTSPECS="results/$INPUT/pidgin-otrng-specs.txt"
    OUTPUTSPECTIME="results/$INPUT/pidgin-otrng-spectime.txt"
    OUTPUTBUGS="results/$INPUT/pidgin-otrng-bugs.txt"
    OUTPUTBUGTIME="results/$INPUT/pidgin-otrng-bugtime.txt"
    if [ "$CMD" = "specs" ]; then
        echo "Pidgin OTRNGv4 specs..."
        (time $EESI_CMD --bitcode $BC --command specs --inputspecs $INPUTSPECS | grep -v WARNING | sort -u) > $OUTPUTSPECS 2> $OUTPUTSPECTIME
    elif [ "$CMD" = "bugs" ]; then
        echo "Pidgin OTRNGv4 bugs..."
        (time $EESI_CMD --bitcode $BC --command bugs --specs $OUTPUTSPECS | grep -v WARNING | sort -u) > $OUTPUTBUGS 2> $OUTPUTBUGTIME
    fi
fi

if [ "$RUN_OPENSSL" = "1" ]; then
    BC="data/bitcode/openssl-reg2mem.bc"
    INPUTSPECS="config/input-specs-openssl.txt"
    ERRORONLY="config/error-only-openssl.txt"
    OUTPUTSPECS="results/$INPUT/openssl-specs.txt"
    OUTPUTSPECTIME="results/$INPUT/openssl-spectime.txt"
    OUTPUTBUGS="results/$INPUT/openssl-bugs.txt"
    OUTPUTBUGTIME="results/$INPUT/openssl-bugtime.txt"
    if [ "$CMD" = "specs" ]; then
        echo "OpenSSL specs..."
        (time $EESI_CMD --bitcode $BC --command specs --inputspecs $INPUTSPECS --erroronly $ERRORONLY | grep -v WARNING | sort -u) > $OUTPUTSPECS 2> $OUTPUTSPECTIME
    elif [ "$CMD" = "bugs" ]; then
        echo "OpenSSL bugs..."
        (time $EESI_CMD --bitcode $BC --command bugs --specs $OUTPUTSPECS | grep -v WARNING | sort -u) > $OUTPUTBUGS 2> $OUTPUTBUGTIME
    fi
fi


if [ "$RUN_MBEDTLS" = "1" ]; then
    BC="data/bitcode/embedtls-reg2mem.bc"
    INPUTSPECS="config/input-specs-mbedtls.txt"
    ERRORONLY="config/error-only-mbedtls.txt"
    OUTPUTSPECS="results/$INPUT/mbedtls-specs.txt"
    OUTPUTSPECTIME="results/$INPUT/mbedtls-spectime.txt"
    OUTPUTBUGS="results/$INPUT/mbedtls-bugs.txt"
    OUTPUTBUGTIME="results/$INPUT/mbedtls-bugtime.txt"
    if [ "$CMD" = "specs" ]; then
        echo "mbedTLS specs..."
        (time $EESI_CMD --bitcode $BC --command specs --inputspecs $INPUTSPECS --erroronly $ERRORONLY | grep -v WARNING | sort -u) > $OUTPUTSPECS 2> $OUTPUTSPECTIME
    elif [ "$CMD" = "bugs" ]; then
        echo "mbedTLS bugs..."
        (time $EESI_CMD --bitcode $BC --command bugs --specs $OUTPUTSPECS | grep -v WARNING | sort -u) > $OUTPUTBUGS 2> $OUTPUTBUGTIME
    fi
fi

if [ "$RUN_NETDATA" = "1" ]; then
    BC="data/bitcode/netdata-reg2mem.bc"
    INPUTSPECS="config/input-specs-netdata.txt"
    ERRORONLY="config/error-only-netdata.txt"
    OUTPUTSPECS="results/$INPUT/netdata-specs.txt"
    OUTPUTSPECTIME="results/$INPUT/netdata-spectime.txt"
    OUTPUTBUGS="results/$INPUT/netdata-bugs.txt"
    OUTPUTBUGTIME="results/$INPUT/netdata-bugtime.txt"
    if [ "$CMD" = "specs" ]; then
        echo "netdata specs..."
        (time $EESI_CMD --bitcode $BC --command specs --inputspecs $INPUTSPECS --erroronly $ERRORONLY | grep -v WARNING | sort -u) > $OUTPUTSPECS 2> $OUTPUTSPECTIME
    elif [ "$CMD" = "bugs" ]; then
        echo "netdata bugs..."
        (time $EESI_CMD --bitcode $BC --command bugs --specs $OUTPUTSPECS | grep -v WARNING | sort -u) > $OUTPUTBUGS 2> $OUTPUTBUGTIME
    fi
fi

if [ "$RUN_LINUX_FS" = "1" ]; then
    BC="data/bitcode/fsmm-errcode-reg2mem-debug.bc"
    INPUTSPECS="config/input-specs-linux.txt"
    ERRORONLY="config/error-only-linux.txt"
    OUTPUTSPECS="results/$INPUT/linux-fs-specs.txt"
    OUTPUTSPECTIME="results/$INPUT/linux-fs-spectime.txt"
    OUTPUTBUGS="results/$INPUT/linux-fs-bugs.txt"
    OUTPUTBUGTIME="results/$INPUT/linux-fs-bugtime.txt"
    if [ "$CMD" = "specs" ]; then
        echo "Linux FS specs..."
        (time $EESI_CMD --bitcode $BC --command specs --inputspecs $INPUTSPECS --erroronly $ERRORONLY | grep -v WARNING | sort -u) > $OUTPUTSPECS 2> $OUTPUTSPECTIME
    elif [ "$CMD" = "bugs" ]; then
        echo "Linux FS bugs..."
        (time $EESI_CMD --bitcode $BC --command bugs --specs $OUTPUTSPECS | grep -v WARNING | sort -u) > $OUTPUTBUGS 2> $OUTPUTBUGTIME
    fi
fi

if [ "$RUN_LINUX_NFC" = "1" ]; then
    BC="data/bitcode/nfc-reg2mem-errcode.bc"
    INPUTSPECS="config/input-specs-linux.txt"
    OUTPUTSPECS="results/$INPUT/linux-nfc-specs.txt"
    OUTPUTSPECTIME="results/$INPUT/linux-nfc-spectime.txt"
    OUTPUTBUGS="results/$INPUT/linux-nfc-bugs.txt"
    OUTPUTBUGTIME="results/$INPUT/linux-nfc-bugtime.txt"
    if [ "$CMD" = "specs" ]; then
        echo "Linux NFC specs..."
        (time $EESI_CMD --bitcode $BC --command specs --inputspecs $INPUTSPECS | grep -v WARNING | sort -u) > $OUTPUTSPECS 2> $OUTPUTSPECTIME
    elif [ "$CMD" = "bugs" ]; then
        echo "Linux NFC bugs..."
        (time $EESI_CMD --bitcode $BC --command bugs --specs $OUTPUTSPECS | grep -v WARNING | sort -u) > $OUTPUTBUGS 2> $OUTPUTBUGTIME
    fi
fi

if [ "$RUN_LINUX_FULLKERNEL" = "1" ]; then
    BC="data/bitcode/llvmlinux-defconfig-reg2mem-debug.bc"
    INPUTSPECS="config/input-specs-linux.txt"
    OUTPUTSPECS="results/$INPUT/linux-fullkernel-specs.txt"
    OUTPUTSPECTIME="results/$INPUT/linux-fullkernel-spectime.txt"
    OUTPUTBUGS="results/$INPUT/linux-fullkernel-bugs.txt"
    OUTPUTBUGTIME="results/$INPUT/linux-fullkernel-bugtime.txt"
    if [ "$CMD" = "specs" ]; then
        echo "Full Linux kernel specs..."
        (time $EESI_CMD --bitcode $BC --command specs --inputspecs $INPUTSPECS | grep -v WARNING | sort -u) > $OUTPUTSPECS 2> $OUTPUTSPECTIME
    elif [ "$CMD" = "bugs" ]; then
        echo "Full Linux kernel bugs..."
        (time $EESI_CMD --bitcode $BC --command bugs --specs $OUTPUTSPECS | grep -v WARNING | sort -u) > $OUTPUTBUGS 2> $OUTPUTBUGTIME
    fi
fi

if [ "$RUN_LITTLEFS" = "1" ]; then
    BC="data/bitcode/lfs-reg2mem.bc"
    INPUTSPECS="config/input-specs-malloc.txt"
    OUTPUTSPECS="results/$INPUT/littlefs-specs.txt"
    OUTPUTSPECTIME="results/$INPUT/littlefs-spectime.txt"
    OUTPUTBUGS="results/$INPUT/littlefs-bugs.txt"
    OUTPUTBUGTIME="results/$INPUT/littlefs-bugtime.txt"
    if [ "$CMD" = "specs" ]; then
        echo "LittleFS specs..."
        (time $EESI_CMD --bitcode $BC --command specs --inputspecs $INPUTSPECS | grep -v WARNING | sort -u) > $OUTPUTSPECS 2> $OUTPUTSPECTIME
    elif [ "$CMD" = "bugs" ]; then
        echo "LittleFS bugs..."
        (time $EESI_CMD --bitcode $BC --command bugs --specs $OUTPUTSPECS | grep -v WARNING | sort -u) > $OUTPUTBUGS 2> $OUTPUTBUGTIME
    fi
fi

if [ "$RUN_ZLIB" = "1" ]; then
    BC="data/bitcode/libz.so.1.2.11.errorcodes-reg2mem.bc"
    INPUTSPECS="config/input-specs-malloc.txt"
    OUTPUTSPECS="results/$INPUT/zlib-specs.txt"
    OUTPUTSPECTIME="results/$INPUT/zlib-spectime.txt"
    OUTPUTBUGS="results/$INPUT/zlib-bugs.txt"
    OUTPUTBUGTIME="results/$INPUT/zlib-bugtime.txt"
    if [ "$CMD" = "specs" ]; then
        echo "zlib specs..."
        (time $EESI_CMD --bitcode $BC --command specs --inputspecs $INPUTSPECS | grep -v WARNING | sort -u) > $OUTPUTSPECS 2> $OUTPUTSPECTIME
    elif [ "$CMD" = "bugs" ]; then
        echo "zlib bugs..."
        (time $EESI_CMD --bitcode $BC --command bugs --specs $OUTPUTSPECS | grep -v WARNING | sort -u) > $OUTPUTBUGS 2> $OUTPUTBUGTIME
    fi
fi
