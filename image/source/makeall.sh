#!/bin/sh
rm -f *.eap
rm -f *.o

. /opt/axis/acapsdk/3.01.0/aarch64/environment-setup-aarch64-poky-linux
create-package.sh

rm -f *.o

. /opt/axis/acapsdk/3.01.0/cortexa9hf-neon/environment-setup-cortexa9hf-neon-poky-linux-gnueabi
create-package.sh
rm -f *.old
rm -f *.o
rm -f *.txt
rm -f *.orig
rm -f .*.old
rm -f .*.txt
